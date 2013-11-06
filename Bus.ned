package org.mixim.examples.Buses;

import org.mixim.modules.node.Host80211;
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2010 OpenSim Ltd.
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


//
// A mobile node that travels at a constant speed, with the heading
// changing by a random value at each update. Able to display
// its position, trail, transmission range etc. via KML.
//
module Bus
{
    parameters:
        @display("i=abstract/person");
        double startX @unit("") = default(0); // the starting coordinates in meter
        double startY @unit("") = default(0); // the starting coordinates in meter
        int id = default(0); // the id of this bus
    submodules:
        mobility: BusMobility {
            @display("p=103,42");
        }
        host: Host80211 {
            @display("p=27,29");
        }

}


