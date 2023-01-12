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

#include "globals.h"

// Include certificate data for local https connection
#include "https/cert.h"
#include "https/private_key.h"

extern const char * WZVersion;
extern void displayStartupScreen();
extern void displayMessage(const char message[60], int16_t y, const uint8_t *font);
extern char display_message_buffer[58];
extern int16_t wzGenInfoLine;
extern String wzDeviceId, wzWiFiId;

Preferences preferences;  // persistant data storage in NVS flash area
WZConfig_s  WZConfig;
uint8_t     configServerUptimeMax = 240; // wait time for user connection in seconds (max: 255 seconds)

String ExpertMode = "off";  // switch between ExpertMode=on (Default mode=off) to display extended config page

// We need to specify some content-type mapping, so the resources get delivered with the
// right content type and are displayed correctly in the browser
char contentTypes[][2][32] = {
  {".txt",  "text/plain"},
  {".html",  "text/html"},
  {".png",   "image/png"},
  {".jpg",   "image/jpg"},
  {"", ""}
};

// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

static const char *const HTTP_GET = "GET";
static const char *const HTTP_POST = "POST";

static const int BYTES_PER_KB = 1024;
static const int BYTES_PER_MB = 1024 * 1024;

static HTTPSServer * server;
static HTTPServer  * insecureServer;

bool configServerWasConnectedViaHttpFlag = false;

bool configServerWasConnectedViaHttp() {
  return configServerWasConnectedViaHttpFlag;
}

void touchConfigServerHttp() {
  configServerWasConnectedViaHttpFlag = true;
}

void eraseConfigData() {
  preferences.begin("WZpreferences", false);
  preferences.clear();
  sprintf(display_message_buffer, "device configuration data erased");
  displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
  delay(10000);
  preferences.end();
  ESP.restart();
}

void readConfigurationData() {
  Log.info(F("readConfigurationData"));
  // max length of preferences name 15 characters
  String wifi_ssid, wifi_password, ap_password;
  String tr_apikey,tr_segmentid,tr_segmentname, tr_WakeupTime, tr_SleepTime, tr_UpdateMinute;
  String project_url, Language, Timezone, ntpServer, gmtOffset, daylightOffset, hb_apiurl, hb_authkey;

  preferences.begin("WZpreferences", false);

  wifi_ssid = preferences.getString("wifi_ssid", "");
  wifi_password = preferences.getString("wifi_password", "");
  ap_password = preferences.getString("ap_password", "");
  project_url = preferences.getString("project_url", "");
  tr_apikey = preferences.getString("tr_apikey", "");
  tr_segmentid = preferences.getString("tr_segmentid", "");
  tr_segmentname = preferences.getString("tr_segmentname", "");
  tr_WakeupTime = preferences.getString("tr_WakeupTime", "5");
  tr_SleepTime = preferences.getString("tr_SleepTime", "23");
  tr_UpdateMinute = preferences.getString("tr_UpdateMinute", "20");
  Language = preferences.getString("Language", "");
  Timezone = preferences.getString("Timezone", "CET-1CEST,M3.5.0,M10.5.0/3");
  ntpServer = preferences.getString("ntpServer", "europe.pool.ntp.org");
  gmtOffset = preferences.getString("gmtOffset", "0");
  daylightOffset = preferences.getString("daylightOffset", "0");
  hb_apiurl = preferences.getString("hb_apiurl", ""); // URL MUST end with a trailing /
  hb_authkey = preferences.getString("hb_authkey", "");

  // save preference variables into global object
  wifi_ssid.toCharArray(WZConfig.WIFI_SSID,wifi_ssid.length()+1);
  wifi_password.toCharArray(WZConfig.WIFI_PASSWORD,wifi_password.length()+1);
  ap_password.toCharArray(WZConfig.AP_PASSWORD,ap_password.length()+1);
  project_url.toCharArray(WZConfig.project_url,project_url.length()+1);
  tr_apikey.toCharArray(WZConfig.tr_apikey,tr_apikey.length()+1);
  tr_segmentid.toCharArray(WZConfig.tr_segmentid,tr_segmentid.length()+1);
  tr_segmentname.toCharArray(WZConfig.tr_segmentname,tr_segmentname.length()+1);
  tr_WakeupTime.toCharArray(WZConfig.tr_WakeupTime,tr_WakeupTime.length()+1);
  tr_SleepTime.toCharArray(WZConfig.tr_SleepTime,tr_SleepTime.length()+1);
  tr_UpdateMinute.toCharArray(WZConfig.tr_UpdateMinute,tr_UpdateMinute.length()+1);
  Language.toCharArray(WZConfig.Language,Language.length()+1);
  Timezone.toCharArray(WZConfig.Timezone,Timezone.length()+1);
  ntpServer.toCharArray(WZConfig.ntpServer,ntpServer.length()+1);
  gmtOffset.toCharArray(WZConfig.gmtOffset,gmtOffset.length()+1);
  daylightOffset.toCharArray(WZConfig.daylightOffset,daylightOffset.length()+1);
  hb_apiurl.toCharArray(WZConfig.hb_apiurl,hb_apiurl.length()+1);
  hb_authkey.toCharArray(WZConfig.hb_authkey,hb_authkey.length()+1);

  Log.verbose(F("readConfigurationData: print configuration data read from NVM"));
  Log.verbose(F("WIFI_SSID: >%s<"), WZConfig.WIFI_SSID);
  Log.verbose(F("WIFI_PASSWORD: >%s<"), WZConfig.WIFI_PASSWORD);
  Log.verbose(F("AP_PASSWORD: >%s<"), WZConfig.AP_PASSWORD);
  Log.verbose(F("Project URL: >%s<"), WZConfig.project_url);
  Log.verbose(F("Telraam API key: >%s<"), WZConfig.tr_apikey);
  Log.verbose(F("Telraam segment id: >%s<"), WZConfig.tr_segmentid);
  Log.verbose(F("Telraam segment name: >%s<"), WZConfig.tr_segmentname);
  Log.verbose(F("wake-up time: >%s<"), WZConfig.tr_WakeupTime);
  Log.verbose(F("sleep time: >%s<"), WZConfig.tr_SleepTime);
  Log.verbose(F("update minute: >%s<"), WZConfig.tr_UpdateMinute);
  Log.verbose(F("Language: >%s<"), WZConfig.Language);
  Log.verbose(F("Timezone: >%s<"), WZConfig.Timezone);
  Log.verbose(F("ntpServer: >%s<"), WZConfig.ntpServer);
  Log.verbose(F("gmtOffset: >%s<"), WZConfig.gmtOffset);
  Log.verbose(F("daylightOffset: >%s<"), WZConfig.daylightOffset);
  Log.verbose(F("hb_apiurl: >%s<"), WZConfig.hb_apiurl);
  Log.verbose(F("hb_authkey: >%s<"), WZConfig.hb_authkey);

  preferences.end();
}

