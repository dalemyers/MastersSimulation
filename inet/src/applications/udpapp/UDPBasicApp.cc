//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2004,2011 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


#include "UDPBasicApp.h"
#include "IPv4InterfaceData.h"
#include "IInterfaceTable.h"
#include "InterfaceTableAccess.h"
#include "IPvXAddressResolver.h"
#include "NodeOperations.h"
#include "UDPControlInfo_m.h"
#include "DataPacket_m.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

Define_Module(UDPBasicApp);

simsignal_t UDPBasicApp::sentPkSignal = SIMSIGNAL_NULL;
simsignal_t UDPBasicApp::rcvdPkSignal = SIMSIGNAL_NULL;

UDPBasicApp::UDPBasicApp()
{
    selfMsg = NULL;
}

UDPBasicApp::~UDPBasicApp()
{
    cancelAndDelete(selfMsg);
    cancelAndDelete(timeoutMsg);
    cancelAndDelete(addData);
    Logger::getInstance().trace("Bus %d has %d packets left in queue\n",id,packetQueue.size());
    while(packetQueue.size() > 0){
        DataPacket *p = packetQueue.front();
        packetQueue.pop();
        delete p;
    }
}

void UDPBasicApp::initialize(int stage)
{
    AppBase::initialize(stage);

    // because of IPvXAddressResolver, we need to wait until interfaces are registered,
    // address auto-assignment takes place etc.
    if (stage == 0)
    {
        Logger::getInstance().setLevel(Logger::TRACE);
        numSent = 0;
        numDropped = 0;
        numReceived = 0;
        sequenceNumber = 0;
        lastSend = -1;
        WATCH(numSent);
        WATCH(numDropped);
        WATCH(numReceived);
        sentPkSignal = registerSignal("sentPk");
        rcvdPkSignal = registerSignal("rcvdPk");

        logLocation = (char*)par("logLocation").stringValue();
        Logger::getInstance().setLocation(logLocation);
        queueSize = par("queueSize");
        localPort = par("localPort");
        destPort = par("destPort");
        startTime = par("startTime").doubleValue();
        stopTime = par("stopTime").doubleValue();
        if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
            error("Invalid startTime/stopTime parameters");
        selfMsg = new cMessage("sendTimer");
        addData = new cMessage("addData");
        timeoutMsg = new cMessage("timeout");

        cModule* p = this->getParentModule();
        id = p->par("id");
    }
}

void UDPBasicApp::finish()
{
    recordScalar("packets generated", numSent);
    recordScalar("packets dropped",numDropped);
    recordScalar("packets received", numReceived);
    AppBase::finish();
}

void UDPBasicApp::setSocketOptions()
{
    int timeToLive = par("timeToLive");
    if (timeToLive != -1){
        apSocket.setTimeToLive(timeToLive);
        busSocket.setTimeToLive(timeToLive);
    }

    int typeOfService = par("typeOfService");
    if (typeOfService != -1){
        apSocket.setTypeOfService(typeOfService);
        busSocket.setTypeOfService(typeOfService);
    }

    const char *multicastInterface = par("multicastInterface");
    if (multicastInterface[0])
    {
        IInterfaceTable *ift = InterfaceTableAccess().get(this);
        InterfaceEntry *ie = ift->getInterfaceByName(multicastInterface);
        if (!ie){
            throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
        }
        apSocket.setMulticastOutputInterface(ie->getInterfaceId());
        busSocket.setMulticastOutputInterface(ie->getInterfaceId());
    }

    apSocket.setBroadcast(false);
    busSocket.setBroadcast(true);

    apSocket.joinLocalMulticastGroups();
    busSocket.joinLocalMulticastGroups();

    busSocket.setMulticastOutputInterface(2);
    apSocket.setMulticastOutputInterface(2);
}

IPvXAddress UDPBasicApp::chooseDestAddr()
{
    int k = intrand(destAddresses.size());
    return destAddresses[k];
}

DataPacket* UDPBasicApp::createBroadcastPacket(DataPacket * p){
    Logger::getInstance().trace("create Broadcast packet\n");
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage("BROADCAST");
    msg->setTimestamp(p->getTimestamp());
    msg->setTemperature(p->getTemperature());
    msg->setBusid(p->getBusid());
    msg->setUuid(p->getUuid());
    msg->setBroadcastPacket(true);
    return msg;
}

