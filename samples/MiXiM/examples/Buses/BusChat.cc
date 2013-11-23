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
#include <cstdlib>


Define_Module(BusChat);

void BusChat::initialize(int stage) {
    cSimpleModule::initialize(stage);
    id = 0;
    if (stage == 3) {
        packetCounter = 0;
        debug = par("debug");
        cModule* p = this->getParentModule();
        id = p->par("id");

        mobilityStateChangedSignal = registerSignal("mobilityStateChanged");

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
    scheduleAt(simTime() + 3, timer);
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
        //printf("Need to broadcast!\n");
        sendMessage();
    }
    delete msg;
}

void BusChat::handleLowerMsg(cMessage* msg) {
    //printf("%d, HANDLING LOWER MESSAGE -> %s\n",id,"msg");
    delete msg;
}

void BusChat::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj) {
    Enter_Method_Silent();
}

DataPacket* BusChat::generateMessage(char* debugString){
    DataPacket* msg = new DataPacket();
    msg->setDebugMessage(debugString);
    msg->setTimestamp(simTime());
    msg->setTemperature(4);
    msg->setBusid(id);
    msg->setUuid(packetCounter++);

    return msg;
}

void BusChat::sendMessage() {

    char* m = (char*)malloc(50*sizeof(char));
    sprintf(m,"Message from bus with id: %d\n",id);

    DataPacket *selfmsg = generateMessage(m);

    printf("Bus %d SENDING MESSAGE: %d\n",id,selfmsg->getUuid());

    socket.sendTo(selfmsg, IPv4Address("10.0.2.1"),12345);
    //socket.sendTo(selfmsg, IPv4Address::ALL_HOSTS_MCAST, 12345);

    scheduleAt(simTime() + 0.01, new cMessage("broadcast"));
}

void BusChat::handlePositionUpdate() {

}