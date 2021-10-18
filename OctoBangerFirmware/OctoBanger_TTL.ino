/*
*************Octo Banger TTL*********
01/21/2017 this is a Banger with 8 channels.
Output is targeted at simple TTL for 2 of the the 4 relay modules
*** 01/22/2017 Adding frame-based state data storage per Joe AKA wickedbeernut

I am going to start by fixing FPS at 20fps, or 50ms per frame.
frame state data is going to be stored in pairs, where:
1) byte 1 represents the state byte (like always)
2) byte 2 represents the delay (in number of frames) to persist the current state
    before moving on to the next pair
The above approach will also free up another byte of eeprom that represents FPS, since
 it is now fixed.

		//20170122 MJN testing with a metronome track tonight is revealing an innacuracy in
		// the 50 ms per frame deal. Maybe a flaw with millis() or timer 0?
		//Anyway, using 49.595 below seems to make it dead on 20fps
		//changed from 49.6 to 49.595 based on this post: http://www.avrfreaks.net/forum/uno-clock-running-slow
		//user Gavin Andrews measured with a scope and found this:
		//Firstly I realised that it wasn't running fast it was running slow... If a real worldworld 60 seconds measures as 59.514 seconds Arduino time that must mean the Arduino clock is SLOW not fast.
        //They concluded that this may be a clone issue.  If this is the case, should I allow
		// for configurable compensation?
THE CONSTANT MILLIS_PER_FRAME IS NOW A FLOAT TO COMPENSATE FOR THE ABOVE.
TWEAK IF YOUR TIMING IS OFF.
09/04/2018 - K. Short:  Changed type declaration of variable "sample_ticker" from "uint8_t" to "volatile unsigned int" to ensure
                        play_sequence exits (endless looping). For example, if "hi_sample" is 150, it will wrap the variable
                        (upper bound was 254) and never exit the "while" scare sequence!
*/
#include <EEPROM.h>
#include "MiniMedia.h"
#include "PinMaps.h"

volatile float MILLIS_PER_FRAME = 50.0; //49.595; //default is 20 frames per sec (50 ms each)

volatile bool stampok = true;
volatile byte stamp_buff[5] = {8,2,0,2,6}; //holder for stamp, 8 tells PC app that we have 8 channels
// 8 = 8 channels


#define ARDUINO_LED 13 //board led
#define DEBOUNCE 5  // button debouncer, how many ms to debounce, 5+ ms is usually plenty

//eeprom offsets
//sequence data 0 - 999

const int SAMPLE_COUNT = 1000; //saving last 24 bytes for config use (total of 1024 eeprom)
//the following 2 bytes are going to be delivered with seq data
const int HI_SAMPLE_LB = 1000;
const int HI_SAMPLE_UB = 1001;

//the next 9 bytes are delivered with a config update
const int MEM_SLOTS = 9;

const int PIN_MAP_SLOT = 1002; //tells us what our pin mapping strategy is
const int TTL_TYPES_SLOT = 1003;
const int MEDIA_TYPE_SLOT = 1004;
const int MEDIA_DELAY_SLOT = 1005;
const int RESET_DELAY_SLOT = 1006; 
const int BOOT_DELAY_SLOT = 1007;
const int VOLUME_SLOT = 1008;
const int TRIGGER_TYPE_SLOT = 1009;
const int TIMING_OFFSET_TYPE_SLOT = 1010;  //some arduinos have timing issues that must be offset

//1019-1023 stamp
const int STAMP_OFFSET = 1019; //start byte of stamp

