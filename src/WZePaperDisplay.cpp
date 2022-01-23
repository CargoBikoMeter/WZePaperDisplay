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
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the WZePaperDisplay firmware.  If not,
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


#include "globals.h"

extern void startConfigServer();
extern void readConfigurationData();
extern int  configServerUptimeMax;
extern void eraseConfigData();
extern WZConfig_s WZConfig;

const char *WZVersion = "1.0.1";

// pin for reading commands, commands depends from the time the button is pressed
// calibration command: button more then 10 seconds pressed 
const uint8_t buttonPin       = 0;     // command button: USER (BOOT) button on device is GPIO0

uint16_t WAIT_TIME_FOR_BUTTON = 10000; // give user time to press USER button, in milliseconds
int buttonState               = HIGH;  // if USER (BOOT)_KEY not pressed: state is HIGH
uint8_t buttonread            = 0;
uint8_t CMD_COUNTER           = 0;     // count the seconds the USER (BOOT)_KEY is pressed

const uint8_t START_CONFIG_SERVER_COUNTER = 15;  // < 15s: start configuration server
const uint8_t CONFIG_RESET_COUNTER        = 20; //  > 20s: reset config data in NVS flash area
bool  ConfigError = false;

bool TrafficSumValid = false;      // ensures display updates only with valid data 

String wzDeviceId, wzWiFiId;

// deep sleep definitions
#define uS_TO_S_FACTOR 1000000.0    // Conversion factor for micro seconds to seconds

// the system should start always every hour at minute WZConfig.tr_UpdateMinute 
// so we must recalculate the sleep time after getting the last time from ntp server
// TIME_TO_SLEEP = (atoi(WZConfig.tr_UpdateMinute) - CurrentMin) * 60;
uint16_t TIME_TO_SLEEP = 3600;     // one hour deep sleep time (seconds)

// we count the boot cycles after powering up the device
RTC_DATA_ATTR int BootCount = 0;       // saved in RTC memory area
RTC_DATA_ATTR int DailyTrafficSum = 0; // summarized daily traffic data
RTC_DATA_ATTR int SavedDay = 0;        // required for resetting DailyTrafficSum at the beginning of each day
RTC_DATA_ATTR int WiFiConnectionsFailed = 0;     // count failed WiFi connection attempts
RTC_DATA_ATTR int NTPRequestsFailed = 0;         // count failed attempts to get traffic data
RTC_DATA_ATTR int TRAPIError = 0;                // count Telraam API errors
RTC_DATA_ATTR int TrafficDataRequestsFailed = 0; // count failed attempts to get traffic data
RTC_DATA_ATTR int SendHeartbeatsFailed = 0;      // count failed attempts to send data to heartbeat server


String  Time_str, Date_str;
uint8_t wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0, CurrentDay;

int httpStatus = 0;
int httpConnectionAttempts; // number of HTTP connection attempts
HTTPClient http;
StaticJsonDocument<600> request;
String httpMessage;

// *****************    ePaper display specific stuff *********************************

// Connections ESP32 Node MCU
static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS   = 5;  // to EPD CS
static const uint8_t EPD_RST  = 21; // to EPD RST
static const uint8_t EPD_DC   = 22; // to EPD DC
static const uint8_t EPD_CLK  = 18; // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // to EPD DIN

// ESP32 NodeMCU pins to ePaper HAT : wire color
// HAT  color     JOY-iT     AZ-Delivery       comment
// ----------------------------------------------------------
// VSS  grey:     3V3
// GND  brown:    GND        GND next to G23
// DIN  blue:     D23        G23               VSPI-MOSI
// CLK  yellow:   D18        G18               VSPI_CLK
// CS   orange:   D5         G5                VSPI_CS
// DC   green:    D22        G22
// RST  white:    D21        G21
// BUSY violet:   D4         G4

#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

#ifdef DISPLAY_MODEL_750c_Z90
//display model: Waveshare 7.5" HD 3 colors ePaper (Reichelt-Shop: item number: DEBO EPA 7.5 RD) 
GxEPD2_3C<GxEPD2_750c_Z90, GxEPD2_750c_Z90::HEIGHT / 2> display(GxEPD2_750c_Z90(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // GDEH075Z90 880x528
#define SCREEN_WIDTH  880
#define SCREEN_HEIGHT 528
int16_t xColumn1=30, xColumn2=500, xColumn3=600, xColumn4=850; // column definitions
int16_t wzTextLine=40, wzUrlLine=60, HeadSepLine=70, wzLocationLine=110, wzDateTimeLine=140, wzGenInfoLine=165, wzSumInfoLine=200;
int16_t TableHeaderLine=210, wzPedestriansLine=250, wzBicyclesLine=300, wzCarsLine=350, wzHeavyVehiclesLine=400;
int16_t V85SeparatorLine=420, wzV85Line=470;
int16_t FooterLine=35, wzFooterLine=10;

// int16_t xColumn1=30, xColumn2=500, xColumn3=600, xColumn4=800; // column definitions
// int16_t wzTextLine=40, wzUrlLine=70, HeadSepLine=80, wzLocationLine=125, wzDateTimeLine=170, wzGenInfoLine=165, wzSumInfoLine=250;
// int16_t TableHeaderLine=260, wzPedestriansLine=300, wzBicyclesLine=350, wzCarsLine=400, wzHeavyVehiclesLine=450; 
// int16_t V85SeparatorLine=385, wzV85Line=430
// int16_t FooterLine=45, wzFooterLine=15;
#endif

#ifdef DISPLAY_MODEL_750_T7
// display model: Waveshare 7.5" B/W (BerryBase-Shop: item number: RPI-EINK75)
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/ EPD_CS, /*DC=*/ EPD_DC, /*RST=*/ EPD_RST, /*BUSY=*/ EPD_BUSY)); // GDEW075T7 800x480
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480
int16_t xColumn1=20, xColumn2=470, xColumn3=560, xColumn4=780; // column definitions
int16_t wzTextLine=30, wzUrlLine=60, HeadSepLine=70, wzLocationLine=110, wzDateTimeLine=140, wzGenInfoLine=165, wzSumInfoLine=195;
int16_t TableHeaderLine=205, wzPedestriansLine=245, wzBicyclesLine=290, wzCarsLine=330, wzHeavyVehiclesLine=370, V85SeparatorLine=385, wzV85Line=430;
int16_t FooterLine=35, wzFooterLine=10;
int16_t wzAPPasswordLine = 160; // used in configServer.cpp for displaying generated soft access point password
#endif

uint16_t white  = 0xFFFF; // white
uint16_t black  = 0x0000; // black
uint16_t red    = 0xF800; // red

const char*  TXT_CONFIG_MODE;
char display_message_buffer[70];  // message buffer for infos and warnings

String  wzTime, wzDate, wzDateTimeStr;

const char tr_apiurl[50] = "https://telraam-api.net/v1/reports/traffic";

