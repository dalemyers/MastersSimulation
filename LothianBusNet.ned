//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 2010 OpenSim Ltd.
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


//
// The Google Earth Demo network.
//
network LothianBusNet
{
    parameters:
        @display("bgb=40000,40000;bgi=background/terrain,s");
        double playgroundLatitude = default(56.0); // geographic position of the playground's north-west corner
        double playgroundLongitude = default(-3.4); // geographic position of the playground's north-west corner
        double playgroundWidth @unit("m") = default(40000m);  // the E-W size of playground 
        double playgroundHeight @unit("m") = default(40000m); // the N-S size of playground

    submodules:
        chanCtrl: ChannelController {
            @display("p=90,250");
        }
        bus[60]: MobileNode { //146 buses in total
        }
}


