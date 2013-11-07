package org.mixim.examples.Buses;

import org.mixim.modules.node.WirelessNode;
import org.mixim.modules.nic.IWirelessNicUpperCtrl;

module Bus extends WirelessNode
{
    parameters:
        @display("i=abstract/person");
        double startX @unit("") = default(0); // the starting coordinates in meter
        double startY @unit("") = default(0); // the starting coordinates in meter
        int id = default(0); // the id of this bus
        nicType = "Nic80211";
        
	connections allowunconnected:
}


