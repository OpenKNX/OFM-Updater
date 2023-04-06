# KNX over Bus Updater

Implement this Module to update your devices over KNX Bus.  

## Step 1
Add the Module to your platform.ini
```ini
lib_deps = 
    [...]
	https://github.com/OpenKnx/OFM-Updater
```

## Step 2
Add the Module to the OpenKnx Stack
```C++
#include <Arduino.h>
#include "OpenKNX.h"
#include "UpdateModule.h"

void setup()
{
	const uint8_t firmwareRevision = 0;
    openknx.init(firmwareRevision);
    openknx.addModule(1, ...);
    openknx.addModule(2, new UpdateModule());
    openknx.setup();

}
```

## Step 3
You can use the [KnxUpdater](https://github.com/thewhobox/KnxUpdater) to upload updates to your device.

## Good to know
The Update Module uses following FunctionProperties.  
These may not used by any other module.
|ObjectIndex|PropertyId|Used for|
|---|---|---|
|0|243|Update start|
|0|244|Update data|
|0|245|Update end|