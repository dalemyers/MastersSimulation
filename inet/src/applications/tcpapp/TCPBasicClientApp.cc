//
// Copyright (C) 2004 Andras Varga
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


#include "TCPBasicClientApp.h"
#include "NodeOperations.h"
#include "ModuleAccess.h"

#define MSGKIND_CONNECT  0
#define MSGKIND_SEND     1


Define_Module(TCPBasicClientApp);

TCPBasicClientApp::TCPBasicClientApp()
{
    timeoutMsg = NULL;
}

TCPBasicClientApp::~TCPBasicClientApp()
{
    cancelAndDelete(timeoutMsg);
}

void TCPBasicClientApp::initialize(int stage)
{
    TCPGenericCliAppBase::initialize(stage);
    if (stage != 3)
        return;

    earlySend = false;  // TBD make it parameter
    WATCH(numRequestsToSend);
    WATCH(earlySend);

    sequenceNumber = 0;

    startTime = par("startTime");
    stopTime = par("stopTime");
    if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
        error("Invalid startTime/stopTime parameters");

    timeoutMsg = new cMessage("timer");
    nodeStatus = dynamic_cast<NodeStatus *>(findContainingNode(this)->getSubmodule("status"));

    if (isNodeUp()) {
        printf("Scheduling timeout message\n");
        timeoutMsg->setKind(MSGKIND_CONNECT);
        scheduleAt(startTime, timeoutMsg);
    } else {
        printf("Node is down. Not starting TCP\n");
    }
}

bool TCPBasicClientApp::isNodeUp()
{
    return !nodeStatus || nodeStatus->getState() == NodeStatus::UP;
}

bool TCPBasicClientApp::handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback)
{
    Enter_Method_Silent();
    if (dynamic_cast<NodeStartOperation *>(operation)) {
        if (stage == NodeStartOperation::STAGE_APPLICATION_LAYER) {
            simtime_t now = simTime();
            simtime_t start = std::max(startTime, now);
            if (timeoutMsg && ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)))
            {
                timeoutMsg->setKind(MSGKIND_CONNECT);
                scheduleAt(start, timeoutMsg);
            }
        }
    }
    else if (dynamic_cast<NodeShutdownOperation *>(operation)) {
        if (stage == NodeShutdownOperation::STAGE_APPLICATION_LAYER) {
            cancelEvent(timeoutMsg);
            if (socket.getState() == TCPSocket::CONNECTED || socket.getState() == TCPSocket::CONNECTING || socket.getState() == TCPSocket::PEER_CLOSED)
                close();
            // TODO: wait until socket is closed
        }
    }
    else if (dynamic_cast<NodeCrashOperation *>(operation)) {
        if (stage == NodeCrashOperation::STAGE_CRASH)
            cancelEvent(timeoutMsg);
    }
    else throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
    return true;
}

void TCPBasicClientApp::sendRequest()
{
    printf("Sending Request\n");
     EV << "sending request, " << numRequestsToSend-1 << " more to go\n";

     long requestLength = par("requestLength");
     long replyLength = par("replyLength");
     if (requestLength < 1)
         requestLength = 1;
     if (replyLength < 1)
         replyLength = 1;

     sendPacket(requestLength, replyLength);
}

void TCPBasicClientApp::handleTimer(cMessage *msg)
{
    printf("handleTimer()\n");
    switch (msg->getKind())
    {
        case MSGKIND_CONNECT:
            printf("starting session\n");
            EV << "starting session\n";
            connect(); // active OPEN

            break;

        case MSGKIND_SEND:
            printf("Sending Message\n");
           sendRequest();
           numRequestsToSend--;
           if(numRequestsToSend > 0){
               scheduleAt(simTime()+0.0005,msg);
           }
           printf("Num: %d\n",numRequestsToSend);
           // no scheduleAt(): next request will be sent when reply to this one
           // arrives (see socketDataArrived())
           break;

        default:
            throw cRuntimeError("Invalid timer msg: kind=%d", msg->getKind());
    }
}

void TCPBasicClientApp::socketEstablished(int connId, void *ptr)
{
    printf("Socket Established\n");
    TCPGenericCliAppBase::socketEstablished(connId, ptr);

    // determine number of requests in this session
    numRequestsToSend = (long) par("numRequestsPerSession");
    printf("Original Requests to Send: %d\n",numRequestsToSend);
    if (numRequestsToSend < 1)
        numRequestsToSend = 1;

    cMessage *sendMsg = new cMessage("timeout");
    sendMsg->setKind(MSGKIND_SEND);
    scheduleAt(simTime()+0.001,sendMsg);

    numRequestsToSend--;
}

void TCPBasicClientApp::rescheduleOrDeleteTimer(simtime_t d, short int msgKind)
{
    printf("rescheduleOrDeleteTimer()\n");
    cancelEvent(timeoutMsg);

    if (stopTime < SIMTIME_ZERO || d < stopTime)
    {
        timeoutMsg->setKind(msgKind);
        scheduleAt(d, timeoutMsg);
    }
    else
    {
        delete timeoutMsg;
        timeoutMsg = NULL;
    }
}

void TCPBasicClientApp::socketDataArrived(int connId, void *ptr, cPacket *msg, bool urgent)
{
    printf("socketDataArrived()\n");
    TCPGenericCliAppBase::socketDataArrived(connId, ptr, msg, urgent);

    if (numRequestsToSend > 0)
    {
        EV << "reply arrived\n";

        if (timeoutMsg)
        {
            simtime_t d = simTime() + (simtime_t) par("thinkTime");
            rescheduleOrDeleteTimer(d, MSGKIND_SEND);
        }
    }
    else if (socket.getState() != TCPSocket::LOCALLY_CLOSED)
    {
        EV << "reply to last request arrived, closing session\n";
        close();
    }
}

void TCPBasicClientApp::socketClosed(int connId, void *ptr)
{
    printf("socketClosed()\n");
    TCPGenericCliAppBase::socketClosed(connId, ptr);

    // start another session after a delay
    if (timeoutMsg)
    {
        simtime_t d = simTime() + (simtime_t) par("idleInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

void TCPBasicClientApp::socketFailure(int connId, void *ptr, int code)
{
    printf("socketFailure()\n");
    TCPGenericCliAppBase::socketFailure(connId, ptr, code);

    // reconnect after a delay
    if (timeoutMsg)
    {
        simtime_t d = simTime() + (simtime_t) par("reconnectInterval");
        rescheduleOrDeleteTimer(d, MSGKIND_CONNECT);
    }
}

