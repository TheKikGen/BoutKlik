/*
  BOUTKLIK - A Roland Boutique better Chain mode and other Boutique stuff
  Based on USB MIDIKLIK 4X4/3X3 firmware
  Copyright (C) 2019 by The KikGen labs.

  MAIN INCLUDE

  ------------------------   CAUTION  ----------------------------------
  THIS NOT A COPY OR A HACK OF ANY EXISTING MIDITECH/MIDIPLUS FIRMWARE.
  THAT FIRMWARE WAS ENTIRELY CREATED FROM A WHITE PAGE, WITHOUT
  DISASSEMBLING ANY SOFTWARE FROM MIDITECH/MIDIPLUS.

  UPLOADING THIS FIRMWARE TO YOUR MIDIPLUS/MIDITECH 4X4 USB MIDI
  INTERFACE  WILL PROBABLY CANCEL YOUR WARRANTY.

  IT WILL NOT BE POSSIBLE ANYMORE TO UPGRADE THE MODIFIED INTERFACE
  WITH THE MIDITECH/MIDIPLUS TOOLS AND PROCEDURES. NO ROLLBACK.

  THE AUTHOR DISCLAIM ANY DAMAGES RESULTING OF MODIFYING YOUR INTERFACE.
  YOU DO IT AT YOUR OWN RISKS.
  ---------------------------------------------------------------------

  This file is part of the USBMIDIKLIK-4x4 distribution
  https://github.com/TheKikGen/USBMidiKliK4x4
  Copyright (c) 2018 TheKikGen Labs team.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.

*/
#ifndef _BOUTKLIK_H_
#define _BOUTKLIK_H_
#pragma once


// Assume a "Blue Pill" like configuration
#warning "BLUEPILL HARDWARE ASSUMED"
#define HARDWARE_TYPE "BLUEPILL"
#define SERIAL_INTERFACE_MAX  3
#define SERIALS_PLIST &Serial1,&Serial2,&Serial3
#define DEFAULT_PORT_MIDICHANNEL_MAP -1,-1,-1
  
// Blue Pill has only one LED...not optimal here...
#define LED_CONNECT PC13

// Default listening MIDI channel
#define ROOT_MIDICHANNEL_DEFAULT 6

// Boutique speficic defines
#define JP08_ID 0X1C
#define JU06_ID 0X1D
#define JX03_ID 0X1E

// Assign modes
#define ASSIGN_MODE_SOLO 2
#define ASSIGN_MODE_UNISON 3
#define ASSIGN_MODE_POLY 0

// Key modes as original JP8
#define KEY_MODE_WHOLE 0
#define KEY_MODE_DUAL 1
#define KEY_MODE_SPLIT 1

// Timer for attachCompare1Interrupt
#define TIMER2_RATE_MICROS 1000

// Sysex used to set some parameters of the interface.
#define SYSEX_INTERNAL_HEADER 0xF0,0x77,0x77,0x80,
#define SYSEX_INTERNAL_ACK 0x7F
#define SYSEX_INTERNAL_BUFF_SIZE 32

// LED light duration in milliseconds
#define LED_PULSE_MILLIS  5


// Functions prototypes
void Timer2Handler(void);
static void SendMidiMsgToSerial(uint8_t const *, uint8_t);
void CheckEEPROM(bool);
int EEPROM_writeBlock(uint16 , const uint8 *, uint16  );
int EEPROM_readBlock(uint16 , uint8 *, uint16  );
static uint8_t GetInt8FromHexChar(char);
static uint16_t GetInt16FromHex4Char(char *);
static uint16_t GetInt16FromHex4Bin(char * );
static char USBSerialGetDigit();
static char USBSerialGetChar();
static uint8_t USBSerialScanHexChar(char *, uint8_t ,char,char);
void ConfigRootMenu();
void boutiqueSysexParse(uint8_t);
void ModeAdvanced();
void ModeBasic();

#endif
