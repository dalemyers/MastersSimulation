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

#include "APChat.h"

#include "ModuleAccess.h"
#include "NotificationBoard.h"
#include "UDPSocket.h"
#include "DataPacket_m.h"

Define_Module(APChat);

void APChat::initialize(int stage) {
    cSimpleModule::initialize(stage);
    if (stage == 3) {
        debug = par("debug");
        cModule* p = this->getParentModule();
        id = p->par("id");
        setupLowerLayer();
    }
}

void APChat::setupLowerLayer() {
    socket.setOutputGate(gate("udp$o"));
    socket.joinLocalMulticastGroups();
    socket.bind(12345);
    socket.setBroadcast(true);
}

void APChat::handleMessage(cMessage* msg) {
    printf("AP GOT MESSAGE\n");
    if (msg->isSelfMessage()) {
        handleSelfMsg(msg);
    } else {
        handleLowerMsg(msg);
    }
}

void APChat::handleSelfMsg(cMessage* msg) {
    //printf("AP Handle Self Message");
}

void APChat::handleLowerMsg(cMessage* msg) {
    //printf("AP HANDLING LOWER MESSAGE\n");
    if(dynamic_cast<DataPacket *>(msg)){
        DataPacket *p = check_and_cast<DataPacket *>(msg);
        printf("AP %d RECEIVED MESSAGE %d FROM BUS %d\n",id,p->getUuid(),p->getBusid());
    }
}

void APChat::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj) {
    Enter_Method_Silent();
    /*printf("AP RECEIVED MESSAGE\n");
    if (signalID == mobilityStateChangedSignal) {
        printf("AP MESSAGE WAS MOBILITY STATE CHANGE\n");
        handlePositionUpdate();
    }*/
}