const int TTL_COUNT = 8; //number of TTL outputs
bool TTL_TYPES[TTL_COUNT] = {1,1,1,1,1,1,1,1}; //translated TTL Types
int8_t TTL_PINS[TTL_COUNT] = {2,3,4,5,6,7,8,9}; //the default
const int TRIGGER_PIN_COUNT = 3; //number of trigger pins
int8_t TRIGGER_PINS[TRIGGER_PIN_COUNT] = {11,10,12}; //stash of trigger pins and media pin
bool _had_bad_pin_map = false;
const int TIMING_OFFSET_TYPE_COUNT = 2;
const float TIMING_OFFSETS[TIMING_OFFSET_TYPE_COUNT] = {-.405,0};
const int SERIAL_WAIT = 5; //delay for serial reads

volatile uint8_t TTL_TYPE_BYTE = 0xFF;
volatile unsigned int hi_sample = 0x0;
volatile uint8_t _trigger_type = 0x1;
volatile uint8_t _pin_map = 0x0;
volatile uint8_t RESET_DELAY_SECS = 30; //default will be 30 secs I suppose
volatile byte samples[SAMPLE_COUNT]; //working buffer so we don't read from eeprom constantly
volatile uint8_t BOOT_DELAY_SECS = 0x0; //delay before the prop "goes hot"
volatile uint8_t _volume = 22;
volatile bool IS_HOT = true;
MiniMedia _Media;
volatile uint8_t _mediaType = 0x0; //0 is none
volatile uint8_t _mediaDelay = 0x0;
volatile uint8_t _timingOffsetType = 0x0;

uint8_t b;
unsigned long _triggerable_millis = 0;

void setup() 
{
  //read config from eeprom
  read_config();
  init_ouputs();
  Serial.begin(115200); //we talk to the PC at 115200
  Serial.println(F(".OBC")); //spit this back immediately tells PC what it just connected to
  _Media.Init(TRIGGER_PINS[2],_volume,_mediaType,_mediaDelay);
  _Media.PlayAmbient();
  buffer_samples();
  //blink_times(5,true); //this is a 5 second delay between the initial Serial response and later serial responses.
                  // the delay lets the PC app disconnect if it is the wrong type
  report_config();
  if(BOOT_DELAY_SECS > 0)
  {
	  Serial.println(F("Waiting for Boot Delay..."));
	  blink_times(BOOT_DELAY_SECS); //this uses 1 second per blink
  }
  delay(1000);
  Serial.println(F("Ready"));
  clear_rx_buffer();
  if(check_trigger())
  {   //this will give the user 30 seconds to upload a different config
	  //before getting stuck in an endless trigger loop.
	  Serial.println(F("Trigger detected on startup-"));
	  delay(100);
	  Serial.println(F("Going cold for 30 secs"));
	  _triggerable_millis = millis() + 30000;
  }
  else
	  _triggerable_millis = millis();
}

void init_ouputs()
{
  //init output pins
  if(_pin_map > (PIN_MAP_COUNT - 1))
  {
	_pin_map = 0; //revert to default on a bad setting
	_had_bad_pin_map = true;
  }
  //additional check, if someone sets to custom, and the custom pin buffer is not set to a 
  // legitimate pin, revert to default
  if(_pin_map > 0)
  {
	  for (int i = 0; i < TTL_COUNT; i++)
	  {
		  //allow D2 thru A7 (A6 and A7 exist on the nano)
		  if ((PIN_MAPS[_pin_map][i] < 2) || (PIN_MAPS[_pin_map][i] > A7))
		  {
				_pin_map = 0; //revert to default on a bad setting
				_had_bad_pin_map = true;
				break;
		  }
	  }
  }

  for (int i = 0; i < TTL_COUNT; i++)
  {
	  TTL_PINS[i] = PIN_MAPS[_pin_map][i];
      pinMode(TTL_PINS[i], OUTPUT);
  }
  for (int i = 0; i < TRIGGER_PIN_COUNT; i++)
  {
	  TRIGGER_PINS[i] = PIN_MAPS[_pin_map][TTL_COUNT + i];
  }
  set_ambient_out();
  pinMode(TRIGGER_PINS[0], INPUT); //init manual trigger pin
  //if we are set for ambient hi trigger, add pull-up
  if(_trigger_type == 0x1)
	  digitalWrite(TRIGGER_PINS[0], HIGH); // connect internal pull-up 
  else
	  digitalWrite(TRIGGER_PINS[0], LOW);
  pinMode(TRIGGER_PINS[1], OUTPUT);
  digitalWrite(TRIGGER_PINS[1], HIGH); // connect internal pull-up 
  pinMode(ARDUINO_LED, OUTPUT);
  digitalWrite(ARDUINO_LED, LOW);
}

