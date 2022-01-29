#include <Arduino.h>

#define EXTERNAL_EEPROM 0x50 // Address of 24LC256 eeprom chip

#ifdef EXTERNAL_EEPROM
#include <Wire.h>

void writeEEPROM(unsigned int eeaddress, byte data)
{
    Wire.beginTransmission(EXTERNAL_EEPROM);
    Wire.write((int)(eeaddress >> 8));   // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.write(data);
    Wire.endTransmission();

    delay(5);
}

byte readEEPROM(unsigned int eeaddress)
{
    byte rdata = 0xFF;

    Wire.beginTransmission(EXTERNAL_EEPROM);
    Wire.write((int)(eeaddress >> 8));   // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();

    Wire.requestFrom(EXTERNAL_EEPROM, 1);

    if (Wire.available())
        rdata = Wire.read();

    return rdata;
}

#else
#include <EEPROM.h>

void writeEEPROM(unsigned int eeaddress, byte data)
{
    EEPROM.write(eeaddress, data);
}
byte readEEPROM(unsigned int eeaddress) {
    return EEPROM.read(eeaddress);
}

#endif