void writeConfigDataWifi()
{
  // Open Preferences with my-app namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("WZpreferences", false);

  preferences.putString("wifi_ssid", WZConfig.WIFI_SSID);
  preferences.putString("wifi_password", WZConfig.WIFI_PASSWORD);

  preferences.end();
}

void writeConfigData()
{
  preferences.begin("WZpreferences", false);

  preferences.putString("project_url", WZConfig.project_url);
  preferences.putString("tr_apikey", WZConfig.tr_apikey);
  preferences.putString("tr_segmentid", WZConfig.tr_segmentid);
  preferences.putString("tr_segmentname", WZConfig.tr_segmentname);
  preferences.putString("tr_WakeupTime", WZConfig.tr_WakeupTime);
  preferences.putString("tr_SleepTime", WZConfig.tr_SleepTime);
  preferences.putString("tr_UpdateMinute", WZConfig.tr_UpdateMinute);
  preferences.putString("Language", WZConfig.Language);
  preferences.putString("Timezone", WZConfig.Timezone);
  preferences.putString("ntpServer", WZConfig.ntpServer);
  preferences.putString("gmtOffset", WZConfig.gmtOffset);
  preferences.putString("daylightOffset", WZConfig.daylightOffset);
  preferences.putString("hb_apiurl", WZConfig.hb_apiurl);
  preferences.putString("hb_authkey", WZConfig.hb_authkey);

  preferences.end();
}

// Create an SSL certificate object from the files included above
SSLCert WZcert = SSLCert(
  wz_crt_DER, wz_crt_DER_len,
  wz_key_DER, wz_key_DER_len
);

// Style
static const String style =
  "<style>"
  "#file-input,input, button {width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px;}"
  "input, button, a.back {background:#f1f1f1;border:0;padding:0;text-align:center;}"
  "body {background:#3498db;font-family:sans-serif;font-size:12px;color:#777}"
  "#file-input {padding:0 5px;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar {background-color:#f1f1f1;border-radius:10px}"
  "#bar {background-color:#3498db;width:0%;height:10px}"
  "form {background:#fff;max-width:400px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn {background:#3498db;color:#fff;cursor:pointer}"
  "h1,h2, h3 {padding:0;margin:0;}"
  "h3 {padding:10px 0;margin-top:10px;margin-bottom:10px;border-top:3px solid #3498db;border-bottom:3px solid #3498db;}"
  "h1 a {color:#777}"
  "h2 {margin-top:5px}"
  "hr { border-top:1px solid #CCC;margin-left:10px;margin-right:10px;}"
  ".deletePrivacyArea, a.back {color: black; text-decoration: none; font-size: x-large;}"
  ".deletePrivacyArea:hover {color: red;}"
  "a.previous {text-decoration: none; display: inline-block; padding: 8px 16px;background-color: #f1f1f1; color: black;border-radius: 50%; font-family: Verdana, sans-serif; font-size: 18px}"
  "a.previous:hover {background-color: #ddd; color: black;}"
  "ul.directory-listing {list-style: none; text-align: left; padding: 0; margin: 0; line-height: 1.5;}"
  "li.directory a {text-decoration: none; font-weight: bold;}"
  "li.file a {text-decoration: none;}"
  "</style>";

static const String previous = "<a href=\"javascript:history.back()\" class='previous'>&#8249;</a>";

static const String header =
  "<!DOCTYPE html>\n"
  "<html lang='en'><head><meta charset='utf-8'/><title>{title}</title>" + style +
  "<link rel='icon' href='data:;base64,iVBORw0KGgo=' />"
  "<script>"
  "window.onload = function() {"
  "  if (window.location.pathname == '/') {"
  "    document.querySelectorAll('.previous')[0].style.display = 'none';"
  "  } else {"
  "    document.querySelectorAll('.previous')[0].style.display = '';"
  "  }"
  "}"
  "</script></head><body>"
  ""
  "<form action='{action}' method='POST'>"
  "<h1><a href='/'>WZePaperDisplay</a></h1>"
  "<h2>{subtitle}</h2>"
  "<p>Firmware version: {version}</p>"
  + previous;

static const String footer = "</form></body></html>";

