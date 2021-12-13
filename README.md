# WZePaperDisplay
This is an e-Paper based device for displaying publicly available mobility data that can be queried via the Telraam Server API (https://telraam-api.net/). The prefix "WZ" in the name WZePaperDisplay is based on the project "Wir Zählen" of the ADFC-Treptow-Köpenick in Berlin:  https://adfc-tk.de/wir-zaehlen/

![alt text](https://github.com/CargoBikoMeter/WZePaperDisplay/blob/main/images/WZePaperDisplay-ENG-Front.jpg)

## Table of contents
* [Overview](#overview)
* [Features](#features)
* [Development Setup](#development-setup)
* [Mechanical construction](#mechanical-construction)
* [Operation](#operation)

## Overview
WZePaperDisplay is an ESP32 based application in a photo frame to display mobility data. The photo frame  could be placed inside a window on a street or other nice locations (e.g. showcase in the school hallway) to show traffic data on an e-Paper based display. 

## Features
 * get traffic data every hour from Telraam server
  * summarizes the traffic data for the entire day
  * shows the percentage of each category
  * shows the traffic data and the V85 speed for cars and heavy vehicles during the last hour
 * Sleep-mode for low energy consumption
 * Device configuration via web server GUI, which could be started via on-board button
   * WiFi configuration (empty password for free WiFi networks possible)
   * selectable Timezone and NTP server
   * Language support: DE, EN, ES, FR, NL, SL
   * Telraam configuration: API key and segment id
   * Expert menu for special configuration parameters 
   * Firmware upload via WiFi
 * can send a heartbeat message with fetched traffic data in JSON format to external server


## Development Setup
![alt text](https://github.com/CargoBikoMeter/WZePaperDisplay/blob/main/images/WZePaperDisplay-Back.jpg)

Device components:
 * ESP32 NodeMCU
   * AZ-Delivery: https://www.az-delivery.de/en/products/esp32-developmentboard
   * JOY-iT: https://joy-it.net/de/products/SBC-NodeMCU-ESP32
 * Waveshare 7,5" e-Paper display with e-Paper driver HAT
   * s/w model: 750_T7 800x480  (https://www.berrybase.de : Item: RPI-EINK75)
   * color model: 750c_Z90 (it works, but not recommended due to the longer display refresh time)
 * experimental board 40x60 mm 
 * two 16 pin header for ESP32
 * one 16 angled pin header for connecting e-Paper driver HAT cable
 * cylinder head screw, slotted, M2, 8 mm
 * 3D photo frame with a minimal depth of 30 mm
 * USB 2.0 cable, USB-A plug to Micro-USB-B plug

Used Libraries with fixed version:
 * zinggjm/GxEPD2 @ 1.3.9  ; 2021-12-12: do not use version 1.4.0 - it generates Busy Timeout errors
 * bblanchon/ArduinoJson @ 6.18.3
 * thijse/ArduinoLog @ 1.1.1
 * adafruit/Adafruit GFX Library @ 1.10.12
 * adafruit/Adafruit BusIO @ 1.9.8
 * olikraus/U8g2_for_Adafruit_GFX @ 1.8.0
 * esp32_https_server @ 1.0.0

Development environment:
 * PlatformIO IDE
 
How to configure the build process:
You must edit the following section in file globals.h to switch between the two display models:
```
//#define DISPLAY_MODEL_750c_Z90
#define DISPLAY_MODEL_750_T7
```
If using an older version of ePaper driver HAT 2.1 following line has to be commented out:
```
#define WAVESHARE_DRIVER_HAT_V2_1 
```

If you want to switch the logging level during development switch the definition from LOGLEVEL LOG_LEVEL_INFO to LOGLEVEL LOG_LEVEL_VERBOSE in file globals.h.

The e-Paper HAT cables has to be connected to the following ESP32 NodeMCU pins:

| HAT  color  |   JOY-iT  |   AZ-Delivery    |   comment
|-------------|-----------|------------------|---------------
| VSS  grey   |    3V3    |  3V3
| GND  brown  |    GND    |  GND next to G23 |
| DIN  blue   |    D23    |  G23             |  MOSI
| CLK  yellow |    D18    |  G18             | 
| CS   orange |    D5     |  G5              |
| DC   green  |    D22    |  G22             |
| RST  white  |    D21    |  G21             |
| BUSY violet |    D4     |  G4              |


## Mechanical Construction
The following notes refer to the object picture frame used in the project with the dimensions of 220x170x45 mm. However, this information can be transferred to other object picture frames, provided that these have a depth of more than 30 mm.

In order to achieve a low overall height, the ESP32 is attached to a universal circuit board using two pin headers. Angled pin headers are soldered on for the cables of the e-Paper HAT.

The e-Paper display is attached to the back of the picture frame passpartous with adhesive strips. The existing passpartout was enlarged to a size of 100x165 mm for the e-Paper display. 

The universal circuit board with the attached ESP32 and the e-Paper HAT module are attached to the inner frame of the picture frame with 2 mm screws on an 8x8 mm beech wood strip. The USB cable is attached to the inner frame with a clamp for strain relief.

The attached pictures give a good overview of the mechanical structure of the device.


## Operation
After powering on the device for the first time the device must be configured. To start the configuration webserver the BOOT button on the ESP board must be pressed and hold down for a maximum of 15 seconds after first device startup until the e-Paper display is showing the configuration mode menu.

Attention: If the BOOT button will be pressed for more the 20 seconds all currently saved configuration data will be deleted.

After releasing the BOOT button the e-Paper display shows the configuration mode messages and is waiting for 240 seconds that the user connects to the webserver configuration access point. The name of the WiFi access point and the access password is displayed on the e-Paper display.

After connecting to the WiFi access point the WZePaperDisplay GUI can be called up in the web browser with the following URL: http://172.20.0.1

The WZePaperDisplay shows the "Https Redirect" page, which must be confirmed via button "Goto https". The browser warning must be accepted, because the device has only a self-signed SSL certificate.

Now the main navigation menu will appear and the initial device configuration can be started.

In menu "Configuration mode" the mode of configuration can be switched between ExpertMode=off (Default) and ExpertMode=on. ExpertMode=on allows the configuration of special configuration parameters. Depending on the setting of ExpertMode the menu "General" shows the associated configuration parameters. 

All configuration parameters are explained in the menus. All optional parameters or parameter sections are marked as optional in the menus, so all other paramters muss be filled. If a required parameter is not filled, the Button "Save" does not work. After a successfull "Save" the parameters are permanently saved in a non-volatile storage (NVS) of the ESP32 device.

If all required parameters are defined the device can be restarted with the menu button "Reboot and activate configuration" or by switching power off and on. All required configuration parameters will be checked during first startup. If there is any parameter missing it will be shown on the e-Paper display and the configuration webserver will be started automatically.

If there are no configuration errors the device will connect to the defined WiFi network and gets the current time from the configured NTP server. If the current minute is lower then the configured parameter "update minute" the device calculates the seconds until that time goes into sleep mode and starts again after that amount of seconds.

Then the device requests the Telraam API to fetch all traffic data of the entire day including the traffic data of the last hour and shows the data on the e-Paper display.

If the optional parameter section "Heartbeat server" was configured, the device sends the fetched traffic data in JSON format to the defined server.


Now have fun with the WZePaperDisplay!
