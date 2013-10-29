//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2010 OpenSim Ltd.
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


#include <omnetpp.h>
#include "KmlUtil.h"
#include "ChannelController.h"
#include <sqlite3.h>
#include <ctime>

/**
 * A mobile node.
 */
class MobileNode : public cSimpleModule, public IMobileNode
{
  protected:
    // configuration
    double playgroundLat,playgroundLon;  // NW corner of playground, in degrees
    double playgroundHeight,playgroundWidth;  // in meters
    double timeStep;
    unsigned int trailLength;
    std::string color;
    bool showTxRange;
    double txRange;
    unsigned int currentStep;
    double positions[60][2];
    bool hidden[60];


    // node position and heading (speed is constant in this model)
    double heading; // in degrees
    double x, y; // in meters, relative to playground origin
    int id; //Id of this bus
    int updateDelta;//seconds
    bool visible;

    int nextTime;

    std::vector<KmlUtil::Pt2D> path; // for visualization


  public:
     MobileNode();
     virtual ~MobileNode();

     double getX() { return x; }
     double getY() { return y; }
     double getTxRange() { return txRange; }
     int static_callback(void *obj, int argc, char **argv, char **azColName);


  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual std::string getKmlFragment();
    int initializePositions(int currentPosition, int updateDelta);
    char* getTimeFromOffset(int offset);

};

Define_Module(MobileNode);

MobileNode::MobileNode()
{
}

MobileNode::~MobileNode()
{
}

void MobileNode::initialize()
{
    updateDelta = 5;//seconds
    currentStep = 0;
    id = par("id");
    printf("Initializing node with id: %d\n",id);
    nextTime = initializePositions(0,updateDelta);
    heading = 0; //TODO: Calculate heading
    timeStep = par("timeStep");

    getDisplayString().setTagArg("p", 0, x);
    getDisplayString().setTagArg("p", 1, y);

    playgroundLat = simulation.getSystemModule()->par("playgroundLatitude");
    playgroundLon = simulation.getSystemModule()->par("playgroundLongitude");
    playgroundHeight = simulation.getSystemModule()->par("playgroundHeight");
    playgroundWidth = simulation.getSystemModule()->par("playgroundWidth");

    trailLength = par("trailLength");
    showTxRange = par("showTxRange");
    txRange = par("txRange");

    color = par("color").stringValue();
    if (color.empty())
    {
        // pick a color with a random hue
        char buf[16];
        double red,green,blue;
        KmlUtil::hsbToRgb(dblrand(), 1.0, 1.0, red, green, blue);
        sprintf(buf, "%2.2x%2.2x%2.2x", int(blue*255), int(green*255), int(red*255));
        color = buf;
    }

    ChannelController::getInstance()->addMobileNode(this);

    // schedule first move
    cMessage *timer = new cMessage("move");
    scheduleAt(simTime(), timer);
}

void MobileNode::handleMessage(cMessage *msg)
{

    currentStep++;
    if(currentStep > 86400){ //24 hours
        printf("SIMULATION FINISHED. 24 HOURS HAS PAST.\n");
        return;
    }

    if(currentStep % updateDelta == 0){
        nextTime = initializePositions(nextTime,updateDelta);
    } else {
        printf("Current time step: %d\n",currentStep);
        printf("Current time: %s\n", getTimeFromOffset(nextTime + (currentStep % updateDelta)));
    }


    // update position
    x = positions[currentStep % updateDelta][1];
    y = positions[currentStep % updateDelta][0];
    visible = !hidden[currentStep % updateDelta];


    //if(visible){

        if(x == (1 << 31) || y == (1<<31)){
            printf("Something terrible has happened: (x,y) -> (%f,%f)\n",x,y);
        } else {
            printf("Current position of node %d: %f, %f, %s\n",id,x,y, (visible ? "visible" : "hidden"));
        }

        // store the position to be able to create a trail
        if (trailLength > 0)
            path.push_back(KmlUtil::Pt2D(x,y));

        // Trail is at max length. Remove the oldest point to keep it at "trailLength"
        // note: this is not very efficient because entire vector is shifted down; should use circular buffer
        if (path.size () > trailLength)
            path.erase(path.begin());

        getDisplayString().setTagArg("p", 0, x);
        getDisplayString().setTagArg("p", 1, y);
    /*} else {
        printf("Node Hidden\n");
    }*/

    // schedule next move
    scheduleAt(simTime()+timeStep, msg);
}