// #########################################
// Upload form
// #########################################
static const String xhrUpload =   
  "<input type='file' name='upload' id='file' accept='{accept}'>"
  "<label id='file-input' for='file'>Choose file...</label>"
  "<input id='btn' type='submit' class=btn value='Upload'>"
  "<br><br>"
  "<div id='prg'></div>"
  "<br><div id='prgbar'><div id='bar'></div></div><br>" // </form>"
  "<script>"
  ""
  "function hide(x) { x.style.display = 'none'; }"
  "function show(x) { x.style.display = 'block'; }"
  ""
  "hide(document.getElementById('file'));"
  "hide(document.getElementById('prgbar'));"
  "hide(document.getElementById('prg'));"
  ""
  "var fileName = '';"
  "document.getElementById('file').addEventListener('change', function(e){"
  "fileNameParts = e.target.value.split('\\\\');"
  "fileName = fileNameParts[fileNameParts.length-1];"
  "console.log(fileName);"
  "document.getElementById('file-input').innerHTML = fileName;"
  "});"
  ""
  "document.getElementById('btn').addEventListener('click', function(e){"
  "e.preventDefault();"
  "if (fileName == '') { alert('No file chosen'); return; }"
  "console.log('Start upload...');"
  ""
  "var form = document.getElementsByTagName('form')[0];"
  "var data = new FormData(form);"
  "console.log(data);"
  //https://developer.mozilla.org/en-US/docs/Web/API/FormData/values
  "for (var v of data.values()) { console.log(v); }"
  ""
  "hide(document.getElementById('file-input'));"
  "hide(document.getElementById('btn'));"
  "show(document.getElementById('prgbar'));"
  "show(document.getElementById('prg'));"
  ""
  "var xhr = new XMLHttpRequest();"
  "xhr.open( 'POST', '{method}', true );"
  "xhr.onreadystatechange = function(s) {"
  "console.log(xhr.responseText);"
  "if (xhr.readyState == 4 && xhr.status == 200) {"
  "document.getElementById('prg').innerHTML = xhr.responseText;"
  "} else if (xhr.readyState == 4 && xhr.status == 500) {"
  "document.getElementById('prg').innerHTML = 'Upload error:' + xhr.responseText;"
  "} else {"
  "document.getElementById('prg').innerHTML = 'Unknown error';"
  "}"
  "};"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = Math.round((evt.loaded * 100) / evt.total);"
  "if(per == 100) document.getElementById('prg').innerHTML = 'Updating...';"
  "else document.getElementById('prg').innerHTML = 'Upload progress: ' + per + '%';"
  "document.getElementById('bar').style.width = per + '%';"
  "}"
  "}, false);"
  "xhr.send( data );"
  "});" // btn click
  ""
  "</script>";

// #########################################
// Navigation
// #########################################
static const String navigationIndex =
  "<h3>Settings</h3>"
  "<input type=button onclick=\"window.location.href='/settings/configmode'\" class=btn value='Configuration Mode'>"
  "<input type=button onclick=\"window.location.href='/settings/general'\" class=btn value='General'>"
  "<input type=button onclick=\"window.location.href='/settings/wifi'\" class=btn value='Wifi'>"

   "<h3>Maintenance</h3>"
  "<input type=button onclick=\"window.location.href='/update'\" class=btn value='Update Firmware'>"
  "<input type=button onclick=\"window.location.href='/about'\" class=btn value='About'>"
  "<input type=button onclick=\"window.location.href='/reboot'\" class=btn value='Reboot and activate configuration'>"
  "{dev}";

static const String httpsRedirect =
  "<h3>HTTPS</h3>"
  "You need to access the device for configuration via secure https. If not done already, you also need to "
  "accept the self signed certificate after pressing 'Goto https'."
  "<input type=button onclick=\"window.location.href='https://{host}'\" class=btn value='Goto https'>";

// Server Index Page
static const String uploadIndex = "<h3>Update</h3>";

// #########################################
// Reboot
// #########################################
static const String rebootIndex =
  "<h3>Device reboots now.</h3>";

// #########################################
// Wifi
// #########################################
static const String wifiSettingsIndex =
  "<script>"
  "function resetPassword() { document.getElementById('pass').value = ''; }"
  "</script>"
  "<h3>Settings</h3>"
  "SSID"
  "<input name=ssid placeholder='ssid' type='text' required maxlength='30' value='{ssid}'>"
  "<small>max length: 30</small>"
  "<hr>"
  "Password"
  "<input id=pass name=pass placeholder='password' type='Password' maxlength='30' value='{password}' onclick='resetPassword()'>"
  "<small>not required (e.g. for Freifunk/Germany) - max length: 30</small>"
  "<input type=submit class=btn value=Save>";

// #########################################
// Config mode index
// #########################################
static const String ConfigModeIndex =
  "<h3>Configuration mode</h3>"
  "<label for=''>ExpertMode </label>"
  "<select name='ExpertMode' id='ExpertMode'>" 
  "<option value='{ExpertMode}' selected>current: {ExpertMode}</option>"
  "<option value='on' >on</option>"
  "<option value='off' >off</option>"
  "</select>"
  "<br>"
  "<small>ExpertMode=on switches to advanced configuration options in menue General for the current session</small>"
  "<hr>"
  "<input type=submit class=btn value=Change>";