DataPacket* UDPBasicApp::copyPacket(DataPacket * p){
    Logger::getInstance().trace("Copy packet\n");
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage(p->getDebugMessage());
    msg->setTimestamp(p->getTimestamp());
    msg->setTemperature(p->getTemperature());
    msg->setBusid(p->getBusid());
    msg->setUuid(p->getUuid());
    msg->setBroadcastPacket(false); //Should always be false unless explicitly set
    return msg;
}

DataPacket* UDPBasicApp::generateMessage(char* debugString){
    Logger::getInstance().trace("Generating packet\n");
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage(debugString);
    msg->setTimestamp(simTime());
    msg->setTemperature(4);
    msg->setBusid(id);
    msg->setUuid(sequenceNumber++);
    msg->setBroadcastPacket(false);
    return msg;
}

void UDPBasicApp::sendPacket()
{
    Logger::getInstance().trace("sendPacket()\n");
    if(!packetQueue.empty()){
        DataPacket *p = copyPacket(packetQueue.front());
        Logger::getInstance().info("Bus %d Sending packet with sequence %d\n",id,p->getUuid());
        IPvXAddress destAddr = chooseDestAddr();
        lastSend = p->getUuid();
        p->removeControlInfo();
        emit(sentPkSignal, p);
        apSocket.sendTo(p, destAddr, destPort);


        DataPacket *q = createBroadcastPacket(p);
        q->removeControlInfo();
        busSocket.sendTo(q,broadcastAddress,destPort+1);


        scheduleAt(simTime()+0.5,timeoutMsg);
        Logger::getInstance().trace("Timeout scheduled in 0.5s\n");
    } else {
        Logger::getInstance().trace("Packet queue empty\n");
    }
}

void UDPBasicApp::sendResponsePacket(DataPacket *p)
{
    Logger::getInstance().trace("sendResponsePacket()\n");
    Logger::getInstance().info("Bus %d Sending response packet with sequence %d\n",id,p->getUuid());
    p->removeControlInfo();
    emit(sentPkSignal, p);
    busSocket.sendTo(p,broadcastAddress,destPort+1);

    //We don't bother checking anything. Duplicates can be found in the database.
}

void UDPBasicApp::processStart()
{
    Logger::getInstance().trace("processStart()\n");

    IInterfaceTable *inet_ift;
    inet_ift = InterfaceTableAccess().get();

    IPv4Address temp;

    for (int32 i=0; i<inet_ift->getNumInterfaces(); ++i){
        if (inet_ift->getInterface(i)->ipv4Data()!=NULL){
            temp = inet_ift->getInterface(i)->ipv4Data()->getIPAddress();
        }
    }

    apSocket.setOutputGate(gate("udpOut"));
    apSocket.bind(localPort);
    busSocket.setOutputGate(gate("udpOut"));
    busSocket.bind(temp,localPort+1);
    setSocketOptions();

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

    IPvXAddressResolver().tryResolve("224.0.0.1", broadcastAddress);
    if(broadcastAddress.isUnspecified()){
        EV << "cannot resolve broadcast address" << endl;
        printf("FAIL\n");
    }

    while ((token = tokenizer.nextToken()) != NULL) {
        IPvXAddress result;
        IPvXAddressResolver().tryResolve(token, result);
        if (result.isUnspecified())
            EV << "cannot resolve destination address: " << token << endl;
        else
            destAddresses.push_back(result);
    }

}


void UDPBasicApp::processStop()
{
    apSocket.close();
    busSocket.close();
}

void UDPBasicApp::addPacketToQueue(DataPacket *p){
    Logger::getInstance().info("Bus %d adding packet to queue\n",id);
    numSent++;
    packetQueue.push(p);
    if(queueSize != -1){
        if(packetQueue.size() > queueSize){
            DataPacket *old = packetQueue.front();
            Logger::getInstance().info("Bus %d dropped packet %d\n",id,old->getUuid());
            packetQueue.pop();
            delete old;
            numDropped++;

        }
    }
    if(packetQueue.size() == 1){
        sendPacket();
    }
}


