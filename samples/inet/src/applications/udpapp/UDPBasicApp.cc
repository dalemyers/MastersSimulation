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
}

void UDPBasicApp::initialize(int stage)
{
    AppBase::initialize(stage);

    // because of IPvXAddressResolver, we need to wait until interfaces are registered,
    // address auto-assignment takes place etc.
    if (stage == 0)
    {
        numSent = 0;
        numReceived = 0;
        sequenceNumber = 0;
        lastSend = -1;
        WATCH(numSent);
        WATCH(numReceived);
        sentPkSignal = registerSignal("sentPk");
        rcvdPkSignal = registerSignal("rcvdPk");

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
    recordScalar("packets sent", numSent);
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
    Logger::getInstance().fprintf("Copy packet\n");
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage(p->getDebugMessage());
    msg->setTimestamp(p->getTimestamp());
    msg->setTemperature(p->getTemperature());
    msg->setBusid(p->getBusid());
    msg->setUuid(p->getUuid());
    return msg;
}

DataPacket* UDPBasicApp::generateMessage(char* debugString){
    Logger::getInstance().fprintf("Generating packet\n");
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
    Logger::getInstance().fprintf("sendPacket()\n");
    if(!packetQueue.empty()){
        DataPacket *p = copyPacket(packetQueue.front());
        Logger::getInstance().fprintf("Sending packet with sequence %d\n",p->getUuid());
        IPvXAddress destAddr = chooseDestAddr();
        lastSend = p->getUuid();
        p->removeControlInfo();
        emit(sentPkSignal, p);
        socket.sendTo(p, destAddr, destPort);
        numSent++;
        scheduleAt(simTime()+0.5,timeoutMsg);
        Logger::getInstance().fprintf("Timeout scheduled in 0.5s\n");
    } else {
        Logger::getInstance().fprintf("Packet queue empty\n");
    }
}

void UDPBasicApp::processStart()
{
    Logger::getInstance().fprintf("processStart()\n");
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


void UDPBasicApp::handleMessageWhenUp(cMessage *msg)
{
    Logger::getInstance().fprintf("handleMessage\n");
    if (msg->isSelfMessage())
    {
        Logger::getInstance().fprintf("isSelfMessage()\n");
        if(msg == selfMsg){
            Logger::getInstance().fprintf("Got self message\n");
            switch (selfMsg->getKind()) {
                case START:     processStart(); break;
                case SEND:      sendPacket(); break;
                case STOP:      processStop(); break;
                default: throw cRuntimeError("Invalid kind %d in self message", (int)selfMsg->getKind());
            }
        } else if (msg == addData) {
            Logger::getInstance().fprintf("Adding data\n");
            DataPacket *p = generateMessage((char*)"debugstr");
            packetQueue.push(p);
            if(packetQueue.size() == 1){
                sendPacket();
            }
            scheduleAt(simTime()+1,addData);
            Logger::getInstance().fprintf("Adding data in 1 second\n");
        } else if (msg == timeoutMsg){
            Logger::getInstance().fprintf("Packet timed out. Resending\n");
            sendPacket();
        } else {
            Logger::getInstance().fprintf("GOT WEIRD MESSAGE\n");
        }
    }
    else if (msg->getKind() == UDP_I_DATA)
    {
        Logger::getInstance().fprintf("DATA\n");
        // process incoming packet
        processPacket(PK(msg));
    }
    else if (msg->getKind() == UDP_I_ERROR)
    {
        Logger::getInstance().fprintf("Error\n");
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
    Logger::getInstance().fprintf("processPacket()\n");
    emit(rcvdPkSignal, pk);
    EV << "Received packet: " << UDPSocket::getReceivedPacketInfo(pk) << endl;
    numReceived++;
    if(dynamic_cast<DataPacket *>(pk)){
        DataPacket *p = check_and_cast<DataPacket *>(pk);
        Logger::getInstance().fprintf("Got packet response: %d\n",p->getUuid());
        if(p->getUuid() == lastSend){
            cancelEvent(timeoutMsg);
            Logger::getInstance().fprintf("Was valid sequence. Timeout cancelled.\n");
            if (selfMsg){
                cancelEvent(selfMsg);
                Logger::getInstance().fprintf("cancelled self message\n");
            }
            DataPacket *old = packetQueue.front();
            packetQueue.pop();
            delete old;
            sendPacket();
        } else {
            Logger::getInstance().fprintf("Incorrect sequence number: %d. Ignoring. \n",p->getUuid());
        }
        p = NULL;
    }
    delete pk;
}

bool UDPBasicApp::startApp(IDoneCallback *doneCallback)
{
    Logger::getInstance().fprintf("startApp()\n");
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime))
    {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
        Logger::getInstance().fprintf("Scheduled start\n");
    }
    scheduleAt(start+1, addData);
    Logger::getInstance().fprintf("Scheduled first data gather\n");
    return true;
}

bool UDPBasicApp::stopApp(IDoneCallback *doneCallback)
{
    Logger::getInstance().fprintf("stopApp()\n");
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
    Logger::getInstance().fprintf("crashApp()\n");
    if (selfMsg){
        cancelEvent(selfMsg);
        cancelEvent(timeoutMsg);
        cancelEvent(addData);
    }
    return true;
}