// #########################################
// Config
// #########################################
static const String configIndexDefault =
  "<script>"
  "function resetAPIKey() { document.getElementById('tr_apikey').value = ''; }"
  "</script>"

  "<h3>Language/Time</h3>"
  "<label for='Language'>Language </label>"
  "<select name='Language' id='Language'>" 
  "<option value='{Language}' selected>current: {Language}</option>"
  "<option value='DE' >DE</option>"
  "<option value='EN' >EN</option>"
  "<option value='NL' >NL</option>"
  "<option value='FR' >FR</option>"
  "<option value='ES' >ES</option>"
  "<option value='SL' >SL</option>"
  "</select>"
  "<hr>"

  // see: https://sites.google.com/a/usapiens.com/opnode/time-zones
  "<label for='Timezone'>Timezone </label>"
  "<select name='Timezone' id='Timezone'>" 
  "<option value='{Timezone}' selected>current: {Timezone}</option>"
  "<option value='CET-1CEST,M3.5.0,M10.5.0/3'      >Amsterdam, Netherlands</option>"
  "<option value='EET-2EEST-3,M3.5.0/3,M10.5.0/4'  >Athens, Greece</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Barcelona, Spain</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Berlin, Germany</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3:' >Brussels, Belgium</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Budapest, Hungary</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Copenhagen, Denmark</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Dublin, Ireland</option>"
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Geneva, Switzerland</option>"
  "<option value='EET-2EEST-3,M3.5.0/3,M10.5.0/4'  >Helsinki, Finland</option>"
  "<option value='EET-2EEST,M3.5.0/3,M10.5.0/4'    >Kyiv, Ukraine</option>"  
  "<option value='WET-0WEST-1,M3.5.0/1,M10.5.0/2'  >Lisbon, Portugal</option>" 
  "<option value='GMT+0BST-1,M3.5.0/1,M10.5.0/2'   >London, Great Britain</option>" 
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Madrid, Spain</option>" 
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Oslo, Norway</option>" 
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Paris, France</option>" 
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Prague, Czech Republic</option>" 
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Roma, Italy</option>" 
  "<option value='MSK-3MSD,M3.5.0/2,M10.5.0/3'     >Moscow, Russia</option>"  
  "<option value='EET-2EEST-3,M3.5.0/3,M10.5.0/4'  >Sofia, Bulgaria</option>" 
  "<option value='MST-3MDT,M3.5.0/2,M10.5.0/3'     >St.Petersburg, Russia</option>"  
  "<option value='CET-1CEST-2,M3.5.0/2,M10.5.0/3'  >Stockholm, Sweden</option>" 
  "<option value='EET-2EEST-3,M3.5.0/3,M10.5.0/4'  >Tallinn, Estonia</option>"   
  "<option value='CET-1CEST,M3.5.0,M10.5.0/3'      >Warsaw, Poland</option>" 
   
  // USA AND CANADA
  "<option value='HAW10'                           >Hawaii Time</option>"
  "<option value='AKST9AKDT,M3.2.0,M11.1.0'        >Alaska Time</option>"
  "<option value='PST8PDT,M3.2.0,M11.1.0'          >Pacific Time</option>"
  "<option value='MST7MDT,M3.2.0,M11.1.0'          >Mountain Time</option>"
  "<option value='MST7'                            >Mountain Time (Arizona, no DST)</option>"
  "<option value='CST6CDT,M3.2.0,M11.1.0'          >Central Time</option>"
  "<option value='EST5EDT,M3.2.0,M11.1.0'          >Eastern Time</option>"
  "<option value='AST4ADT'                         >Atlantic Time</option>"

  // AUSTRALIA
  "<option value='AEST-10AEDT-11,M10.5.0/2,M4.1.0/3'      >Melbourne,Canberra,Sydney</option>"
  "<option value='AWST-8AWDT-9,M10.5.0,M3.5.0/3'          >Perth</option>"
  "<option value='AEST-10'                                >Brisbane</option>"
  "<option value='ACST-9:30ACDT-10:30,M10.5.0/2,M4.1.0/3' >Adelaide</option>"
  "<option value='ACST-9:30'                              >Darwin</option>"
  "<option value='AEST-10AEDT-11,M10.1.0/2,M4.1.0/3'      >Hobart</option>"

  // NEW ZEALAND
  "<option value='NZST-12NZDT-13,M10.1.0/2,M3.3.0/3'      >Auckland, Wellington</option>"

  // ASIA
  "<option value='WIB-7'                           >Jakarta</option>"
  "<option value='SGT-8'                           >Singapore</option>"
  "<option value='HKT-8'                           >Hong Kong</option>"
  "<option value='ULAT-8ULAST,M3.5.0/2,M9.5.0/2'   >Ulaanbaatar, Mongolia</option>"

  // CENTRAL AND SOUTH AMERICA
  "<option value='BRST+3BRDT+2,M10.3.0,M2.3.0'     >Brazil, São Paulo</option>"
  "<option value='UTC+5'                           >Colombia</option>"
  "<option value='UTC+3'                           >Argentina</option>"
  "<option value='UTC+6'                           >Central America</option>"

  "</select>"
  "<hr>"

  "<label for='ntpServer'>ntpServer </label>"
  "<select name='ntpServer' id='ntpServer'>" 
  "<option value='{ntpServer}' selected>current: {ntpServer}</option>"
  "<option value='europe.pool.ntp.org'            >Europe</option>"
  "<option value='north-america.pool.ntp.org'     >North-America</option>"
  "<option value='oceania.pool.ntp.org'           >Oceania</option>"
  "<option value='asia.pool.ntp.org'              >Asia</option>"
  "<option value='pool.ntp.org'                   >global: pool.ntp.org</option>"
  "</select>"

  "<h3>Telraam</h3>"
  "API key"
  "<input name=tr_apikey placeholder='tr_apikey' type='text' required pattern='[a-zA-Z0-9]{40}' value='{tr_apikey}' onclick='resetAPIKey()'>"
  "<small>API token from your Telraam account - length must equal to: 40 - allowed pattern:[a-zA-Z0-9]</small>"
   "<hr>"

  "API segment id"
  "<input name=tr_segmentid placeholder='tr_segmentid' type='text' required pattern='[0-9]{10}' value='{tr_segmentid}'"
  "<small>Telraam internal street number - length must equal to: 10 - allowed pattern: [0-9]</small>"
  "<hr>"

  "display - segment name"
  "<input name=tr_segmentname placeholder='tr_segmentname' type='text' required maxlength='35' value='{tr_segmentname}'>"
  "<small>street name on display - max length: 35</small>"
  "<hr>"

  "update minute"
  "<input name=tr_UpdateMinute placeholder='tr_UpdateMinute' type='number' required step='1' min='20' max='55' value='{tr_UpdateMinute}'>"
  "<small>traffic data update hourly at that minute - allowed value: 20-55</small>"
 
"<h3>Other</h3>"
  "wake-up time (hour of day)"
  "<input name=tr_WakeupTime placeholder='tr_WakeupTime' type='number' required step='1' min='0' max='23' value='{tr_WakeupTime}'>"
  "<small>don't fetch traffic data before that time - allowed value: 0-23</small>"
  "<hr>"

  "<label id='sleep time' >sleep time (hour of day)</label>"
  "sleep time (hour of day)"
  "<input name=tr_SleepTime placeholder='tr_SleepTime' type='number' required step='1' min='0' max='23' value='{tr_SleepTime}'>"
  "<small>don't fetch traffic data after that time - allowed value: 0-23</small>"
  "<hr>"

  "project URL (optional)"
  "<input name=project_url placeholder='project_url' type='text' maxlength='50' value='{project_url}'>"
  "<small>your own project page URL - max length: 50</small>"

  "<input type=submit class=btn value=Save>";

