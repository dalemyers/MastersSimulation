//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2010 OpenSim Ltd.
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


#include <omnetpp.h>
#include "KmlHttpServer.h"
#include "KmlUtil.h"
#include "ChannelController.h"
#include <sqlite3.h>
#include <ctime>

/**
 * A mobile node.
 */
class MobileNode : public cSimpleModule, public IKmlFragmentProvider, public IMobileNode
{
  protected:
    // configuration
    double playgroundLat,playgroundLon;  // NW corner of playground, in degrees
    double playgroundHeight,playgroundWidth;  // in meters
    double timeStep;
    unsigned int trailLength;
    std::string color;
    std::string modelURL;
    double modelScale;
    bool showTxRange;
    double txRange;
    unsigned int currentStep;
    double positions[60][2];


    // node position and heading (speed is constant in this model)
    double heading; // in degrees
    double x, y; // in meters, relative to playground origin
    int id; //Id of this bus
    int updateDelta;//seconds

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
    updateDelta = 30;//seconds
    nextTime = 0;
    currentStep = 0;
    x = par("startX");
    y = par("startY");
    id = par("id");
    printf("Initializing node with id: %d\n",id);
    nextTime = initializePositions(nextTime,updateDelta);
    heading = 0; //TODO: Calculate heading
    timeStep = par("timeStep");

    getDisplayString().setTagArg("p", 0, x);
    getDisplayString().setTagArg("p", 1, y);

    playgroundLat = simulation.getSystemModule()->par("playgroundLatitude");
    playgroundLon = simulation.getSystemModule()->par("playgroundLongitude");
    playgroundHeight = simulation.getSystemModule()->par("playgroundHeight");
    playgroundWidth = simulation.getSystemModule()->par("playgroundWidth");

    trailLength = par("trailLength");
    modelURL = par("modelURL").stringValue();
    modelScale = par("modelScale");
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

    KmlHttpServer::getInstance()->addKmlFragmentProvider(this);
    ChannelController::getInstance()->addMobileNode(this);

    // schedule first move
    cMessage *timer = new cMessage("move");
    scheduleAt(simTime(), timer);
}

void MobileNode::handleMessage(cMessage *msg)
{

    currentStep++;
    if(currentStep > 19000){
        return;
    }

    if(currentStep % updateDelta == 0){
        nextTime = initializePositions(nextTime,updateDelta);
    } else {
        printf("Current time step: %d\n",currentStep);
    }

    // TODO: Calculate heading

    // update position
    x = positions[currentStep % 60][0];
    y = positions[currentStep % 60][1];

    printf("Current position of node %d: %f, %f\n",id,x,y);

    // store the position to be able to create a trail
    if (trailLength > 0)
        path.push_back(KmlUtil::Pt2D(x,y));

    // Trail is at max length. Remove the oldest point to keep it at "trailLength"
    // note: this is not very efficient because entire vector is shifted down; should use circular buffer
    if (path.size () > trailLength)
        path.erase(path.begin());

    getDisplayString().setTagArg("p", 0, x);
    getDisplayString().setTagArg("p", 1, y);

    // schedule next move
    scheduleAt(simTime()+timeStep, msg);
}

std::string MobileNode::getKmlFragment()
{
    double longitude = x;
    double latitude = y;
    char buf[16];
    sprintf(buf, "%d", getIndex());
    std::string fragment;
    fragment += KmlUtil::folderHeader((std::string("folder_")+buf).c_str(), getFullName());

#ifdef USE_TRACK
    fragment += KmlUtil::track((std::string("track_")+buf).c_str(), path, timeStep, modelScale, modelURL.c_str(), "movement trail", NULL, (std::string("ff")+color).c_str());
#else
    fragment += KmlUtil::placemark((std::string("placemark_")+buf).c_str(), longitude, latitude, 2*modelScale, getFullName(), NULL);
    if (trailLength > 0)
        fragment += KmlUtil::lineString((std::string("trail_")+buf).c_str(), path, "movement trail", NULL, (std::string("ff")+color).c_str());
    if (showTxRange)
        fragment += KmlUtil::disk((std::string("disk_")+buf).c_str(), longitude, latitude, txRange, "transmission range", NULL, (std::string("40")+color).c_str());
    if (!modelURL.empty()) {
        double modelheading = fmod((360 + 90 + heading), 360);
        fragment += KmlUtil::model((std::string("model_")+buf).c_str(), longitude, latitude, modelheading, modelScale, modelURL.c_str(), "3D model", NULL);
    }
#endif

    fragment += "</Folder>\n";
    return fragment;
}