bool check_trigger()
{
	if(_triggerable_millis > millis()) return false;
    //this is dumbed-down debounce check.
	//button is ussumed off at first
	//2 samples are taken for each input, separated by n milliseconds.
	//if the samples are both on, we consider it a true on condition
  static byte state1;
  static byte state2;
  state1 = digitalRead(TRIGGER_PINS[0]);   // read the button
  delay(DEBOUNCE);
  state2 = digitalRead(TRIGGER_PINS[0]);   // read the button
  if(_trigger_type == 0x0)
	  return !(state1 + state2 == 0);
  else
	  return (state1 + state2 == 0);
}
void buffer_samples()
{
  //fetch whatever eeprom values we have into buffer
  if(hi_sample > 0)
  {
	  for (int i = 0; i < SAMPLE_COUNT; i++)
	  {
		 samples[i] = EEPROM.read(i); 
	  }
  }
}

void read_config()
{
  byte dtemp = 0;
  stampok = true;
  for(int i = 0; i < 5; i++)
  {
	  dtemp = EEPROM.read(STAMP_OFFSET + i);
	  if(dtemp != stamp_buff[i])
	  {
		  stampok = false;
		  break;
	  }
  }
  
  if(stampok) //control number so we don't read in junk
  {
	  hi_sample = EEPROM.read(HI_SAMPLE_UB);   //read the high/upper byte into hi_sample
	  hi_sample = hi_sample << 8;  
	  //bitshift the high byte left 8 bits to make room for the low byte
	  hi_sample = hi_sample | EEPROM.read(HI_SAMPLE_LB); //read the low byte
	  //get reset_delay out of EEPROM, represents seconds
	  RESET_DELAY_SECS = EEPROM.read(RESET_DELAY_SLOT); 
	  BOOT_DELAY_SECS = EEPROM.read(BOOT_DELAY_SLOT);
	  TTL_TYPE_BYTE = EEPROM.read(TTL_TYPES_SLOT); 
	  _volume = EEPROM.read(VOLUME_SLOT); 
	  if(_volume < 1 || _volume > 30)
		  _volume = 15;
	  _mediaType = EEPROM.read(MEDIA_TYPE_SLOT);
	  _mediaDelay = EEPROM.read(MEDIA_DELAY_SLOT);
	  _pin_map = EEPROM.read(PIN_MAP_SLOT);
	  _trigger_type = EEPROM.read(TRIGGER_TYPE_SLOT); 
	  _timingOffsetType = EEPROM.read(TIMING_OFFSET_TYPE_SLOT);
	  for (int i = 0; i < TTL_COUNT; i++)
	  {
		TTL_TYPES[i] = bitRead(TTL_TYPE_BYTE,i); //default to TTL low Output default, active high
	  }
  }
  if(_timingOffsetType > TIMING_OFFSET_TYPE_COUNT - 1)
	  _timingOffsetType = 0x0; //cannot point to non-existent element

}
void report_config()
{
	Serial.print(F("OctoBanger TTL v"));
	for (int i = 0; i < 3; i++)
	{
        Serial.print(stamp_buff[i]);
		delay(1);
		if(i < 2) 
			Serial.print(".");
		else
			Serial.println();
	}
	if(stampok) //control number so we don't read in junk
	{
		Serial.println(F("Config OK"));
	}
	else
	{
		Serial.println(F("Config NOT FOUND, using defaults"));
	}

	Serial.print(F("Frame Count: "));
	Serial.println(hi_sample);
	float duration = 0;
	for(int i = 0; i < (hi_sample * 2); i+=2)
		duration += (float)(samples[i + 1]) * MILLIS_PER_FRAME;
	Serial.print(F("Seq Len Secs: "));
	Serial.println(duration / 1000.0);

	Serial.print(F("Reset Delay Secs: "));
	Serial.println(RESET_DELAY_SECS);
	if(BOOT_DELAY_SECS != 0)
	{
		Serial.print(F("Boot Delay Secs: "));
		Serial.println(BOOT_DELAY_SECS);
	}
	if(_had_bad_pin_map)
		Serial.println(F("Invalid custom pinmap, reset."));
	else
		Serial.print(F("Pin Map: "));
	//Serial.println(_pin_map);
	switch(_pin_map)
	{
	case 0x0: 
		Serial.println(F("Default_TTL")); break;
	case 0x1: 
		Serial.println(F("Shield")); break;
	case 0x2:
		Serial.println(F("Custom")); break;
	default: 
		Serial.println(F("Unknown")); break;
	}
	delay(300); //let serial catch its breath
	Serial.print(F("Trigger Pin in: "));
	format_pin_print(TRIGGER_PINS[0]);
	Serial.print(F("Trigger Ambient Type: "));
	if(_trigger_type == 0)
		Serial.println(F("Low (PIR or + trigger)"));
	else
		Serial.println(F("Hi (to gnd trigger)"));
	Serial.print(F("Trigger Pin Out: "));
	format_pin_print(TRIGGER_PINS[1]);
	Serial.print(F("Media Serial Pin: "));
	format_pin_print(TRIGGER_PINS[2]);

	delay(300); //let serial catch its breath

	_Media.ReportConfig();

	Serial.print(F("Timing Offset ms: "));
	Serial.println(TIMING_OFFSETS[_timingOffsetType],3);

	Serial.print(F("TTL PINS:  "));
	for (int i = 0; i < TTL_COUNT; i++)
	{
	    Serial.print(TTL_PINS[i]); 
		if(i<TTL_COUNT - 1)
			Serial.print(","); 
		else
			Serial.println();
	}
	Serial.print(F("TTL TYPES: "));
	for (int i = 0; i < TTL_COUNT; i++)
	{
	    Serial.print(TTL_TYPES[i]); 
		if(i<TTL_COUNT - 1)
			Serial.print(","); 
		else
			Serial.println();
	}

}

