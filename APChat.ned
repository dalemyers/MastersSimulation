package org.mixim.examples.Buses;
simple APChat
{
    parameters:
        bool debug = default(false);  // output debugging information

        @display("i=block/app2");
    gates:
        inout udp;
}
