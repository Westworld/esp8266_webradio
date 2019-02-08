#include <Console.h>

#include "player.h"

#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3

// Default volume
const short startVOLUME = 75;

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

extern ESP8266WebServer server;

String playlist[10];
String playliststation[10];
short playlistcounter=0;
short currentplaylist=0;
short VOLUME;

WiFiClient client;

char host[100] = "mp3channels.webradio.rockantenne.de";
char path[200] = "/rockantenne";
int httpPort = 80;
File fsUploadFile; 
uint8_t mp3buff[32];

void playerbegin() {
  player.begin();
     //player.switchToMp3Mode();
  SetVolume(startVOLUME);
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
      URL url;
      url = parseURL(playlist[currentplaylist]);
      if (url.isValid) {
        url.host.toCharArray(host, 100);
        url.path.toCharArray(path, 200);
        client.stop();
      }
      else
      {
        Console::error("ungültige URL ");
        Serial.println(playlist[currentplaylist]);
      }
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
      URL url;
      url = parseURL(playlist[currentplaylist]);
      if (url.isValid) {
        url.host.toCharArray(host, 100);
        url.path.toCharArray(path, 200);
        client.stop();
      }
      else
      {
        Console::error("ungültige URL ");
        Serial.println(playlist[currentplaylist]);
      }

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
              f.println(host);
              f.println(path);
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
            Serial.println(line);
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
      URL url;
      url = parseURL(playlist[currentplaylist]);
      if (url.isValid) {
        url.host.toCharArray(host, 100);
        url.path.toCharArray(path, 200);
      }
      else
      {
        Console::error("ungültige URL ");
        Serial.println(playlist[currentplaylist]);
      }    
    }
}

void Stream_Connect() {
    Console::info("connecting to %s", host);  

    if (!client.connect(host, httpPort)) {
      Console::error("Connection failed");
      return;
    }

// wie bekommt man 302 mit?

    Console::info("Requesting stream: %s",path);
    
    client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" + 
                  "Connection: close\r\n\r\n");

}


void Stream_Play() {

    if(!client.connected()){
        Stream_Connect();
    }
  
    if(client.available() > 0){
      uint8_t bytesread = client.read(mp3buff, 32);
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