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
uint8_t portMidiChannelMap[SERIAL_INTERFACE_MAX] = {DEFAULT_PORT_MIDICHANNEL_MAP};


// Prepare LEDs pulse for Connect, MIDIN and MIDIOUT
// From MIDI SERIAL point of view
// Use a PulseOutManager factory to create the pulses
PulseOutManager flashLEDManager;

PulseOut* flashLED_CONNECT = flashLEDManager.factory(LED_CONNECT,LED_PULSE_MILLIS,LOW);

// MIDI Parsers for serial 1 to n
midiXparser midiSerial[SERIAL_INTERFACE_MAX];

uint8_t assignMode = ASSIGN_MODE_POLY;
boolean upperPart = false;
uint8_t keyMode = KEY_MODE_WHOLE;
uint8_t currentChorusFxType = 0 ;
boolean splitMode = false;

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
    EEPROM_Params.advancedMode = true;

    //Write the whole param struct
    EEPROM_writeBlock(0, (uint8*)&EEPROM_Params,sizeof(EEPROM_Params) );

  }

}
///////////////////////////////////////////////////////////////////////////////
// EEPROM EMULATION UTILITIES
///////////////////////////////////////////////////////////////////////////////
int EEPROM_writeBlock(uint16 ee, const uint8 *bloc, uint16 size )
{
    uint16 i;
    for (i = 0; i < size; i++) EEPROM.write(ee+i, *(bloc+i));

    return i;
}

