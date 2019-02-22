#include <Console.h>

#include "player.h"

#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3
#define LOCATION "Location"



// Default volume
const short startVOLUME = 75;
int oldstatus=0;

const char * headerkeys[] = {"Location","ice-audio-info","icy-genre","ice-name","icy-metaint","Content-Type","Connection","transfer-encoding"} ;
size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);

const char tagheader[] = "StreamTitle=";
short tagsize = 0, tagcounter = 0;
char  tagvalue[255];

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
extern ESP8266WebServer server;

String ice_audio_info, icy_genre, ice_name, streammetadata;
String playlist[10];
String playliststation[10];
short playlistcounter=0;
short currentplaylist=0;
short VOLUME;
bool DoNotReportError = false;
long mediacounter = 0;
long metaint;

WiFiClient client;
HTTPClient http;

File fsUploadFile; 
uint8_t mp3buff[32];

void playerbegin() {
  player.begin();
  player.switchToMp3Mode();
  SetVolume(startVOLUME);
  bool result = player.isChipConnected();
  if (!result)
    Console::error("mp3 chip not connected!");
}


void SetVolume(short volume)
{
    VOLUME=volume;
    if (VOLUME<0) VOLUME=0;
    if (VOLUME>100) VOLUME=100;

    player.setVolume(VOLUME);
}

// ################# WEB ###################

void handleRootPath() {
   Console::info("handleRootPath");
   server.send(200, "text/html", prepareHtmlPage());
}

void handleLinks() {
    Console::info("links");
    currentplaylist--;
    if (currentplaylist<0) currentplaylist=playlistcounter;
    Console::info("choose playlist %d from %d", currentplaylist, playlistcounter);
    Serial.println(playlist[currentplaylist]);
    client.stop();
    server.sendHeader("Location","/");      // Redirect the client to the success page
    server.send(303);
      //server.send(200, "text/html", prepareHtmlPage());
      
}
void handleRechts() {
    Console::info("rechts");
    currentplaylist++;
    if (currentplaylist>=playlistcounter) currentplaylist=0;
    Console::info("choose playlist %d from %d", currentplaylist, playlistcounter);
    Serial.println(playlist[currentplaylist]);
    client.stop();
    server.sendHeader("Location","/");      // Redirect the client to the success page
    server.send(303);
}

void handleLauter() {
    Console::info("lauter");
    SetVolume(VOLUME+5);

    server.sendHeader("Location","/");      // Redirect the client to the success page
    server.send(303);    
}
void handleLeiser() {
    Console::info("leiser");
    SetVolume(VOLUME-5);

    server.sendHeader("Location","/");      // Redirect the client to the success page
    server.send(303); 
}

String prepareHtmlPage() {
    String htmlPage =
     String("<!DOCTYPE HTML>") +
            "<html>" +
            "Click <a href=\"/Links\">here</a> open links<br> "+
            "Click <a href=\"/Rechts\">here</a> open rechts<br> " +
            "Volume: "+ String(VOLUME)+"<br>"+
            "Station: "+ playliststation[currentplaylist] +"<br>"+
            "Click <a href=\"/Lauter\">here</a> Lauter<br> "+
            "Click <a href=\"/Leiser\">here</a> Leiser<br> \n" +
            "<form action=\"/Upload\" method=\"post\" enctype=\"multipart/form-data\"> " +
            "<input type=\"file\" name=\"name\"> "+
            "<input class=\"button\" type=\"submit\" value=\"Upload\"> "+
            "</form> <br><br>"+
            "</html> " +
            "\r\n";
  return htmlPage;
}
  
void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  Console::info("handleFileUpload: %d", upload.status);

  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    //SPIFFS.remove(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    //filename = String();
     if (!fsUploadFile) {
        Console::error("file open failed");
      }
  } else if(upload.status == UPLOAD_FILE_WRITE) {
    if(fsUploadFile) {
      Console::info("start writing %d", upload.currentSize);
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
    }
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Console::info("handleFileUpload Size: %d",upload.totalSize);
      server.send(200, "text/plain", "200: success, file uploaded, reboot");
      delay(1000);
      ESP.restart();
    } else {
      Console::info("Upload error: %d",upload.status);
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}


void FileFS_Init()
{
short filesuccess = SPIFFS.begin();
    if (filesuccess)
      Console::info("filesystem mounted correctly");
    else
    {
       Console::info("formating filesystem");
       filesuccess = SPIFFS.format();
       if (filesuccess) {
          Console::info("filesystem formated correctly");           
          filesuccess = SPIFFS.begin();
          if (filesuccess) {
            Console::info("filesystem mounted correctly");     
            File f = SPIFFS.open("/station.txt", "w");
            if (!f) {
              Console::error("file open failed");
            }
            else {
              f.close();
            }
          }     
       }    
       else
       {   Console::error("filesystem formating failed!");
           //return;
       }
    }
}

