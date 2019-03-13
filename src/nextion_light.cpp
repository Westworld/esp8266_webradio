#include "Wiring.h"
#include <nextion_light.h>
#include <SoftwareSerial.h>

SoftwareSerial nexSerial(NextionRx, NextionTx);

void Nextion::begin(int baudRate)
{
    nexSerial.begin(baudRate);
    //ButtonCounter = 0;
}

void Nextion::ShowText(const String name, const String message) {
    String command = name+"=\""+message+"\"";

    while (nexSerial.available())
    {
        nexSerial.read();
    }

    for (unsigned int i = 0; i < command.length(); i++)
    {
      nexSerial.write(command[i]);   // Push each char 1 by 1 on each loop pass
    }
  
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
}

void Nextion::ShowUTF8Text(const String name, const String message) {
    String converted = utf8ascii(message);
    String command = name+"=\""+converted+"\"";

    while (nexSerial.available())
    {
        nexSerial.read();
    }

    for (unsigned int i = 0; i < command.length(); i++)
    {
      nexSerial.write(command[i]);   // Push each char 1 by 1 on each loop pass
    }
  
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
    nexSerial.write(0xFF);
}

short Nextion::IsEvent(void) {
    static short ButtonCounter=0;
    short result=0;

    if (nexSerial.available()) {  
        short received = nexSerial.read();

        if ((ButtonCounter==0) && (received == 101)) 
        ButtonCounter++;
        else 
        if ((ButtonCounter==1) && (received == 0)) 
            ButtonCounter++;
            else 
            if (ButtonCounter==2) {
                if (received == 3) {
                    result = 2;
                }
                else { 
                    result = 1;
                }
                ButtonCounter=0;
            }
            else
                ButtonCounter=0;

    }
    return result;

}


// ****** UTF8-Decoder: convert UTF8-string to extended ASCII *******
// http://playground.arduino.cc/main/Utf8ascii

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored
byte Nextion::utf8asciibyte(byte ascii, byte &previousbyte) {
    if ( ascii<128 )   // Standard ASCII-set 0..0x7F handling  
    {   previousbyte=0; 
        return( ascii ); 
    }

    // get previous input
    byte last = previousbyte;   // get last char
    previousbyte=ascii;         // remember actual character

    switch (last)     // conversion depending on first UTF8-character
    {   case 0xC2: return  (ascii);  break;
        case 0xC3: return  (ascii | 0xC0);  break;
        case 0x82: if(ascii==0xAC) return(0x80);       // special case Euro-symbol
    }

    return  (0);                                     // otherwise: return zero, if character has to be ignored
}

// convert String object from UTF8 String to Extended ASCII
String Nextion::utf8ascii(String s)
{       
        String r="";
        byte previousbyte=0;
        char c;
        for (unsigned int i=0; i<s.length(); i++)
        {
                c = utf8asciibyte(s.charAt(i), previousbyte);
                if (c!=0) r+=c;
        }
        return r;
}

// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!)
void Nextion::utf8ascii(char* s)
{       
        int k=0;
        byte previousbyte=0;
        char c;
        for (unsigned int i=0; i<strlen(s); i++)
        {
                c = utf8asciibyte(s[i], previousbyte);
                if (c!=0) 
                        s[k++]=c;
        }
        s[k]=0;
}