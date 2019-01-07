#ifndef __IN_SKETCH
#define __IN_SKETCH
#include "Arduino.h"
#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>

#include <AsyncPrinter.h>
#include <async_config.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncTCPbuffer.h>
#include <SyncClient.h>
#include <tcp_axtls.h>
#include <uRTCLib.h>
#include <FS.h>
#include <font6x8.h>
#include <nano_engine.h>
#include <nano_gfx.h>
#include <nano_gfx_types.h>
#include <sprite_pool.h>
#include <Ticker.h>
#include <ssd1306.h>
#include <ssd1306_16bit.h>
#include <ssd1306_1bit.h>
#include <ssd1306_8bit.h>
#include <ssd1306_console.h>
#include <ssd1306_fonts.h>
#include <ssd1306_uart.h>
#include <BearSSLHelpers.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClientSecureAxTLS.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiServer.h>
#include <WiFiServerSecure.h>
#include <WiFiServerSecureAxTLS.h>
#include <WiFiServerSecureBearSSL.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "html.h"
#endif

#define DEBUG_DIMMER

#if defined(DEBUG_DIMMER)
#define DIMMER_DEBUG(x) x
#else
#define DIMMER_DEBUG(x)
#endif

// Wi-Fi parameters
#define WIFI_SSID        "MenionAqua"
#define WIFI_PASSWORD    "zy681350ef"
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#if defined(DEBUG_DIMMER)
#define NUM_OF_PWM_CHANNELS     8
#else
#define NUM_OF_PWM_CHANNELS     8
#endif

#define NUM_OF_SUNSHINE_SLOTS  16
#define PWM_UPDATE_RATE         1.0f

#define MAX_WEB_CHUNK_SIZE    256

#define SUNSHINE_MODE           0
#define MANUAL_MODE             1
#define MAX_PWM_SINGLE_STEP     25
#define MAX_AP_SSID_LEN         32
#define MAX_AP_PWD_LEN          32
#define MAX_STA_SSID_LEN        8
#define MAX_STA_PWD_LEN         16

/* Information model struct */
typedef struct DimmerInfoModel_s {
  struct network_s {
    char AP_SSID[MAX_AP_SSID_LEN];
    char AP_Pwd[MAX_AP_PWD_LEN];
    unsigned char AP_IP[4];
    char STA_SSID[MAX_STA_SSID_LEN];
    char STA_Pwd[MAX_STA_PWD_LEN];
  } network;

  struct Sunshine_s {
    char hh;
    char mm;
    char pwm;
  } sunshine[NUM_OF_SUNSHINE_SLOTS][NUM_OF_PWM_CHANNELS];

  char currentMode;
  unsigned char crc8;
} __attribute__((packed, aligned(1))) DimmerInfoModel;

typedef struct Dimmer_s {
  struct channel_s {
    char curr_pwm;
  } channel[NUM_OF_PWM_CHANNELS];
} Dimmer;

DimmerInfoModel infoModel;
Dimmer dimmer;
Ticker pwm;
AsyncWebServer server(80);
uRTCLib rtc(0x68, 0x50);

uint8_t data[MAX_WEB_CHUNK_SIZE];

const IPAddress subnet(255,255,255,0);

WiFiEventHandler onStationModeConnectedHandler;
WiFiEventHandler onStationModeDisconnectedHandler;
WiFiEventHandler onStationModeAuthModeChangedHandler;
WiFiEventHandler onStationModeGotIPHandler;
WiFiEventHandler onStationModeDHCPTimeoutHandler;
WiFiEventHandler onSoftAPModeStationConnectedHandler;
WiFiEventHandler onSoftAPModeStationDisconnectedHandler;
WiFiEventHandler onSoftAPModeProbeRequestReceivedHandler;

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String getTimeString(const int idx, const int j)
{
  char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", infoModel.sunshine[idx][j].hh, infoModel.sunshine[idx][j].mm);
  return String(buf);
}

int getHHFromString(String &s)
{
  int hh, mm;
    sscanf(s.c_str(), "%02d:%02d", &hh, &mm);
  return hh;
}

int getMMFromString(String &s)
{
  int hh, mm;
    sscanf(s.c_str(), "%02d:%02d", &hh, &mm);
  return mm;
}

void onStationModeConnected(const WiFiEventStationModeConnected &ev)
{
  Serial.print("Connected to SSID: " + ev.ssid);
  Serial.print(" BSSID: " + macToString(ev.bssid));
  Serial.println(" ch: " + String(ev.channel));
}

