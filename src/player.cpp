#include "Wiring.h"

#include <Console.h>

#include "player.h"
#include "nextion_light.h"

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
String lastStation="";
short lastStationCounter=0;

WiFiClient client;
HTTPClient http;

File fsUploadFile; 
static __attribute__((aligned(4))) uint8_t mp3buff[32];


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

String PreviousStationName() {
    short prev = currentplaylist - 1;
    //Console::info("PreviousStation %d",prev);
    if (prev<0) prev=playlistcounter-1;
    return playliststation[prev];
}

String NextStationName() {
    short prev = currentplaylist + 1;
    //Console::info("NextStation %d",prev);
    if (prev>=playlistcounter) prev=0;
    return playliststation[prev];
}

void PreviousStation() {
    currentplaylist--;
    if (currentplaylist<0) currentplaylist=playlistcounter-1;
    Console::info("choose playlist %d from %d", currentplaylist, playlistcounter);
    Serial.println(playlist[currentplaylist]);
    client.stop();
}

void NextStation() {
    currentplaylist++;
    if (currentplaylist>=playlistcounter) currentplaylist=0;
    Console::info("choose playlist %d from %d", currentplaylist, playlistcounter);
    Serial.println(playlist[currentplaylist]);
    client.stop();
}

// ################# WEB ###################

void handleRootPath() {
   Console::info("handleRootPath");
   server.send(200, "text/html", prepareHtmlPage());
}

