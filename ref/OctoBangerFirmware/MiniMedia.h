/*
 *  MiniMedia.h
 *
 *  A tiny class to hold properties and functions for audio/video control output.

 */

#ifndef _MINIMEDIA_H
#define _MINIMEDIA_H

#include <Arduino.h>
#include <avr/pgmspace.h>

#define MEDIA_TYPE_NONE 0X0
#define MEDIA_TYPE_CATALEX_SCARE 0X1
#define MEDIA_TYPE_CATALEX_AMB_SCARE 0X2
#define MEDIA_TYPE_SPRITE 0X03

/************catalex mp3 board constants**************************/
static int8_t Send_buf[8] = {0} ;

#define CMD_PLAY_SINGLE 0X03
#define CMD_SET_VOLUME 0X06
#define CMD_PLAY_LOOP 0X08
#define CMD_SEL_DEV 0X09
#define DEV_TF 0X02
#define CMD_STOP_PLAY 0X16
//#define CMD_PLAY_W_VOL 0X22
/*********************************************************************/

class MiniMedia
{

  public:
    MiniMedia();
    void Init(uint8_t pin, uint8_t vol, uint8_t media_type, uint8_t media_delay); 
	void PlayAmbient();
	void PlayScare();
	void Stop();
	void SetVolume(uint8_t vol); 
	void ReportConfig();
  private:
  uint8_t _transmitBitMask;
  uint8_t _volume;
  uint8_t _mediaType;
  uint8_t _mediaDelayHundreds;

  bool _init_first;
  
  volatile uint8_t *_transmitPortRegister;
  volatile uint8_t _serial_pin;
  uint16_t _tx_delay;
  // private methods
  void tx_pin_write(uint8_t pin_state);
  void write(uint8_t byte);
  // private static method for timing
  static inline void tunedDelay(uint16_t delay);
  void sendCommand(int8_t command, int16_t dat);
};


MiniMedia::MiniMedia()
{
	_init_first = false;
	_mediaType = 0x0;

}