static const String configIndexExpert =
  "<script>"
  "function resetHBAUTHKey() { document.getElementById('hb_authkey').value = ''; }"
  "</script>"

  "<hr>"
  "Timezone"
  "<input name=Timezone placeholder='Timezone  ' type='text' required maxlength='39' value='{Timezone}'>"
  "<small>see: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv "
  "or https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html "
  "or https://sites.google.com/a/usapiens.com/opnode/time-zones "
  "- max length: 39</small>"
  "<hr>"

  "ntpServer"
  "<input name=ntpServer placeholder='ntpServer ' type='text' required maxlength='39' value='{ntpServer}'>"
  "<small>choose from https://www.ntppool.org "
  "- max length: 39</small>"

  "<hr>"
  "gmtOffset (in seconds)" 
  "<input name=gmtOffset placeholder='gmtOffset' type='text' required maxlength='39' pattern='[0-9]' value='{gmtOffset}'>"
  "<small>UK normal time is GMT, so GMT Offset is 0, for DE (+1hr) use 3600, "
  "for US (-5hrs) is typically -18000, AU is typically (+8hrs) 28800 "
  "- max length: 39 - allowed pattern: [0-9]</small>"
  "<hr>"

  "daylightOffset (in seconds)"
  "<input name=daylightOffset placeholder='daylightOffset' type='text' required maxlength='39' pattern='[0-9]' value='{daylightOffset}'>"
  "<small>In the UK DST is +1hr (3600-secs), other countries (e.g. DE) may use 2hrs (7200-secs) "
  "or 30-mins (1800-secs) or 5.5hrs (19800-secs); ahead of GMT use + offset behind - offset "
  "- max length: 39 - allowed pattern: [0-9]</small>"

  "<h3>Heartbeat server (optional)</h3>"
  "URL"
  "<input name=hb_apiurl placeholder='hb_apiurl' type='text' maxlength='50' value='{hb_apiurl}'>"
  "<small>server which receives heartbeat messages from the device - max length: 50 </small>"
  "<hr>"

  "Authentication key"
  "<input name=hb_authkey placeholder='hb_authkey' type='password'  maxlength='70' value='{hb_authkey}' onclick='resetHBAUTHKey()'>"
  "<small>key for http basic authentication (Base64) to access the heartbeat server - max length: 70</small>"

  "<input type=submit class=btn value=Save>";

String encodeForXmlAttribute(const String &text) {
  String result(text);
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  result.replace(">", "&gt;");
  result.replace("'", "&#39;");
  result.replace("\"", "&#34;");
  return result;
}

String encodeForXmlText(const String &text) {
  String result(text);
  result.replace("&", "&amp;");
  result.replace("<", "&lt;");
  return result;
}

static String replaceHtml(String &body, const String &key, const String &value) {
  String str(body);
  str.replace(key, encodeForXmlAttribute(value));
  return str;
}

static String keyValue(const String& key, const String& value, const String& suffix = "") {
  return "<b>" + encodeForXmlText(key) + ":</b> " + value + suffix + "<br />";
}

static void handleNotFound(HTTPRequest * req, HTTPResponse * res);
static void handleIndex(HTTPRequest * req, HTTPResponse * res);
static void handleAbout(HTTPRequest * req, HTTPResponse * res);
static void handleReboot(HTTPRequest * req, HTTPResponse * res);
static void handleWifi(HTTPRequest * req, HTTPResponse * res);
static void handleWifiSave(HTTPRequest * req, HTTPResponse * res);
static void handleConfigMode(HTTPRequest * req, HTTPResponse * res);
static void handleConfigModeSave(HTTPRequest * req, HTTPResponse * res);
static void handleConfig(HTTPRequest * req, HTTPResponse * res);
static void handleConfigSave(HTTPRequest * req, HTTPResponse * res);
static void handleFirmwareUpdate(HTTPRequest * req, HTTPResponse * res);
static void handleFirmwareUpdateAction(HTTPRequest * req, HTTPResponse * res);
static void handleHttpsRedirect(HTTPRequest *req, HTTPResponse *res);
// not used currently static void accessFilter(HTTPRequest * req, HTTPResponse * res, std::function<void()> next);

String getIp() {
  if (WiFiClass::status() != WL_CONNECTED) {
    return WiFi.softAPIP().toString();
  } else {
    return WiFi.localIP().toString();
  }
}

bool CreateWifiSoftAP() {
  bool SoftAccOK;
  WiFi.disconnect();
  Log.info(F("Initalize SoftAP "));
  String APName = wzWiFiId;
  String APPassword = WZConfig.AP_PASSWORD;

  if (APPassword.length() < 9) {
    // Generate a new random password
    char defaultAPPassword[11];
    snprintf(defaultAPPassword, 10, "%09u", esp_random() % 1000000000);
    APPassword = String(defaultAPPassword);
    Log.verbose(F("APPassword generated: %s"), APPassword);

    // Store the password into NVS so it does not change
    preferences.begin("WZpreferences", false);
    APPassword.toCharArray(WZConfig.AP_PASSWORD,APPassword.length()+1);
    preferences.putString("ap_password", WZConfig.AP_PASSWORD);
    // read saved value for verification
    Log.verbose(F("Read saved ap_password from NVS: %s"), preferences.getString("ap_password", ""));
    // Close the Preferences
    preferences.end();
  }

  sprintf(display_message_buffer, "connect to %s with password: %s", APName.c_str(), APPassword.c_str());
  displayMessage(display_message_buffer, wzGenInfoLine, u8g2_font_lubB14_tf);
  
  SoftAccOK  =  WiFi.softAP(APName.c_str(), APPassword.c_str());
  delay(2000); // Without delay I've seen the IP address blank
  /* Soft AP network parameters */
  IPAddress apIP(172, 20, 0, 1);
  IPAddress netMsk(255, 255, 255, 0);

  WiFi.softAPConfig(apIP, apIP, netMsk);
  
  if (SoftAccOK) {
    Log.info("AP successful.");
  } else {
    Log.error(F("ERROR: soft access point for configuration web server "));
    Log.error(F("ERROR: name= %s"), APName.c_str());
    Log.error(F("ERROR: password= %s"), APPassword.c_str());
  }
  return SoftAccOK;
}

