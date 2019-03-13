/**
   web radio  ESP8266

based on code from ince Gellár (github.com/vincegellar)
  
  --------------------------------
  | VS1053  | ESP8266 |  Other   |
  --------------------------------
  |   SCK   |   D5    |    -     |
  |   MISO  |   D6    |    -     |
  |   MOSI  |   D7    |    -     |
  |   XRST  |    -    |    -     |
  |   CS    |   D1    |    -     |
  |   DCS   |   D0    |    -     |
  |   DREQ  |   D3    |    -     |
  |   5V    |    -    |   VCC    |
  |   GND   |    -    |   GND    |
  --------------------------------
  D2+D4 Nextion

  Dependencies:
  -VS1053 library by baldram (https://github.com/baldram/ESP_VS1053_Library)
  

  IDE Settings (Tools):
  -IwIP Variant: v2 Higher Bandwidth läuft besser!
  -CPU Frequency: 160Hz
*/

#include <Arduino.h>

#include "Wiring.h"

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include <ESP8266WebServer.h>
#include <Console.h>
#include <nextion_light.h>

#include "player.h"
#include "esp8266WebRadioDemo.h"

// pins defined in wiring.h
    
const char* wifihostname = "ESPRadio";

ESP8266WebServer server(80); 

void setup () {
    Console::begin();
    Console::line();

    Nextion::begin(9600);
    Nextion::ShowText("t0.txt", "Radio");
    Nextion::ShowText("t1.txt", "starte");
    Nextion::ShowText("t2.txt", "Verbindung");

    // Wait for VS1053 and PAM8403 to power up
    // otherwise the system might not start up correctly
    delay(3000);

    // This can be set in the IDE no need for ext library
    // system_update_cpu_freq(160);
    
    Console::info("Simple Radio Node WiFi Radio");
  
    SPI.begin();

    Console::info("Connecting to SSID %s", WIFI_SSID);
    WiFi.hostname(wifihostname);  
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    short counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (++counter > 50)
        ESP.restart();
    }

    Console::info("WiFi connected");
    IPAddress ip = WiFi.localIP();
    Console::info("IP Address: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); 
    char buffer[100];
    sprintf(buffer, "IP Address: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); 
    Nextion::ShowText("t0.txt", buffer);

    ArduinoOTA.setHostname(wifihostname);  
    
  ArduinoOTA.onStart([]() {
    if (ArduinoOTA.getCommand() == U_FLASH) {
      Console::info("Start updating sketch");
    } else { // U_SPIFFS
      Console::info("Start updating filesystem");
       // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      SPIFFS.end();
    }  
  });
  ArduinoOTA.onEnd([]() {
    Console::info("End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Console::info("Progress: %d", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Console::error("[%d]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Console::error("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Console::error("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Console::error("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Console::error("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Console::error("End Failed");
    }
  });


    ArduinoOTA.begin();

    FileFS_Init();
    FileFS_Load();

    playerbegin();

    server.on("/", handleRootPath);
    server.on("/Links", handleLinks);
    server.on("/Rechts", handleRechts);
    server.on("/Lauter", handleLauter);
    server.on("/Leiser", handleLeiser);
    server.onNotFound([](){
      server.send(404, "text/plain", "404: Not found");
      });
    server.on("/Upload", HTTP_POST,                       // if the client posts to the upload page
      [](){ server.send(200); },                          // Send status 200 (OK) to tell the client we are ready to receive
      handleFileUpload                                    // Receive and save the file
    );
    server.begin();
    Stream_Connect();
}

void loop() {

    ArduinoOTA.handle();
    
    server.handleClient();

    Stream_Play();

     short received = Nextion::IsEvent();
     if (received == 2) {
              Console::info("Next");
              NextStation();
     }
     else if (received == 1) { 
              Console::info("Previous");
              PreviousStation();
      }
}