// day of week
const char* weekday_D_DE[] =  { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" };
const char* weekday_D_EN[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char* weekday_D_NL[] = { "Zo", "Ma", "Di", "Wo", "Do", "Vr", "Za" };
const char* weekday_D_FR[] = { "Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam" };
const char* weekday_D_ES[] = { "Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb" };
const char* weekday_D_SL[] = { "Pon", "Tor", "Sre", "Čet", "Pet", "Sob", "Ned" };

// month
const char* month_M_DE[] = { "Jan", "Feb", "März", "Apr", "Mai", "Juni", "Juli", "Aug", "Sep", "Okt", "Nov", "Dez" };
const char* month_M_EN[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char* month_M_NL[] = { "Jan", "Feb", "Mrt", "Apr", "Mei", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dec" };
const char* month_M_FR[] = { "Janv.", "Févr.", "Mars", "Avril", "Mai", "Juin", "Juil.", "Août", "Sept", "Oct.", "Nov.", "Déc." };
const char* month_M_ES[] = { "Ene", "Feb", "Mar", "Abr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dic" };
const char* month_M_SL[] = { "Jan", "Feb", "Poh", "Apr", "Maj", "Jun", "Jul", "Avg", "Sep", "Oct", "Nov", "Dic" };

const char* TXT_UPDATED;
const char* TXT_UPDATED_DE = " --- aktualisiert:";
const char* TXT_UPDATED_EN = " --- updated:";
const char* TXT_UPDATED_NL = " --- bijgewerkt:";
const char* TXT_UPDATED_FR = " --- mis à jour:";
const char* TXT_UPDATED_ES = " --- actualizado:";
const char* TXT_UPDATED_SL = " --- updated:";

const char*  wzTextStr;
const char   wzTextStr_DE[25]          = "Wir Zählen - Mobilität";
const char   wzTextStr_EN[20]          = "We count - mobility";
const char   wzTextStr_NL[24]          = "Wij tellen - Mobiliteit";
const char   wzTextStr_FR[26]          = "Nous comptons - Mobilité";
const char   wzTextStr_ES[21]          = "Contamos - Movilidad";
const char   wzTextStr_SL[20]          = "Stejemo - Mobilnost";

const char*  wzGenInfoStr;
const char   wzGenInfoStr_DE[50]       = "Datengenerierung alle 60 Minuten (telraam.net)";
const char   wzGenInfoStr_EN[50]       = "data generation every 60 minutes (telraam.net)";
const char   wzGenInfoStr_NL[50]       = "Gegevens genereren elke 60 minuten (telraam.net)";
const char   wzGenInfoStr_FR[65]       = "Génération de données toutes les 60 minutes (telraam.net)";
const char   wzGenInfoStr_ES[55]       = "Generación de datos cada 60 minutos (telraam.net)";
const char   wzGenInfoStr_SL[55]       = "Proizvodnja podatkov vsakih 60 minut (telraam.net)";

const char   wzProportionString[2]     = "%";

const char*  wzDailySumString;
const char   wzDailySumString_DE[4]    = "Tag";
const char   wzDailySumString_EN[4]    = "day";
const char   wzDailySumString_NL[4]    = "Dag";
const char   wzDailySumString_FR[5]    = "Jour";
const char   wzDailySumString_ES[5]    = "Día";
const char   wzDailySumString_SL[4]    = "Dan";

const char*  wzHourlySumString;
const char   wzHourlySumString_DE[14]  = "letzte Stunde";
const char   wzHourlySumString_EN[14]  = "  last hour  ";
const char   wzHourlySumString_NL[14]  = " laatste uur ";
const char   wzHourlySumString_FR[16]  = "dernière heure";
const char   wzHourlySumString_ES[16]  = " ultima hora ";
const char   wzHourlySumString_SL[16]  = " zadnja ura  ";

const char*  wzPedestriansStr;
const char   wzPedestriansStr_DE[12]   = "Fußgänger";
const char   wzPedestriansStr_EN[12]   = "Pedestrians";
const char   wzPedestriansStr_NL[12]   = "Voetgangers";
const char   wzPedestriansStr_FR[9]    = "Piétons";
const char   wzPedestriansStr_ES[10]   = "Peatonal";
const char   wzPedestriansStr_SL[7]    = "Pešci";

const char*  wzBicyclesStr;
const char   wzBicyclesStr_DE[11]      = "Fahrräder";
const char   wzBicyclesStr_EN[9]       = "Bicycles";
const char   wzBicyclesStr_NL[9]       = "Fietsers";
const char   wzBicyclesStr_FR[7]       = "Vélos";
const char   wzBicyclesStr_ES[7]       = "Ciclos"; 
const char   wzBicyclesStr_SL[10]      = "Kolesarji";   

const char*  wzCarsStr;
const char   wzCarsStr_DE[6]           = "Autos";
const char   wzCarsStr_EN[5]           = "Cars";
const char   wzCarsStr_NL[7]           = "Auto's";
const char   wzCarsStr_FR[9]           = "Voitures";
const char   wzCarsStr_ES[9]           = "Voitures";
const char   wzCarsStr_SL[11]          = "Avtomobili";

const char*  wzHeavyVehiclesStr;
const char   wzHeavyVehiclesStr_DE[17] = "große Fahrzeuge";
const char   wzHeavyVehiclesStr_EN[15] = "Heavy vehicles";
const char   wzHeavyVehiclesStr_NL[17] = "Grote voertuigen";
const char   wzHeavyVehiclesStr_FR[18] = "Véhicules grands";
const char   wzHeavyVehiclesStr_ES[19] = "Vehículos grandes";
const char   wzHeavyVehiclesStr_SL[14] = "Težka vozila";

const char*  wzV85Str;
const char   wzV85Str_DE[20] = "V85 Geschwindigkeit";
const char   wzV85Str_EN[15] = "Speed cars v85";
const char   wzV85Str_NL[21] = "Snelheid auto's v85";
const char   wzV85Str_FR[25] = "Vitesse des voitures v85";
const char   wzV85Str_ES[17] = "V85 la velocidad";
const char   wzV85Str_SL[18] = "Hitrost vozil v85";

const char*  wzFooterStr; 
String FooterStr;
const String TXT_VERSION = "Version: ";
const String TXT_SAILAB  = " - SAI-Lab Berlin "; 
const String TXT_BOOT_CNT = " - Wake Up Count: ";

typedef struct { // Telraam traffic response report object
  int      instance_id;
  int      segment_id;
  String   date;
  String   interval;
  int      uptime;
  int      heavy_cars;
  int      cars;
  int      bicycles;
  int      pedestrians;
  int      heavy_cars_lft;
  int      heavy_cars_rgt;
  int      cars_lft;
  int      cars_rgt;
  int      bicycles_lft;
  int      bicycles_rgt;
  int      pedestrians_lft;
  int      pedestrians_rgt;
  int      direction;
  String   timezone;
  String   car_speed_hist_0to70plus;
  String   car_speed_hist_0to120plus;
  int      v85;

} Traffic_report_type;

Traffic_report_type  TrafficData[1];
double  heavy_vehicles_daily=0, cars_daily=0, bicycles_daily=0, pedestrians_daily=0, daily_traffic_sum=0;
double  heavy_vehicles_last_hour=0, cars_last_hour=0, bicycles_last_hour=0, pedestrians_last_hour=0, v85_speed=0;
byte    report_records=0; // count the number of report records in JSon object


// ****************  subfunctions **********************************

// Logging helper routines
void printTimestamp(Print* _logOutput, int logLevel) {
  static char c[12];
  sprintf(c, "%lu ", millis());
  _logOutput->print(c);
}

void printNewline(Print* _logOutput, int logLevel) {
  _logOutput->print('\n');
}

void setupLogging() {
  Log.begin(LOGLEVEL, &Serial);
  Log.setPrefix(printTimestamp);
  Log.setSuffix(printNewline);
  Log.verbose("Logging has started");
}

// set language specific text strings
void setMessagesByLanguage() {
  
  if ( ! strcmp(WZConfig.Language, "DE") ) {
    Log.verbose(F("setting params to: DE"));
    wzTextStr = wzTextStr_DE;
    wzGenInfoStr = wzGenInfoStr_DE;
    TXT_UPDATED = TXT_UPDATED_DE;
    wzDailySumString = wzDailySumString_DE;
    wzHourlySumString = wzHourlySumString_DE;
    wzPedestriansStr = wzPedestriansStr_DE;
    wzBicyclesStr = wzBicyclesStr_DE;
    wzCarsStr = wzCarsStr_DE;
    wzHeavyVehiclesStr = wzHeavyVehiclesStr_DE;
    wzV85Str = wzV85Str_DE;
  }

  if ( ! strcmp(WZConfig.Language,"EN") ) {
    Log.verbose(F("setting params to: EN" ));
    wzTextStr = wzTextStr_EN;
    wzGenInfoStr = wzGenInfoStr_EN;
    TXT_UPDATED = TXT_UPDATED_EN;
    wzDailySumString = wzDailySumString_EN;
    wzHourlySumString = wzHourlySumString_EN;
    wzPedestriansStr = wzPedestriansStr_EN;
    wzBicyclesStr = wzBicyclesStr_EN;
    wzCarsStr = wzCarsStr_EN;
    wzHeavyVehiclesStr = wzHeavyVehiclesStr_EN;
    wzV85Str = wzV85Str_EN;
  }

  if ( ! strcmp(WZConfig.Language, "NL") ) {
    Log.verbose(F("setting params to: NL"));
    wzTextStr = wzTextStr_NL;
    wzGenInfoStr = wzGenInfoStr_NL;
    TXT_UPDATED = TXT_UPDATED_NL;
    wzDailySumString = wzDailySumString_NL;
    wzHourlySumString = wzHourlySumString_NL;
    wzPedestriansStr = wzPedestriansStr_NL;
    wzBicyclesStr = wzBicyclesStr_NL;
    wzCarsStr = wzCarsStr_NL;
    wzHeavyVehiclesStr = wzHeavyVehiclesStr_NL;
    wzV85Str = wzV85Str_NL;
  }

  if ( ! strcmp(WZConfig.Language, "FR") ) {
    Log.verbose(F("setting params to: FR"));
    wzTextStr = wzTextStr_FR;
    wzGenInfoStr = wzGenInfoStr_FR;
    TXT_UPDATED = TXT_UPDATED_FR;
    wzDailySumString = wzDailySumString_FR;
    wzHourlySumString = wzHourlySumString_FR;
    wzPedestriansStr = wzPedestriansStr_FR;
    wzBicyclesStr = wzBicyclesStr_FR;
    wzCarsStr = wzCarsStr_FR;
    wzHeavyVehiclesStr = wzHeavyVehiclesStr_FR;
    wzV85Str = wzV85Str_FR;
  }

  if ( ! strcmp(WZConfig.Language, "ES") ) {
    Log.verbose(F("setting params to: ES"));
    wzTextStr = wzTextStr_ES;
    wzGenInfoStr = wzGenInfoStr_ES;
    TXT_UPDATED = TXT_UPDATED_ES;
    wzDailySumString = wzDailySumString_ES;
    wzHourlySumString = wzHourlySumString_ES;
    wzPedestriansStr = wzPedestriansStr_ES;
    wzBicyclesStr = wzBicyclesStr_ES;
    wzCarsStr = wzCarsStr_ES;
    wzHeavyVehiclesStr = wzHeavyVehiclesStr_ES;
    wzV85Str = wzV85Str_ES;
  }

  if ( ! strcmp(WZConfig.Language, "SL") ) {
    Log.verbose(F("setting params to: SL"));
    wzTextStr = wzTextStr_SL;
    wzGenInfoStr = wzGenInfoStr_SL;
    TXT_UPDATED = TXT_UPDATED_SL;
    wzDailySumString = wzDailySumString_SL;
    wzHourlySumString = wzHourlySumString_SL;
    wzPedestriansStr = wzPedestriansStr_SL;
    wzBicyclesStr = wzBicyclesStr_SL;
    wzCarsStr = wzCarsStr_SL;
    wzHeavyVehiclesStr = wzHeavyVehiclesStr_SL;
    wzV85Str = wzV85Str_SL;
  }  
}

// Method to print the reason by which ESP32 has been awaken from sleep
void printWakeupReason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : Log.info(F("Wakeup caused by external signal using RTC_IO")); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Log.info(F("Wakeup caused by external signal using RTC_CNTL")); break;
    case ESP_SLEEP_WAKEUP_TIMER : Log.info(F("Wakeup caused by timer")); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Log.info(F("Wakeup caused by touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP : Log.info(F("Wakeup caused by ULP program")); break;
    default : Log.info(F("Wakeup was not caused by deep sleep: %d\n"),wakeup_reason); break;
  }
}

// set basic ePaper display configuration values
void setBasicDisplayConfigs() {
  u8g2Fonts.setFontMode(1);                 // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);            // left to right (this is default)
  u8g2Fonts.setForegroundColor(black);      // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(white);      // apply Adafruit GFX color
  display.setTextColor(black);
  display.setRotation(0);
}

// display footer line on ePaper display
void displayFooterLine() {
    // footer line
    display.fillRect(0, display.height() - FooterLine, display.width(), 3, black);
    u8g2Fonts.setFont(u8g2_font_lubB14_tf); 
    FooterStr = TXT_VERSION + WZVersion +  TXT_SAILAB + TXT_BOOT_CNT + String(BootCount);
    wzFooterStr = FooterStr.c_str();
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(wzFooterStr)) / 2, display.height() - wzFooterLine);
    u8g2Fonts.print(wzFooterStr); 
}

// display startup messages during first startup 
void displayStartupScreen() {

  setBasicDisplayConfigs();
  display.setFullWindow();

  display.firstPage();
  do
  { 
    u8g2Fonts.setFont(u8g2_font_lubB14_tf);  
    
    sprintf(display_message_buffer, "*** WZePaperDisplay started ***");
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(display_message_buffer)) / 2, 20);
    u8g2Fonts.print(display_message_buffer); 
    
    displayFooterLine();
  }
  while (display.nextPage());
}