void format_pin_print(uint8_t inpin)
{
	if(inpin >= A0)
	{
		Serial.print("A"); 
		Serial.println((inpin - A0));
	}
	else
		Serial.println(inpin);
}

void loop()
{
	check_serial();
	if(IS_HOT)
	{
		if (check_trigger())
			play_sequence(); 
	}
}

void play_sequence()
{
	if (hi_sample == 0) return; //nothing recorded
	digitalWrite(TRIGGER_PINS[2], LOW); //trigger output pin for daisy chain
	Serial.println(F("Playing sequence..."));
    int play_sample = 0;
    _Media.PlayScare(); 
	delay(100); // .1 sec should be sufficient
	digitalWrite(TRIGGER_PINS[2], HIGH); //put back
	volatile unsigned int sample_ticker = 0;
	unsigned long start_time = millis();
	unsigned long delay_total = 0;
	float MPF_ACTUAL = MILLIS_PER_FRAME + TIMING_OFFSETS[_timingOffsetType];
	while (sample_ticker < (hi_sample * 2))
	{
		OutputTTL(samples[sample_ticker],0);
		sample_ticker++;
		delay_total += samples[sample_ticker];
		while(millis() <= (start_time + (MPF_ACTUAL * delay_total))) {}
		sample_ticker++;
	}

	if(RESET_DELAY_SECS > 0)
	{
		Serial.print(F("Waiting delay secs: "));
		Serial.println(RESET_DELAY_SECS);
	}
	set_ambient_out();
	_Media.PlayAmbient();
	blink_times(RESET_DELAY_SECS); //this uses 1 second per blink
	delay(100);
	Serial.println(F("Sequence complete, Ready"));
	clear_rx_buffer(); //this will wipe any crap that may have been transmitted in the time we were playing
}
void set_ambient_out()
{
    //set all outputs to default
	byte n = 0;
    OutputTTL(n,0);
}

