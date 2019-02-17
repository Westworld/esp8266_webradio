#include <Console.h>

#include "player.h"

#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3
#define LOCATION "Location"

enum datamode_t { DATA = 4,          // State for datastream
                  METADATA = 8, PLAYLISTINIT = 16,
                  PLAYLISTHEADER = 32, PLAYLISTDATA = 64,
                  STOPREQD = 128, STOPPED = 256
                } ;

// Default volume
const short startVOLUME = 75;
static int oldstatus=0;

const char * headerkeys[] = {"Location","ice-audio-info","icy-genre","ice-name","icy-metaint","Content-Type","Connection","transfer-encoding"} ;
size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
bool             chunked = false ;                         // Station provides chunked transfer
int              chunkcount = 0 ;                          // Counter for chunked transfer
datamode_t       datamode ;                                // State of datastream
int              metaint = 0 ;                             // Number of databytes between metadata
uint32_t         totalcount = 0 ;                          // Counter mp3 data                               // State of datastream
int              metacount ;                               // Number of bytes in metadata
int              datacount ;                               // Counter databytes before metadata
String           metaline ;                                // Readable line in metadata
bool             firstchunk = true ;                 // First chunk as input
int8_t           playlist_num = 0 ;                        // Nonzero for selection from playlist
int              bufcnt = 0 ;                        // Data in chunk
 

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
extern ESP8266WebServer server;

