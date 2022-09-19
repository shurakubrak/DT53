#include "device.h"
#include <wiringPi.h>
#include <iostream>
#include <unistd.h>

using namespace std;

bool device_t::dsp_load()
{
	// Reset DSP soft
	digitalWrite(DSP_RESET, LOW);
	msleep(20);
	digitalWrite(DSP_RESET, HIGH);
	ssleep(1);

	if (!dsp_file_load())
		return false;
	ssleep(1);

	close(m_uart->m_fd);
	if (!m_uart->open_uart())
		return false;
	return true;
}
//---------------------------------------------------------------

bool device_t::dsp_file_load()
{
	FILE* fd = NULL;
	char f_data[BUFF_SIZE];
	int count = 0;

	fd = fopen((m_path + "aer.bin").c_str(), "r");
	if (fd == NULL)
		return false;

	cout << "Load DSP start" << endl;
	count = fread(f_data, 1, BUFF_SIZE, fd);
	if (count == 0)
		return false;
	fclose(fd);
	m_uart->write_soft(f_data, count);
	cout << "DSP loading OK" << endl;
	return true;
}
//---------------------------------------------------------------------------------

bool device_t::device_open(keypad_t* kp, gpio_t* gpio, uart_t* uart)
{
	wiringPiSetup();

	if (kp == nullptr || gpio == nullptr || uart == nullptr)
		return false;
	m_gpio = gpio;
	m_kp = kp;
	if (!kp->open())
		return false;
	if (!gpio->open())
		return false;

	gpio->set(DSP_RESET, HIGH);

	m_uart = uart;
	if (!m_uart->open_uart())
		return false;

	cout << "Device init (UART, WiringPI) OK" << endl;
	return true;
}
//--------------------------------------------------------------

void device_t::device_write(dev_cmd_t command, int index, gpio_t* gpio, keypad_t* kp)
{
	flash_t flash;
	try
	{
		switch (command) {
		case dev_cmd_t::GENLEDRED_OFF:
			if (gpio == nullptr) return;
			gpio->del_flash(GLedRed);
			gpio->set(GLedRed, LOW);
			break;
		case dev_cmd_t::GENLEDRED_ON:
			if (gpio == nullptr) return;
			gpio->del_flash(GLedRed);
			gpio->set(GLedRed, HIGH);
			break;
		case dev_cmd_t::GENLEDRED_FLASH:
			if (gpio == nullptr) return;
			flash.gpio = GLedRed;
			flash.interval_off = INT_GL_RED_OFF;
			flash.interval_on = INT_GL_RED_ON;
			gpio->add_flash(&flash);
			break;
		case dev_cmd_t::GENLEDGREEN_OFF:
			if (gpio == nullptr) return;
			gpio->del_flash(GLedGreen);
			gpio->set(GLedGreen, LOW);
			break;
		case dev_cmd_t::GENLEDGREEN_ON:
			if (gpio == nullptr) return;
			gpio->del_flash(GLedGreen);
			gpio->set(GLedGreen, HIGH);
			break;
		case dev_cmd_t::GENLEDGREEN_FLASH:
			if (gpio == nullptr) return;
			flash.gpio = GLedGreen;
			flash.interval_off = INT_GL_GREEN_OFF;
			flash.interval_on = INT_GL_GREEN_ON;
			gpio->add_flash(&flash);
			break;

		case dev_cmd_t::KPLEDBUTTON_OFF:
			if (kp == nullptr) return;
			kp->del_flash(index);
			kp->led(index, false);
			break;
		case dev_cmd_t::KPLEDBUTTON_ON:
			if (kp == nullptr) return;
			kp->del_flash(index);
			kp->led(index, true);
			break;
		case dev_cmd_t::KPLEDBUTTON_FLASH:
			if (kp == nullptr) return;
			flash.gpio = index;
			flash.interval_off = INT_BUTT_OFF;
			flash.interval_on = INT_BUTT_ON;
			kp->add_flash(&flash);
			break;

		case dev_cmd_t::Mic:
			device_write_aec(beg_aec_t::AEC_MIC_SEL, static_cast<char>(index));
			break;
		case dev_cmd_t::AMP:
			device_write_aec(beg_aec_t::AEC_AMP_SEL, static_cast<char>(index));
			break;
		}
	}
	catch (...) {
		cout << "ERROR: MyGPIO::CommandsWrite()" << endl;
	}
}
//---------------------------------------------------------------

