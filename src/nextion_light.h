#ifndef NEXTION_light_H
#define NEXTION_light_H

#include <Arduino.h>

class Nextion
{


public:
    static void begin(int baudRate=9600);
    static void ShowText(const String name, const String message);
    static void ShowUTF8Text(const String name, const String message);
    static short IsEvent(void);

private:
    static void utf8ascii(char* s);
    static String utf8ascii(String s);
    static byte utf8asciibyte(byte ascii, byte &previousbyte);
};

#endif