void onStationModeDisconnected(const WiFiEventStationModeDisconnected &ev)
{
  Serial.print("Disconnected from SSID: " + ev.ssid);
  Serial.print(" BSSID: " + macToString(ev.bssid));
  Serial.println(" reason: " + String(ev.reason));

}

void onStationModeAuthModeChanged(const WiFiEventStationModeAuthModeChanged &ev)
{
  Serial.println("STA Auth mode changed to: " + String(ev.newMode) + " from: " + String(ev.oldMode));
}

void onStationModeGotIP(const WiFiEventStationModeGotIP &ev)
{
  Serial.println("STA IP: " + ev.ip.toString() + " mask: " + ev.mask.toString() + " gw: " + ev.gw.toString());
}

void onStationModeDHCPTimeout(void)
{
  Serial.println("DHCP Timeout!");
}

void onSoftAPModeStationConnected(const WiFiEventSoftAPModeStationConnected &ev)
{
  Serial.print("Connected STA: " + String(ev.aid));
  Serial.println(" MAC: " + macToString(ev.mac));
}

void onSoftAPModeStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &ev)
{
  Serial.print("Disconnected STA: " + String(ev.aid));
  Serial.println(" MAC: " + macToString(ev.mac));
}


void onSoftAPModeProbeRequestReceived(const WiFiEventSoftAPModeProbeRequestReceived& evt) {
  /*Serial.print("Probe request from: ");
  Serial.print(macToString(evt.mac));
  Serial.print(" RSSI: ");
  Serial.println(evt.rssi);*/
}

