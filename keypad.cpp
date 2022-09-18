#include "device.h"
#include <bitset>
#include <iostream>
#include <wiringPi.h>
#include <wiringPiSPI.h>

using namespace std;

//extern GPIO Gpio;
//extern vector<stButtons> Buttons;

//KeyChip commands
char kcRESET_SPI[2] = { 0x7A, 0x7A };
char kcREGISTER[2] = { 0x7D, 0x00 };
char kcREAD[2] = { 0x7F, 0x7F };
char kcWRITE[2] = { 0x7E, 0x7E };
char kcREG_SENS[4] = { 0x7D, 0x03, 0x7F, 0x7F };
char kcREG_LED_GET[4] = { 0x7D, 0x74, 0x7F, 0x7F };
char kcREG_LED_SET[4] = { 0x7D, 0x74, 0x7E, 0x7F };

/*keychip settings*/
char kcREG_RESET[4]			= { 0x7D, 0x00, 0x7E, 0x00 };	//ќбнуление главного регистра
char kcLED_MODE[4]			= { 0x7D, 0x71, 0x7E, 0xFF };	 //светодиоды в режим Push-Pull
char kcLED_BEHAVIOR_0_3[4]	= { 0x7D, 0x81, 0x7E, 0x00 };	//управление поведением светодиодов
char kcLED_BEHAVIOR_4_7[4]	= { 0x7D, 0x82, 0x7E, 0x00 };	//управление поведением светодиодов
char kcLED_POL[4]			= { 0x7D, 0x73, 0x7E, 0xFF };	//пол€рность работы светодиодов
char kcKEY_M_PRESS[4]		= { 0x7D, 0x23, 0x7E, 0x03 };	//таймер антидребезга (08-315мс)
char kcSAMPLE[4] 			= { 0x7D, 0x24, 0x7E, 0x01 };	/*врем€ и кол-во выборок*/
char kcAUTOCOLIBR[4]		= { 0x7D, 0x2F, 0x7E, 0x9A };	// откл. автокалибровку
char kcAUTOSET[4]			= { 0x7D, 0x26, 0x7E, 0xFF };	//јктиваци€ автокалибровки принудительно
char kcKEY_PER[4]			= { 0x7D, 0x28, 0x7E, 0xFF };	//одиночное/многокр прерывание
char kcKEY_DETOUCH[4]		= { 0x7D, 0x44, 0x7E, 0x41 };	//откл. прервание на отпускание
char kcKEY_SENS[4]			= { 0x7D, 0x1F, 0x7E, 0x2F };	//чувствительность кнопок
char kcMTP[4]				= { 0x7D, 0x2A, 0x7E, 0x00 };	//MTP вкл/выкл.
 
bool keypad_t::lst_u2s_access(device_event_t* event, access_t access)
{
	bool ret;

	unique_lock<mutex> locker(m_mx_u2s);
	switch (access)
	{
	case access_t::WRITE:
		m_lst_u2s.push_back(*event);
		ret = true;
		break;
	case access_t::READ:
		if (m_lst_u2s.size() > 0) {
			*event = m_lst_u2s.front();
			m_lst_u2s.pop_front();
			ret = true;
		}
		else
			ret = false;
		break;
	case access_t::DELETE:
		m_lst_u2s.clear();
		break;
	}

	return ret;
}
/***************************************************/

bool keypad_t::lst_flash_access(flash_t* flash, access_t access)
{
	bool ret;

	unique_lock<mutex> locker(m_mx_flash);
	switch (access)
	{
	case access_t::WRITE:
		m_lst_flash.push_back(*flash);
		ret = true;
		break;
	case access_t::READ_NEXT:
		if (m_cur <= m_lst_flash.size() - 1
			&& m_lst_flash.size() > 0) {
			*flash = m_lst_flash[m_cur];
			m_cur++;
			ret = true;
		}
		else {
			m_cur = 0;
			ret = false;
		}
		break;
	case access_t::UPDATE:
		for (unsigned int i = 0; i < m_lst_flash.size(); i++)
			if (m_lst_flash[i].gpio == flash->gpio)
				m_lst_flash[i] = *flash;
		break;
	case access_t::DELETE:
		if (flash == nullptr)
			m_lst_flash.clear();
		else
			for (unsigned int i = 0; i < m_lst_flash.size(); i++)
				if (m_lst_flash[i].gpio == flash->gpio)
					m_lst_flash.erase(m_lst_flash.begin() + i);
		break;
	}

	return ret;
}
//----------------------------------------------------------------------

