package org.mixim.examples.Buses;

import org.mixim.modules.node.WirelessNode;
import org.mixim.modules.nic.IWirelessNicUpperCtrl;

module AP extends WirelessNode
{
    parameters:
        @display("i=abstract/penguin");
        double x @unit("") = default(0); // the starting position
        double y @unit("") = default(0); // the starting position
        int id = default(0); // the id of this AP
        nicType = "Nic80211";

    connections allowunconnected:
}


