#include <Ticker.h>
#include <Webbino.h>
#include <webbino_common.h>
#include <webbino_config.h>
#include <webbino_debug.h>
#include <WebbinoInterfaces/AllWiFi.h>
#include <RTClib.h>

#include <font6x8.h>
#include <nano_engine.h>
#include <nano_gfx.h>
#include <nano_gfx_types.h>
#include <sprite_pool.h>
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

#define DEBUG_DIMMER
// Wi-Fi parameters
#define WIFI_SSID        "ssid"
#define WIFI_PASSWORD    "password"
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#if defined(DEBUG_DIMMER)
#define NUM_OF_PWM_CHANNELS     6
#else
#define NUM_OF_PWM_CHANNELS     8
#endif

#define PWM_UPDATE_RATE         1.0f

#define SUNSHINE_MODE           0
#define MANUAL_MODE             1
#define MAX_PWM_SINGLE_STEP     25
#define MAX_AP_SSID_LEN         8
#define MAX_AP_PWD_LEN          16
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
  } sunshine[NUM_OF_PWM_CHANNELS];

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
NetworkInterfaceWiFi netint;
WebServer webserver;
RTC_DS1307 rtc;
DateTime now;

const IPAddress subnet(255,255,255,0);

void pwm_control(void)
{
  short curr_sec;
  now = rtc.now();
  curr_sec = 60*(24*now.hour() + 60*now.minute() + now.second());
  
  if(infoModel.currentMode == SUNSHINE_MODE)
  {
    for(char i = 0; i < NUM_OF_PWM_CHANNELS; i++)
    {
      char j, pwm;
      short x0;
      float y0, a; /* linear interpolation */ 
      
      if(i == 0)
        j = NUM_OF_PWM_CHANNELS-1;
      else
        j = i-1;

      if(i == 0)
      {
        j = NUM_OF_PWM_CHANNELS-1;    
        a = ( (float)(infoModel.sunshine[0].pwm - infoModel.sunshine[j].pwm) / 
            (float)60.0*((60*infoModel.sunshine[0].hh+infoModel.sunshine[0].mm) + 
                    (1440 - 60*infoModel.sunshine[j].hh - infoModel.sunshine[j].mm)));
        x0 = 60*(1440 - 60*infoModel.sunshine[j].hh - infoModel.sunshine[j].mm);          
      }
      else
      {
        j = i-1;
        a = ( (float)(infoModel.sunshine[0].pwm - infoModel.sunshine[j].pwm) / 
              (float)60.0*((60*infoModel.sunshine[0].hh+infoModel.sunshine[0].mm) - 
                      (60*infoModel.sunshine[j].hh + infoModel.sunshine[j].mm))); 
        x0 = 60*(60*infoModel.sunshine[j].hh + infoModel.sunshine[j].mm);              
      }
      y0 = infoModel.sunshine[j].pwm;

      pwm = a*(curr_sec-x0)+y0;

      if(pwm > dimmer.channel[i].curr_pwm)
      {
        if((pwm - dimmer.channel[i].curr_pwm) > MAX_PWM_SINGLE_STEP)
          dimmer.channel[i].curr_pwm += MAX_PWM_SINGLE_STEP;
        else
          dimmer.channel[i].curr_pwm = pwm;
      }
      else
      {
        if((dimmer.channel[i].curr_pwm - pwm)> MAX_PWM_SINGLE_STEP)
          dimmer.channel[i].curr_pwm -= MAX_PWM_SINGLE_STEP;
        else
          dimmer.channel[i].curr_pwm = pwm;        
      }
    }
  }

  /* Update PWM channels PWM */
}

void setup() {
  // put your setup code here, to run once:
  rtc.begin();
  /* load info model from eeprom */

#if defined(DEBUG_DIMMER)
  Serial.begin(115200);
#endif
  
  IPAddress local_IP(infoModel.network.AP_IP[0],infoModel.network.AP_IP[1],infoModel.network.AP_IP[2],infoModel.network.AP_IP[3]);

  for(int i = 0; i < NUM_OF_PWM_CHANNELS; i++)
  {
    dimmer.channel[i].curr_pwm = 0;
  }
  
  ssd1306_128x64_i2c_init();
  ssd1306_fillScreen(0x00);
  ssd1306_setFixedFont(ssd1306xled_font6x8);
  ssd1306_printFixed (0,  8, "Line 1. Normal text", STYLE_NORMAL);
  ssd1306_printFixed (0, 16, "Line 2. Bold text", STYLE_BOLD);
  ssd1306_printFixed (0, 24, "Line 3. Italic text", STYLE_ITALIC);
  ssd1306_printFixedN (0, 32, "Line 4. Double size", STYLE_BOLD, FONT_SIZE_2X);

  WiFi.softAPConfig(local_IP, local_IP, subnet);
  WiFi.softAP(infoModel.network.AP_SSID, infoModel.network.AP_Pwd);
  
  bool ok = netint.begin (infoModel.network.STA_SSID, infoModel.network.STA_Pwd);
  webserver.begin (netint);

#if !defined(DEBUG_DIMMER)
  Serial.end();
#endif
  pwm.attach(PWM_UPDATE_RATE, pwm_control);
}

void loop() {
  // put your main code here, to run repeatedly:
  webserver.loop ();
}
