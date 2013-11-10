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
#include "DataPacket_m.h"

Define_Module(BusChat);

void BusChat::initialize(int stage) {
    printf("Intitalizing CHAT PROTOCOL\n");
    cSimpleModule::initialize(stage);
    id = 0;
    if (stage == 1)
    {
        bool isOperational;
        isOperational = true;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
    }
    else if (stage == 3) {
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
    if (!sentMessage) sendMessage();
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

void BusChat::sendMessage() {

    printf("Bus %d SENDING MESSAGE\n",id);
    sentMessage = true;

    cPacket* newMessage = new cPacket("Hurr");
    socket.sendTo(newMessage, IPv4Address("192.168.0.3"),12345);
    DataPacket *selfmsg = new DataPacket();
    selfmsg->setDebugMessage("Hurr Durr Debug");
    selfmsg->setKind(9822);
    //socket.sendTo(selfmsg, IPv4Address::LOOPBACK_ADDRESS,12345);
    socket.sendTo(selfmsg, IPv4Address::ALL_HOSTS_MCAST, 12345);
    cMessage *timer = new cMessage("broadcast");
    scheduleAt(simTime() + 10, timer);
}

void BusChat::handlePositionUpdate() {
    if (!sentMessage) sendMessage();
}
