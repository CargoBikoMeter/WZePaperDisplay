/*
 * Copyright (C) 2021 WZePaperDisplay Contributors
 * Contact: SAI-Lab Berlin (https://www.chemie.tu-berlin.de/sai_lab/sei_real_labor_sai_lab)
 * 
 * This file is part of the WZePaperDisplay firmware.
 *
 * The WZePaperDisplay firmware is free software: you can
 * redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * WZePaperDisplay firmware is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the WZePaperDisplay firmware. If not,
 * see <http://www.gnu.org/licenses/>.
 */

// Based on https://github.com/openbikesensor/OpenBikeSensorFirmware/
// The OpenBikeSensor firmware is free software: you can
// redistribute it and/or modify it under the terms of the GNU
// Lesser General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option)
// any later version.

// Based on https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/
// The information provided on the LastMinuteEngineers.com may be used, copied,
// remix, transform, build upon the material and distributed for any purposes
// only if provided appropriate credit to the author and link to the original article.

// Based on parts of ESP32 Weather Display using an EPD 7.5" Display, obtains data from Open Weather Map, decodes and then displays it.
// ###############################################################################################################################
// This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.

// Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
// 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
// 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
// 3. You may not, except with my express written permission, distribute or commercially exploit the content.
// 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

// The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
// software use is visible to an end-user.

// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
// OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// See more at http://www.dsbird.org.uk


#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ArduinoLog.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPSServer.hpp>
#include <HTTPMultipartBodyParser.hpp>
#include <HTTPURLEncodedBodyParser.hpp>
#include <Preferences.h>
#include <SSLCert.hpp>
//#include <StreamUtils.h>  // only for JSon Stream debugging in WZePaperDisplay.cpp
#include <U8g2_for_Adafruit_GFX.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "Wire.h"  // required for Adafruit

const char BUILD_DATE[] = __DATE__ " " __TIME__;

// configuration structure object for global usage
// name length: max 15 character 
struct WZConfig_s {
  char WIFI_SSID[33];        // wlan SSID
  char WIFI_PASSWORD[33];    // wlan password
  char AP_PASSWORD[13];      // password for soft access point 
  char project_url[51];      // url of project description 
  char tr_apikey[41];        // Telraam API key   
  char tr_segmentid[11];     // internal id of the Telraam segment
  char tr_segmentname[40];   // name of the Telraam segment  
  char tr_WakeupTime[3];     // Don't fetch traffic data before that time
  char tr_SleepTime[3];      // Don't fetch traffic data after that time
  char tr_UpdateMinute[3];   // traffic data update hourly at that minutes
  char Language[3];          // switch between: DE | EN | NL | FR | ES | SL
  char Timezone[40];         // choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
  char dnsServer[20];        // DNS server to use
  char ntpServer[40];        // choose from https://www.ntppool.org
  char gmtOffset[40];        // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800 
  char daylightOffset[40];   // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset
  char hb_apiurl[51];        // server which receive heartbeat messages from the device - max length: 50
  char hb_authkey[71];        // API token to access the heartbeat server - max length: 70
};

#define LOGLEVEL LOG_LEVEL_VERBOSE // SILENT|FATAL|ERROR|WARNING|INFO|NOTICE|TRACE|VERBOSE

// define on of the supported display models
//#define DISPLAY_MODEL_750c_Z90
#define DISPLAY_MODEL_750_T7

// V2_1 requires lower reset duration time the older HAT version
// see: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865/2052#msg4716764
#define WAVESHARE_DRIVER_HAT_V2_1 

#endif