# m5-atom-satellite
Connect m5 Atom Matrix devies to Bitfocus Companion using the Satellite API

This is released as-is, in both Arduino Sketch format, as as a precompiled hex file.

## Flashing
The easiest way to get started is to flash the included hex file, this can be done using esptool.py or your ES32 flashing tool of choice.
This file should be flashed at offset 0x0, i.e. in esptool, you can use the following command from the directory you downloaded the hex file to.
```
esptool.py --chip ESP32 write_flash 0x0 m5-atom-satellite.hex
```

## Configuration
On first boot, the Atom Matrix will create a WiFi network starting with 'm5Atom-' and a semi-unique code. Join this network and configure at IP 192.168.4.1 if the captive portal doesn't load automatically.

If connection to WiFi fails, this will autostart, or it can be manually started by holding the button for 5 seconds if you are not connected to companion (when you see the 4 blue corner lights, they will turn purple to indicate portal reopened)

The Config page lets you enter your Companion IP, port should remain at default for now.