void handleLinks() {
    Console::info("links");
    PreviousStation();
    server.sendHeader("Location","/");      // Redirect the client to the success page
    server.send(303);
      //server.send(200, "text/html", prepareHtmlPage());
      
}
void handleRechts() {
    Console::info("rechts");
    NextStation();
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
  player.stopSong();

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
    Nextion::ShowText("t2.txt", "");
    Nextion::ShowText("t3.txt", "");

    if (lastStation == playlist[currentplaylist])  {
      if (lastStationCounter++ > 10) {
          Console::info("10 mal Fehler zu %s", playlist[currentplaylist].c_str()); 
          NextStation();
      }
    }
    else
    {
        lastStation = playlist[currentplaylist];
        lastStationCounter = 0;
    }
    

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

        // sonderfall URL = .m3u
    short result = playlist[currentplaylist].indexOf(".m3u");
    if (result>0) { 
            Console::info("Sonderfall m3u"); 
            http.begin(playlist[currentplaylist]);
            int httpCode = http.GET();
            //Console::info("Sonderfall m3u httpcode %d", httpCode); 
            if (httpCode == 200) {
              if (http.getSize()<1000) {
                //Console::info("Sonderfall m3u size %d", http.getSize()); 
                String httpresult = http.getString();
                Console::info("Sonderfall downloaded %s", httpresult.c_str()); 
                // nun Eintrag mit http suchen
                playlist[currentplaylist] = M3U_FindStream(httpresult);
                //Console::info("Sondefall neue URL %s", playlist[currentplaylist].c_str()); 
              }
              else
              {
                Console::info("Sonderfall m3u size failed %d", http.getSize()); 
              }
            }
            else
            {
              Console::info("Sonderfall m3u http failed %d", httpCode);
            }           
        }

    if (playlist[currentplaylist] != "")  {
        // update buttons
        Nextion::ShowText("b0.txt", PreviousStationName());
        Nextion::ShowText("b1.txt", NextStationName());

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
                http.addHeader("Icy-MetaData", "1");
                Console::warn("overwriting playlist %s with '%s'",playlist[currentplaylist].c_str(), uri.c_str()); 
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
              //Console::warn("overwriting playlist %s with ''",playlist[currentplaylist].c_str()); 
              //playlist[currentplaylist] = "";
            }  // 302
              
          } // http >0

          if (httpCode == 200) {                                    // No metaint found
            int headers = http.headers();
            for (short i=0;i<headers;i++) {
                      Console::info("header %s: %s",http.headerName(i).c_str(), http.header(i).c_str());

                      if (http.headerName(i) == "ice-audio-info")
                        ice_audio_info = http.header(i);
                      else if (http.headerName(i) == "icy-genre")
                        icy_genre = http.header(i);
                      else if (http.headerName(i) == "ice_name")
                        ice_name = http.header(i);
                      else if (http.headerName(i) == "icy-metaint") {
                        metaint = http.header(i).toInt();   // Found metaint tag, read the value 
                        Console::info("metaint found %d",metaint);
                        }              
                      }  // loop headers

           Console::info("audio-info: %s, genre: %s, name %s",ice_audio_info.c_str(),
                icy_genre.c_str(), ice_name.c_str()); 
           Nextion::ShowText("t0.txt", playliststation[currentplaylist]);    
           Nextion::ShowText("t2.txt", icy_genre+" "+ice_name);    
           Nextion::ShowText("t1.txt", ice_audio_info);    

           client = http.getStream(); 
           player.startSong();
           mediacounter=0;
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
    if (player.data_request()) {
      if(client.available() > 0){
        uint8_t bytesread = client.read(mp3buff, 32);

        uint8_t goodbytes = 0;
        uint8_t newbuffer[32];


        for (uint8_t i=0;i<bytesread;i++) {
          if( handlebyte ( mp3buff[i] )) {
            newbuffer[goodbytes++] = mp3buff[i];
          }
        }
        player.playChunk(newbuffer, goodbytes);
      }
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


uint8_t handlebyte ( uint8_t b ) {
  mediacounter++;

  if (mediacounter > metaint) {
    long metablock = b*16;
    Console::info("Metaint block size %d", metablock); 
    mediacounter = -(metablock);
  }

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

            Console::info("%s", &tagvalue[13]); 

            char buffer[200];
            strncpy(buffer, &tagvalue[13], 200);
            String title1="";
            String title2="";
            String title = buffer;
            short pos = title.indexOf("-");
            if (pos>0) {
              title1=title.substring(0,pos-1);
              title2=title.substring(pos+1);
            }
            else
            {
              pos = title.indexOf(":");
              if (pos>0) {
                title1=title.substring(0,pos);
                title2=title.substring(pos+1);
              }
              else
                title1 = title;
                if (title1.length()>33) {
                  title2 = title1.substring(33);
                  title1 = title1.substring(0,32);
                }
            }
            Nextion::ShowUTF8Text("t2.txt", title1);  
            Nextion::ShowUTF8Text("t3.txt", title2);  
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

  if (mediacounter <=0)
    return 0;
  else
    return 1; 
}

String M3U_FindStream(String m3uString) {
  /*
  #EXTM3U
  #EXTINF:-1, (COUNTRY•108 - Your Country Music)
  http://icepool.silvacast.com/COUNTRY108.mp3
  */

  unsigned short von = 0, bis=0;
  unsigned short lang = m3uString.length();
  char curPos;
  String playurl = "";

  //Console::info("M3UFindStream %s", m3uString.c_str()); 

  while (bis<lang) {
    curPos = m3uString.charAt(bis);
    if ((curPos == 10) || (curPos == 13)) {
      // neue Zeile
      if (m3uString.charAt(von) == '#') {
        // Zeile ignorieren
        //Console::info("zeile ignorieren %d %d",von, bis);
        von = bis+1;
      }
      else {
        Console::info("M3UFindStream %d %d %d", von, bis, lang); 
        if (m3uString.charAt(von) == 'h') {
          // gefunden
          playurl = m3uString.substring(von, bis);
          Console::info("playlist gefunden %s",playurl.c_str());
          break;
        }
        else
        {
            von = bis+1;
        }
        
      }
    }
    bis++;
  }

  if ((von > 0) && (bis >= lang)) {
    playurl = m3uString.substring(von);
    //Console::info("playlist in letzter Zeile gefunden %s",playurl.c_str());
  }

  if ((von == 0) && (bis >= lang)) {
    playurl = m3uString;
    //Console::info("playlist in einziger Zeile gefunden %s",playurl.c_str());
  }

  Console::info("playlist verwenden %s",playurl.c_str()); 

  return playurl;
}