int MobileNode::initializePositions(int currentPosition, int updateDelta)
{
    time_t t = time(0);
    //-----------------------------------------------------------

    //2013-08-09 18:10:59
    //1376071859
    time_t startTime    = 1376071859 + currentPosition;
    time_t endTime      = 1376071859 + currentPosition + updateDelta;

    printf("Raw current start time: %d\n",(int)startTime);
    printf("Raw current end   time: %d\n",(int)endTime);

    tm startLocalTime   = *gmtime(&startTime);
    tm endLocalTime     = *gmtime(&endTime);

    printf("%d-%d-%d %d:%d:%d\n",startLocalTime.tm_year,startLocalTime.tm_mon+1,startLocalTime.tm_mday,startLocalTime.tm_hour,startLocalTime.tm_min,startLocalTime.tm_sec);
    printf("%d-%d-%d %d:%d:%d\n",endLocalTime.tm_year,endLocalTime.tm_mon+1,endLocalTime.tm_mday,endLocalTime.tm_hour,endLocalTime.tm_min,endLocalTime.tm_sec);


    //--------------

    int year    = 1900 + startLocalTime.tm_year;
    int month   = 1 + startLocalTime.tm_mon; //months since January
    int sday    = startLocalTime.tm_mday;
    int shour   = startLocalTime.tm_hour;
    int sminute = startLocalTime.tm_min;
    int ssecond = startLocalTime.tm_sec;
    int eday    = endLocalTime.tm_mday;
    int ehour   = endLocalTime.tm_hour;
    int eminute = endLocalTime.tm_min;
    int esecond = endLocalTime.tm_sec;

    //YYYY-mm-dd hh:mm:ss
    char* stimeStr = (char*)malloc(20*sizeof(char));
    char* etimeStr = (char*)malloc(20*sizeof(char));
    sprintf(stimeStr,"%04d-%02d-%02d %02d:%02d:%02d",year,month,sday,shour,sminute,ssecond);
    sprintf(etimeStr,"%04d-%02d-%02d %02d:%02d:%02d",year,month,eday,ehour,eminute,esecond);

    /*printf("Starting Time:\n");
    printf("%s",stimeStr);
    printf("\nEndingTime:\n");
    printf("%s",etimeStr);
    printf("\n");*/


    char *sqlQuery = (char*)malloc(500);
    sprintf(sqlQuery,
            "Select InterpolatedPositions.Latitude,InterpolatedPositions.Longitude,InterpolatedPositions.Time FROM InterpolatedPositions JOIN CanonicalBusId ON InterpolatedPositions.BusId=CanonicalBusId.BusId WHERE CanonicalBusId.CanonicalId=%d AND InterpolatedPositions.Time>'%s' AND InterpolatedPositions.Time<='%s' ORDER BY InterpolatedPositions.Time ASC",
            id,
            stimeStr,
            etimeStr);

    printf("%s",sqlQuery);


    sqlite3 *db;

    int rc = sqlite3_open("/Volumes/Data/database.sqlite", &db);

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
        //int timestamp       = sqlite3_column_int(stmt, 2);
        positions[counter][0] = latitude;
        positions[counter][1] = longitude;
        counter++;
        //printf("%f,%f,%d\n",latitude,longitude,timestamp);

        rc = sqlite3_step(stmt);
    }
    if(rc != SQLITE_DONE){
        printf("An error occurred");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    int diff = (int)time(0) - (int)t;
    printf("Database read time taken: %d\n",diff);

    return currentPosition + 60;
}