// display config messages after configuration button was pressed 
void displayConfigScreen() {

  setBasicDisplayConfigs();
  display.setFullWindow();

  display.firstPage();
  do
  { 
    u8g2Fonts.setFont(u8g2_font_lubB14_tf); 

    sprintf(display_message_buffer, "*** WZePaperDisplay configuration mode ***"); 
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(display_message_buffer)) / 2, 20);
    u8g2Fonts.print(display_message_buffer); 

    sprintf(display_message_buffer, "hold down internal BOOT button for:");
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(display_message_buffer)) / 2, 50);
    u8g2Fonts.print(display_message_buffer); 

    sprintf(display_message_buffer, "1-%ds: configuration server", START_CONFIG_SERVER_COUNTER);
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(display_message_buffer)) / 2, 75);
    u8g2Fonts.print(display_message_buffer);

    sprintf(display_message_buffer, ">%ds until confirmation message: erase configuration data",CONFIG_RESET_COUNTER);
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(display_message_buffer)) / 2, 100);
    u8g2Fonts.print(display_message_buffer); 

    displayFooterLine();
  }
  while (display.nextPage());
  
  delay(1000); // wait some time for reading display by the user
}

// display a single message to ePaper display
void displayMessage(const char message[60], int16_t y, const uint8_t *font) {
  Log.verbose(F("displayMessage entry point"));
  
  int16_t x, tw, ta, td, th;

  setBasicDisplayConfigs();
 
  u8g2Fonts.setFont(font);  
  ta = u8g2Fonts.getFontAscent(); // positive
  td = u8g2Fonts.getFontDescent(); // negative; in mathematicians view
  th = ta - td; // text box height - td is always a negative number
  tw = u8g2Fonts.getUTF8Width(message); // text box width
  x = (display.width() - tw) / 2 ;
  
  Log.verbose(F("ta: %d - td: %d - th: %d - tw: %d"),ta,td,th,tw);

  display.setPartialWindow(0, y - th, display.width(), th+5);
  display.firstPage();
  do
  { 
    //Log.verbose(F("do while loop ...");
    display.fillScreen(white);

    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(message);   
  }
  while (display.nextPage());
}