String createPage(const String& content, const String& additionalContent = "") {
  configServerWasConnectedViaHttpFlag = true;
  String result;
  result += header;
  result += content;
  result += additionalContent;
  result += footer;

  return result;
}

void sendHtml(HTTPResponse * res, String& data) {
  res->setHeader("Content-Type", "text/html");
  res->print(data);
}

void sendHtml(HTTPResponse * res, const char * data) {
  res->setHeader("Content-Type", "text/html");
  res->print(data);
}

void sendRedirect(HTTPResponse * res, String location) {
  res->setHeader("Location", location.c_str());
  res->setStatusCode(302);
  res->finalize();
}

String replaceDefault(String html, const String& subTitle, const String& action = "#") {
  configServerWasConnectedViaHttpFlag = true;
  html = replaceHtml(html, "{title}",wzWiFiId + " - " + subTitle);
  html = replaceHtml(html, "{version}", WZVersion);
  html = replaceHtml(html, "{subtitle}", subTitle);
  html = replaceHtml(html, "{action}", action);

  String ip;
  if (WiFiGenericClass::getMode() == WIFI_MODE_STA) {
    ip = WiFi.localIP().toString();
  } else {
    ip = WiFi.softAPIP().toString();
  }
  
  return html;
}

std::vector<std::pair<String,String>> extractParameters(HTTPRequest *req) {
  Log.info(F("Extracting parameters"));
  std::vector<std::pair<String,String>> parameters;
  if (String(req->getHeader("Content-Type").c_str()).startsWith("application/x-www-form-urlencoded")) {
    HTTPURLEncodedBodyParser parser(req);
    while(parser.nextField()) {
      std::pair<String,String> data;
      data.first = String(parser.getFieldName().c_str());
      data.second = String();
      while (!parser.endOfField()) {
        char buf[513];
        size_t readLength = parser.read((uint8_t *)buf, 512);
        buf[readLength] = 0;
        data.second += String(buf);
      }
      Log.info(F("Http Parameter %s = %s"), data.first.c_str(), data.second.c_str());
      parameters.push_back(data);
    }
  } else {
    Log.error(F("ERROR: Unexpected content type: %s"), req->getHeader("Content-Type").c_str());
  }
  return parameters;
}

static String getParameter(const std::vector<std::pair<String,String>> &params, const String& name, const String&  def = "") {
  for (auto param : params) {
    if (param.first == name) {
      return param.second;
    }
  }
  return def;
}

String toScaledByteString(uint32_t size) {
  String result;
  if (size <= BYTES_PER_KB * 10) {
    result = String(size) + "b";
  } else if (size <= BYTES_PER_MB * 10) {
    result = String(size / BYTES_PER_KB) + "kb";
  } else {
    result = String(size / BYTES_PER_MB) + "mb";
  }
  return result;
}

static void handleIndex(HTTPRequest *, HTTPResponse * res) {
// ###############################################################
// ### Index ###
// ###############################################################
  String html = createPage(navigationIndex);
  html = replaceDefault(html, "Navigation");
#ifdef DEVELOP
  html.replace("{dev}", development);
#else
  html.replace("{dev}", "");
#endif
  sendHtml(res, html);
}

static void handleNotFound(HTTPRequest * req, HTTPResponse * res) {
  // Discard request body, if we received any
  // We do this, as this is the default node and may also server POST/PUT requests
  req->discardRequestBody();

  // Set the response status
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  // Set content type of the response
  res->setHeader("Content-Type", "text/html");

  // Write a tiny HTTP page
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Not Found</title></head>");
  res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
  res->println("</html>");
}

static void handleReboot(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(rebootIndex);
  html = replaceDefault(html, "Reboot");
  sendHtml(res, html);
  res->finalize();
  delay(1000);
  ESP.restart();
};

static void handleAbout(HTTPRequest *, HTTPResponse * res) {
  res->setHeader("Content-Type", "text/html");
  res->print(replaceDefault(header, "About"));
  String page;
  
  res->print("<h3>ESP32</h3>"); // SPDIFF
  page += keyValue("Heap size", toScaledByteString(ESP.getHeapSize()));
  page += keyValue("Free heap", toScaledByteString(ESP.getFreeHeap()));
  page += keyValue("Min. free heap", toScaledByteString(ESP.getMinFreeHeap()));
  String chipId = String((uint32_t) ESP.getEfuseMac(), HEX) + String((uint32_t) (ESP.getEfuseMac() >> 32), HEX);
  chipId.toUpperCase();
  page += keyValue("Chip id", chipId);
  page += keyValue("IDF Version", esp_get_idf_version());
  page += keyValue("wzDeviceId", wzDeviceId);
  page += keyValue("Build date:", BUILD_DATE);

  res->print(page);
  res->print(footer);
}

static void handleConfigMode(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(ConfigModeIndex);
  html = replaceDefault(html, "Configuration Mode", "/settings/configmode/action");

  // Form data
  html = replaceHtml(html, "{ExpertMode}", ExpertMode);
  sendHtml(res, html);
};

static void handleConfigModeSave(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  ExpertMode = getParameter(params, "ExpertMode");
  Log.info(F("Configuration ExpertMode set to: %s"), ExpertMode);
  sendRedirect(res, "/settings/configmode");
}

static void handleWifi(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(wifiSettingsIndex);
  html = replaceDefault(html, "WiFi", "/settings/wifi/action");

  // Form data
  html = replaceHtml(html, "{ssid}", WZConfig.WIFI_SSID);
  if (strlen(WZConfig.WIFI_PASSWORD) > 0 ) {
    html = replaceHtml(html, "{password}", "******");
  } else {
    html = replaceHtml(html, "{password}", "");
  }
  sendHtml(res, html);
};