void UDPBasicApp::handleMessageWhenUp(cMessage *msg)
{
    Logger::getInstance().trace("handleMessage\n");
    if (msg->isSelfMessage())
    {
        Logger::getInstance().trace("isSelfMessage()\n");
        if(msg == selfMsg){
            Logger::getInstance().trace("Got self message\n");
            switch (selfMsg->getKind()) {
                case START:     processStart(); break;
                case SEND:      sendPacket(); break;
                case STOP:      processStop(); break;
                default: throw cRuntimeError("Invalid kind %d in self message", (int)selfMsg->getKind());
            }
        } else if (msg == addData) {
            Logger::getInstance().trace("Adding data\n");
            DataPacket *p = generateMessage((char*)"debugstr");
            addPacketToQueue(p);
            scheduleAt(simTime()+1,addData);
            Logger::getInstance().trace("Bus %d Adding data in 1 second\n",id);
        } else if (msg == timeoutMsg){
            Logger::getInstance().trace("Bus %d Packet timed out. Resending\n",id);
            sendPacket();
        } else {
            Logger::getInstance().error("GOT WEIRD MESSAGE\n");
        }
    }
    else if (msg->getKind() == UDP_I_DATA)
    {
        Logger::getInstance().trace("DATA\n");
        // process incoming packet
        processPacket(PK(msg));
    }
    else if (msg->getKind() == UDP_I_ERROR)
    {
        Logger::getInstance().error("Error\n");
        EV << "Ignoring UDP error report\n";
        delete msg;
    }
    else
    {
        error("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }

    if (ev.isGUI())
    {
        char buf[40];
        sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
        getDisplayString().setTagArg("t", 0, buf);
    }
}

void UDPBasicApp::processPacket(cPacket *pk)
{
    Logger::getInstance().trace("processPacket()\n");
    emit(rcvdPkSignal, pk);
    EV << "Received packet: " << UDPSocket::getReceivedPacketInfo(pk) << endl;
    if(dynamic_cast<DataPacket *>(pk)){
        DataPacket *p = check_and_cast<DataPacket *>(pk);
        if(p->getIsResponsePacket()){
            Logger::getInstance().trace("Bus %d got response packet\n",id);
            numReceived++;
            if(p->getBusid() == id && p->getUuid() == lastSend){
                //This is a response to one of our previous packets
                cancelEvent(timeoutMsg);
                Logger::getInstance().trace("Was valid sequence. Timeout cancelled.\n");
                if (selfMsg){
                    cancelEvent(selfMsg);
                    Logger::getInstance().trace("cancelled self message\n");
                }
                DataPacket *old = packetQueue.front();
                packetQueue.pop();
                delete old;
                sendPacket();
            } else {
                if(p->getBusid() != id){
                    Logger::getInstance().trace("Bus %d received a response for bus %d\n",id,p->getBusid());
                } else {
                    Logger::getInstance().trace("Bus %d received a response with invalid sequence number.\n",id);
                }
            }
        } else {
            //This is a data packet
            if(p->getBroadcastPacket() && p->getBusid() != id){
                Logger::getInstance().trace("Bus %d got a data packet from bus %d\n",id,p->getBusid());
                DataPacket *storedPacket = copyPacket(p);
                DataPacket *responsePacket = createBroadcastPacket(p);
                responsePacket->setIsResponsePacket(true);
                sendResponsePacket(responsePacket);
                addPacketToQueue(storedPacket);
            } else {
                if(!p->getBroadcastPacket()){
                    Logger::getInstance().trace("Bus %d received a data packet that was NOT a broadcast packet. THIS SHOULD NEVER HAPPEN.\n",id);
                } else {
                    Logger::getInstance().trace("Bus %d received its own data packet\n",id);
                }
            }
        }
        p = NULL;
    }
    delete pk;
}

bool UDPBasicApp::startApp(IDoneCallback *doneCallback)
{
    Logger::getInstance().trace("startApp()\n");
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))
    {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
        Logger::getInstance().trace("Scheduled start\n");
    }
    int randomInt = rand();
    float randomFloat = (float)randomInt;
    float randMax = (float)RAND_MAX;
    float randomTime = randomFloat/randMax;
    scheduleAt(start+randomTime, addData);
    Logger::getInstance().trace("Scheduled first data gather\n");
    return true;
}

bool UDPBasicApp::stopApp(IDoneCallback *doneCallback)
{
    Logger::getInstance().trace("stopApp()\n");
    if (selfMsg){
        cancelEvent(selfMsg);
        cancelEvent(timeoutMsg);
        cancelEvent(addData);
    }
    //TODO if(socket.isOpened()) socket.close();
    return true;
}

bool UDPBasicApp::crashApp(IDoneCallback *doneCallback)
{
    Logger::getInstance().trace("crashApp()\n");
    if (selfMsg){
        cancelEvent(selfMsg);
        cancelEvent(timeoutMsg);
        cancelEvent(addData);
    }
    return true;
}

