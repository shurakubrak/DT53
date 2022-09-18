#include "device.h"
#include <iostream>
#include <wiringPi.h>

using namespace std;

bool gpio_t::lst_flash_access(flash_t* flash, access_t access)
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
		if (cur <= (int)m_lst_flash.size() - 1
			&& m_lst_flash.size() > 0) {
			*flash = m_lst_flash[cur];
			cur++;
			ret = true;
		}
		else {
			cur = 0;
			ret = false;
		}
		break;
	case access_t::UPDATE:
		for (unsigned int i = 0; i < m_lst_flash.size(); i++)
			if (m_lst_flash[i].gpio == flash->gpio)
				m_lst_flash[i] = *flash;
		break;
	case access_t::DELETE:
		for (unsigned int i = 0; i < m_lst_flash.size(); i++)
			if (m_lst_flash[i].gpio == flash->gpio)
				m_lst_flash.erase(m_lst_flash.begin() + i);
		break;
	}
	return ret;
}

bool gpio_t::open()
{
	//	OUTPUT
	pinMode(AMP_MUTE, OUTPUT);
	pinMode(DSP_RESET, OUTPUT);
	pinMode(GLedRed, OUTPUT);
	pinMode(GLedGreen, OUTPUT);

	digitalWrite(AMP_MUTE, LOW);
	digitalWrite(DSP_RESET, HIGH);
	
	cout << "GPIO init OK" << endl;
	return true;
}
/***************************************************/

void gpio_t::set(int gp, bool state)
{
	digitalWrite(gp, state);
}
/****************************************************/

int gpio_t::get(int gp)
{
	return digitalRead(gp);
}
/******************************************************/

void gpio_t::flashing()
{
	flash_t flh;

	while (get_flash(&flh)) {
		if (flh.status == OFF) {
			if (get_time_ms() - get_time_ms(flh.tm_off)
				>= flh.interval_off)
			{
				digitalWrite(flh.gpio, HIGH);
				flh.status = ON;
				flh.tm_on = std::chrono::system_clock::now();
			}
		}
		else if (flh.status == ON) {
			if (get_time_ms() - get_time_ms(flh.tm_on)
				>= flh.interval_on)
			{
				digitalWrite(flh.gpio, LOW);
				flh.status = OFF;
				flh.tm_off = std::chrono::system_clock::now();
			}
		}
		upd_flash(&flh);
	}
}