bool keypad_t::open()
{
		fill_butt_coord();
			
		/*управление чипами клавиатуры*/
		pinMode(KPlocal_CS, OUTPUT);
		pinMode(KPext_CS, OUTPUT);
		pinMode(KPselect_0, OUTPUT);
		pinMode(KPselect_1, OUTPUT);
		pinMode(KPselect_2, OUTPUT);
		pinMode(KPlocal_IRQ, INPUT);
		pinMode(KPext_IRQ, INPUT);
		

		/*SPI open*/
		if (wiringPiSPISetup(SPI_CHANNEL, 100000) < 0)
		{
			cout << "SPI setup ERROR" << endl;
			return false;
		}

		/*конфигураци€ чипов*/
		for (unsigned i = 0; i < KEY_CHIPs; i++) {
			chip_select(i);
			msleep(10);
			spi_write(kcRESET_SPI, 2);
			spi_write(kcLED_MODE, 4);
			spi_write(kcLED_BEHAVIOR_0_3, 4);
			spi_write(kcLED_BEHAVIOR_4_7, 4);
			spi_write(kcLED_POL, 4);
			spi_write(kcKEY_SENS, 4);
			spi_write(kcKEY_PER, 4);
			spi_write(kcKEY_M_PRESS, 4);
			spi_write(kcKEY_DETOUCH, 4);
			spi_write(kcREG_RESET, 4);
			spi_write(kcAUTOSET, 4);
			spi_write(kcMTP, 4);
			spi_write(kcSAMPLE, 4);
		}

		cout << "KeyPad init OK" << endl;
		return true;
}
//------------------------------------------------------

void keypad_t::fill_butt_coord()
{
	try
	{
		/*Key map, 7 чипов по 8 кнопок в телефоне
		и 7 в расширении*/
		int KPmap[KEY_CHIPs][8] = {
			{ 0,	1,	2,	3,	4,	5,	6,	7 },
			{ 10,	11,	12,	13,	14,	15,	16,	17 },
			{ 20,	21,	22,	23,	24,	25,	26,	27 },
			{ 30,	31,	32,	33,	34,	35,	36,	37 },
			{ 40,	41,	42,	43,	44,	45,	46,	47 },
			{ 56,	57,	58,	59,	49,	39,	38,	48 },
			{ 8,	18,	28,	29,	19,	-1,	-1,	-1 },
			{ 60,	61,	62,	63,	64,	65,	66,	67 },
			{ 70,	71,	72,	73,	74,	75,	76,	77 },
			{ 80,	81,	82,	83,	84,	85,	86,	87 },
			{ 90,	91,	92,	93,	94,	95,	96,	97 },
			{ 100,	101,102,103,104,105,106,107},
			{ 68,	69,	78,	79,	88,	89,	98,	99 },
			{ 108,109,-1,	-1,	-1, -1, -1, -1 } };
		for (int i = 0; i < KEY_CHIPs; i++)
			for (int a = 0; a < 8; a++) {
				m_keychip[i][a].key_map = KPmap[i][a];
				m_buttons->at(KPmap[i][a]).Chip = i;
				m_buttons->at(KPmap[i][a]).Pin = a;
			}
	}
	catch (const std::exception&)
	{

	}
}
//-----------------------------------------------------

void keypad_t::chip_select(int chip_num)
{
	if (chip_num < KEY_CHIPs / 2) {
		/*активный 0*/
		digitalWrite(KPext_CS, HIGH);
		digitalWrite(KPlocal_CS, LOW);
		/*активна€ 1*/
		digitalWrite(KPselect_0, BitTst(chip_num, 0));
		digitalWrite(KPselect_1, BitTst(chip_num, 1));
		digitalWrite(KPselect_2, BitTst(chip_num, 2));
	}
	else {
		digitalWrite(KPlocal_CS, HIGH);
		digitalWrite(KPext_CS, LOW);
		chip_num -= 7;
		digitalWrite(KPselect_0, BitTst(chip_num, 0));
		digitalWrite(KPselect_1, BitTst(chip_num, 1));
		digitalWrite(KPselect_2, BitTst(chip_num, 2));
	}
}
//--------------------------------------------------------

