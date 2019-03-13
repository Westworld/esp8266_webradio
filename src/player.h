#ifndef player_H
#define player_H

#include <Arduino.h>
#include <VS1053.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <URL.h>
#include <URLParser.h>
#include <FS.h>   // Include the SPIFFS library

void handleRootPath();
void handleLinks();
void handleRechts();
void handleLauter();
void handleLeiser();
String prepareHtmlPage();
void handleFileUpload();
void SetVolume(short volume);
void FileFS_Init();
void FileFS_Load();
String getValue(String data, char separator, int index);
void Stream_Connect();
void Stream_Play();
void playerbegin();
uint8_t handlebyte ( uint8_t b );
String PreviousStationName();
String NextStationName();
void PreviousStation();
void NextStation();

String M3U_FindStream(String m3uString);
#endif