void FileFS_Load()
{
      File f = SPIFFS.open("/station.txt", "r");
      if (!f) {
        Console::error("file open failed");
      }
      else {
        f.seek(0, SeekSet);
        String line;
        while (f.available())
        {
          line = f.readStringUntil('\n');
          if (line.length() != 0) {
            //Serial.println(line);
            if (playlistcounter<10) {
              // zerlegen nach Name, tab, sender
              line.trim();
              playlist[playlistcounter] = getValue(line,'\t',1);
              playliststation[playlistcounter] = getValue(line,'\t',0);
              Serial.println( playlist[playlistcounter]);
              Serial.println(playliststation[playlistcounter]);
              playlistcounter++;
            }
          } 
        }
        f.close();
      } 

      if (playlistcounter>0) {
      Console::info("choose playlist 1 from %d", playlistcounter);
      Serial.println(playlist[currentplaylist]);   
    }
}

void Stream_Connect() 
{
    short AllSenderTested = 0;
    metaint = 0;
    http.end();
    player.stopSong();

    Console::info("connecting to %s", playlist[currentplaylist].c_str());  
    if (playlist[currentplaylist] == "")
              {
                  if (!DoNotReportError)
                    Console::warn("empty playlist, switch to next");  
                    while ((playlist[currentplaylist] == "") & (AllSenderTested <2)) {
                        currentplaylist++;
                        if (currentplaylist>=playlistcounter) 
                            { currentplaylist=0; AllSenderTested++; }
                    }
                  
                  if (playlist[currentplaylist] == "")
                    { DoNotReportError = true; return; }
                  else
                    Console::info("connecting to %s", playlist[currentplaylist].c_str()); 
              }


    if (playlist[currentplaylist] != "")  {
        http.collectHeaders(headerkeys,headerkeyssize);  
        http.begin(playlist[currentplaylist]);
        http.addHeader("Icy-MetaData", "1");
        int httpCode = http.GET();
        if (httpCode > 0) {
            Console::info("connecting result %d",httpCode); 
            if (httpCode == 302) {
              if (http.hasHeader(LOCATION)) {
                String uri = http.header(LOCATION);
                Console::info("reconnecting to %s",uri.c_str()); 
                http.end();
                http.begin(uri);
                Console::warn("overwriting playlist %s with %s",playlist[currentplaylist].c_str(), uri.c_str()); 
                playlist[currentplaylist] = uri;
                httpCode = http.GET();
                Console::info("redirection connecting result %d",httpCode); 
              }
              else
              {
                int headers = http.headers();
                Console::info("redirection num headers %d",headers);
                for (short i=0;i<headers;i++) 
                  Console::info("redirect header %s mit %s",http.headerName(i).c_str(),http.header(i).c_str());
              }

              Console::error("connecting failed %d %s",httpCode, http.errorToString(httpCode).c_str()); 
              Console::warn("overwriting playlist %s with ''",playlist[currentplaylist].c_str()); 
              playlist[currentplaylist] = "";
            }  // 302
              
          } // http >0

          if (httpCode == 200) {                                    // No metaint found
            int headers = http.headers();
            for (short i=0;i<headers;i++) {
                      Console::info("header %s: %s",http.headerName(i).c_str(), http.header(i).c_str());

                      if (http.headerName(i) == "ice-audio-info")
                        ice_audio_info = http.header(i);
                      else if (http.headerName(i) == "icy_genre")
                        ice_audio_info = http.header(i);
                      else if (http.headerName(i) == "ice_name")
                        ice_name = http.header(i);
                      else if (http.headerName(i) == "icy-metaint") {
                        metaint = http.header(i).toInt();   // Found metaint tag, read the value 
                        Console::info("metaint found %d",metaint);
                        }              
                      }  // loop headers

           Console::info("audio-info: %s, genre: %s, name %s",ice_audio_info.c_str(),
                icy_genre.c_str(), ice_name.c_str());   
            client = http.getStream(); 
            player.startSong();
          } // http 200
    else
    {
      Console::error("connecting failed %d %s",httpCode, http.errorToString(httpCode).c_str()); 
      Console::warn("overwriting playlist %s with ''",playlist[currentplaylist].c_str()); 
      playlist[currentplaylist] = "";
    }
  }
}


void Stream_Play() {

    if(!client.connected()){
        http.end();
        Stream_Connect();
    }

    if(client.available() > 0){
      uint8_t bytesread = client.read(mp3buff, 32);

      for (uint8_t i=0;i<bytesread;i++) {
        handlebyte ( mp3buff[i] );
      }
      player.playChunk(mp3buff, bytesread);

    }
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void handlebyte ( uint8_t b ) {
//  mediacounter++;
  if ((b == tagheader[tagsize]) || (tagsize > 11)) {
      tagvalue[tagcounter++] = b;
      if (tagsize <= 11) {  // "StreamTitle="
        tagsize++;
      }
      else {
        if ((tagcounter > 250) || (b == 0) || (b == 59)) {
          // wir haben einen Tag gefunden
          tagcounter-=2;
          if (tagcounter>14){
            tagvalue[tagcounter] = 0;
//          Console::info("MP3 Tag gefunden pos %d", mediacounter); 
            Console::info("%s", &tagvalue[13]); 
          }
          tagsize = 0;
          tagcounter = 0;
//          mediacounter = 0;
        }
      }
  }
  else
  {
    tagsize = 0;
    tagcounter = 0;
  }
}

