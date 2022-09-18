#pragma once
#include <string>
#include <vector>

/*заголовки классов*/
class player_t;
class account_t;
class call_t;
class sip_t;
class buddy_t;

//----------Functions for buttons
enum class func_t
{
	NoFunction = 0,
	d1,
	d2,
	d3,
	d4,
	d5,
	d6,
	d7,
	d8,
	d9,
	d0,
	OK = 11,
	SpeedDial = 12,
	SpeeckerPhone = 13,
	Mic = 14,
	Call_Horn = 15,
	Call_Speecker = 16,
	Call_Mic = 17,
	Hold = 18,
	Conf = 19,
	ConfPort = 22,
	DTMF = 23,
	Mixer1 = 24,
	Mixer2 = 25,
	Mixer3 = 26,
	Mixer4 = 27,
	MixerAll = 28,
	Simplex_rec_tran = 29,
	xFer = 30,
	ConfSpkr = 31,
	VMsg_1 = 32,
	VMsg_2 = 33,
	VMsg_3 = 34,
	VMsg_4 = 35,
	VMsg_5 = 36,
	VoiceIP = 38,
	Opex_0 = 40,
	Opex_1 = 41,
	Opex_2 = 42,
	Opex_3 = 43,
	Opex_4 = 44,
	Opex_5 = 45,
	Opex_6 = 46
};

enum class led_button_status_t
{
	LED_OFF = 0,
	LED_ON,
	LED_FLASH,
	SUBS_BUSY,
	SUBS_CALL,
};

//Buttons
struct button_t
{
	func_t function;
	std::string server;
	std::string phone;
	int buttonID;
	bool mixer;
	led_button_status_t status;
	int8_t Chip;
	int8_t Pin;
	std::vector<call_t*> calls;
};
