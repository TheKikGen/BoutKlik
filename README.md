# BoutKlik
A better Roland Boutique Chain mode

I own 2 JP-08 and I love the sound this mini Jupiter 8 is producing.
But as I used the Roland Boutique "chain mode"to 8 notes polyphony, as many of Boutique users, I faced to 2 main issues :

1. The midi channel of the Boutiques must mandatory be 1, and only 1.  The overflow notes are sent on channel 1, whatever the midi channel is on the master.  If you change the midi channel to other than 1 on the master, the chain mode works, but you are note able to user the mod wheel and the pich bend (and all other channel messages) stops working.  This behaviour forbids to use many Boutiques chained (I own also 2 JU-06 and 2 JX-03) in the same midi loop, i.e. midi dedicated port for each chain on midi channel 1.

2. When you play more than 8 notes, the "steal note" issue surround.  With a real Jupiter 8, if you attempt to play more than 8 notes the 9th will not be sent to the sound module.  With the JP-08, the 9th note will replace one of the existing 8 notes currently playing.  This behaviour is very annoying for me.

[![Watch the video](https://i.imgur.com/vKb2F1B.png)](https://youtu.be/Ejpw7GsyAGg)


 Serial.print("\n\nAdvanced mode :\n");
        Serial.print("In that mode you will be able to connect 1 master Roland Boutique device \nand 1 slave, being fully controlled by BoutKlik.\n\n");
        Serial.print(". All Boutique devices must be set to midi channel 1.\n");
        Serial.print(". The master must be set in Chain mode, with MIDI IN connected to \n  the MIDI OUT 2, and MIDI OUT connected to MIDI IN 2.\n");
        Serial.print(". The slave MIDI IN must be connected to the MIDI OUT 3.\n");
        Serial.print("The midi root channel sets the listening midi channel on \nmerged BouKlik MIDI IN port 1 and 3.\n");

        if ( getChoice("\nSet BoutKlik to advanced mode ?","") == 'y' ) {
            EEPROM_Params.advancedMode = true;
        }
        break;

      // BoutKlik mode basic
      case 'b':
        Serial.print("\n\nBasic mode :\n");
        Serial.print("In that mode you will be able to connect 3x2 Roland Boutique devices, \nto remap their MIDI IN channel.\n");
        Serial.print(". All Boutique devices must be set to midi channel 1.\n");
        Serial.print(". Each of the masters must be set in Chain mode,\n");
        Serial.print(". Boutique Masters MIDI IN jack must be connected \n  to the BoutKlik MIDI OUT port 1, 2, 3. \n");
        Serial.print(". Boutique Slaves MIDI IN jack must be connected \n  to the MIDI OUT of their Boutique master.\n");
        Serial.print("The midi root channel will be used to set the \nlistening midi channels on BouKlik MIDI IN port 1, 2 and 3,\n");
        Serial.print("respectively, to <MIDI root channel>, <MIDI root channel +1>,\n <midi root channel + 2>.\n\n");