//writes the bits of the current byte to our 8 TTL pins.
//It will use the usage booleans to know whether to invert
void OutputTTL(byte valIn, int offset)
{
	bool val = 0;
	for(int x = 0;x < TTL_COUNT; x++)
    {
		val = bitRead(valIn,x + offset) ^ TTL_TYPES[x];
		digitalWrite(TTL_PINS[x],val);
    }
}
void blink_alert()
{
        for (byte i = 0; i < 15; i++)
        { //blink LED 4 times when new address is received.
		  digitalWrite(ARDUINO_LED, (i & 1));
          //i & 1 will bitwise-and to 1 if i is odd, 0 if even.
          delay(100);
        }
}
void blink_once()
{
	digitalWrite(ARDUINO_LED, HIGH);
    delay(500);
	digitalWrite(ARDUINO_LED, LOW);
    delay(500);
}
void blink_times(int times)
{
	blink_times(times,false);
}
void blink_times(int times, bool with_listen)
{
	for(int i = 0; i < times; i++)
	{
		Serial.print(F("."));
		digitalWrite(ARDUINO_LED, HIGH);
		if(with_listen)check_serial(); //decided to check serial while we are blinking, as to not be "deaf"
		delay(500);
		digitalWrite(ARDUINO_LED, LOW);
		delay(500);
		if(with_listen)check_serial();
	}
	Serial.println();
}

void check_serial()
{
  //return;
  if (Serial.available() > 0) 
  {
     byte b = Serial.read();
     if (b == '@')  // command header.
     {
        delay(SERIAL_WAIT);
        if (Serial.available() > 0)
        {
          b = Serial.read();
		  delay(SERIAL_WAIT);
          switch (b)
          {
			case 'V':
			    //return version
				for (int i = 0; i < 3; i++)
				{
				  if(i < 2)
				  {
					  Serial.print(stamp_buff[i]);
					  Serial.print(".");
				  }
				  else
					  Serial.println(stamp_buff[i]);
				}
				break;
			case 'H':
			    //go hot
				IS_HOT = true;
				Serial.println(F("Ready"));
				break;
			case 'C':
			    //go cold
				IS_HOT = false;
				Serial.println(F("Standby..."));
				break;
			case 'D':
			    //download eeprom contents back to Serial
				tx_memory();
				break;
			case 'F':
			    //download just the config eeprom contents back to Serial
				tx_config();
				break;
			case 'S':
				rx_seq_data(); //here comes an Upload of seq data
				break;
		    case 'U':
				rx_controller_config(); //here comes an Upload of seq data
				break;
			case 'P': //ping back
				report_config();
				break;
            case 'T': //trigger test
				play_sequence();
				break;
            case 'M': //manual TTL state command
				b = Serial.read(); //get the state byte
				OutputTTL(b,0);
				break;  
			case 'O':
			    //is the stamp OK?
				if(stampok)
					Serial.println(F("OK"));
				else
					Serial.println(F("NO"));
				break;
			default:
				Serial.print(F("unk char:"));
				Serial.print(b);
				clear_rx_buffer();
				break;
          }
        }
     }
  }
}

