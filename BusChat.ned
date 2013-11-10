package org.mixim.examples.Buses;
simple BusChat
{
    parameters:
        bool debug = default(false);  // output debugging information

        @display("i=block/app2");
    gates:
        inout udp;
}