String ice_audio_info, icy_genre, ice_name, streammetadata;
String playlist[10];
String playliststation[10];
short playlistcounter=0;
short currentplaylist=0;
short VOLUME;
bool DoNotReportError = false;
 
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
    http.end();

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
        http.addHeader("Icy-MetaData", "1");
        http.begin(playlist[currentplaylist]);
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
                for (short i=0;i<headers;i++) {
                  Console::info("redirect header %s mit %s",http.headerName(i).c_str(),http.header(i).c_str());
              }

              Console::error("connecting failed %d %s",httpCode, http.errorToString(httpCode).c_str()); 
              Console::warn("overwriting playlist %s with ''",playlist[currentplaylist].c_str()); 
              playlist[currentplaylist] = "";
            }  // 302
              
          } // http >0

          if (httpCode == 200) {
            metaint = 0 ;                                      // No metaint found
            int headers = http.headers();
            for (short i=0;i<headers;i++) {
              Console::info("header %s: %s",http.headerName(i).c_str(), http.header(i).c_str());

              if (http.headerName(i) == "ice-audio-info")
                ice_audio_info = http.header(i);
              else if (http.headerName(i) == "icy_genre")
                ice_audio_info = http.header(i);
              else if (http.headerName(i) == "ice_name")
                ice_name = http.header(i);
              else if (http.headerName(i) == "icy-metaint")
                metaint = http.header(i).substring(12).toInt() ;   // Found metaint tag, read the value
              else if (http.headerName(i) == "transfer-encoding") {
                if ( http.header(i).endsWith ( "chunked" ) )
                {
                  chunked = true ;                           // Remember chunked transfer mode
                  chunkcount = 0 ;                           // Expect chunkcount in DATA
                }
                else
                {
                    chunked = false;
                }
                
              }

             } 
           Console::info("audio-info: %s, genre: %s, name %s",ice_audio_info.c_str(),
                icy_genre.c_str(), ice_name.c_str());  
            datacount = metaint ;                          // Number of bytes before first metadata
            bufcnt = 0 ;                                   // Reset buffer count            
            datamode = DATA ;                                // Handle header
            totalcount = 0 ;                                   // Reset totalcount
            metaline = "" ;                                    // No metadata yet
            firstchunk = true ;                                // First chunk expected
 
            client = http.getStream(); 
          }
        }
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
  //    if (bytesread != 32)
  //      Console::info("bytesread %d ********", bytesread); 
      //player.playChunk(mp3buff, bytesread);
      for (uint8_t i=0;i<bytesread;i++) {
        handlebyte_ch ( mp3buff[i], false );
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

//**************************************************************************************************
//                                   H A N D L E B Y T E _ C H                                     *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// Chunked transfer encoding aware. Chunk extensions are not supported.                            *
//**************************************************************************************************
void handlebyte_ch ( uint8_t b, bool force )
{
  static int  chunksize = 0 ;                         // Chunkcount read from stream

  if ( chunked && !force &&
       ( datamode & ( DATA |                          // Test op DATA handling
                      METADATA |
                      PLAYLISTDATA ) ) )
  {
    if ( chunkcount == 0 )                            // Expecting a new chunkcount?
    {
      if ( b == '\r' )                               // Skip CR
      {
        return ;
      }
      else if ( b == '\n' )                          // LF ?
      {
        chunkcount = chunksize ;                     // Yes, set new count
        chunksize = 0 ;                              // For next decode
        return ;
      }
      // We have received a hexadecimal character.  Decode it and add to the result.
      b = toupper ( b ) - '0' ;                      // Be sure we have uppercase
      if ( b > 9 )
      {
        b = b - 7 ;                                  // Translate A..F to 10..15
      }
      chunksize = ( chunksize << 4 ) + b ;
    }
    else
    {
      handlebyte ( b, force ) ;                       // Normal data byte
      chunkcount-- ;                                  // Update count to next chunksize block
    }
  }
  else
  {
    handlebyte ( b, force ) ;                         // Normal handling of this byte
  }
}


//**************************************************************************************************
//                                   H A N D L E B Y T E                                           *
//**************************************************************************************************
// Handle the next byte of data from server.                                                       *
// This byte will be send to the VS1053 most of the time.                                          *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.           *
// Set force to true if chunkbuffer must be flushed.                                               *
//**************************************************************************************************
void handlebyte ( uint8_t b, bool force )
{
  static uint16_t  playlistcnt ;                       // Counter to find right entry in playlist
  static bool      firstmetabyte ;                     // True if first metabyte (counter)
  static int       LFcount ;                           // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32] ; // Buffer for chunk
  String           lcml ;                              // Lower case metaline
  String           ct ;                                // Contents type
  static bool      ctseen = false ;                    // First line of header seen or not
  int              inx ;                               // Pointer in metaline
  int              i ;                                 // Loop control


  if ( datamode == DATA )                              // Handle next byte of MP3/Ogg data
  {
    buf[bufcnt++] = b ;                                // Save byte in chunkbuffer
    if ( bufcnt == sizeof(buf) || force )              // Buffer full?
    {
      if ( firstchunk )
      {
        firstchunk = false ;
        Console::info("First chunk");
      }
      totalcount += bufcnt ;                           // Count number of bytes, ignore overflow
      player.playChunk ( buf, bufcnt ) ;         // Yes, send to player
      bufcnt = 0 ;                                     // Reset count
    }
    if ( metaint != 0 )                                // No METADATA on Ogg streams or mp3 files
      {
        if ( --datacount == 0 )                          // End of datablock?
        {
          if ( bufcnt )                                  // Yes, still data in buffer?
          {
            totalcount += bufcnt ;                       // Count number of bytes, ignore overflow
            player.playChunk ( buf, bufcnt ) ;     // Yes, send to player
            bufcnt = 0 ;                                 // Reset count
          }
          datamode = METADATA ;
          firstmetabyte = true ;                         // Expecting first metabyte (counter)
        }
      }
     return ;
  }

  if ( datamode == METADATA )                          // Handle next byte of metadata
  {
    if ( firstmetabyte )                               // First byte of metadata?
    {
      firstmetabyte = false ;                          // Not the first anymore
      metacount = b * 16 + 1 ;                         // New count for metadata including length byte
      if ( metacount > 1 )
      {
        Console::info("Metadata block %d bytes", metacount - 1 ); // Most of the time there are zero bytes of metadata
      }
      metaline = "" ;                                  // Set to empty
    }
    else
    {
      metaline += (char)b ;                            // Normal character, put new char in metaline
    }
    if ( --metacount == 0 )
    {
      if ( metaline.length() )                         // Any info present?
      {
        // metaline contains artist and song name.  For example:
        // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
        // Sometimes it is just other info like:
        // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
        // Isolate the StreamTitle, remove leading and trailing quotes if present.
        streammetadata = metaline;
        Console::info("Stream meta data %s", metaline.c_str() ) ;         // Show artist and title if present in metadata
      }
      if ( metaline.length() > 1500 )                  // Unlikely metaline length?
      {
        Console::info("Metadata block too long! Skipping all Metadata from now on." ) ;
        metaint = 0 ;                                  // Probably no metadata
        metaline = "" ;                                // Do not waste memory on this
      }
      datacount = metaint ;                            // Reset data count
      bufcnt = 0 ;                                     // Reset buffer count
      datamode = DATA ;                                // Expecting data
    }
  }
  if ( datamode == PLAYLISTINIT )                      // Initialize for receive .m3u file
  {
    // We are going to use metadata to read the lines from the .m3u file
    // Sometimes this will only contain a single line
    metaline = "" ;                                    // Prepare for new line
    LFcount = 0 ;                                      // For detection end of header
    datamode = PLAYLISTHEADER ;                        // Handle playlist data
    playlistcnt = 1 ;                                  // Reset for compare
    totalcount = 0 ;                                   // Reset totalcount
    Console::info("Read from playlist" ) ;
  }
  if ( datamode == PLAYLISTHEADER )                    // Read header
  {
    if ( ( b > 0x7F ) ||                               // Ignore unprintable characters
         ( b == '\r' ) ||                              // Ignore CR
         ( b == '\0' ) )                               // Ignore NULL
    {
      // Yes, ignore
    }
    else if ( b == '\n' )                              // Linefeed ?
    {
      LFcount++ ;                                      // Count linefeeds
      Console::info( "Playlistheader: %s",                 // Show playlistheader
                 metaline.c_str() ) ;
      metaline = "" ;                                  // Ready for next line
      if ( LFcount == 2 )
      {
        Console::info(  "Switch to PLAYLISTDATA" ) ;
        datamode = PLAYLISTDATA ;                      // Expecting data now
        return ;
      }
    }
    else
    {
      metaline += (char)b ;                            // Normal character, put new char in metaline
      LFcount = 0 ;                                    // Reset double CRLF detection
    }
  }
  if ( datamode == PLAYLISTDATA )                      // Read next byte of .m3u file data
  {
    if ( ( b > 0x7F ) ||                               // Ignore unprintable characters
         ( b == '\r' ) ||                              // Ignore CR
         ( b == '\0' ) )                               // Ignore NULL
    {
      // Yes, ignore
    }
    else if ( b == '\n' )                              // Linefeed ?
    {
      Console::info("Playlistdata: %s",                   // Show playlistheader
                 metaline.c_str() ) ;
      if ( metaline.length() < 5 )                     // Skip short lines
      {
        metaline = "" ;                                // Flush line
        return ;
      }
      if ( metaline.indexOf ( "#EXTINF:" ) >= 0 )      // Info?
      {
        if ( playlist_num == playlistcnt )             // Info for this entry?
        {
          inx = metaline.indexOf ( "," ) ;             // Comma in this line?
          if ( inx > 0 )
          {
            // Show artist and title if present in metadata
            streammetadata = metaline.substring ( inx + 1 );
            Console::info("Stream meta data %s", streammetadata.c_str() ) ;         // Show artist and title if present in metadata
          }
        }
      }
      if ( metaline.startsWith ( "#" ) )               // Commentline?
      {
        metaline = "" ;
        return ;                                       // Ignore commentlines
      }
      // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
      Console::info("Entry %d in playlist found: %s", playlistcnt, metaline.c_str() ) ;
      Console::error("missing code ***** " ) ;
      /*if ( playlist_num == playlistcnt  )
      {
        inx = metaline.indexOf ( "http://" ) ;         // Search for "http://"
        if ( inx >= 0 )                                // Does URL contain "http://"?
        {
          host = metaline.substring ( inx + 7 ) ;      // Yes, remove it and set host
        }
        else
        {
          host = metaline ;                            // Yes, set new host
        }
        connecttohost() ;                              // Connect to it
      }
      metaline = "" ;
      host = playlist ;                                // Back to the .m3u host
      playlistcnt++ ;                                  // Next entry in playlist
      */
    }
    else
    {
      metaline += (char)b ;                            // Normal character, add it to metaline
    }
    return ;
  }
}

