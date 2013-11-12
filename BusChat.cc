//
// Copyright (C) 2006-2011 Christoph Sommer <christoph.sommer@uibk.ac.at>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "BusChat.h"

#include "ModuleAccess.h"
#include "NotificationBoard.h"
#include "UDPSocket.h"


Define_Module(BusChat);

void BusChat::initialize(int stage) {
    cSimpleModule::initialize(stage);
    id = 0;
    if (stage == 3) {
        debug = par("debug");
        cModule* p = this->getParentModule();
        id = p->par("id");

        mobilityStateChangedSignal = registerSignal("mobilityStateChanged");
        //traci = TraCIMobilityAccess().get();
        //traci->subscribe(mobilityStateChangedSignal, this);

        sentMessage = false;

        setupLowerLayer();
    }
}

void BusChat::setupLowerLayer() {
    socket.setOutputGate(gate("udp$o"));
    socket.joinLocalMulticastGroups();
    socket.bind(12345);
    socket.setBroadcast(true);
    cMessage *timer = new cMessage("broadcast");
    scheduleAt(simTime() + id + 1, timer);
}

void BusChat::handleMessage(cMessage* msg) {
    if (msg->isSelfMessage()) {
        handleSelfMsg(msg);
    } else {
        handleLowerMsg(msg);
    }
}

void BusChat::handleSelfMsg(cMessage* msg) {
    if(msg->getKind() == 9822){
        DataPacket *p = check_and_cast<DataPacket *>(msg);
        printf("GOT A DATA PACKET\n");
        printf("%s",p->getDebugMessage());
    } else {
        printf("Need to broadcast!\n");
        sendMessage();
    }
}

void BusChat::handleLowerMsg(cMessage* msg) {
    printf("%d, HANDLING LOWER MESSAGE -> %s\n",id,"msg");
    delete msg;
}

void BusChat::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj) {
    Enter_Method_Silent();
    printf("RECEIVED MESSAGE\n");
    if (signalID == mobilityStateChangedSignal) {
        printf("MESSAGE WAS MOBILITY STATE CHANGE\n");
        handlePositionUpdate();
    }
}

DataPacket* BusChat::generateMessage(char* debugString){
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage(debugString);
    msg->setTimestamp(simTime());
    msg->setTemperature(4);
    msg->setBusid(id);
    return msg;
}

void BusChat::sendMessage() {

    printf("Bus %d SENDING MESSAGE\n",id);

    cPacket* newMessage = new cPacket("Hurr");
    socket.sendTo(newMessage, IPv4Address("192.168.0.3"),12345);
    DataPacket *selfmsg = generateMessage((char*)"Hurr Durr");
    //socket.sendTo(selfmsg, IPv4Address::LOOPBACK_ADDRESS,12345);

    socket.sendTo(selfmsg, IPv4Address::ALL_HOSTS_MCAST, 12345);

    scheduleAt(simTime() + 10, new cMessage("broadcast"));
}

void BusChat::handlePositionUpdate() {

}