void MiniMedia::Init(uint8_t pin, uint8_t vol, uint8_t media_type, uint8_t media_delay)
{
	_mediaType = media_type;
	_tx_delay = 238; //9600
 //_tx_delay = 239; //9600
	if(_mediaType == MEDIA_TYPE_NONE) return;
	if(!_init_first)
	{
		_serial_pin = pin;
		pinMode(_serial_pin, OUTPUT);
		digitalWrite(_serial_pin, HIGH);
		_transmitBitMask = digitalPinToBitMask(_serial_pin);
		uint8_t port = digitalPinToPort(_serial_pin);
		_transmitPortRegister = portOutputRegister(port);
		digitalWrite(_serial_pin,LOW);
		delay(500);
		if(_mediaType == MEDIA_TYPE_CATALEX_SCARE || _mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
		{
			sendCommand(CMD_SEL_DEV, DEV_TF);  //select the TF card  
			delay(500);
		}
		if(_mediaType == MEDIA_TYPE_SPRITE)
		{
			write(0x00);  //hello
			delay(500);
		}
	}
	_volume = 15; //default in case a bogus value is passed
	_mediaDelayHundreds = media_delay;
	if(_mediaType == MEDIA_TYPE_CATALEX_SCARE || _mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
	{
		if (vol != _volume && (vol > 0 && vol < 31))
			_volume = vol;
		sendCommand(CMD_SET_VOLUME, _volume); 
		delay(200);
	}
	_init_first = true;
}

void MiniMedia::ReportConfig()
{
	Serial.print(F("Media Type: "));
	switch(_mediaType)
	{
		case MEDIA_TYPE_NONE: Serial.println(F("None")); return;
		case MEDIA_TYPE_CATALEX_SCARE: Serial.println(F("Catalex Mp3 Scare Only")); break;
		case MEDIA_TYPE_CATALEX_AMB_SCARE: Serial.println(F("Catalex Mp3 Ambient + Scare")); break;
		case MEDIA_TYPE_SPRITE: Serial.println(F("Sprite Video")); break;
        default: Serial.println(F("Unknown!")); return;
	}
	Serial.print(F("Ambient Volume: "));
	Serial.println(_volume);
	if(_mediaDelayHundreds != 0)
	{
		Serial.print(F("Media Delay: "));
		Serial.println(_mediaDelayHundreds);
	}
}

void MiniMedia::SetVolume(uint8_t vol)
{
	if ((_mediaType == MEDIA_TYPE_CATALEX_SCARE || _mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
		&& vol != _volume && (vol > 0 && vol < 31))
	{
		_volume = vol;
		sendCommand(CMD_SET_VOLUME, _volume); 
		delay(100);
	}
}
void MiniMedia::PlayAmbient()
{
	if (_mediaType == MEDIA_TYPE_CATALEX_SCARE || _mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
	{
		Stop(); //added this stop() here because the Catalex was flakily looping the scare audio
		delay(20); //adding the stop and delay seems to have fixed it. MJN 20170115
		sendCommand(CMD_SET_VOLUME, _volume); //go back to ambient volume
		delay(50);
		if(_mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
			sendCommand(CMD_PLAY_LOOP, 1); //CMD_PLAY_W_INDEX loops
	}
}
void MiniMedia::PlayScare()
{
	int16_t bdata;

	if (_mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
	{
    sendCommand(CMD_SET_VOLUME, _volume); 
    delay(50);		//sendCommand(CMD_STOP_PLAY,0); //added this stop() here because the Catalex was flakily looping the scare audio
    //sendCommand(CMD_PLAY_LOOP, 2);
		//delay(30); //adding the stop and delay seems to have fixed it. MJN 20170115
		sendCommand(CMD_PLAY_SINGLE, 2);
		//20210530 MJN, reverting to old command above
		//bdata = (unsigned int)(((unsigned long)0x30 << 8) | 0x2);
		//sendCommand(CMD_PLAY_W_VOL, bdata);
   delay(10);
	}
	if (_mediaType == MEDIA_TYPE_CATALEX_SCARE)
	{
		//Stop(); //added this stop() here because the Catalex was flakily looping the scare audio
		//delay(20); //adding the stop and delay seems to have fixed it. MJN 20170115
		sendCommand(CMD_PLAY_SINGLE, 1); //we only have a scare file, so it is file 1
		//20210530 MJN, reverting to old command above
		//bdata = (unsigned int)(((unsigned long)0x30 << 8) | 0x1);
		//sendCommand(CMD_PLAY_W_VOL, bdata);
	}
	if (_mediaType == MEDIA_TYPE_SPRITE)
	{
		write(0x01);
	}
	for (int i = 0; i < _mediaDelayHundreds; i++)
		delay(10);
}
void MiniMedia::Stop()
{
	if (_mediaType == MEDIA_TYPE_CATALEX_SCARE || _mediaType == MEDIA_TYPE_CATALEX_AMB_SCARE)
		sendCommand(CMD_STOP_PLAY,0);
}
void MiniMedia::sendCommand(int8_t command, int16_t dat)
{
	if (_mediaType == MEDIA_TYPE_SPRITE) return;

	delay(20);
	Send_buf[0] = 0x7e; //
	Send_buf[1] = 0xff; //
	Send_buf[2] = 0x06; //
	Send_buf[3] = command; //
	Send_buf[4] = 0x00;//
	Send_buf[5] = (int8_t)(dat >> 8);//datah
	Send_buf[6] = (int8_t)(dat); //datal
	Send_buf[7] = 0xef; //
	for(uint8_t i=0; i<8; i++)//
	{
	write(Send_buf[i]) ;
	}
}
/* static */ 
inline void MiniMedia::tunedDelay(uint16_t delay) { 
  uint8_t tmp=0;

  asm volatile("sbiw    %0, 0x01 \n\t"
    "ldi %1, 0xFF \n\t"
    "cpi %A0, 0xFF \n\t"
    "cpc %B0, %1 \n\t"
    "brne .-10 \n\t"
    : "+r" (delay), "+a" (tmp)
    : "0" (delay)
    );
}


void MiniMedia::tx_pin_write(uint8_t pin_state)
{
  if (pin_state == LOW)
    *_transmitPortRegister &= ~_transmitBitMask;
  else
    *_transmitPortRegister |= _transmitBitMask;
}


void MiniMedia::write(uint8_t b)
{
  if (_tx_delay == 0) return;

  uint8_t oldSREG = SREG;
  cli();  // turn off interrupts for a clean txmit

  // Write the start bit
  tx_pin_write(LOW);
  tunedDelay(_tx_delay);

	for (byte mask = 0x01; mask; mask <<= 1)
	{
		if (b & mask) // choose bit
		tx_pin_write(HIGH); // send 1
		else
		tx_pin_write(LOW); // send 0
    
		tunedDelay(_tx_delay);
	}

	tx_pin_write(HIGH); // restore pin to natural state

  SREG = oldSREG; // turn interrupts back on
  tunedDelay(_tx_delay);

}



#endif // _MiniMedia_H
