package org.mixim.examples.Buses;
import org.mixim.base.modules.BaseNetwork;
import org.mixim.modules.node.Host80211;
network LothianBusNet extends BaseNetwork
{
    parameters:
        @display("bgb=30000,20000;");
        double playgroundLatitude = default(56.0); // geographic position of the playground's north-west corner
        double playgroundLongitude = default(-3.4); // geographic position of the playground's north-west corner
        double playgroundWidth @unit("m") = default(30000m);  // the E-W size of playground 
        double playgroundHeight @unit("m") = default(20000m); // the N-S size of playground
        int numHosts; // total number of hosts in the network  

    submodules:
        //bus[145]: Bus;
        host[numHosts]: Host80211 {
            @display("p=137,95");
        }

    connections allowunconnected:
}


