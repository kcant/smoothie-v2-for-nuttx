# smoothie-v2
Smoothie for version 2 boards using nuttx as base, using https://github.com/Smoothieware/smoothie-nuttx

Dev/... is for devlopment and testing
Firmware/... is for Smoothie firmware code

First you need to clone and build the smoothie-nuttx with "make export".
Then unzip the nuttx-export.zip in Firmware/. 
documented here https://github.com/Smoothieware/smoothie-nuttx/blob/master/README.md

To build ```cd Firmware; rake```
To build unit tests ```cd Firmware; rake testing=1```

For Dev ```cd Dev; rake```

To compile only some unit tests in Firmware:

rake testing=1 test=streams

rake testing=1 test=dispatch

To compile with debug symbols:

rake testing=1 test=streams debug=1

You need to install ruby (and rake)