void keypad_t::read()
{
	device_event_t event;
	event.ev = dcButton;
	bool ret = false;
	for (int i = 0; i < KEY_CHIPs / (m_ext ? 1 : 2); i++)
	{
		/*проверим прерывание*/
		if ((i < KEY_CHIPs / 2 && digitalRead(KPlocal_IRQ) == LOW)
			|| (i >= KEY_CHIPs / 2 && digitalRead(KPext_IRQ) == LOW))
			ret = key_read(i);

		/*дл€ всех кнопок не зависимо от прерываний*/
		for (int k = 0; k < 8; k++) {
			/*только дл€ нажатых кнопок*/
			if (m_keychip[i][k].status != HIGH) {
				/*усли не было прерываний долше чем KEY_UP_TIMER
				кнопку считаем отпущеной*/
				if (get_time_ms() - m_keychip[i][k].tm > KEY_UP_TIMER) {
					m_keychip[i][k].status = HIGH;
					///*дл€ симплекса ќрех отправим событие UP*/
					event.index = m_keychip[i][k].key_map;
					event.flag = false;
					add_event(&event);
				}
			}
		}
		if (ret)
			break;/*обрабатываем только одну кнопку из всей клавиатуры*/
	}
}
//------------------------------------------------------

bool keypad_t::key_read(int chip)
{
	device_event_t event;
	event.ev = dcButton;
	char inBuf[4] = { 0,0,0,0 };

	unique_lock<mutex> locker(m_mx_chip);
	/*chip select*/
	chip_select(chip);
	/*сбрасываем прерывание REG 0*/
	spi_write(kcREG_RESET, 4);
	/*читаем кнопки*/
	spi_xfer(kcREG_SENS, inBuf, 4);
	///*chip select*/

	if (inBuf[3] > 0) {
		for (int a = 0; a < 8; a++) {
			if (0x01 & (inBuf[3] >> a)) {
				event.index = m_keychip[chip][a].key_map;
				m_keychip[chip][a].tm = get_time_ms();
				switch (m_keychip[chip][a].status)
				{
				case HIGH:/*взводим кнопку*/
					m_keychip[chip][a].status = LOW;
					break;
				case LOW:/*клик*/
					event.flag = true;
					add_event(&event);
					m_keychip[chip][a].status = LOW_LONG;
					break;
				case LOW_LONG:/*кнопку удерживают*/
					break;
				}
				return true; /*только по одной кнопке за цикл*/
			}
		}
	}
	return false;
}
//------------------------------------------------------

vector<size_t> keypad_t::lst_key_down()
{
	vector<uint> v_key;
	for (int i = 0; i < KEY_CHIPs; i++)
		for (int a = 0; a < 8; a++)
			if (m_keychip[i][a].status != HIGH)
				v_key.push_back(i);

	return v_key;
}
//--------------------------------------------------------------------------------

void keypad_t::flashing()
{
		flash_t flash;

		while (get_flash(&flash)) {
			if (flash.status == OFF) {
				if (get_time_ms() - get_time_ms(flash.tm_off)
					>= flash.interval_off)
				{
					led(flash.gpio, true);
					flash.status = ON;
					flash.tm_on = std::chrono::system_clock::now();
				}
			}
			else if (flash.status == ON) {
				if (get_time_ms() - get_time_ms(flash.tm_on)
					>= flash.interval_on)
				{
					led(flash.gpio, false);
					flash.status = OFF;
					flash.tm_off = std::chrono::system_clock::now();
				}
			}
			upd_flash(&flash);
		}
}
//--------------------------------------------------------------------------

void keypad_t::led(int index, bool status)
{
	char inBuf[4] = { 0, 0, 0, 0 };

	if (m_buttons->at(index).Pin == -1)
		return;

	unique_lock<mutex> locker(m_mx_chip);
	chip_select(m_buttons->at(index).Chip);
	spi_xfer(kcREG_LED_GET, inBuf, sizeof(inBuf));
	if (BitTst(inBuf[3], m_buttons->at(index).Pin) != status) {
		switch (status) {
		case OFF:
			kcREG_LED_SET[3] = inBuf[3] & ~(1 << m_buttons->at(index).Pin);
			break;
		case ON:/*установка в "1" - светодиод светитс€*/
			kcREG_LED_SET[3] = inBuf[3] | (1 << m_buttons->at(index).Pin);
			break;
		case FLASH:
			break;
		}
		spi_write(kcREG_LED_SET, sizeof(kcREG_LED_SET));
	}
	return;
}  
//-------------------------------------------------------

void keypad_t::spi_write(char* buf, int count)
{
	unsigned char buffer[count];
	for (int i = 0; i < count; i++)
		buffer[i] = buf[i];
	wiringPiSPIDataRW(SPI_CHANNEL, buffer, count);
}

void keypad_t::spi_xfer(char* buf_w, char* buf_r, int count)
{
	unsigned char buffer[count];
	for (int i = 0; i < count; i++)
		buffer[i] = buf_w[i];
	wiringPiSPIDataRW(SPI_CHANNEL, buffer, count);
	for (int i = 0; i < count; i++)
		buf_r[i] = buffer[i];
}