#define SIM_RTC
void pwm_control(void)
{
  int curr_sec;
#if defined(SIM_RTC)
  static int seconds = 0, minutes = 59, hours = 23;
      
  seconds++;
  
  if(seconds >= 60)
  {
    seconds = 0;
    minutes++;
  }

  if(minutes >= 60)
  {
    minutes = 0;
    hours++;
  }

  if(hours >= 24)
  {
    hours = 0;
  }
#else  
  int minutes, hours, seconds;  
  rtc.refresh();

  hours = rtc.hour();
  minutes = rtc.minute();
  seconds = rtc.second();
#endif
  
  curr_sec = 3600*hours + 60*minutes + seconds;
  if (infoModel.currentMode == SUNSHINE_MODE)
  {
    /* Loop over channels */
    for (int i = 0; i < NUM_OF_PWM_CHANNELS; i++)
    {
      int j, k, pwm;
      int x0;
      float y0, a, a1, a2; // linear interpolation

      /* Find sunshine timeslot bin where we are and store in j, bin will be K=j-1;j */
      for (j = 0; j < NUM_OF_SUNSHINE_SLOTS; j++)
      {
        if (curr_sec <= (3600 * infoModel.sunshine[j][i].hh + 60 * infoModel.sunshine[j][i].mm))
        {
          break;
        }
      }

      if (j == NUM_OF_SUNSHINE_SLOTS)
      {
        /* No valid slot has been found, means we are between last slot and
           midnight, so choose the first slot in the morning to compute current value*/
        j = 0;
      }

      if (j == 0)
      {
        k = NUM_OF_SUNSHINE_SLOTS - 1;
        a1 = infoModel.sunshine[0][i].pwm - infoModel.sunshine[k][i].pwm;
        a2 = 60 * infoModel.sunshine[0][i].hh + infoModel.sunshine[0][i].mm;
        a2 = a2 + (1440 - 60 * infoModel.sunshine[k][i].hh - infoModel.sunshine[k][i].mm);
        a2 = 60 * a2;

        a = (float)(a1) / (float)(a2);
        x0 = 60 * (60 * infoModel.sunshine[k][i].hh + infoModel.sunshine[k][i].mm);
        
        if (curr_sec < x0)
        {
          /* We are above midnight*/
          curr_sec = curr_sec + 24 * 60 * 60;
        }
      }
      else
      {
        k = j - 1;
        a1 = (infoModel.sunshine[j][i].pwm - infoModel.sunshine[k][i].pwm);
        a2 =       60 * (int)infoModel.sunshine[j][i].hh + (int)infoModel.sunshine[j][i].mm;
        a2 = a2 - (60 * (int)infoModel.sunshine[k][i].hh + (int)infoModel.sunshine[k][i].mm);
        a2 = 60 * a2;

        a = (float)(a1) / (float)a2;
        x0 = 60 * (60 * (int)infoModel.sunshine[k][i].hh + (int)infoModel.sunshine[k][i].mm);
      }

      y0 = infoModel.sunshine[k][i].pwm;

      pwm = a*(curr_sec - x0) + y0;

      if (pwm > dimmer.channel[i].curr_pwm)
      {
        DIMMER_DEBUG(Serial.printf("Updating PWM CH[%2d], Time: %02d:%02d:%02d, %3d ", i, hours, minutes, seconds, dimmer.channel[i].curr_pwm);)

        if ((pwm - dimmer.channel[i].curr_pwm) > MAX_PWM_SINGLE_STEP)
          dimmer.channel[i].curr_pwm += MAX_PWM_SINGLE_STEP;
        else
          dimmer.channel[i].curr_pwm = pwm;
          
        DIMMER_DEBUG(Serial.printf("<- %3d\n", dimmer.channel[i].curr_pwm);)
      }
      else if (pwm < dimmer.channel[i].curr_pwm)
      {
        DIMMER_DEBUG(Serial.printf("Updating PWM CH[%2d], Time: %02d:%02d:%02d, %3d ", i, hours, minutes, seconds, dimmer.channel[i].curr_pwm);)

        if ((dimmer.channel[i].curr_pwm - pwm)> MAX_PWM_SINGLE_STEP)
          dimmer.channel[i].curr_pwm -= MAX_PWM_SINGLE_STEP;
        else
          dimmer.channel[i].curr_pwm = pwm;

        DIMMER_DEBUG(Serial.printf("<- %3d\n", dimmer.channel[i].curr_pwm);)
      }
    }
  }
/*
  /* Update PWM channels PWM */
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

void handlePage(AsyncWebServerRequest *request)
{
  size_t data_len, len;
  File f;
  String contentType, path;

  if(request->url().endsWith("/"))
    path = request->url() + "index.html";
  else
    path = request->url();

  DIMMER_DEBUG(Serial.println("Request URI: " + path););

  //List all parameters (Compatibility)
  int args = request->args();
  for(int i=0;i<args;i++){
    DIMMER_DEBUG(Serial.printf("ARG[%s]: %s\n", request->argName(i).c_str(), request->arg(i).c_str());)
  }

  request->send(SPIFFS, path);
}

void handleSunshine(AsyncWebServerRequest *request)
{
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;
  int i, idx;
  /*
   * Retreive table index
   */

  //List all parameters (Compatibility)
  int args = request->args();
  if(args == 0)
  {
    DIMMER_DEBUG(Serial.println("No GET parameters"););
    request->send(404, "text/plain", "No GET parameters");
    return;
  }

  DIMMER_DEBUG(Serial.printf("Sunshine ARG[%s]: %s\n", request->argName((size_t)0).c_str(), request->arg((size_t)0).c_str());)

  if(!request->argName((size_t)0).equals("table"))
  {
    DIMMER_DEBUG(Serial.println("Wrong GET parameter " + request->argName(0)););
    request->send(404, "text/plain", "Wrong GET parameter " + request->argName(0));
    return;
  }

  idx = request->arg((size_t)0).toInt();

  if(idx > 15)
  {
    DIMMER_DEBUG(Serial.println("Wrong GET table value " + String(idx)););
    request->send(404, "text/plain", "Wrong GET table value " + String(idx));
    return;
  }

  JsonObject &root = jsonBuffer.createObject();
  if(root.success() == false)
  {
    DIMMER_DEBUG(Serial.println("Unable to create root JsonObject"););
    request->send(404, "text/plain", "Unable to create root JsonObject");
    return;
  }

  root["ID"] = String(idx);
  JsonArray  &array = jsonBuffer.createArray();
  if(array.success() == false)
  {
    DIMMER_DEBUG(Serial.println("Unable to create array JsonArray"););
    request->send(404, "text/plain", "Unable to create array JsonArray");
    return;
  }

  for(i = 0; i < NUM_OF_PWM_CHANNELS; i++)
  {
    JsonObject &obj = jsonBuffer.createObject();
    if(obj.success() == false)
    {
      DIMMER_DEBUG(Serial.println("Unable to create obj JsonObject"););
      request->send(404, "text/plain", "Unable to create obj JsonObject");
      return;
    }
    obj["CH"] = String(i);
    obj["Time"] = getTimeString(idx, i);
    obj["PWM"] = String((int)infoModel.sunshine[idx][i].pwm);
    array.add(obj);
  }

  root["SUNSHINE"] = array;

  root.printTo(*response);
  DIMMER_DEBUG(root.prettyPrintTo(Serial););
  request->send(response);
}

void handleStatus(AsyncWebServerRequest *request)
{
  int i;
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  DynamicJsonBuffer jsonBuffer;

  JsonArray  &array = jsonBuffer.createArray();

  if(array.success() == false)
  {
    DIMMER_DEBUG(Serial.println("Unable to create array JsonArray"););
    request->send(404, "text/plain", "Unable to create array JsonArray");
    return;
  }
  
  for(i = 0; i < NUM_OF_PWM_CHANNELS; i++)
  {
    JsonObject &obj = jsonBuffer.createObject();
    if(obj.success() == false)
    {
      DIMMER_DEBUG(Serial.println("Unable to create obj JsonObject"););
      request->send(404, "text/plain", "Unable to create obj JsonObject");
      return;
    }
    
    obj["CH"] = String(i);
    obj["PWM"] = String((int)dimmer.channel[i].curr_pwm);
    array.add(obj);
  }

  array.printTo(*response);
  DIMMER_DEBUG(array.prettyPrintTo(Serial););
  request->send(response);
}


void handleNotFound(AsyncWebServerRequest *request)
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += request->args();
  message += "\n";

  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " +  request->argName(i) + ": " +  request->arg(i) + "\n";
  }

  request->send(404, "text/plain", message);
}