// connect to the configured WLAN
uint8_t startWiFi() {
  Log.info(F("startWiFi - connecting to: %s"), WZConfig.WIFI_SSID);
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // switch off AP
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(wzWiFiId.c_str());
  WiFi.begin(WZConfig.WIFI_SSID, WZConfig.WIFI_PASSWORD);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status(); // connected=3
    Log.verbose(F("WiFi connection status: %d"), connectionStatus);
    if (millis() > start + 60000) { // try for 60 seconds
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
      // wait some time if WiFi started short before
      delay(5000);
    }
    delay(100);
  }
  
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    // Serial.print("WiFi connected at: ");
    // Serial.println(WiFi.localIP().toString());
    //Log.info(F("WiFi connected at: %s"), WiFi.localIP().toString());
  } else { 
    Log.error(F("ERROR: WiFi connection *** FAILED ***"));
    WiFiConnectionsFailed++;
    Log.error(F("ERROR: WiFiConnectionsFailed counter: >%d<"), WiFiConnectionsFailed);
    if ( BootCount == 1  ) {
      sprintf(display_message_buffer, "ERROR: WiFi connection failed - check WiFi configuration");
      displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);

      sprintf(display_message_buffer, "current SSID: >%s<", WZConfig.WIFI_SSID);
      displayMessage(display_message_buffer, wzGenInfoLine+25, u8g2_font_lubB14_tf);

      sprintf(display_message_buffer, "device restarts in 5 minutes");
      displayMessage(display_message_buffer, wzGenInfoLine+50, u8g2_font_lubB14_tf);
    }
  }  
  Log.verbose(F("WiFi connection status: %d"), connectionStatus);
  return connectionStatus;
}

