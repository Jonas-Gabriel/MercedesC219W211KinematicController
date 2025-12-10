/*
 Name:		EEPROMBitAccess.h
 Created:	12/6/2025 10:00:21 PM
 Author:	Jonas-Gabriel
*/

#ifndef EEPROMBITACCESS_H
#define EEPROMBITACCESS_H

#include "EEPROM.h"

class EEPROMBitAccess
{
public:

	/* Reads the bit at the specified bit position from the EEPROM byte stored at the given address. 
	The function retrieves the full byte from EEPROM, isolates the requested bit, 
	and returns its value as either 0 or 1. No other bits or EEPROM contents are modified. */
	static uint8_t readBit(uint8_t address, uint8_t bitPos);

	/* Writes a single bit value (0 or 1) to the specified bit position within the EEPROM byte located at the given address. 
	The function reads the existing byte, updates only the targeted bit while preserving all other bits, 
	and writes the modified byte back to EEPROM. */
	static void writeBit(uint8_t address, uint8_t bitPos, uint8_t bitValue);
};

#endif