void nullHandler(AsyncWebServerRequest *request)
{

}

void nullUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{

}

void statusPostJson(AsyncWebServerRequest *request, JsonVariant &json)
{
  JsonArray &status = json.as<JsonArray>();

  if(!status.success())
  {
    DIMMER_DEBUG(Serial.println("Error in deserialize JSON"););
  }
  else
  {
    for(JsonArray::iterator it = status.begin(); it != status.end(); ++it)
    {
      JsonObject &ch = it->as<JsonObject&>();

      DIMMER_DEBUG(Serial.print("CH: "););
      DIMMER_DEBUG(Serial.println(ch["CH"].as<String>()););

      DIMMER_DEBUG(Serial.print("PWM: "););
      DIMMER_DEBUG(Serial.println(ch["PWM"].as<String>()););
    }
  }

  request->send(200, "text/html", "");
}

void sunshinePostJson(AsyncWebServerRequest *request, JsonVariant &json)
{
  JsonObject &object = json.as<JsonObject>();

  if(!object.success())
  {
  DIMMER_DEBUG(Serial.println("Error in deserialize JSON"););
  }
  else
  {
    int chnum;
    int id = object["ID"].as<int>();
    JsonArray &sunshine = object["SUNSHINE"].as<JsonArray&>();

    DIMMER_DEBUG(Serial.println("Sunshine page: " + String(id)););
    for(JsonArray::iterator it = sunshine.begin(); it != sunshine.end(); ++it)
    {
      JsonObject &ch = it->as<JsonObject&>();
      String time;

      DIMMER_DEBUG(Serial.print("CH: "););
      DIMMER_DEBUG(Serial.println(ch["CH"].as<String>()););

      DIMMER_DEBUG(Serial.print("Time: "););
      DIMMER_DEBUG(Serial.println(ch["Time"].as<String>()););

      DIMMER_DEBUG(Serial.print("PWM: "););
      DIMMER_DEBUG(Serial.println(ch["PWM"].as<String>()););

      chnum = ch["CH"].as<int>();
      time = ch["Time"].as<String>();

      infoModel.sunshine[id][chnum].pwm = ch["PWM"].as<int>();
      infoModel.sunshine[id][chnum].hh = getHHFromString(time);
      infoModel.sunshine[id][chnum].mm = getMMFromString(time);
    }
  }

  request->send(200, "text/html", "");
}

void networkPostJson(AsyncWebServerRequest *request, JsonVariant &json)
{

}

void handleNetworkPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{

}

void initInfoModel(void)
{
  int i, j;

  for(i = 0; i < NUM_OF_SUNSHINE_SLOTS; i++)
  {
    for(j = 0; j < NUM_OF_PWM_CHANNELS; j++)
    {
      infoModel.sunshine[i][j].hh = 2 + i;
      infoModel.sunshine[i][j].mm = 5*j;
      if(i < 8)
      {
        infoModel.sunshine[i][j].pwm = 5*j + 5*i;
      }
      else
      {
        infoModel.sunshine[i][j].pwm = 100 - 5*(7-j) - 5*(i - 8);
      }
    }
  }

  for(j = 0; j < NUM_OF_PWM_CHANNELS; j++)
  {
    dimmer.channel[j].curr_pwm = 0;
  }

  infoModel.currentMode = SUNSHINE_MODE;
}