// convert time info related to language selection
boolean UpdateLocalTime() {
  Log.verbose(F("UpdateLocalTime called"));
  struct tm timeinfo;
  char   time_output[30], day_output[30], update_time[30];
  while (!getLocalTime(&timeinfo, 10000)) { // Wait for 10-sec for time to synchronize
    Log.error(F("ERROR: Failed to obtain time from NTP server"));
    NTPRequestsFailed++;
    Log.error(F("ERROR: NTPRequestsFailed counter: >%d<"), NTPRequestsFailed);
    if ( BootCount == 1  ) {
      sprintf(display_message_buffer, "ERROR: Failed to obtain time from NTP server");
      displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
    }
    return false;
  }

  Log.verbose(F("  set time regarding to language"));

  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  CurrentDay  = timeinfo.tm_mday;

  Log.verbose(F("CurrentDay: %d - SavedDay: %d"), CurrentDay, SavedDay);
  // save CurrentDay into RTC menory, if a new day has arrived and reset DailyTrafficSum
  if ( CurrentDay > SavedDay ) {
    SavedDay = CurrentDay;
    DailyTrafficSum = 0; 
    Log.verbose(F("new day has arrived, DailyTrafficSum resettet: %d"), DailyTrafficSum);
  }
 
  if ( ! strcmp(WZConfig.Language,"DE") ) {
    sprintf(day_output, "%s, %02u. %s %04u", weekday_D_DE[timeinfo.tm_wday], timeinfo.tm_mday, month_M_DE[timeinfo.tm_mon], (timeinfo.tm_year) + 1900); // day_output >> So., 23. Juni 2019 <<
    strftime(update_time, sizeof(update_time), "%R", &timeinfo);  // Creates: '14:05'
  }

  if ( ! strcmp(WZConfig.Language,"EN") ) {
    sprintf(day_output, "%s %02u %s %04u", weekday_D_EN[timeinfo.tm_wday], timeinfo.tm_mday, month_M_EN[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);  // Creates: '02:05:49pm'
  }

  if ( ! strcmp(WZConfig.Language,"NL") ) {
    sprintf(day_output, "%s %02u %s %04u", weekday_D_NL[timeinfo.tm_wday], timeinfo.tm_mday, month_M_NL[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%R", &timeinfo);  // Creates: '14:05'
  }

  if ( ! strcmp(WZConfig.Language,"FR") ) {
    sprintf(day_output, "%s %02u %s %04u", weekday_D_FR[timeinfo.tm_wday], timeinfo.tm_mday, month_M_FR[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%R", &timeinfo);  // Creates: '14:05'
  } 

  if ( ! strcmp(WZConfig.Language,"ES") ) {
    sprintf(day_output, "%s %02u %s %04u", weekday_D_ES[timeinfo.tm_wday], timeinfo.tm_mday, month_M_ES[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%R", &timeinfo);  // Creates: '14:05'
  } 

  if ( ! strcmp(WZConfig.Language,"SL") ) {
    sprintf(day_output, "%s %02u %s %04u", weekday_D_SL[timeinfo.tm_wday], timeinfo.tm_mday, month_M_SL[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    strftime(update_time, sizeof(update_time), "%R", &timeinfo);  // Creates: '14:05'
  } 

  sprintf(time_output, "%s %s", TXT_UPDATED, update_time);

  // date and time for ePaper display
  wzDate = day_output;
  wzTime = time_output;
  wzDateTimeStr = wzDate + wzTime;
  
  // next lines required for requesting Telraam data
  strftime(day_output, sizeof(day_output), "%Y-%m-%d", &timeinfo);  // creates  '2001-08-23'
  Date_str = day_output;
  
  return true;
}

// get the time from configured NTP server 
boolean setupTime() {
  configTime(atol(WZConfig.gmtOffset), atoi(WZConfig.daylightOffset), WZConfig.ntpServer, "time.nist.gov"); // (gmtOffset, daylightOffset, ntpServer)
  setenv("TZ", WZConfig.Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}

// get the traffic data from Telraam server
bool getTrafficData(WiFiClient& client) {
  Log.info(F("getTrafficData"));

  httpStatus = 0; // reset status flag
  httpMessage = ""; // reset message
  request.clear(); // reset request data

  // for calculating Json object size see: https://arduinojson.org/v6/assistant/ and
  // https://documenter.getpostman.com/view/8210376/TWDRqyaV#3bb3c6bd-ea23-4329-b885-0d142403ecbb
  //StaticJsonDocument<135> request;
  DynamicJsonDocument doc(22 * 3072);
  JsonObject root;

  //String httpMessage="{\"level\": \"segments\", \"format\": \"per-hour\", \"id\": \"9000002074\", \"time_start\": \"2021-08-29 01:00:00Z\", \"time_end\": \"2021-08-29 20:00:00Z\"}";
  // create the following request for Telraam API with five Json objects
  // {
  //   "level": "segments",
  //   "format": "per-hour",
  //   "id": "9000002074",
  //   "time_start": "2021-08-29 01:00:00Z",
  //   "time_end": "2021-08-29 20:00:00Z"
  // }
  Log.verbose(F("getTrafficData called"));

  if ( BootCount == 1) {
    sprintf(display_message_buffer, "get traffic data from Telraam server");
    displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
  }

  // Add the objects
  request["level"] = "segments";
  request["format"] = "per-hour";
  request["id"] = WZConfig.tr_segmentid;
  request["time_start"] = Date_str + " 01:00:00Z";
  request["time_end"]   = Date_str + " 21:00:00Z";

  serializeJson(request, httpMessage);
  // Serial.print("JSON httpMessage: ");
  // Serial.println(httpMessage);
  
  // now try to get the traffic data
  httpConnectionAttempts = 0;
  while ( ( httpConnectionAttempts < 6 ) && ( httpStatus != 200 )  ) {
    client.stop(); // close connection before sending a new request

    // Send request
    http.useHTTP10(true);
    http.begin(tr_apiurl);
    http.addHeader("Content-Type", "application/json"); 
    http.addHeader("X-Api-Key", WZConfig.tr_apikey);
  
    httpStatus = http.POST(httpMessage);
    httpConnectionAttempts++;
    Log.verbose(F("httpConnectionAttempts: >%d< - HTTP httpStatus is: >%d<"), httpConnectionAttempts, httpStatus);
   
    if ( httpStatus != 200 ) {
      Log.verbose(F("Wait 10 seconds before next attempt"));
      delay(10000); // wait 10 seconds for next connection attempt
    }
  }

  if ( httpStatus != 200 ) {
    Log.error(F("ERROR: HTTP httpStatus is: >%d<"), httpStatus);
    TrafficDataRequestsFailed++;
    Log.error(F("ERROR: TrafficDataRequestsFailed counter: >%d<"), TrafficDataRequestsFailed);
    if ( BootCount == 1  ) {  // send error message to display
      sprintf(display_message_buffer, "ERROR: traffic data connection failed: %d attempts, Status: >%d<", httpConnectionAttempts, httpStatus);
      displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
    }
    return false; // break data processing
  }

  // Get a reference to the stream
  // Stream &response = http.getStream();
  // Parse response
  // one Telraam report record contains 3072 Byte, see: https://arduinojson.org/v6/assistant/
  // and https://documenter.getpostman.com/view/8210376/TWDRqyaV#3bb3c6bd-ea23-4329-b885-0d142403ecbb

  deserializeJson(doc, http.getStream());

  // Read values
  //Log.verbose(F("traffic data %d "), doc["time"].as<long>() );
  //Telraam data
  Log.verbose(F("print json objects from response ..."));
  Log.verbose(F("print HTTP header variables ..."));
  root = doc.as<JsonObject>();
  int status_code = root["status_code"].as<int>();
  Log.verbose(F("Telraam status_code: >%d<"), status_code);
  Log.verbose(F("message: %s"), root["message"].as<const char *>());

  if ( status_code != 200 ) {
    Log.error(F("ERROR: Telraam API returned status_code: >%s<"), String(status_code));
    TRAPIError++;
    if ( BootCount == 1 ) {
      sprintf(display_message_buffer, "ERROR: Telraam API returned status_code: >%d<", status_code);
      displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);

      sprintf(display_message_buffer, "current segment id: >%s<", WZConfig.tr_segmentid);
      displayMessage(display_message_buffer, wzGenInfoLine+25, u8g2_font_lubB14_tf);
    }
    return false; // break data processing
  }

  // reset trafic data
  pedestrians_last_hour=0;
  bicycles_last_hour=0;
  cars_last_hour=0;
  heavy_vehicles_last_hour=0;
  pedestrians_daily = 0;
  bicycles_daily = 0;
  cars_daily = 0;
  heavy_vehicles_daily = 0; 
  v85_speed = 0;
  
  report_records = 0;  // count the number of report records

  JsonArray reports = root["report"];

  for (JsonObject report : reports) {
    report_records++;
 
    pedestrians_last_hour = report["pedestrian"].as<double>();
    bicycles_last_hour = report["bike"].as<double>();
    cars_last_hour = report["car"].as<double>();
    heavy_vehicles_last_hour = report["heavy"].as<double>();
    v85_speed = report["v85"].as<double>();
    
    // summarize the data
    pedestrians_daily = pedestrians_daily + pedestrians_last_hour;
    bicycles_daily = bicycles_daily + bicycles_last_hour;
    cars_daily = cars_daily + cars_last_hour;
    heavy_vehicles_daily = heavy_vehicles_daily + heavy_vehicles_last_hour;
    daily_traffic_sum = pedestrians_daily + bicycles_daily + cars_daily + heavy_vehicles_daily;

  }

  // save current traffic data sum into RTC memory
  // this prevents display updates in the case that the sum of fetched traffic data is lower than 
  // the last sum of the same day - this could happen if something was wrong during the communication 
  // to the Telraam API
  if ( (int)daily_traffic_sum >= DailyTrafficSum ) {
    Log.verbose(F("new traffic data sum: update RTC variable DailyTrafficSum"));
    DailyTrafficSum = daily_traffic_sum;
    TrafficSumValid = true;
  } else {
    TrafficSumValid = false; // don't update traffic data on display
  }

  Log.verbose(F("print traffic report data..."));
  // print last hour data
  Log.verbose(F("last hour results from number of report records: %s"), String(report_records));
  Log.verbose(F("Pedestrians: %s"), String(pedestrians_last_hour));
  Log.verbose(F("Bicycles: %s"), String(bicycles_last_hour));
  Log.verbose(F("Cars: %s"), String(cars_last_hour));   
  Log.verbose(F("Heavy Cars: %s"), String(heavy_vehicles_last_hour));
  Log.verbose(F("V85 speed: %s"), String(v85_speed));
  // print daily summary
  Log.verbose(F("daily summary results from number of report records: %d"), report_records);
  Log.verbose(F("Pedestrians daily: %s"), String(pedestrians_daily));
  Log.verbose(F("Bicycles daily: %s"), String(bicycles_daily));
  Log.verbose(F("Cars daily: %s"), String(cars_daily));   
  Log.verbose(F("Heavy Cars daily: %s"), String(heavy_vehicles_daily));
  Log.verbose(F("daily_traffic_sum daily: %s"), String(daily_traffic_sum));
  Log.verbose(F("RTC DailyTrafficSum: %s - TrafficSumValid: %s"), String(DailyTrafficSum), String(TrafficSumValid));

  // Disconnect
  client.stop();
  http.end();
  return true;
}

// show the entire traffic data on ePaper display
void displayTrafficData()
{
  Log.info(F("displayTrafficData"));
  
  double proportion;
  char   DataStr[6]; // string representation for traffic data

  setBasicDisplayConfigs();
  // display all data in full window mode
  display.setFullWindow();

  display.firstPage();
  do
  { 
    Log.verbose(F("do while loop ..."));
    display.fillScreen(white);
    
    // print wzTextStr
    // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
    u8g2Fonts.setFont(u8g2_font_luBS24_tf);  
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(wzTextStr)) / 2, wzTextLine);
    u8g2Fonts.print(wzTextStr);

    // print wzUrlStr
    u8g2Fonts.setFont(u8g2_font_lubB14_tf);  
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(WZConfig.project_url)) / 2 , wzUrlLine);
    u8g2Fonts.print(WZConfig.project_url); 

    // header separator line
    display.fillRect(0, HeadSepLine, display.width(), 3, black);  

    // print wzLocationStr
    u8g2Fonts.setFont(u8g2_font_luBS24_tf);  
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(WZConfig.tr_segmentname)) / 2, wzLocationLine);
    u8g2Fonts.print(WZConfig.tr_segmentname);
    
    // print wzDate and wzTime
    u8g2Fonts.setFont(u8g2_font_lubB14_tf);
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(wzDateTimeStr.c_str())) / 2 , wzDateTimeLine);
    u8g2Fonts.print(wzDateTimeStr);

    // print wzGenInfoStr
    u8g2Fonts.setFont(u8g2_font_lubB14_tf);
    u8g2Fonts.setCursor((display.width() - u8g2Fonts.getUTF8Width(wzGenInfoStr)) / 2, wzGenInfoLine);
    u8g2Fonts.print(wzGenInfoStr);    
    
    // print wzSumInfoStr
    u8g2Fonts.setFont(u8g2_font_lubB19_tf);
    u8g2Fonts.setCursor(xColumn2 - u8g2Fonts.getUTF8Width(wzDailySumString) - 10, wzSumInfoLine);
    u8g2Fonts.print(wzDailySumString);

    u8g2Fonts.setCursor(xColumn3 - u8g2Fonts.getUTF8Width(wzProportionString) + 20, wzSumInfoLine);
    u8g2Fonts.print(wzProportionString);

    String SumString = String(CurrentHour - 1) + "h - " + String(CurrentHour) + "h";
    Log.verbose(F("SumString: %s"), SumString );
    wzHourlySumString = SumString.c_str();
    u8g2Fonts.setCursor(xColumn4 - u8g2Fonts.getUTF8Width(wzHourlySumString), wzSumInfoLine);
    u8g2Fonts.print(wzHourlySumString);

    // traffic table header line
    display.fillRect(0, TableHeaderLine, display.width(), 2, black);

    // print columns 1 and 2
    // set the font size for all traffic table data
    u8g2Fonts.setFont(u8g2_font_luBS24_tf);
  
    // print pedestrians_daily
    u8g2Fonts.setCursor(xColumn1, wzPedestriansLine);
    u8g2Fonts.print(wzPedestriansStr);
    // get the pre-decimal places to calculate the x start position
    itoa((int)round(pedestrians_daily), DataStr,10);
    Log.verbose(F("pedestrians_daily: %s - DataStr: %s"), String(pedestrians_daily), DataStr);
    Log.verbose(F("pedestrians_daily width: %d"), u8g2Fonts.getUTF8Width(DataStr));
    // now calculate the current x position to the left beginning from fixed xColumn2
    u8g2Fonts.setCursor(xColumn2 - u8g2Fonts.getUTF8Width(DataStr), wzPedestriansLine);
    u8g2Fonts.print((int)round(pedestrians_daily));

    // print pedestrians counted last hour    
    itoa((int)round(pedestrians_last_hour), DataStr,10);
    u8g2Fonts.setCursor(xColumn4 - u8g2Fonts.getUTF8Width(DataStr), wzPedestriansLine);
    u8g2Fonts.print((int)round(pedestrians_last_hour));

    // print bicycles_daily
    u8g2Fonts.setCursor(xColumn1, wzBicyclesLine);
    u8g2Fonts.print(wzBicyclesStr);
    itoa((int)round(bicycles_daily), DataStr,10);
    Log.verbose(F("bicycles_daily: %s - DataStr: %s"), String(bicycles_daily), DataStr);
    Log.verbose(F("bicycles_daily width: %d"), u8g2Fonts.getUTF8Width(DataStr));
    u8g2Fonts.setCursor(xColumn2 - u8g2Fonts.getUTF8Width(DataStr), wzBicyclesLine);
    u8g2Fonts.print((int)round(bicycles_daily));

    // print bicycles counted last hour
    itoa((int)round(bicycles_last_hour), DataStr,10);
    u8g2Fonts.setCursor(xColumn4 - u8g2Fonts.getUTF8Width(DataStr), wzBicyclesLine);
    u8g2Fonts.print((int)round(bicycles_last_hour));

    // print cars_daily
    u8g2Fonts.setCursor(xColumn1, wzCarsLine);
    u8g2Fonts.print(wzCarsStr);
    itoa((int)round(cars_daily), DataStr,10);
    Log.verbose(F("cars_daily: %s - DataStr: %s"), String(cars_daily), DataStr);
    Log.verbose(F("cars_daily width: %d"), u8g2Fonts.getUTF8Width(DataStr));
    u8g2Fonts.setCursor(xColumn2 - u8g2Fonts.getUTF8Width(DataStr), wzCarsLine);
    u8g2Fonts.print((int)round(cars_daily));

    // print cars counted last hour
    itoa((int)round(cars_last_hour), DataStr,10);
    u8g2Fonts.setCursor(xColumn4 - u8g2Fonts.getUTF8Width(DataStr), wzCarsLine);
    u8g2Fonts.print((int)round(cars_last_hour));

    // print heavy_vehicles_daily
    u8g2Fonts.setCursor(xColumn1, wzHeavyVehiclesLine);
    u8g2Fonts.print(wzHeavyVehiclesStr);
    itoa((int)round(heavy_vehicles_daily), DataStr,10);
    Log.verbose(F("heavy_vehicles_daily: %s - DataStr: %s"), String(heavy_vehicles_daily), DataStr);
    Log.verbose(F("heavy_vehicles_daily width: %d"), u8g2Fonts.getUTF8Width(DataStr));
    u8g2Fonts.setCursor(xColumn2 - u8g2Fonts.getUTF8Width(DataStr), wzHeavyVehiclesLine);
    u8g2Fonts.print((int)round(heavy_vehicles_daily));
 
    // print heavy_cars counted last hour
    itoa((int)round(heavy_vehicles_last_hour), DataStr,10);
    u8g2Fonts.setCursor(xColumn4 - u8g2Fonts.getUTF8Width(DataStr), wzHeavyVehiclesLine);
    u8g2Fonts.print((int)round(heavy_vehicles_last_hour));

    // V85 separator line
    display.fillRect(0, V85SeparatorLine, display.width(), 2, black);

    // print V85, speed which 85 percent of car drivers do not exeed during last hour 
    u8g2Fonts.setCursor(xColumn1, wzV85Line);
    u8g2Fonts.print(wzV85Str);
    itoa((int)(v85_speed), DataStr,10);
    u8g2Fonts.setCursor(xColumn4 - u8g2Fonts.getUTF8Width(DataStr), wzV85Line);
    u8g2Fonts.print((int)round(v85_speed));

    // calculate and print column 3 only if traffic data sum > 0 
    if ( daily_traffic_sum > 0 ) {

      proportion= pedestrians_daily * 100 / daily_traffic_sum;
      Log.verbose(F("pedestrians proportion: %s"), String(proportion));
      itoa((int)(proportion), DataStr,10);
      u8g2Fonts.setCursor(xColumn3 - u8g2Fonts.getUTF8Width(DataStr), wzPedestriansLine);
      u8g2Fonts.print(String(proportion));

      proportion= bicycles_daily * 100 / daily_traffic_sum;
      Log.verbose(F("bicycles proportion: %s"), String(proportion));
      itoa((int)(proportion), DataStr,10);
      u8g2Fonts.setCursor(xColumn3 - u8g2Fonts.getUTF8Width(DataStr), wzBicyclesLine);
      u8g2Fonts.print(String(proportion));

      proportion= cars_daily * 100 / daily_traffic_sum;
      Log.verbose(F("cars proportion: %s"), String(proportion));
      itoa((int)(proportion), DataStr,10);
      u8g2Fonts.setCursor(xColumn3 - u8g2Fonts.getUTF8Width(DataStr), wzCarsLine);
      u8g2Fonts.print(String(proportion));

      proportion= heavy_vehicles_daily * 100 / daily_traffic_sum;
      Log.verbose(F("heavy cars proportion: %s"), String(proportion));
      itoa((int)(proportion), DataStr,10);
      u8g2Fonts.setCursor(xColumn3 - u8g2Fonts.getUTF8Width(DataStr), wzHeavyVehiclesLine);
      u8g2Fonts.print(String(proportion));
    }

    displayFooterLine();
  }
  while (display.nextPage());
}

// check if user has pressed button for configuration menue
void checkUserButton() {
  Log.info(F("checkUserButton"));

  CMD_COUNTER = 0;
  // check if button is pressed, will change from HIGH to LOW
  buttonread = digitalRead(buttonPin);
  Log.verbose(F("  buttonread state (pressed=0): %i"), buttonread);
    
  if (buttonread == LOW) { //check if button was pressed before and being pressed now
    
    // show configuration screen messages
    displayConfigScreen();

    if (buttonState == HIGH)
    {
      buttonState = LOW;
      Log.verbose(F("  button pressed"));
      // now count the seconds the button is pressed
      while ( buttonState != HIGH )
      {
        buttonState = digitalRead(buttonPin);
        CMD_COUNTER++;
        Log.verbose(F(" CMD_COUNTER: %i"), CMD_COUNTER);

        if ( CMD_COUNTER  > CONFIG_RESET_COUNTER ) {
          sprintf(display_message_buffer, "button pressed (>%ds): erase configuration data",CONFIG_RESET_COUNTER);
          displayMessage(display_message_buffer, 130, u8g2_font_lubB14_tf);
          
          // erase configuration data
          eraseConfigData();
        }

        // increment counter every second
        delay(1000);

      }  // while loop

      // now process specific functions
      if ( CMD_COUNTER <= START_CONFIG_SERVER_COUNTER) {
          // signal the pressed button to the user
          sprintf(display_message_buffer, "button pressed (1-%ds): start configuration server", START_CONFIG_SERVER_COUNTER);
          displayMessage(display_message_buffer, 130, u8g2_font_lubB14_tf);
          
          startConfigServer();   

          CMD_COUNTER = 0;  // reset counter
          buttonState = HIGH;
      }

      CMD_COUNTER = 0;   // reset counter
      buttonState = HIGH;
    }
  }
  else {
    if (buttonState == LOW) {
      buttonState = HIGH;
    }
  }
  Log.verbose(F("checkUserButton - end"));
}

// send heartbeat to external server
bool sendHeartbeat(WiFiClient& client) {
  Log.info(F("sendHeartbeat"));

  httpStatus = 0; // reset status flag
  httpMessage = ""; // reset message
  request.clear(); // reset request data
  
  // Add the objects
  request["app_id"] = "wzepaper";
  request["build_version"] = WZVersion;
  request["build_date"] = BUILD_DATE;
  request["bootcount"] = BootCount;
  request["device_id"] = wzDeviceId;
  request["segment_id"] = WZConfig.tr_segmentid;
  request["segment_name"] = WZConfig.tr_segmentname;
  request["pedestrians_daily"] = (int)round(pedestrians_daily);
  request["pedestrians_last_hour"] = (int)round(pedestrians_last_hour);
  request["bicycles_daily"] = (int)round(bicycles_daily);
  request["bicycles_last_hour"] = (int)round(bicycles_last_hour);
  request["cars_daily"] = (int)round(cars_daily);
  request["cars_last_hour"] = (int)round(cars_last_hour);
  request["heavy_vehicles_daily"] = (int)round(heavy_vehicles_daily);
  request["heavy_vehicles_last_hour"] = (int)round(heavy_vehicles_last_hour);
  request["v85_speed"] = (int)round(v85_speed);
  request["wifi_rssi"] = WiFi.RSSI();
  request["language"] = WZConfig.Language;
  request["timezone"] = WZConfig.Timezone;
  request["ntpserver"] = WZConfig.ntpServer;
  request["sleeptime"] = WZConfig.tr_SleepTime;
  request["updateminute"] = WZConfig.tr_UpdateMinute;
  request["wakeuptime"] = WZConfig.tr_WakeupTime;
  request["dailytrafficsum"] = DailyTrafficSum;
  request["wificonnectionsfailed"] = WiFiConnectionsFailed;
  request["ntprequestsfailed"] = NTPRequestsFailed;
  request["trapierror"] = TRAPIError;
  request["trafficdatarequestsfailed"] = TrafficDataRequestsFailed;
  request["sendheartbeatsfailed"] = SendHeartbeatsFailed;

  serializeJson(request, httpMessage);

  // Serial.print("JSON httpMessage length: ");
  // Serial.println(httpMessage.length());
  // Serial.print("JSON httpMessage: ");
  // Serial.println(httpMessage);

  httpConnectionAttempts = 0;
  while ( ( httpConnectionAttempts < 6 ) && ( httpStatus != 200 )  ) {
    client.stop();

    // we MUST ensure, that a trailing / is appended to hb_apiurl
    // without / there is a space in the Header message:  POST   HTTP/1.1\r\n
    // which is not allowed and the following server error message will be generated
    // AH03448: HTTP Request Line; Excess whitespace (disallowed by HttpProtocolOptions Strict
    http.begin(WZConfig.hb_apiurl);
    http.addHeader("Content-Type", "application/json"); 
    String authHeader = "Basic " + String(WZConfig.hb_authkey);
    http.addHeader("Authorization", authHeader); 

    httpStatus = http.POST(httpMessage);
    //http.writeToStream(&Serial);  // Print the response body
  
    httpConnectionAttempts++;
    Log.verbose(F("httpConnectionAttempts: >%d< - HTTP status is: >%d<"), httpConnectionAttempts, httpStatus);
    
    if ( httpStatus != 200 ) {
      Log.verbose(F("Wait 10 seconds before next attempt"));
      delay(10000); // wait 10 seconds for next connection attempt
    }

  }
  // 
  if ( httpStatus != 200 ) {
    Log.error(F("ERROR: HTTP status is: >%d<"), httpStatus);
    SendHeartbeatsFailed++;
    Log.error(F("ERROR: SendHeartbeatsFailed counter: >%d<"), SendHeartbeatsFailed);
    if ( BootCount == 1  ) {
      sprintf(display_message_buffer, "ERROR: heartbeat connection failed: %d attempts, Status: >%d<", httpConnectionAttempts, httpStatus);
      displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
    }
    return false; // break data processing
  }

  // Disconnect
  client.stop();
  http.end();

  return true;
}

// ############ main programm ############# 

// all device functionality is included in setup function
void setup() {
  Serial.begin(115200);
  delay(1000);

  setupLogging();
  delay(100);
  
  Log.info(F("Version: %s Build: %s"), WZVersion, BUILD_DATE);

  // generate device specific id
  wzDeviceId = String((uint16_t)(ESP.getEfuseMac() >> 32), HEX);
  wzDeviceId.toUpperCase();
  wzWiFiId = "WZePaperDisplay-" + wzDeviceId;
  // Serial.print("wzWiFiId: ");
  // Serial.println(wzWiFiId);
  Log.info(F("wzDeviceId: %s"), wzDeviceId);
   
#ifdef WAVESHARE_DRIVER_HAT_V2_1
  // we MUST use the extended display.init function with four parameters to avoid Busy Timeout
  Log.info(F("display initialization for WAVESHARE_DRIVER_HAT_V2_1"));
  delay(100);
  display.init(115200, true, 2 /*reset duration*/, false);
#else
  Log.info(F("display initialization for old WAVESHARE_DRIVER_HAT"));
  delay(100);
  display.init(115200);
#endif
  Log.info(F("")); // print only line feed to terminate linr fter display.init

  Log.verbose(F("SPI init"));
  SPI.end(); // release standard SPI pins, e.g. SCK(18), MISO(19), MOSI(23), SS(5)
  SPI.begin(EPD_CLK, EPD_MISO, EPD_MOSI, EPD_CS); // map and init SPI pins SCK(13), MISO(12), MOSI(14), SS(15)
 
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
 
  // check if display has partitial update 
  uint16_t incr = display.epd2.hasPartialUpdate;
  Log.verbose(F("Check hasPartitialUpdate: %d"), incr);
  
 	//Increment boot number and print it every reboot
	++BootCount;
  Log.info(F("Boot number: %d"), BootCount);

	//Print the wakeup reason for ESP32
	printWakeupReason();

  // read configuration data from NVM RAM
  readConfigurationData();

  // configuration webserver GUI only available after pressing user button
  // during device start for the first time
  // important configuration data will be checked
  if ( BootCount == 1  ) {
 	  
    // define pin mode for setting debug mode on
	  pinMode(buttonPin, INPUT_PULLUP);
  
    // display the first screen after system startup with a full refresh
    displayStartupScreen();
  
    // read the state of the user button value now
    // config server only starts after pressing the user button for 1 second
    checkUserButton();   
    
    // check important configuration data
    if ( strlen(WZConfig.Language) == 0 ) {
      Log.error(F("ERROR: Language empty"));
      displayMessage("ERROR: Language empty - check configuration!", wzGenInfoLine, u8g2_font_lubB14_tf);
      ConfigError = true;
      delay(5000);
    }

    if ( strlen(WZConfig.WIFI_SSID) == 0 ) {
      Log.error(F("ERROR: WIFI SSID empty"));
      displayMessage("ERROR: WIFI SSID empty - check configuration!", wzGenInfoLine, u8g2_font_lubB14_tf);
      ConfigError = true;
      delay(5000);
    }

    if ( strlen(WZConfig.tr_apikey) == 0 ) {
      Log.error(F("ERROR: API key empty, check config"));
      displayMessage("ERROR: API key empty - check configuration!", wzGenInfoLine, u8g2_font_lubB14_tf);
      ConfigError = true;
      delay(5000);
    }

    if ( strlen(WZConfig.tr_segmentid) == 0 ) {
      Log.error(F("ERROR: segment id not defined"));
      displayMessage("ERROR: segment id empty - check configuration!", wzGenInfoLine, u8g2_font_lubB14_tf);
      ConfigError = true;
      delay(5000);
    }

    if ( strlen(WZConfig.tr_segmentname) == 0 ) {
      Log.error(F("ERROR: segment name empty"));
      displayMessage("ERROR: segment name empty - check configuration!", wzGenInfoLine, u8g2_font_lubB14_tf);
      ConfigError = true;
      delay(5000);
    }
   
    if (ConfigError) {
      Log.error(F("ERROR: Wrong configuration, reboot!"));
      sprintf(display_message_buffer, "Press internal button for device configuration during first startup!");
      displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
      delay(5000);
      ESP.restart(); // restart device to give user a chance to edit configuration data
    }

  } // BootCount == 1
  
  Log.info(F("WiFiConnectionsFailed counter: >%d<"), WiFiConnectionsFailed);
  Log.info(F("NTPRequestsFailed counter: >%d<"), NTPRequestsFailed);
  Log.info(F("TrafficDataRequestsFailed counter: >%d<"), TrafficDataRequestsFailed);
  Log.info(F("SendHeartbeatsFailed counter: >%d<"), SendHeartbeatsFailed);
  Log.info(F("DailyTrafficSum: >%d<"), DailyTrafficSum);
  Log.info(F("SavedDay: >%d<"), SavedDay);

  // set text messages for ePaper based on configured language
  setMessagesByLanguage();

  if (startWiFi() == WL_CONNECTED && setupTime() == true ) {
    if ( CurrentHour >= atoi(WZConfig.tr_WakeupTime) && CurrentHour < atoi(WZConfig.tr_SleepTime) ) {
      WiFiClient client;
      delay(3000);

      Log.verbose(F("CurrentHour: %d"), CurrentHour);
      Log.verbose(F("CurrentMin: %d"), CurrentMin);
      Log.verbose(F("CurrentSec: %d"), CurrentSec);
          
      // recalculate sleep time in seconds
      if ( CurrentMin < atoi(WZConfig.tr_UpdateMinute)) {
        TIME_TO_SLEEP = (atoi(WZConfig.tr_UpdateMinute) - CurrentMin) * 60;
      } else {
        TIME_TO_SLEEP = (60 - CurrentMin + atoi(WZConfig.tr_UpdateMinute)) * 60;
      } 

      if ( CurrentMin >= atoi(WZConfig.tr_UpdateMinute) ) {
        Log.verbose(F("get traffic data and update display"));
        // request traffic data from Telraam API
        if ( getTrafficData(client) ) {
          // show traffic data on display
          if ( TrafficSumValid ) {
            displayTrafficData();
          }
        } else {
          Log.error(F("No valid traffic data, retry in 300 seconds"));
          TIME_TO_SLEEP = 300; // no valid traffic data: retry in 300 seconds
        }
        // don't send heartbeat if hb_apiurl is not defined
        if ( strlen(WZConfig.hb_apiurl) > 0 ) {
          sendHeartbeat(client);
        }
      } else {
        if ( BootCount == 1  ) {
          sprintf(display_message_buffer, "traffic data update hourly at minute: >%d<", atoi(WZConfig.tr_UpdateMinute) );
          displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
        }
      }
    } else {
      Log.info(F("current hour %d not inside time window from %d - %d"), CurrentHour, atoi(WZConfig.tr_WakeupTime), atoi(WZConfig.tr_SleepTime));
      if ( BootCount == 1  ) {
        sprintf(display_message_buffer, "current hour %d not inside time window from %d - %d", CurrentHour, atoi(WZConfig.tr_WakeupTime), atoi(WZConfig.tr_SleepTime));
        displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
      }
    }
  } else {
    // no WiFi connection: retry in 300 seconds
    Log.error(F("No WiFi connection, retry in 300 seconds"));
    TIME_TO_SLEEP = 300;
  }

  // stop WLAN connection
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);

  display.hibernate();
  // pinMode(EPD_CLK, INPUT_PULLUP); // TODO: for saving power in deep sleep

	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Log.info(F("Prepare ESP32 to sleep for %d seconds"), TIME_TO_SLEEP);
	// goto deep sleep now
  Log.info(F("going into deep sleep mode in 5 seconds, by by ..."));
  delay(5000);
	esp_deep_sleep_start();  

} // setup

// we don't need looping
void loop() { 

}