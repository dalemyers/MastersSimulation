//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2010 OpenSim Ltd.
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


#include <omnetpp.h>
#include <sqlite3.h>
#include <ctime>
#include <cmath>

/**
 * A mobile node.
 */
class BusMobility : public cSimpleModule
{
  protected:
    // configuration
    double playgroundLat,playgroundLon;  // NW corner of playground, in degrees
    double playgroundHeight,playgroundWidth;  // in meters
    double timeStep;
    unsigned int currentStep;
    double positions[60][2];


    double x, y; // longitude and latitude,
    int id; //Id of this bus
    int updateDelta;//seconds
    int nextTime;


  public:
     BusMobility();
     virtual ~BusMobility();

     double getX() { return x; }
     double getY() { return y; }
     int static_callback(void *obj, int argc, char **argv, char **azColName);


  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    int initializePositions(int currentPosition, int updateDelta);
    char* getTimeFromOffset(int offset);
    double* getPlaygroundPosition(double x, double y);

};

Define_Module(BusMobility);

BusMobility::BusMobility()
{
}

BusMobility::~BusMobility()
{
}

void BusMobility::initialize()
{
    updateDelta = 30;//seconds
    currentStep = 0;
    id = getParentModule()->par("id");
    printf("Initializing node with id: %d\n",id);
    nextTime = initializePositions(0,updateDelta);
    timeStep = par("timeStep");

    getDisplayString().setTagArg("p", 0, x);
    getDisplayString().setTagArg("p", 1, y);

    playgroundLat = simulation.getSystemModule()->par("playgroundLatitude");
    playgroundLon = simulation.getSystemModule()->par("playgroundLongitude");
    playgroundHeight = simulation.getSystemModule()->par("playgroundHeight");
    playgroundWidth = simulation.getSystemModule()->par("playgroundWidth");

    // schedule first move
    cMessage *timer = new cMessage("move");
    scheduleAt(simTime(), timer);
}

void BusMobility::handleMessage(cMessage *msg)
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


    x = positions[currentStep % updateDelta][1];
    y = positions[currentStep % updateDelta][0];

    if(y < 55.9527 && y > 55.9396 && x < -3.1738 && x > -3.2197){

        double* positions = getPlaygroundPosition(x,y);
        x = positions[0];
        y = positions[1];

        printf("Current position of node %d: %f, %f\n",id,x,y);

        //I think this sets the position in the little diagram.
        getParentModule()->getDisplayString().setTagArg("p", 0, x);
        getParentModule()->getDisplayString().setTagArg("p", 1, y);
    } else {
        getParentModule()->getDisplayString().setTagArg("p", 0, -10.0);
        getParentModule()->getDisplayString().setTagArg("p", 1, -10.0);
    }


    // schedule next move
    scheduleAt(simTime()+timeStep, msg);
}


int BusMobility::initializePositions(int currentPosition, int updateDelta)
{

    char* stimeStr = getTimeFromOffset(currentPosition);
    char* etimeStr = getTimeFromOffset(currentPosition + updateDelta);

    char *sqlQuery = (char*)malloc(500);
    sprintf(sqlQuery,
            "SELECT Latitude, Longitude FROM  InterpolatedPositions WHERE CanonicalId='%d' AND Time>'%s' AND Time<='%s' ORDER BY Time ASC;",
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
        if(counter >updateDelta) break;
        double latitude     = sqlite3_column_double(stmt, 0);
        double longitude    = sqlite3_column_double(stmt, 1);

        positions[counter][0] = latitude;
        positions[counter][1] = longitude;

        printf("%f,%f\n",positions[counter][0],positions[counter][1]);
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

char* BusMobility::getTimeFromOffset(int offset){

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

double* BusMobility::getPlaygroundPosition(double x, double y){

    //Top left corner had coordinates 56, -3.4

    double TOPLEFTX = -3.4;
    double TOPLEFTY = 56;

    double* position = (double*)malloc(2*sizeof(double));

    double distanceBetweenDegreesLatitude = 111340.01;
    double distanceBetweenDegreesLongitude = 62473.28;
    double deltaLongitude = x - TOPLEFTX;
    double deltaLatitude = TOPLEFTY - y;

    position[0] = deltaLongitude*distanceBetweenDegreesLongitude;
    position[1] = deltaLatitude*distanceBetweenDegreesLatitude;

    return position;
}