void device_t::device_write_aec(beg_aec_t begin, char value)
{
	/*не отправлять повторные команды*/
	switch (begin) {
	case beg_aec_t::AEC_AMP_SEL:
		if (vAEC_AMP_SEL != value)
			vAEC_AMP_SEL = value;
		else
			return;
		break;
	case beg_aec_t::AEC_MIC_SEL:
		if (vAEC_MIC_SEL != value)
			vAEC_MIC_SEL = value;
		else
			return;
		break;
	}
	char aec_cmd[aec_cmd_len];
	aec_cmd[0] = static_cast<char>(begin);
	aec_cmd[1] = value;
	m_uart->write_data(aec_cmd, aec_cmd_len);
}
//-------------------------------------------------------

void device_t::led_button_on(func_t function, bool functionIsID)
{
	string strCommand = "";

	if (functionIsID) {
		device_write(dev_cmd_t::KPLEDBUTTON_ON, static_cast<int>(function));
		m_buttons[static_cast<int>(function)].status = led_button_status_t::LED_ON;
	}
	else {
		for (int i = 0; i < NumberButtons; i++)
			if (m_buttons[i].function == function)	{
				device_write(dev_cmd_t::KPLEDBUTTON_ON, i);
				m_buttons[i].status = led_button_status_t::LED_ON;
				break;
			}
	}
}
//-----------------------------------------------------

void device_t::led_button_off(func_t function, bool functionIsID)
{
	string strCommand = "";

	if (functionIsID) {
		device_write(dev_cmd_t::KPLEDBUTTON_OFF, static_cast<int>(function));
		m_buttons[static_cast<int>(function)].status = led_button_status_t::LED_OFF;
	}
	else {
		for (int i = 0; i < NumberButtons; i++)
			if (m_buttons[i].function == function)	{
				device_write(dev_cmd_t::KPLEDBUTTON_OFF, i);
				m_buttons[i].status = led_button_status_t::LED_OFF;
			}
	}
}
//------------------------------------------------------------

void device_t::led_button_flash(func_t function, bool functionIsID)
{
	string strCommand = "";

	if (functionIsID) {
		device_write(dev_cmd_t::KPLEDBUTTON_FLASH, static_cast<int>(function));
		m_buttons[static_cast<int>(function)].status = led_button_status_t::LED_FLASH;
	}
	else {
		for (int i = 0; i < NumberButtons; i++)
			if (m_buttons[i].function == static_cast<func_t>(function))	{
				device_write(dev_cmd_t::KPLEDBUTTON_FLASH, i);
				m_buttons[i].status = led_button_status_t::LED_FLASH;
				break;
			}
	}
}
//-----------------------------------------------------------------

vector<int> device_t::find_button_by_phone_presents(string URI, bool fullUri)
{
	string phone = "";
	string server("");
	vector<int> vButton;

	if (fullUri) {
		phone = format_addr(URI);
		server = format_addr_server(URI);
		for (int i = 0; i < NumberButtons; i++)
			if (m_buttons[i].phone == phone && m_buttons[i].server == server)
				vButton.push_back(m_buttons[i].buttonID);
	}
	else {
		for (int i = 0; i < NumberButtons; i++)
			if (m_buttons[i].phone == URI)
				vButton.push_back(m_buttons[i].buttonID);
	}
	return vButton;
}
//------------------------------------------------------------------------

button_t device_t::get_button_by_function(func_t function)
{
	for (int i = 0; i < NumberButtons; i++)
		if (m_buttons[i].function == function)
			return m_buttons[i];
	return button_t();
}