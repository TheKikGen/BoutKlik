/*
  BOUTKLIK - A Roland Boutique better Chain mode and other Boutique stuff
  Based on USB MIDIKLIK 4X4/3X3 firmware
  Copyright (C) 2019 by The KikGen labs.

  MAIN SOURCE

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

#include <libmaple/nvic.h>

#include <EEPROM.h>


#include <PulseOutManager.h>
#include <midiXparser.h>

#include "build_number_defines.h"
#include "BoutKlik.h"
#include "EEPROM_Params.h"

#if SERIAL_INTERFACE_MAX > 4
#error "SERIAL_INTERFACE IS 4 MAX"
#endif

// Serial interfaces Array
HardwareSerial * serialHw[SERIAL_INTERFACE_MAX] = {SERIALS_PLIST};

// EEPROMS parameters
EEPROM_Params_t EEPROM_Params;

// Timer
HardwareTimer timer(2);

// Sysex used to set some parameters of the interface.
// Be aware that the 0x77 manufacturer id is reserved in the MIDI standard (but not used)
// The second byte is usually an id number or a func code + the midi channel (any here)
// The Third is the product id
static  const uint8_t sysExInternalHeader[] = {SYSEX_INTERNAL_HEADER} ;
static  uint8_t sysExInternalBuffer[SYSEX_INTERNAL_BUFF_SIZE] ;

// Jack port vs midi channel auto mapping
int8_t portMidiChannelMap[SERIAL_INTERFACE_MAX] = {DEFAULT_PORT_MIDICHANNEL_MAP};


// Prepare LEDs pulse for Connect, MIDIN and MIDIOUT
// From MIDI SERIAL point of view
// Use a PulseOutManager factory to create the pulses
PulseOutManager flashLEDManager;

PulseOut* flashLED_CONNECT = flashLEDManager.factory(LED_CONNECT,LED_PULSE_MILLIS,LOW);

// MIDI Parsers for serial 1 to n
midiXparser midiSerial[SERIAL_INTERFACE_MAX];

uint8_t JP08assignMode = ASSIGN_MODE_POLY;
uint8_t JP08lastChoosenAssigMode = ASSIGN_MODE_POLY;
boolean JP08upperPart = false;
uint8_t JP08keyMode = KEY_MODE_WHOLE;
uint8_t JP08currentChorusFxType = 0 ;
boolean JP08splitMode = false;

// Specific Boutique sysex messages
uint8_t sysExBuffer[10];
uint8_t JP08boutiqueSysexHeader[]     = { 0xF0, 0x41, 0x10, 0x00, 0x00, 0x00, JP08_ID , 0x12 };
uint8_t JP08boutiqueSysexModePoly[]   = { 0x03, 0x00, 0x11, 0x06, 0x00, 0x00, 0x66, 0xF7 };
uint8_t JP08SysexDual[]               = { 0x01, 0x10, 0x00, 0x02, 0x01, 0x6C, 0xF7 };
uint8_t JP08SysexDualOff[]            = { 0x01, 0x10, 0x00, 0x02, 0x00, 0x6D, 0xF7 };
uint8_t JP08SysexUpper[]              = { 0x01, 0x10, 0x00, 0x03, 0x01, 0x6B, 0xF7 };



///////////////////////////////////////////////////////////////////////////////
// Timer2 interrupt handler
///////////////////////////////////////////////////////////////////////////////
void Timer2Handler(void) {
     // Update LEDS
     flashLEDManager.update(millis());
}

///////////////////////////////////////////////////////////////////////////////
// Send a midi msg to serial MIDI. 0 is Serial1.
///////////////////////////////////////////////////////////////////////////////
static void SendMidiMsgToSerial(uint8_t const *msg, uint8_t serialNo) {

  if (serialNo >= SERIAL_INTERFACE_MAX ) return;

	uint8_t msgLen = midiXparser::getMidiStatusMsgLen(msg[0]);

	if ( msgLen > 0 ) {
	  if (msgLen >= 1 ) serialHw[serialNo]->write(msg[0]);
	  if (msgLen >= 2 ) serialHw[serialNo]->write(msg[1]);
	  if (msgLen >= 3 ) serialHw[serialNo]->write(msg[2]);
	  #ifdef LEDS_MIDI
	  flashLED_OUT[serialNo]->start();
	  #else
	  flashLED_CONNECT->start();
	  #endif
	}
}

///////////////////////////////////////////////////////////////////////////////
// CHECK EEPROM
//----------------------------------------------------------------------------
// Retrieve global parameters from EEPROM, or Initalize it
// If factorySetting is true, all settings will be forced to factory default
//////////////////////////////////////////////////////////////////////////////
void CheckEEPROM(bool factorySettings=false) {

  // Set EEPROM parameters for the STMF103RC
  EEPROM.PageBase0 = 0x801F000;
  EEPROM.PageBase1 = 0x801F800;
  EEPROM.PageSize  = 0x800;


  // Read the EEPROM parameters structure
  EEPROM_readBlock(0, (uint8 *)&EEPROM_Params, sizeof(EEPROM_Params) );

  // If the signature is not found, of not the same version of parameters structure,
  // or new build, then initialize (factory settings)
  if (  factorySettings ||
        memcmp( EEPROM_Params.signature,EE_SIGNATURE,sizeof(EEPROM_Params.signature) ) ||
        EEPROM_Params.prmVer != EE_PRMVER ||
        memcmp( EEPROM_Params.TimestampedVersion,TimestampedVersion,sizeof(EEPROM_Params.TimestampedVersion) )
     )
  {
    memset( &EEPROM_Params,0,sizeof(EEPROM_Params) );
    memcpy( EEPROM_Params.signature,EE_SIGNATURE,sizeof(EEPROM_Params.signature) );

    EEPROM_Params.prmVer = EE_PRMVER;

    memcpy( EEPROM_Params.TimestampedVersion,TimestampedVersion,sizeof(EEPROM_Params.TimestampedVersion) );

    EEPROM_Params.rootMidiChannel = ROOT_MIDICHANNEL_DEFAULT;
    EEPROM_Params.debug_mode = false;

    //Write the whole param struct
    EEPROM_writeBlock(0, (uint8*)&EEPROM_Params,sizeof(EEPROM_Params) );

  }

}
///////////////////////////////////////////////////////////////////////////////
// EEPROM EMULATION UTILITIES
///////////////////////////////////////////////////////////////////////////////
int EEPROM_writeBlock(uint16_t ee, const uint8 *bloc, uint16_t size )
{
    uint16_t i;
    for (i = 0; i < size; i++) EEPROM.write(ee+i, *(bloc+i));

    return i;
}

int EEPROM_readBlock(uint16_t ee, uint8 *bloc, uint16_t size )
{
    uint16_t i;
    for (i = 0; i < size; i++) *(bloc +i) = EEPROM.read(ee+i);
    return i;
}
///////////////////////////////////////////////////////////////////////////////
// Get an Int8 From a hex char.  letters are minus.
// Warning : No control of the size or hex validity!!
///////////////////////////////////////////////////////////////////////////////
static uint8_t GetInt8FromHexChar(char c) {
	return (uint8_t) ( c <= '9' ? c - '0' : c - 'a' + 0xa );
}

///////////////////////////////////////////////////////////////////////////////
// Get an Int16 From a 4 Char hex array.  letters are minus.
// Warning : No control of the size or hex validity!!
///////////////////////////////////////////////////////////////////////////////
static uint16_t GetInt16FromHex4Char(char * buff) {
	char val[4];

	val[0] = GetInt8FromHexChar(buff[0]);
	val[1] = GetInt8FromHexChar(buff[1]);
	val[2] = GetInt8FromHexChar(buff[2]);
	val[3] = GetInt8FromHexChar(buff[3]);

	return GetInt16FromHex4Bin(val);
}

///////////////////////////////////////////////////////////////////////////////
// Get an Int16 From a 4 hex digit binary array.
// Warning : No control of the size or hex validity!!
///////////////////////////////////////////////////////////////////////////////
static uint16_t GetInt16FromHex4Bin(char * buff) {
	return (uint16_t) ( (buff[0] << 12) + (buff[1] << 8) + (buff[2] << 4) + buff[3] );
}

///////////////////////////////////////////////////////////////////////////////
// USB serial getdigit
///////////////////////////////////////////////////////////////////////////////
static char USBSerialGetDigit() {
  char c;
  while ( ( c = USBSerialGetChar() ) <'0' && c > '9');
  return c;
}
///////////////////////////////////////////////////////////////////////////////
// USB serial getchar
///////////////////////////////////////////////////////////////////////////////
static char USBSerialGetChar() {
  while (!Serial.available() >0);
  char c = Serial.read();
  // Flush
  while (Serial.available()>0) Serial.read();
  return c;
}

///////////////////////////////////////////////////////////////////////////////
// "Scanf like" for hexadecimal inputs
///////////////////////////////////////////////////////////////////////////////

static uint8_t USBSerialScanHexChar(char *buff, uint8_t len,char exitchar,char sepa) {

	uint8_t i = 0, c = 0;

	while ( i < len ) {
		c = USBSerialGetChar();
		if ( (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'  ) ) {
			Serial.write(c);
			if (sepa) Serial.write(sepa);
			buff[i++]	= GetInt8FromHexChar(c);
		} else if (c == exitchar && exitchar !=0 ) break;
	}
	return i;
}

///////////////////////////////////////////////////////////////////////////////
// Get a choice from a question
///////////////////////////////////////////////////////////////////////////////
char getChoice(const char * qLabel, char * choices) {
	char c;
	char * yn = "yn";
	char * ch;

	if ( *choices == 0 ) ch = yn; else ch = choices;
	Serial.print(qLabel);
	Serial.print(" (");
	Serial.print(ch);
	Serial.print(") ? ");

	while (1) {
		c = USBSerialGetChar();
		uint8_t i=0;
		while (ch[i] )
				if ( c == ch[i++] ) { Serial.write(c); return c; }
	}
}

///////////////////////////////////////////////////////////////////////////////
// Blink a LED n times
///////////////////////////////////////////////////////////////////////////////

void BlinkLED_CONNECT(uint8_t nb) {
  for ( uint8_t i = 1 ; i <= nb ; i++ ) {
    flashLED_CONNECT->start() ;
    delay(300);
  }
}

///////////////////////////////////////////////////////////////////////////////
// CONFIG ROOT MENU
//----------------------------------------------------------------------------
// Allow BOUTKLIK configuration by using a menu as a user interface
// from the USB serial port.
///////////////////////////////////////////////////////////////////////////////
void ConfigRootMenu()
{
	char choice=0;
	char buff [32];
	uint8_t i,j;
	char c;

	for ( ;; )
	{
    Serial.println("");Serial.println("");
    Serial.println("BOUTKLIK CONFIGURATION MENU");
    Serial.println("(c) TheKikGenLabs");
    Serial.println("");
    Serial.print("Magic number       : "); Serial.write(EEPROM_Params.signature , sizeof(EEPROM_Params.signature));
    Serial.print( EEPROM_Params.prmVer);  Serial.print("-") ; Serial.println( (char *)EEPROM_Params.TimestampedVersion);
    Serial.print("JP-08 Midi OUT/IN# : OUT "); Serial.print(JP08+1);Serial.print(" / IN ");Serial.println(JP08_FBK_PORT+1);
    Serial.print("JU-06 Midi OUT#    : OUT "); Serial.println(JU06+1);
    Serial.print("JX-03 Midi OUT#    : OUT "); Serial.println(JX03+1);
    Serial.print("Root MIDI channel  : "); Serial.println(EEPROM_Params.rootMidiChannel+1);
    Serial.println("");
    Serial.println("m. Set root MIDI channel");
    Serial.println("f. Restore factory settings");
    Serial.println("s. Save configuration & exit");
    Serial.println("x. Abort without saving");
    Serial.println("");
		Serial.println(" Your choice =>");
		choice = USBSerialGetChar();
		Serial.println(choice);
    Serial.println("");

		switch (choice)
		{

      // Midi rooot channel
      case 'm':
        Serial.println("Enter the midi root channel (01-16 / 00 to exit) :");
        i = 0; j = 0;
        while ( i < 2 ) {
          c  = USBSerialGetDigit();Serial.write(c);
          j +=  ( (uint8_t) (c - '0' ) ) * pow(10 ,1-i);
          i++;
        }
        if (j == 0 or j >16 )
           Serial.println(". No change made. Incorrect value.");
        else {
          EEPROM_Params.rootMidiChannel = j-1;
        }
        break;

			// Reload the EEPROM parameters structure
			case 'e':
				if ( getChoice("Reload current settings from EEPROM","") == 'y' ) {
					CheckEEPROM();
          Serial.println("");
					Serial.println("Settings reloaded from EEPROM.");
				}
				break;

			// Restore factory settings
			case 'f':
				if ( getChoice("Restore all factory settings","") == 'y' ) {
          Serial.println("");
					if ( getChoice("Your own settings will be erased. Are you really sure","") == 'y' ) {
						CheckEEPROM(true);
            Serial.println("");
						Serial.println("Factory settings restored.");
					}
				}
				break;

			// Save & quit
			case 's':
				if ( getChoice("Save settings and exit to midi mode","") == 'y' ) {
					//Write the whole param struct
					EEPROM_writeBlock(0, (uint8*)&EEPROM_Params,sizeof(EEPROM_Params) );


					delay(100);
					nvic_sys_reset();
				}
				break;

			// Abort
			case 'x':
				if ( getChoice("\nAbort","") == 'y' ) nvic_sys_reset();
				break;
		}

	}
}


///////////////////////////////////////////////////////////////////////////////
// SEND CHORUS CC
///////////////////////////////////////////////////////////////////////////////
void SendCCChorusFxType(uint8_t s,uint8_t channel,uint8_t chorusFxType) {
  serialHw[s]->write(midiXparser::controlChangeStatus+channel);
  serialHw[s]->write(93); // Chorus
  serialHw[s]->write(chorusFxType); // 0 : Stop or Chorus 1, 2, 3.;

}

///////////////////////////////////////////////////////////////////////////////
// PARSE SOME JP-08 BOUTIQUE SYSEX
///////////////////////////////////////////////////////////////////////////////
void JP08BoutiqueSysexParse(uint8_t byteReceived) {

  // Pointers
  static uint8_t p = 0 ;
  static uint8_t p1 = 0 ;

  if ( p == sizeof ( JP08boutiqueSysexHeader ) ) {

    sysExBuffer[p1] = byteReceived;
    if ( byteReceived == 0xF7 ) {

      if ( memcmp( JP08boutiqueSysexModePoly,sysExBuffer,   5 ) == 0  ) {
            uint8_t a = ( sysExBuffer[5] == 3 ? ASSIGN_MODE_UNISON : sysExBuffer[5] == 2 ? ASSIGN_MODE_SOLO:ASSIGN_MODE_POLY );

            // CHORUS Activation : Mode POLY 2 times
            if (JP08lastChoosenAssigMode == ASSIGN_MODE_POLY && a == ASSIGN_MODE_POLY) {
                  if ( ++JP08currentChorusFxType > 3 ) JP08currentChorusFxType = 0;
                  BlinkLED_CONNECT(JP08currentChorusFxType);
                  SendCCChorusFxType(JP08,0,JP08currentChorusFxType);
                  SendCCChorusFxType(JP08,1,JP08currentChorusFxType);

            } else JP08currentChorusFxType = 0;

            // Change assign mode only if Lower active (a Boutique rule)
            // But keep memory of the last assign mode choosen.
            if ( !JP08upperPart) JP08assignMode = a;
            JP08lastChoosenAssigMode = a ;
      }

      else if ( memcmp( JP08SysexDual,sysExBuffer,  4 ) == 0 ) {
            if ( sysExBuffer[4] == 1 )
                JP08keyMode = KEY_MODE_DUAL ;
            else
            if ( sysExBuffer[4] == 0 )
                JP08keyMode = KEY_MODE_WHOLE ;
      }

      else if ( memcmp( JP08SysexUpper,sysExBuffer, 4 ) == 0 ) {

            JP08upperPart = ( sysExBuffer[4] == 0 ? false:true );
      }

      p = p1 = 0;
      return;

    }

    if ( ++p1 >= sizeof(sysExBuffer)) {
      p = p1 = 0;
      return;
    }

  } else {

    if (p == 0 && byteReceived != 0xF0 ) return;

    // Start by parsing header
    if ( JP08boutiqueSysexHeader[p] == byteReceived  ) {
      p++;
    }
    else
      p = 0 ;
  }

}

///////////////////////////////////////////////////////////////////////////////
// CHAIN ADVANCED MODE
//
// Configuration :
// IN 1,2 : MIDI IN signal merged
// IN 3   : JP-08 Master MIDI OUT for feedback
//
// OUT 1 : JP-08 (advanced chain mode. Midi channel master=1, slave = 2) listening on RootChannel
// OUT 2 : JU-06 (standard chain mode. Midi channel for master and slave = 1) listening on RootChannel+1
// OUT 3 : JX-03 (standard chain mode. Midi channel for master and slave = 1) listening on RootChannel+2
//
///////////////////////////////////////////////////////////////////////////////

void ChainAdvancedMode() {

    static int8_t noteCounter;
    static uint8_t s = MIDIIN1_PORT;

    uint8_t byteRead;
    uint8_t midiMsg[3];

    // Midi feedback from Master JP-08 Boutique
    // High Priority. Sysex only  and first.
    // All others events are purely ignored.

    if ( serialHw[JP08_FBK_PORT]->available() )  {

        // Do we have Sysex from JP-08 ?
        byteRead = serialHw[JP08_FBK_PORT]->read();
        boolean msgAvail = midiSerial[JP08_FBK_PORT].parse( byteRead );
        if ( midiSerial[JP08_FBK_PORT].isByteCaptured() ) {
              JP08BoutiqueSysexParse(byteRead);
              if (msgAvail) {
                  flashLED_CONNECT->start();
                  //Serial.print("SX Len=");
                  //Serial.println(midiSerial[JP08_FBK_PORT].getMidiMsgLen());
              }
        }
    }
    else // No Sysex
    {
      if ( serialHw[s]->available() ) {

        byteRead = serialHw[s]->read();

        // Incoming Midi 1 or 2 msg  = MIDI merge.
        if ( midiSerial[s].parse(byteRead ) ) {

           uint8_t msgLen =  midiSerial[s].getMidiMsgLen();
           uint8_t channel = midiSerial[s].getMidiMsg()[0] & 0x0F;
           memcpy(midiMsg, midiSerial[s].getMidiMsg(),msgLen);

           // Check for a Boutique midi channel
           for ( uint8_t bt = 0; bt < SERIAL_INTERFACE_MAX ; bt++ ) {

            // 3 Boutique chains max. Are we concerned by this channel msg ?
            if ( portMidiChannelMap[bt] == channel ) {
                 flashLED_CONNECT->start();
//               Serial.print ("MSG : ");
//               Serial.print(midiMsg[0],HEX);Serial.print(" ");
//               Serial.print(midiMsg[1],HEX);Serial.print(" ");
//               Serial.println(midiMsg[2],HEX);

               // Remap midi channel to 1
               midiMsg[0] = midiMsg[0] & 0xF0;

               // Do we have to handle JP08 avanced chain mode for note on / note off ?
               if (JP08 == bt ){
                 if ( midiMsg[0] == midiXparser::noteOffStatus || midiMsg[0] == midiXparser::noteOnStatus) {
                  // Handle velocity 0 as Note off
                  if ( midiMsg[2] == 0) midiMsg[0] = midiXparser::noteOffStatus;

                  // Send NoteON to the right Boutique module
                  if ( midiMsg[0] == midiXparser::noteOnStatus ) {

                      noteCounter++;

                      if ( JP08keyMode ==  KEY_MODE_DUAL ) {

                          if ( JP08assignMode == ASSIGN_MODE_SOLO ) {
                              // If Solo mode in DUAL , send both to master and slave
                              // Let the JP-08 to manage solo mode
                              serialHw[bt]->write(midiMsg,msgLen);
                              // Send to the slave midi channel 2
                              midiMsg[0]++;
                              serialHw[bt]->write(midiMsg,msgLen);
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( JP08assignMode == ASSIGN_MODE_UNISON ) {
                              // If UNISON mode in DUAL , send both to master and slave
                              // Let each of the JP-08 module to manage UNISON mode
                              // Max polyphony is 2*4
                              if ( noteCounter<=4 ) {
                                 serialHw[bt]->write(midiMsg,msgLen);
                                 // Send to the slave midi channel
                                 midiMsg[0]++;
                                 serialHw[bt]->write(midiMsg,msgLen);
                              }
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( JP08assignMode == ASSIGN_MODE_POLY ) {
                             // Alternate notes between Master and slave
                             // Max polyphony is 4x2
                             if (noteCounter <=4) {
                                 midiMsg[0]+= (noteCounter +1) % 2 ;
                                 serialHw[bt]->write(midiMsg,msgLen);
                                 flashLED_CONNECT->start();
                             }
                             return;
                          }
                      }
                      else {
                          if ( JP08assignMode == ASSIGN_MODE_SOLO ) {
                              // If Solo mode in WHOLE , send to master only
                              // Let the JP-08 to manage solo mode
                              serialHw[bt]->write(midiMsg,msgLen);
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( JP08assignMode == ASSIGN_MODE_UNISON ) {
                              // If UNISON mode in Whole , send both to master and slave
                              // Let each of the JP-08 module to manage UNISON mode
                              // Max polyphony is 2*4
                              if ( noteCounter <=4 ) {
                                 serialHw[bt]->write(midiMsg,msgLen);
                                 midiMsg[0]++;
                                 serialHw[bt]->write(midiMsg,msgLen);
                              }
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( JP08assignMode == ASSIGN_MODE_POLY ) {
                             // Alternate notes between Master and slave
                             // Max notes is 8
                            if (noteCounter <=8 ) {
                                 midiMsg[0]+= (noteCounter +1) % 2 ;
                                 serialHw[bt]->write(midiMsg,msgLen);
                                 flashLED_CONNECT->start();
                             }
                            return;
                          }
                      }

                  } else  { // Send Note OFF to master and slave

                      noteCounter--;
                      serialHw[bt]->write(midiMsg,msgLen);
                      midiMsg[0]++;
                      serialHw[bt]->write(midiMsg,msgLen);
                      flashLED_CONNECT->start();

                      // Overflowed notes....
                      if ( noteCounter < 0 ) {

                        BlinkLED_CONNECT(10);
                        noteCounter = 0;
                        // Send All notes off to master and slave in case of counter error
                        serialHw[bt]->write(midiXparser::controlChangeStatus);
                        serialHw[bt]->write(0x7B);serialHw[1]->write(0);
                        serialHw[bt]->write(midiXparser::controlChangeStatus+1);
                        serialHw[bt]->write(0x7B);serialHw[2]->write(0);
                      }
                      flashLED_CONNECT->start();
                      return;
                  }
               }
               else {
                // Other channel voice messages are sent to Master and slave
                      serialHw[bt]->write(midiMsg,msgLen);
                      midiMsg[0]++;
                      serialHw[bt]->write(midiMsg,msgLen);
                      //Serial.println(" sent to Sale port 2.");
                      flashLED_CONNECT->start();
                      return;
               }
             } else {
              // Not the JP-08. Simply send the event to the master.
              serialHw[bt]->write(midiMsg,msgLen);
             }
           }
          }
       }

       // Other bytes not captured (real time, etc..),  send the received byte on the flow
       // to all the master & slave. Looks like a MIDI THRU....
       else if (! midiSerial[s].isByteCaptured() ) {
           for ( uint8_t bt = 0; bt < SERIAL_INTERFACE_MAX ; bt++ )
                  serialHw[bt]->write(byteRead);

           flashLED_CONNECT->start();
        }
      }

      // Only port 0 and 1 are scanned. Port 2 is the Feedback of JP08 midi chain.
      s = ( s == MIDIIN1_PORT ? MIDIIN2_PORT : MIDIIN1_PORT) ;

    }
}

///////////////////////////////////////////////////////////////////////////////
// SETUP
///////////////////////////////////////////////////////////////////////////////
void setup() {

    // Retrieve EEPROM parameters
    CheckEEPROM();

    // Configure the TIMER2
    timer.pause();
    timer.setPeriod(TIMER2_RATE_MICROS); // in microseconds
    // Set up an interrupt on channel 1
    timer.setChannel1Mode(TIMER_OUTPUT_COMPARE);
    timer.setCompare(TIMER_CH1, 1);  // Interrupt 1 count after each update
    timer.attachCompare1Interrupt(Timer2Handler);
    timer.refresh();   // Refresh the timer's count, prescale, and overflow
    timer.resume();    // Start the timer counting

    // Start the LED Manager
    flashLEDManager.begin();

    // MIDI SERIAL PORTS 1-3 set Baud rates

    for ( uint8_t s=0; s < SERIAL_INTERFACE_MAX ; s++ ) {
      serialHw[s]->begin(31250);
      uint8_t c = EEPROM_Params.rootMidiChannel + s;
      if (c > 0x0F ) c -= 0x0F;
      portMidiChannelMap[s] = c;
    }

    // Set parser filters
    midiSerial[MIDIIN1_PORT].setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk );
    midiSerial[MIDIIN2_PORT].setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk );
    midiSerial[JP08_FBK_PORT].setMidiMsgFilter( midiXparser::sysExMsgTypeMsk );

    // start USB serial
    Serial.begin();
}

///////////////////////////////////////////////////////////////////////////////
// LOOP
///////////////////////////////////////////////////////////////////////////////
void loop() {

    ChainAdvancedMode();
    // 'C' character received on Serial will activate the configuration menu
    if ( Serial.available() && Serial.read() == 'C') ConfigRootMenu();

}
