/*
 Name:        EEPROMBitAccess.cpp
 Created:     12/6/2025 10:00:21 PM
 Author:      Jonas-Gabriel
*/

#include "EEPROMBitAccess.h"

uint8_t EEPROMBitAccess::readBit(uint8_t address, uint8_t bitPos)
{
    uint8_t byteValue = EEPROM.read(address);
    return (byteValue >> bitPos) & 0x01;
}

void EEPROMBitAccess::writeBit(uint8_t address, uint8_t bitPos, uint8_t bitValue)
{
    uint8_t byteValue = EEPROM.read(address);

    if (bitValue)
        byteValue |= (1 << bitPos);     // Set bit
    else
        byteValue &= ~(1 << bitPos);    // Clear bit

    EEPROM.write(address, byteValue);
}