int EEPROM_readBlock(uint16 ee, uint8 *bloc, uint16 size )
{
    uint16 i;
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
    Serial.print("\n\nBOUTKLIK CONFIGURATION MENU\n");
    Serial.print("(c) TheKikGenLabs\n\n");
    Serial.print("Magic number      : "); Serial.write(EEPROM_Params.signature , sizeof(EEPROM_Params.signature));
    Serial.print( EEPROM_Params.prmVer);  Serial.print("-") ; Serial.println( (char *)EEPROM_Params.TimestampedVersion);
    Serial.print("Current mode      : "); Serial.println(EEPROM_Params.advancedMode ? "Advanced":"Basic");
    Serial.print("Root MIDI channel : "); Serial.println(EEPROM_Params.rootMidiChannel+1);Serial.println(".\n");
    Serial.print("a. Set BoutKlik to advanced mode\n");
    Serial.print("b. Set BoutKlik to basic mode\n");
    Serial.print("m. Set root MIDI channel\n");
    Serial.print("f. Restore factory settings\n");
    Serial.print("s. Save configuration & exit\n");
    Serial.print("x. Abort without saving\n");
		Serial.print("\n=>");
		choice = USBSerialGetChar();
		Serial.println(choice);

		switch (choice)
		{

			// BoutKlik mode advanced
      case 'a':
        if ( getChoice("\nSet BoutKlik to advanced mode ?","") == 'y' ) {
            EEPROM_Params.advancedMode = true;
        }
        break;

      // BoutKlik mode basic
      case 'b':
        if ( getChoice("\nSet BoutKlik to basic mode ?","") == 'y' ) {
            EEPROM_Params.advancedMode = false;
        }
        break;

      // Midi rooot channel
      case 'm':
        Serial.print("\nEnter the midi root channel (01-16 / 00 to exit) :\n");
        i = 0; j = 0;
        while ( i < 2 ) {
          c  = USBSerialGetDigit();Serial.write(c);
          j +=  ( (uint8_t) (c - '0' ) ) * pow(10 ,1-i);
          i++;
        }
        if (j == 0 or j >16 )
           Serial.print(". No change made. Incorrect value.\n");
        else {
          EEPROM_Params.rootMidiChannel = j-1;
        }
        break;

			// Reload the EEPROM parameters structure
			case 'e':
				if ( getChoice("\nReload current settings from EEPROM","") == 'y' ) {
					CheckEEPROM();
					Serial.print("\nSettings reloaded from EEPROM.\n");
				}
				break;

			// Restore factory settings
			case 'f':
				if ( getChoice("\nRestore all factory settings","") == 'y' ) {
					if ( getChoice("\nYour own settings will be erased. Are you really sure","") == 'y' ) {
						CheckEEPROM(true);
						Serial.print("\nFactory settings restored.\n");
					}
				}
				break;

			// Save & quit
			case 's':
				if ( getChoice("\nSave settings and exit to midi mode","") == 'y' ) {
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
// BASIC MODE WITHOUT ADVANCED CHAINING. 3 Boutiques.
//
// Configuration :
// OUT 1,2,3 : Boutique masters MIDI IN in chain mode. Midi channel 1.
// Boutique masters MIDI OUT to Boutique corresponding slaves. Midi channel 1.
//
///////////////////////////////////////////////////////////////////////////////

void ModeBasic() {

    // SERIAL MIDI PROCESS

    for ( uint8_t s = 0; s< SERIAL_INTERFACE_MAX ; s++ )
    {
        // Do we have any channel voice MSG to convert  ?
        if ( serialHw[s]->available() ) {
           if ( midiSerial[s].parse( serialHw[s]->read() ) ) {

                uint8_t msgLen =  midiSerial[s].getMidiMsgLen();
                uint8_t channel = midiSerial[s].getMidiMsg()[0] & 0x0F;
                uint8_t midiStatus  = midiSerial[s].getMidiMsg()[0] & 0xF0;

                if ( portMidiChannelMap[s] == channel ) {
                  // Fix Midi CHANNEL to be always 1 in chain mode for any msg if the channel is used by a Boutique
                  // Ignore events on others channels

                    serialHw[s]->write(midiStatus);
                    serialHw[s]->write(midiSerial[s].getMidiMsg()[1]);
                    if (msgLen == 3 ) serialHw[s]->write(midiSerial[s].getMidiMsg()[2]);

                    flashLED_CONNECT->start();

                }

           }
           else if (! midiSerial[s].isByteCaptured() ) {
                // Not captured, just send the byte received to the corresponding MIDI OUT
                serialHw[s]->write(midiSerial[s].getByte());
                flashLED_CONNECT->start();
           }
        }

    }
}

///////////////////////////////////////////////////////////////////////////////
// SEND CHORUS CC
///////////////////////////////////////////////////////////////////////////////
void SendCCChorusFxType(uint8_t s,uint8_t chorusFxType) {
  
  serialHw[s]->write(midiXparser::controlChangeStatus);
  serialHw[s]->write(93); // Chorus
  serialHw[s]->write(chorusFxType); // 0 : Stop or Chorus 1, 2, 3.


  Serial.print("Send CC Chorus on port : "); 
  Serial.print(s); Serial.print (" Msg was :"); 
  Serial.print(0x0B,HEX);Serial.print(" - ");
  Serial.print(93,HEX);Serial.print(" - ");
  Serial.println(chorusFxType);
  
}

///////////////////////////////////////////////////////////////////////////////
// SET CHORUS EFFECT TYPE
///////////////////////////////////////////////////////////////////////////////
void SetChorusFxType() {
    currentChorusFxType++;
    if ( currentChorusFxType > 3 ) currentChorusFxType = 0;
    // Send CC to the master and the slave
    delay(300);
    SendCCChorusFxType(2,currentChorusFxType);
 delay(1000);
    //SendCCChorusFxType(1,currentChorusFxType);
    //SendCCChorusFxType(2,currentChorusFxType);
}

///////////////////////////////////////////////////////////////////////////////
// PARSE SOME BOUTIQUE SYSEX
///////////////////////////////////////////////////////////////////////////////
void boutiqueSysexParse(uint8_t byteReceived) {

  // Specific Boutique sysex messages
  static uint8_t boutiqueSysexHeader[]         = { 0xF0, 0x41, 0x10, 0x00, 0x00, 0x00, JP08_ID   , 0x12 };
  static uint8_t boutiqueSysexModePoly[]       = { 0x03, 0x00, 0x11, 0x06, 0x00, 0x00, 0x66, 0xF7 };
  static uint8_t JP08SysexDual[]               = { 0x01, 0x10, 0x00, 0x02, 0x01, 0x6C, 0xF7 };
  static uint8_t JP08SysexDualOff[]            = { 0x01, 0x10, 0x00, 0x02, 0x00, 0x6D, 0xF7 };
  static uint8_t JP08SysexUpper[]              = { 0x01, 0x10, 0x00, 0x03, 0x01, 0x6B, 0xF7 };

  static uint8_t sysExBuffer[8];

  // Pointers
  static uint8_t p = 0 ;
  static uint8_t p1 = 0 ;


  if ( p == sizeof ( boutiqueSysexHeader ) ) {

    sysExBuffer[p1] = byteReceived;
    if ( byteReceived == 0xF7 ) {

      if ( memcmp( boutiqueSysexModePoly,sysExBuffer,   5 ) == 0 && !upperPart ) {
            uint8_t lastAssignMode = assignMode;
            assignMode = ( sysExBuffer[5] == 3 ? ASSIGN_MODE_UNISON : sysExBuffer[5] == 2 ? ASSIGN_MODE_SOLO:ASSIGN_MODE_POLY );

            // Special features (mainly for JP08 but works for others boutiques)
            // Resend the current assign Mode again IN POLY to switch different chorus (2 times POLY)
            if (assignMode == ASSIGN_MODE_POLY && lastAssignMode == assignMode) {
                //SetChorusFxType();
               //delay(150);
               
                   //SendCCChorusFxType(0,currentChorusFxType);

            
            }
      }

      else if ( memcmp( JP08SysexDual,sysExBuffer,  4 ) == 0 ) {
            if ( sysExBuffer[4] == 1 )
                keyMode = KEY_MODE_DUAL ;
            else
            if ( sysExBuffer[4] == 0 )
                keyMode = KEY_MODE_WHOLE ;  
      }

      else if ( memcmp( JP08SysexUpper,sysExBuffer, 4 ) == 0 ) {
          
            upperPart = ( sysExBuffer[4] == 0 ? false:true );
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
    if ( boutiqueSysexHeader[p] == byteReceived || p == 6 ) {  // Ignore device ID at byte # 7
      p++;
    }
    else
      p = 0 ;
  }

}


///////////////////////////////////////////////////////////////////////////////
// MODE WITH ADVANCED CHAINING. ONLY 1 Boutique
//
// Configuration :
// IN 1,3 : MIDI IN signal merged
// IN 2 : Master Boutique MIDI OUT in chain Mode
//
// OUT 1 : MIDI THRU (all MIDI IN from 1,2)
// OUT 2 : Master Boutique MIDI IN in chain mode. Midi channel 1.
// OUT 3 : Slave Boutique MIDI IN. Midi channel 1.
//
///////////////////////////////////////////////////////////////////////////////

void ModeAdvanced() {

    static int8_t noteCounter=0;
    static uint8_t s = 0;
    
    uint8_t byteRead;
    uint8_t midiMsg[3];

    // Midi Out from Master Boutique. High Priority. Sysex only  and first. 
    // All others events are purely ignored.
    // NB : It is not possible to deactivate the MIDI THRU behaviour on the Boutique.
    
    if ( serialHw[1]->available() )  {
        // Do we have Sysex from Master ?
        byteRead = serialHw[1]->read();
        
        boolean msgAvail = midiSerial[1].parse( byteRead );
        if ( midiSerial[1].isByteCaptured() ) {           
              boutiqueSysexParse(byteRead);
              serialHw[2]->write(byteRead); // Send to slave
              if ( midiSerial[1].getMidiCurrentMsgType() == midiXparser::sysExMsgTypeMsk) Serial.print("x");                      
              Serial.print(byteRead,HEX);Serial.print(" ");
              if (msgAvail) {
                  flashLED_CONNECT->start();
                  Serial.print("Sysex.Len =");
                  Serial.println(midiSerial[1].getMidiMsgLen());
                  //Serial.println(midiSerial[1].getSysExMsgLen());
              }
        }              
    } 
    else // No Sysex
    {   
        
      if ( serialHw[s]->available() ) {
         
        byteRead = serialHw[s]->read();
        
        // Incoming Midi channel msg alternate from port 1 or 3 = MIDI merge.
        if ( midiSerial[s].parse(byteRead ) ) {            
           
           uint8_t msgLen =  midiSerial[s].getMidiMsgLen();
           uint8_t channel = midiSerial[s].getMidiMsg()[0] & 0x0F;
           memcpy(midiMsg, midiSerial[s].getMidiMsg(),msgLen);
           
           // Are we concerned by this channel msg ?
           if ( channel == EEPROM_Params.rootMidiChannel ) {
               
               Serial.print ("MSG : ");
               flashLED_CONNECT->start();
               Serial.print(midiMsg[0],HEX);Serial.print(" ");
               Serial.print(midiMsg[1],HEX);Serial.print(" ");
               Serial.println(midiMsg[2],HEX);
               
               // Force channel to 1
               midiMsg[0] = midiMsg[0] & 0xF0;                

               // Do we have to handle avanced chain mode for note on / note off ?
               if ( midiMsg[0] == midiXparser::noteOffStatus || midiMsg[0] == midiXparser::noteOnStatus) {
                  // Handle velocity 0 as Note off
                  if ( midiMsg[2] == 0) midiMsg[0] = midiXparser::noteOffStatus;

                  // Send NoteON to the right Boutique module
                  if ( midiMsg[0] == midiXparser::noteOnStatus ) {
                      
                      noteCounter++;
                      
                      if ( keyMode ==  KEY_MODE_DUAL ) {

                          if ( assignMode == ASSIGN_MODE_SOLO ) {
                              // If Solo mode in DUAL , send both to master and slave
                              // Let the JP-08 to manage solo mode
                              serialHw[1]->write(midiMsg,msgLen);
                              serialHw[2]->write(midiMsg,msgLen);
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( assignMode == ASSIGN_MODE_UNISON ) {
                              // If UNISON mode in DUAL , send both to master and slave
                              // Let each of the JP-08 module to manage UNISON mode
                              // Max polyphony is 2*4
                              if ( noteCounter<=4 ) {
                                 serialHw[1]->write(midiMsg,msgLen);
                                 serialHw[2]->write(midiMsg,msgLen);
                              }
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( assignMode == ASSIGN_MODE_POLY ) {
                             // Alternate notes between Master and slave
                             // Max polyphony is 4x2
                             if (noteCounter <=4) {
                                 uint8_t midiPort = (noteCounter +1) % 2 +1 ;
                                 serialHw[midiPort]->write(midiMsg,msgLen);
                                 flashLED_CONNECT->start();
                             }
                             return;
                          }

                      }
                      
                      else {
                          if ( assignMode == ASSIGN_MODE_SOLO ) {
                              // If Solo mode in WHOLE , send to master only
                              // Let the JP-08 to manage solo mode
                              serialHw[1]->write(midiMsg,msgLen);
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( assignMode == ASSIGN_MODE_UNISON ) {
                              // If UNISON mode in Whole , send both to master and slave
                              // Let each of the JP-08 module to manage UNISON mode
                              // Max polyphony is 2*4
                              if ( noteCounter<=4 ) {
                                 serialHw[1]->write(midiMsg,msgLen);
                                 serialHw[2]->write(midiMsg,msgLen);
                              }
                              flashLED_CONNECT->start();
                              return;
                          }

                          if ( assignMode == ASSIGN_MODE_POLY ) {
                             // Alternate notes between Master and slave
                             // Max notes is 8
                             if (noteCounter <=8 ) {
                                 uint8_t midiPort = (noteCounter +1) % 2 +1 ;
                                 serialHw[midiPort]->write(midiMsg,msgLen);
                                 flashLED_CONNECT->start();
                             }
                            return;
                          }
                      }

                  } else { // Send Note OFF to master and slave

                      noteCounter--;
                      serialHw[1]->write(midiMsg,msgLen);
                      serialHw[2]->write(midiMsg,msgLen);
                      flashLED_CONNECT->start();
                      if ( noteCounter < 0 ) {
                        noteCounter = 0;
                        // Send All notes off to master and slave in case of counter error
                        Serial.println("Overflowed notes....All notes off sent.");
                        serialHw[1]->write(midiXparser::controlChangeStatus);
                        serialHw[2]->write(midiXparser::controlChangeStatus);
                        serialHw[1]->write(0x7B);serialHw[1]->write(0);
                        serialHw[2]->write(0x7B);serialHw[2]->write(0);
                      }
                      flashLED_CONNECT->start();
                      return;
                  }
               }
               else { 
                // Other channel voice messages are sent to Master and slave
                      serialHw[1]->write(midiMsg,msgLen);
                      serialHw[2]->write(midiMsg,msgLen);
                      flashLED_CONNECT->start();
                      return;
               }
           } 
           // Ignore if not the listening channel  
       }
       
       // Other bytes not captured (real time, etc..),  send the received byte on the flow to the master & slave
       else if (! midiSerial[s].isByteCaptured() ) {
                
           serialHw[1]->write(byteRead);
           serialHw[2]->write(byteRead);
           
           // Do not send SYSEX to the SLAVE as it will be resend via the MIDI OUT of the master
           // if ( midiSerial[s].getMidiCurrentMsgType() != midiXparser::sysExMsgTypeMsk ) 
          
              
           flashLED_CONNECT->start();
        }
      }
        
        s = ( s == 0 ? 2 : 0) ;
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

		// Activate serial mode for configuration menu.
		Serial.begin();

    // MIDI SERIAL PORTS set Baud rates

    for ( uint8_t s=0; s < SERIAL_INTERFACE_MAX ; s++ ) {
      serialHw[s]->begin(31250);
      midiSerial[s].setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk ); // By Default
      uint8_t c = EEPROM_Params.rootMidiChannel + s;
      if (c > 0x0F ) c -= 0x0F;
      portMidiChannelMap[s] = c;
    }

    // If Advanced mode, set only SYSEX filter for Boutique master port
    if (EEPROM_Params.advancedMode) midiSerial[1].setMidiMsgFilter( midiXparser::sysExMsgTypeMsk );

    // start USB serial
    Serial.begin();

 
}

///////////////////////////////////////////////////////////////////////////////
// LOOP
///////////////////////////////////////////////////////////////////////////////
void loop() {

    if (EEPROM_Params.advancedMode) ModeAdvanced(); else ModeBasic();
    // Any character received on Serial will activate the configuration menu
    if ( Serial.available() && Serial.read() == 'C') ConfigRootMenu();
}
