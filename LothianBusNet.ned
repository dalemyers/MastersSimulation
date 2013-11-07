package org.mixim.examples.Buses;
import org.mixim.modules.node.Host80211;
import org.mixim.base.connectionManager.IConnectionManager;
import org.mixim.base.modules.IWorldUtility;

network LothianBusNet
{
    parameters:
        @display("bgb=30000,20000;");
        double playgroundLatitude = default(56.0); // geographic position of the playground's north-west corner
        double playgroundLongitude = default(-3.4); // geographic position of the playground's north-west corner
        double playgroundWidth @unit("m") = default(30000m);  // the E-W size of playground 
        double playgroundHeight @unit("m") = default(20000m); // the N-S size of playground
        int numHosts; // total number of hosts in the network  
        string cmType = default("org.mixim.base.connectionManager.ConnectionManager");        
        string wuType = default("org.mixim.base.modules.BaseWorldUtility");            // world utility to use
        

    submodules:
        connectionManager: <cmType> like IConnectionManager {
            parameters:
                @display("p=201,0;i=abstract/multicast;is=s");
        }
        world: <wuType> like IWorldUtility {
            parameters:
                playgroundSizeX = playgroundWidth;
                playgroundSizeY = playgroundHeight;
                playgroundSizeZ = 100m;
                @display("p=280,0;i=misc/globe;is=s");
        }
        bus[numHosts]: Bus;
        ap: AP{
            
        }
        //host[numHosts]: Host80211 {
        //    @display("p=137,95");
        //}

    connections allowunconnected:
}