std::string MobileNode::getKmlFragment()
{
    std::string fragment;
    if(visible){
    double longitude = x;
    double latitude = y;
    char buf[16];
    sprintf(buf, "%d", getIndex());

    fragment += KmlUtil::folderHeader((std::string("folder_")+buf).c_str(), getFullName());

#ifdef USE_TRACK
    fragment += KmlUtil::track((std::string("track_")+buf).c_str(), path, timeStep, 1, "", "movement trail", NULL, (std::string("ff")+color).c_str());
#else
    fragment += KmlUtil::placemark((std::string("placemark_")+buf).c_str(), longitude, latitude, 2, getFullName(), NULL);
    if (trailLength > 0)
        fragment += KmlUtil::lineString((std::string("trail_")+buf).c_str(), path, "movement trail", NULL, (std::string("ff")+color).c_str());
    if (showTxRange)
        fragment += KmlUtil::disk((std::string("disk_")+buf).c_str(), longitude, latitude, txRange, "transmission range", NULL, (std::string("40")+color).c_str());
#endif

    fragment += "</Folder>\n";
    } else {
        fragment = "";
    }
    return fragment;
}

int MobileNode::initializePositions(int currentPosition, int updateDelta)
{

    char* stimeStr = getTimeFromOffset(currentPosition);
    char* etimeStr = getTimeFromOffset(currentPosition + updateDelta);

    char *sqlQuery = (char*)malloc(500);
    sprintf(sqlQuery,
            "SELECT Latitude, Longitude, Hidden FROM  InterpolatedPositions WHERE CanonicalId='%d' AND Time>'%s' AND Time<='%s' ORDER BY Time ASC;",
            id,
            stimeStr,
            etimeStr);

    printf("%s\n",sqlQuery);


    sqlite3 *db;

    int rc = sqlite3_open("database.sqlite", &db);

    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return(1);
    }

    //-----------------------------------------------------------------

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sqlQuery, -1, &stmt, NULL);
    if (rc != SQLITE_OK){
        printf("%s",sqlite3_errmsg(db));
        return -1;
    }

    rc = sqlite3_step(stmt);
    int counter = 0;
    while(rc == SQLITE_ROW){
        if(counter >=60) break;
        double latitude     = sqlite3_column_double(stmt, 0);
        double longitude    = sqlite3_column_double(stmt, 1);
        bool h              = (bool)sqlite3_column_int(stmt,2);
        if(latitude < -90){
            printf("LATITUDE IS HUUUUUUGE");
        }
        positions[counter][0] = latitude;
        positions[counter][1] = longitude;
        hidden[counter] = h;
        printf("%f,%f, %s\n",positions[counter][0],positions[counter][1],(h ? "HIDDEN" : "VISIBLE"));
        counter++;
        rc = sqlite3_step(stmt);
    }
    if(rc != SQLITE_DONE){
        printf("An error occurred");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return currentPosition + 60;
}

char* MobileNode::getTimeFromOffset(int offset){

       //2013-08-09 18:10:59
       //1376071859
       time_t t    = 1376071859 + offset;

       tm tLocal   = *gmtime(&t);

       int year    = 1900 + tLocal.tm_year;
       int month   = 1 + tLocal.tm_mon; //months since January
       int day     = tLocal.tm_mday;
       int hour    = tLocal.tm_hour;
       int minute  = tLocal.tm_min;
       int second  = tLocal.tm_sec;

       //YYYY-mm-dd hh:mm:ss
       char* timeStr = (char*)malloc(20*sizeof(char));
       sprintf(timeStr,"%04d-%02d-%02d %02d:%02d:%02d",year,month,day,hour,minute,second);

       return timeStr;

}