void initWebServer(void)
{
  AsyncCallbackJsonWebHandler* statusJson = new AsyncCallbackJsonWebHandler("/rest/status.json", statusPostJson);
  AsyncCallbackJsonWebHandler* sunshineJson = new AsyncCallbackJsonWebHandler("/rest/sunshine.json", sunshinePostJson);
  AsyncCallbackJsonWebHandler* networkJson = new AsyncCallbackJsonWebHandler(network_html_name, networkPostJson);


  server.on(Chart_bundle_js_name, handlePage);
  server.on(bootstrap_min_css_name, handlePage);
  server.on(bootstrap_min_js_name, handlePage);
  server.on(dashboard_css_name, handlePage);
  server.on(feather_min_js_name, handlePage);
  server.on(("/"), handlePage);
  server.on(index_html_name, handlePage);
  server.on(jquery_3_3_1_min_js_name, handlePage);
  server.on(moment_min_js_name, handlePage);

  server.on(network_html_name, HTTP_GET, handlePage);
  server.addHandler(networkJson);
  server.on(network_json_name, handlePage);

  server.on(status_html_name, HTTP_GET, handlePage);
  server.addHandler(statusJson);
  server.on(status_json_name, handleStatus);

  server.on(sunshine_html_name, HTTP_GET, handlePage);
  server.addHandler(sunshineJson);
  server.on(sunshine_json_name, handleSunshine);

  server.onNotFound(handleNotFound);

  server.begin();

  DIMMER_DEBUG(Serial.println("HTTP server started");)
}

void setup() {
  // put your setup code here, to run once:
  DIMMER_DEBUG(Serial.begin(115200);)
  DIMMER_DEBUG(Serial.println("Starting..."););

//  rtc.begin();
  SPIFFS.begin();

  /* load info model from eeprom */
  infoModel.network.AP_IP[0] = 192;
  infoModel.network.AP_IP[1] = 168;
  infoModel.network.AP_IP[2] = 44;
  infoModel.network.AP_IP[3] = 1;


  IPAddress local_IP(infoModel.network.AP_IP[0],infoModel.network.AP_IP[1],infoModel.network.AP_IP[2],infoModel.network.AP_IP[3]);

  strcpy(infoModel.network.AP_SSID, WIFI_SSID);
  strcpy(infoModel.network.AP_Pwd, WIFI_PASSWORD);

  /*
   * Reset the current pwm
   */
  for(int i = 0; i < NUM_OF_PWM_CHANNELS; i++)
  {
    dimmer.channel[i].curr_pwm = 0;
  }
/*
  ssd1306_128x64_i2c_init();
  ssd1306_fillScreen(0x00);
  ssd1306_setFixedFont(ssd1306xled_font6x8);
  ssd1306_printFixed (0,  8, "Line 1. Normal text", STYLE_NORMAL);
  ssd1306_printFixed (0, 16, "Line 2. Bold text", STYLE_BOLD);
  ssd1306_printFixed (0, 24, "Line 3. Italic text", STYLE_ITALIC);
  ssd1306_printFixedN (0, 32, "Line 4. Double size", STYLE_BOLD, FONT_SIZE_2X);
*/
  DIMMER_DEBUG(Serial.println("Starting soft AP");)

  // Don't save WiFi configuration in flash - optional
  WiFi.persistent(false);

  onSoftAPModeStationConnectedHandler = WiFi.onSoftAPModeStationConnected(onSoftAPModeStationConnected);
  onSoftAPModeStationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(onSoftAPModeStationDisconnected);
  onSoftAPModeProbeRequestReceivedHandler = WiFi.onSoftAPModeProbeRequestReceived(onSoftAPModeProbeRequestReceived);


  WiFi.softAPConfig(local_IP, local_IP, subnet);
  bool ret = WiFi.softAP(infoModel.network.AP_SSID, infoModel.network.AP_Pwd);

  if(ret)
  {
    DIMMER_DEBUG(Serial.println("Started soft AP, SSID: " + WiFi.softAPSSID() + ", Password: " + WiFi.softAPPSK());)
  }
  else
  {
    DIMMER_DEBUG(Serial.println("Failed to start soft AP");)
  }

  onStationModeConnectedHandler = WiFi.onStationModeConnected(onStationModeConnected);
  onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected(onStationModeDisconnected);
  onStationModeAuthModeChangedHandler = WiFi.onStationModeAuthModeChanged(onStationModeAuthModeChanged);
  onStationModeGotIPHandler = WiFi.onStationModeGotIP(onStationModeGotIP);
  onStationModeDHCPTimeoutHandler = WiFi.onStationModeDHCPTimeout(onStationModeDHCPTimeout);

  WiFi.begin("MenionAP", "zy681350ab");


#if !defined(DEBUG_DIMMER)
  Serial.end();
#endif

  initInfoModel();

  initWebServer();

  pwm.attach(PWM_UPDATE_RATE, pwm_control);
}

void loop() {
  // put your main code here, to run repeatedly:

}
