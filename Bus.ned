package org.mixim.examples.Buses;

import org.mixim.modules.node.Host80211;

module Bus
{
    parameters:
        @display("i=abstract/person");
        double startX @unit("") = default(0); // the starting coordinates in meter
        double startY @unit("") = default(0); // the starting coordinates in meter
        int id = default(0); // the id of this bus
    submodules:
        host: Host80211 {
            @display("p=27,29");
        }
	connections allowunconnected:
}


