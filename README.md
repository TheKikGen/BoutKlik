# BoutKlik

BETA 2 VERSION.
A better Roland Boutique Chain mode

I own 2 JP-08 and I love the sound this "mini" Jupiter 8 is producing.
But as I used the Roland Boutique "chain mode" to get 8 notes polyphony, I faced to several issues, as many users :

1. The midi channel of the Boutiques must be mandatory 1, and only 1.  
The overflow notes are sent on channel 1, whatever the midi channel is on the master.  
If you change the midi channel to other than 1 on the master, the chain mode works, but mod wheel, pich bend and all other channel messages stops working because not sent on the same channel as notes.  
You can't change the midi channel on the slave, as the chain mode is sent to channel 1, else chain mode stops to work.  This behaviour forbids to use many Boutiques chained (I own also 2 JU-06 and 2 JX-03) in the same midi loop, i.e. midi dedicated port for each chain on midi channel 1.

2. When you play more than 8 notes, the "steal note" issue surround.  With a real Jupiter 8, if you attempt to play more than 8 notes the 9th will not be sent to the sound module.  With the JP-08, the 9th note will replace one of the existing 8 notes currently playing.  This behaviour is very annoying for me and many Boutique users.

As the chain mode is a full MIDI process, I decided to rewrite my own one.  This is the BoutKlik project.
This project reuses the design of the USBMIDIKLIK4x4 board and firmware (one of my other project).

https://github.com/TheKikGen/USBMidiKliK4x4
<img border="0" src="https://2.bp.blogspot.com/-wo1H27RQYiU/XDzO9VG3vdI/AAAAAAAAAWA/KehLjyXhLTg_nmjjmEkO7LZtY5H83Rr-ACLcBGAs/s1600/20190113_221557.jpg"  />

## BoutKlik logic 

BoutKlik maps the midi listening channel of a 3 chains ( 3x2 boutiques) to any midi channel you want, allowing you to use these chain in the same midi loop than your other synths.

<img border="0" src="https://github.com/TheKikGen/BoutKlik/blob/master/doc/BoutKlik_Topology.PNG?raw=true"  />

. The Midi OUT Port 1,2,3 are assigned respectively to the JP-08, JU-06 and JX-03.

. All master Boutique devices must be set to midi channel 1

. The JP-08 slave must be set to midi channel 2. All other slaves must set to midi channel 1.

. Each of the masters must be set in Chain mode

. The slave JP-08 Boutique MIDI OUT must be connected to the BoutKlik MIDI IN 3.

. Boutique Slaves MIDI IN jack must be connected to the MIDI OUT of their Boutique master

The midi root channel will be used to set the listening midi channels on BouKlik MIDI IN port 1, 2 and 3, respectively, to <MIDI root channel>, <MIDI root channel +1>,\n <midi root channel + 2>.

## About the advanced chain mode 

This is a full rewritten chain mode allowing a "better" chain mode for 2 JP-08 Roland Boutique. 
The original Roland Boutique Chain mode transmits all notes beyond the 4th to the slave.
The BoutKlik chain mode distributes notes equally : odd notes are transmitted to the master, and even notes to the slave. 

For example : notes 1,2,3,4 played will be distributed as (Master 1)(Slave 2)(Master 3)(Slave 4). 
All notes played exceeding the polyphony of 8 (or 4 in DUAL) are simply not processed as the original JP-8.

## Chorus FX mode

It is possible to activate the hidden 3 chorus mode on the JP-08 directly on the master.

To set the chorus, Press DUAL+POLY when in POLY. That will switch from no chorus to chorus 1, 2, 3.

If The POLY mode is not the current one, go to POLY first, the press DUAL + POLY to change the chorus, and come back to your original mode SOLO or UNISON.
This can be obviously obtained also by sending the CC 93 (V = 0,1,2 or 3) to the MIDI IN 1 or 2 (being merged) to the "root" midi channel adressing the midi OUT 1.