//read 4 bytes from the Serial buffer and return as a 32bit integer
unsigned long SerialInt32()
{
	byte b[4];
	for(int i = 0; i < 4; i++)
	{
		while(Serial.available() == 0){}
		b[i] = Serial.read();
	}
	return (unsigned long)(((unsigned long)b[3] << 24) | ((unsigned long)b[2] << 16) | ((unsigned long)b[1] << 8) | (b[0]));
}
//read 2 bytes from the Serial buffer and return as a 16bit integer
unsigned long SerialInt16()
{
	byte b[2];
	for(int i = 0; i < 2; i++)
	{
		while(Serial.available() == 0){}
		b[i] = Serial.read();
	}
	return (unsigned int)(((unsigned long)b[1] << 8) | (b[0]));
}
void rx_seq_data()
{
	while(Serial.available() == 0){}
	unsigned int howmany = SerialInt16(); //this how many state pair bytes we are getting to read

	for(int i = 0; i < howmany && i < 1000; i++)
	{
		int waited = 0;
		while(Serial.available() == 0)
		{
			waited++;
			if(waited > 1000)
			{
				Serial.print(F("Stuck on: "));
				Serial.println(i);
				Serial.println("Upload terminated, please retry");
				return;
			}
			delay(1);
		}
		byte b = Serial.read();
   		EEPROM.write(i,b);
	}
	Serial.print(F("received "));
	Serial.print(howmany / 2);
	Serial.println(F(" frames"));
	buffer_samples(); //transfer into working buffer
	hi_sample = howmany / 2;
	EEPROM.write(HI_SAMPLE_UB, highByte(hi_sample));//writes the first byte of hi_sample
    EEPROM.write(HI_SAMPLE_LB, lowByte(hi_sample));//writes the second byte of hi_sample
	delay(300);
	float duration = 0;
	for(int i = 0; i < (hi_sample * 2); i+=2)
		duration += (float)(samples[i + 1]) * MILLIS_PER_FRAME;
	Serial.print(F("Seq Len Secs: "));
	Serial.println(duration / 1000.0);
	Serial.println(F("Saved, Ready"));
}

void rx_controller_config()
{
	while(Serial.available() == 0){}
	unsigned int howmany = SerialInt16(); //how many bytes we are getting, should be 9
	if(howmany != MEM_SLOTS)
	{
		Serial.print(F("Unknown config length passed: "));
		Serial.println(howmany);
		clear_rx_buffer();
		return;
	}

	for(int i = 0; i < howmany; i++)
	{
		int waited = 0;
		while(Serial.available() == 0)
		{
			waited++;
			if(waited > 1000)
			{
				Serial.print(F("Stuck on: "));
				Serial.println(i);
				Serial.println("Upload terminated, please retry");
				return;
			}
			delay(1);
		}
		byte b = Serial.read();
   		EEPROM.write(i + PIN_MAP_SLOT,b); //PIN_MAP_SLOT is the first byte, then they are in order
	}

	Serial.print(F("Received "));
	Serial.print(howmany);
	Serial.println(F(" config bytes"));
	Serial.println(F("Please reconnect"));
	if(!stampok)
	{
		hi_sample = 0x0;
		EEPROM.write(HI_SAMPLE_UB, highByte(hi_sample));//writes the first byte of hi_sample
		EEPROM.write(HI_SAMPLE_LB, lowByte(hi_sample));//writes the second byte of hi_sample
	}
	for(int i = 0; i < 5; i++)
		EEPROM.write(STAMP_OFFSET + i,stamp_buff[i]);
	stampok = true; //now that we have written it
	read_config();
	set_ambient_out();
    //after we do this we will disconnect to force reboot
}

void tx_memory()
{
	for(int i = 0; i < STAMP_OFFSET; i++)
	{
		byte b = EEPROM.read(i);
		Serial.write(b);
		delay(1);
	}
}
void tx_config()
{
	for(int i = PIN_MAP_SLOT; i < (PIN_MAP_SLOT + MEM_SLOTS); i++)
	{
		byte b = EEPROM.read(i);
		Serial.write(b);
		delay(1);
	}
}
void clear_rx_buffer()
{
	while(Serial.available() != 0)
	 byte b = Serial.read();
}