static void handleWifiSave(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  const auto ssid = getParameter(params, "ssid");
  if (ssid) {
    ssid.toCharArray(WZConfig.WIFI_SSID,ssid.length()+1);
  }
  const auto password = getParameter(params, "pass");
  if (password != "******") {
    password.toCharArray(WZConfig.WIFI_PASSWORD,password.length()+1);
  }

  Log.info(F("WIFI_SSID set to: %s"), WZConfig.WIFI_SSID);
  Log.info(F("Save WZConfig to NVS RAM ..."));
  writeConfigDataWifi();

  sendRedirect(res, "/settings/wifi");
}

static void handleConfig(HTTPRequest *, HTTPResponse * res) {
  String html;
  if ( ExpertMode == "on" ) {
    html = createPage(configIndexExpert);
    html = replaceDefault(html, "General", "/settings/general/action");
  } else {
    html = createPage(configIndexDefault);
    html = replaceDefault(html, "General", "/settings/general/action");
  }

  // Form data
  html = replaceHtml(html, "{Language}", WZConfig.Language);
  html = replaceHtml(html, "{Timezone}", WZConfig.Timezone);
  html = replaceHtml(html, "{ntpServer}", WZConfig.ntpServer);
  html = replaceHtml(html, "{tr_apikey}", WZConfig.tr_apikey);
  html = replaceHtml(html, "{tr_segmentid}", WZConfig.tr_segmentid);
  html = replaceHtml(html, "{tr_segmentname}", WZConfig.tr_segmentname);
  html = replaceHtml(html, "{tr_WakeupTime}", WZConfig.tr_WakeupTime);
  html = replaceHtml(html, "{tr_SleepTime}", WZConfig.tr_SleepTime);
  html = replaceHtml(html, "{tr_UpdateMinute}", WZConfig.tr_UpdateMinute);
  html = replaceHtml(html, "{project_url}", WZConfig.project_url);

  if ( ExpertMode == "on" ) {
    html = replaceHtml(html, "{Timezone}", WZConfig.Timezone);
    html = replaceHtml(html, "{ntpServer}", WZConfig.ntpServer);
    html = replaceHtml(html, "{gmtOffset}", WZConfig.gmtOffset);
    html = replaceHtml(html, "{daylightOffset}", WZConfig.daylightOffset);
    html = replaceHtml(html, "{hb_apiurl}", WZConfig.hb_apiurl);
    html = replaceHtml(html, "{hb_authkey}", WZConfig.hb_authkey); 
  }

  sendHtml(res, html);
};

static void handleConfigSave(HTTPRequest * req, HTTPResponse * res) {
  const auto params = extractParameters(req);
  
  if ( ExpertMode == "off" ) {
  const auto Language = getParameter(params, "Language");
  if (Language) {
    Language.toCharArray(WZConfig.Language,Language.length()+1);
  }

  const auto Timezone = getParameter(params, "Timezone");
  if (Timezone) {
    Timezone.toCharArray(WZConfig.Timezone,Timezone.length()+1);
  }

  const auto ntpServer = getParameter(params, "ntpServer");
  if (ntpServer) {
    ntpServer.toCharArray(WZConfig.ntpServer,ntpServer.length()+1);
  }

  const auto tr_apikey = getParameter(params, "tr_apikey");
  if (tr_apikey) {
    tr_apikey.toCharArray(WZConfig.tr_apikey,tr_apikey.length()+1);
  }

  const auto tr_segmentid = getParameter(params, "tr_segmentid");
  if (tr_segmentid) {
    tr_segmentid.toCharArray(WZConfig.tr_segmentid,tr_segmentid.length()+1);
  }

  const auto tr_segmentname = getParameter(params, "tr_segmentname");
  if (tr_segmentname) {
    tr_segmentname.toCharArray(WZConfig.tr_segmentname,tr_segmentname.length()+1);
  }

  const auto tr_WakeupTime = getParameter(params, "tr_WakeupTime");
  if (tr_WakeupTime) {
    tr_WakeupTime.toCharArray(WZConfig.tr_WakeupTime,tr_WakeupTime.length()+1);
  }

  const auto tr_SleepTime = getParameter(params, "tr_SleepTime");
  if (tr_SleepTime) {
    tr_SleepTime.toCharArray(WZConfig.tr_SleepTime,tr_SleepTime.length()+1);
  } 

  const auto tr_UpdateMinute = getParameter(params, "tr_UpdateMinute");
  if (tr_UpdateMinute) {
    tr_UpdateMinute.toCharArray(WZConfig.tr_UpdateMinute,tr_UpdateMinute.length()+1);
  } 

  const auto project_url = getParameter(params, "project_url");
  if (project_url) {
    project_url.toCharArray(WZConfig.project_url,project_url.length()+1);
  }

  } else {

  const auto Timezone = getParameter(params, "Timezone");
  if (Timezone) {
    Timezone.toCharArray(WZConfig.Timezone,Timezone.length()+1);
  }

  const auto ntpServer = getParameter(params, "ntpServer");
  if (ntpServer) {
    ntpServer.toCharArray(WZConfig.ntpServer,ntpServer.length()+1);
  }

    const auto gmtOffset = getParameter(params, "gmtOffset");
      if (gmtOffset) {
        gmtOffset.toCharArray(WZConfig.gmtOffset,gmtOffset.length()+1);
      }

    const auto daylightOffset = getParameter(params, "daylightOffset");
    if (daylightOffset) {
      daylightOffset.toCharArray(WZConfig.daylightOffset,daylightOffset.length()+1);
    } 

    const auto hb_apiurl = getParameter(params, "hb_apiurl");
    if (hb_apiurl) {
      hb_apiurl.toCharArray(WZConfig.hb_apiurl,hb_apiurl.length()+1);
    } 

    const auto hb_authkey = getParameter(params, "hb_authkey");
    if (hb_authkey) {
      hb_authkey.toCharArray(WZConfig.hb_authkey,hb_authkey.length()+1);
    }         
  }

  Log.info(F("Save WZConfig to NVS RAM ..."));
  writeConfigData();
  sendRedirect(res, "/settings/general");
}

