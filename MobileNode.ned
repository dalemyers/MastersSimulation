//
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
simple MobileNode
{
    parameters:
        @display("i=abstract/person");
        double timeStep @unit("s") = default(0.1s); // the time granularity of movement calculation
        int trailLength = default(600);	// the length of the trail in time steps showing the past movement of the node. 0 disables the trail
        string color = default(""); // the color of the trail and range indicator in hex BBGGRR format or "" for random color
        bool showTxRange = default(true);  // whether to show the transmission range around the nodes
        double startX @unit("") = default(0); // the starting coordinates in meter
        double startY @unit("") = default(0); // the starting coordinates in meter
        int id = default(0); // the id of this bus
        double txRange @unit("m") = default(100m);  // the transmission range of the mobile node's radio
}


