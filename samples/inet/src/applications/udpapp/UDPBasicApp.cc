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

#include "InterfaceTableAccess.h"
#include "IPvXAddressResolver.h"
#include "NodeOperations.h"
#include "UDPControlInfo_m.h"
#include "DataPacket_m.h"


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
    Logger::getInstance().info("Bus %d ending with %d packets in queue\n",id,packetQueue.size());
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
        Logger::getInstance().setLevel(Logger::INFO);
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
    if (timeToLive != -1)
        socket.setTimeToLive(timeToLive);

    int typeOfService = par("typeOfService");
    if (typeOfService != -1)
        socket.setTypeOfService(typeOfService);

    const char *multicastInterface = par("multicastInterface");
    if (multicastInterface[0])
    {
        IInterfaceTable *ift = InterfaceTableAccess().get(this);
        InterfaceEntry *ie = ift->getInterfaceByName(multicastInterface);
        if (!ie)
            throw cRuntimeError("Wrong multicastInterface setting: no interface named \"%s\"", multicastInterface);
        socket.setMulticastOutputInterface(ie->getInterfaceId());
    }

    bool receiveBroadcast = par("receiveBroadcast");
    if (receiveBroadcast)
        socket.setBroadcast(true);

    bool joinLocalMulticastGroups = par("joinLocalMulticastGroups");
    if (joinLocalMulticastGroups)
        socket.joinLocalMulticastGroups();
}

IPvXAddress UDPBasicApp::chooseDestAddr()
{
    int k = intrand(destAddresses.size());
    return destAddresses[k];
}

DataPacket* UDPBasicApp::copyPacket(DataPacket * p){
    Logger::getInstance().trace("Copy packet\n");
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage(p->getDebugMessage());
    msg->setTimestamp(p->getTimestamp());
    msg->setTemperature(p->getTemperature());
    msg->setBusid(p->getBusid());
    msg->setUuid(p->getUuid());
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
        socket.sendTo(p, destAddr, destPort);
        scheduleAt(simTime()+0.5,timeoutMsg);
        Logger::getInstance().trace("Timeout scheduled in 0.5s\n");
    } else {
        Logger::getInstance().trace("Packet queue empty\n");
    }
}

void UDPBasicApp::processStart()
{
    Logger::getInstance().trace("processStart()\n");
    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);
    setSocketOptions();

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

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
    socket.close();
}

void UDPBasicApp::addPacketToQueue(DataPacket *p){
    Logger::getInstance().info("Bus %d generated packet\n",id);
    numSent++;
    packetQueue.push(p);
    if(queueSize != -1){
        if(packetQueue.size() > queueSize){
            DataPacket *old = packetQueue.front();
            packetQueue.pop();
            delete old;
            numDropped++;
            Logger::getInstance().info("Bus %d dropped packet\n",id);
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
            Logger::getInstance().trace("Adding data in 1 second\n");
        } else if (msg == timeoutMsg){
            Logger::getInstance().trace("Packet timed out. Resending\n");
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
    numReceived++;
    if(dynamic_cast<DataPacket *>(pk)){
        DataPacket *p = check_and_cast<DataPacket *>(pk);
        Logger::getInstance().trace("Got packet response: %d\n",p->getUuid());
        if(p->getUuid() == lastSend){
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
            Logger::getInstance().trace("Incorrect sequence number: %d. Ignoring. \n",p->getUuid());
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
    scheduleAt(start+1, addData);
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