static void handleFirmwareUpdate(HTTPRequest *, HTTPResponse * res) {
  String html = createPage(uploadIndex, xhrUpload);
  html = replaceDefault(html, "Update Firmware");
  html = replaceHtml(html, "{method}", "/update");
  html = replaceHtml(html, "{accept}", ".bin");
  sendHtml(res, html);
};

static void handleFirmwareUpdateAction(HTTPRequest * req, HTTPResponse * res) {
  HTTPMultipartBodyParser parser(req);
  Update.begin();

  sprintf(display_message_buffer, "firmware will be load into device ...");
  displayMessage(display_message_buffer, wzGenInfoLine + 30, u8g2_font_lubB14_tf);

  while(parser.nextField()) {
    if (parser.getFieldName() != "upload") {
      Log.info(F("Skipping form data %s type %s filename %s"), parser.getFieldName().c_str(),
            parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());
      continue;
    }
    Log.info(F("Got form data %s type %s filename %s"), parser.getFieldName().c_str(),
          parser.getFieldMimeType().c_str(), parser.getFieldFilename().c_str());

    while (!parser.endOfField()) {
      byte buffer[512];
      size_t len = parser.read(buffer, 512);
      Log.info(F("Read data %d"), len);
      if (Update.write(buffer, len) != len) {
        Update.printError(Serial);
      }
    }
    Log.info(F("Done writing firmware image"));
    if (Update.end(true)) { // true to set the size to the current progress
      sendHtml(res, "<h1>Update successful! Device reboots now!</h1>");
      sprintf(display_message_buffer, "firmware update finished, device reboots now");
      displayMessage(display_message_buffer, wzGenInfoLine + 60, u8g2_font_lubB14_tf);
      res->finalize();
      delay(5000);
      ESP.restart();
    } else {
      String errorMsg = Update.errorString();
      Log.error(F("ERROR: Update: %s"), errorMsg.c_str());
      sprintf(display_message_buffer, "ERROR: firmware update - ivalid data");
      displayMessage(display_message_buffer, wzGenInfoLine + 60, u8g2_font_lubB14_tf); 
      res->setStatusCode(400);
      res->setStatusText("Invalid data!");
      res->print("ERROR");
    }
  }
}

static void handleHttpsRedirect(HTTPRequest *req, HTTPResponse *res) {
  String html = createPage(httpsRedirect);
  html = replaceDefault(html, "Https Redirect");
  String linkHost(req->getHTTPHeaders()->getValue("linkHost").c_str());
  // this could be more hardened?
  if (!linkHost || linkHost == "") {
    linkHost = getIp();
  }
  html = replaceHtml(html, "{host}", linkHost);
  sendHtml(res, html);
}

void beginPages() {
  // For every resource available on the server, we need to create a ResourceNode
  // The ResourceNode links URL and HTTP method to a handler function
  server->setDefaultNode(new ResourceNode("", HTTP_GET,  handleNotFound));
  server->registerNode(new ResourceNode("/", HTTP_GET,  handleIndex));
  server->registerNode(new ResourceNode("/about", HTTP_GET,  handleAbout));
  server->registerNode(new ResourceNode("/reboot", HTTP_GET,  handleReboot));
  server->registerNode(new ResourceNode("/settings/wifi", HTTP_GET,  handleWifi));
  server->registerNode(new ResourceNode("/settings/wifi/action", HTTP_POST, handleWifiSave));
  server->registerNode(new ResourceNode("/settings/configmode", HTTP_GET,  handleConfigMode));
  server->registerNode(new ResourceNode("/settings/configmode/action", HTTP_POST,  handleConfigModeSave));
  server->registerNode(new ResourceNode("/settings/general", HTTP_GET,  handleConfig));
  server->registerNode(new ResourceNode("/settings/general/action", HTTP_POST, handleConfigSave));
  server->registerNode(new ResourceNode("/update", HTTP_GET, handleFirmwareUpdate));
  server->registerNode(new ResourceNode("/update", HTTP_POST, handleFirmwareUpdateAction));

  server->setDefaultHeader("Server", std::string("WZePaper/") + WZVersion);

  insecureServer->setDefaultNode(new ResourceNode("", HTTP_GET,  handleHttpsRedirect));
  insecureServer->setDefaultHeader("Server", std::string("WZePaper/") + WZVersion);
}

void startConfigServer() 
{
  uint32_t currentSeconds;
  uint32_t configServerStartTime;

  // read config variables from NVS memory
  readConfigurationData();
  Log.info(F("StartConfigServer: WIFI_SSID read from NVS RAM: %s"), WZConfig.WIFI_SSID);
  Log.info(F("Start local webserver for configuration ..."));

  // create a local WiFi access point on ESP32 without external connection
  CreateWifiSoftAP();

  server = new HTTPSServer(&WZcert);
  insecureServer = new HTTPServer();

  // register all pages
  beginPages();

  Log.info(F("Starting HTTPS server..."));
  server->start();
  Log.info(F("Starting HTTP server..."));
  insecureServer->start();

  if (server->isRunning()) {
    Log.info(F("Server ready."));
  }

  // now looping for configServerUptimeMax and wait for user http connection
  currentSeconds = (uint32_t) millis() / 1000;
  configServerStartTime = currentSeconds; 

  sprintf(display_message_buffer, "device in configuration mode for %d seconds", configServerUptimeMax);
  displayMessage(display_message_buffer, wzGenInfoLine + 30, u8g2_font_lubB14_tf);

  while ( ((currentSeconds - configServerStartTime) < configServerUptimeMax) || configServerWasConnectedViaHttpFlag)
  {
    server->loop();
    insecureServer->loop();
    currentSeconds = (uint32_t) millis() / 1000;
  }  
  Log.info(F("ConfigServer closed, now process main loop ..."));
  // refresh display 
  displayStartupScreen();
}