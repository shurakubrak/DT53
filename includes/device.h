#pragma once

#include <string>
#include <list>
#include <vector>
#include <mutex>
#include "utils.h"
#include "classes.h"

#define NumberButtons		110

#define OFF					0
#define ON					1
#define FLASH				2
#define LOW					0
#define HIGH				1
#define LOW_LONG			2

#define THRWRITE_PER		25000
#define INT_BUTT_OFF 		200
#define INT_BUTT_ON			500
#define INT_GL_RED_ON		1000
#define INT_GL_RED_OFF		1500
#define INT_GL_GREEN_ON		1000
#define INT_GL_GREEN_OFF	200


// UART
#define COM				"/dev/ttyAMA0"
//#define COM				"/dev/ttyS0"
//#define COM				"/dev/ttyserial0"
#define BeginCOM					'@'
#define EndCOM						'^'
#define GPIO_OFFSET (0x200000)
#define BLOCK_SIZE (4*1024)
#define GFPSEL1 (1)
#define GPIO1617mask 0x00fc0000 /* GPIO 16 for CTS0 and 17 for RTS0 */
#define GPIO_header_40 0x01
#define VERSION "1.5"

 /////Команды SPI
#define dcButton	3
#define SPI_CHANNEL	0


//gpio
#define HOOK				-1
#define AMP_MUTE			23
#define DSP_RESET			4
#define GLedRed				2
#define GLedGreen			3

//KeyPad
#define KPlocal_CS			26
#define KPext_CS			5
#define KPselect_0			6
#define KPselect_1			25
#define KPselect_2			22
#define KPlocal_IRQ			10
#define KPext_IRQ			11

//////////////////////////////////////
//	Commands for device
enum class dev_cmd_t
{
	Mic = 0,
	AMP,
	GENLEDRED_OFF,
	GENLEDRED_ON,
	GENLEDRED_FLASH,
	GENLEDGREEN_OFF,
	GENLEDGREEN_ON,
	GENLEDGREEN_FLASH,
	KPLEDBUTTON_OFF,
	KPLEDBUTTON_ON,
	KPLEDBUTTON_FLASH
};

//AEC
//Params for DSP
#define aec_cmd_len					2
#define BUFF_SIZE					200000
#define BeginCOM					'@'
#define EndCOM						'^'

#define KEY_CHIPs					14
#define KEY_UP_TIMER				400

enum class beg_aec_t
{
	AEC_FULL_PACK = 0xFF,
	AEC_AMP_SEL = 0xFC,
	AEC_MIC_SEL = 0xFB,
	AEC_GAIN_MIC1 = 0xEF,
	AEC_NOISE_MIC1 = 0xED,
	AEC_OFF = 0xF0,
	AEC_AGC1 = 0xFE,
	AEC_AGC_TARGET1 = 0xF9,
	AEC_HIS1 = 0xF7,
	AEC_GAIN_STEP_MIC1 = 0xF5
};

enum class access_t
{
	READ = 0,
	WRITE,
	DELETE,
	WRITE_LAST,
	SELECT,
	SET_NODEID,
	READ_NEXT,
	SET_TARGET,
	ADD_ANCH,
	UPD_ANCH,
	DEL_ANCH,
	GET_ANCH,
	FIND_SECT,
	READ_THIS,
	SET_LEVEL,
	UPDATE
};

enum class mic_select_t
{
	MIC_OFF = 0,
	MIC_HF,
	MIC_HS
};

enum class amp_select_t
{
	AMP_OFF,
	AMP_HF,
	AMP_HORN,
	AMP_HS,
	AMP_HS_HF
};



struct device_event_t
{
	int ev = -1;
	bool flag = OFF;
	int index = -1;
};

struct keys_t
{
	int key_map = 0;
	uint status = 0;
	uint64_t tm = get_time_ms();
};

struct key_event_t
{
	int ev = -1;
	bool flag = OFF;
	int index = -1;
};

struct led_chip_t
{
	unsigned ST;
	unsigned OE;
	uint last_status = 0;
};

struct flash_t
{
	int gpio = -1;
	int interval_off = 1000;
	int interval_on = 500;
	Time tm_on = std::chrono::system_clock::now();
	Time tm_off = tm_on;
	bool status = ON;
};



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* class UART*/
class uart_t
{
public:
	int m_fd;
private:
	bool m_begin = false;
	std::string m_str;
public:
	uart_t() {}

	bool open_uart();
	void write_soft(char* buf, int size);
	void write_data(char* buf, int size);
	void read_data();
	void buffer_analize(uint8_t byte);

private:
	unsigned gpio_base();
	bool set_CTS();

};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/* class KeyPed */
class keypad_t
{
public:
	bool m_ext = true;
	keys_t m_keychip[KEY_CHIPs][8];

private:
	std::list<device_event_t> m_lst_u2s;
	std::mutex m_mx_u2s;
	bool lst_u2s_access(device_event_t* event, access_t access);
	
	std::vector<flash_t> m_lst_flash;
	std::mutex m_mx_flash;
	bool lst_flash_access(flash_t* flash, access_t access);
	
	size_t m_cur = 0;
	std::mutex m_mx_chip;
	std::vector<button_t> *m_buttons;

public:
	keypad_t(std::vector<button_t>* buttons) {
		m_buttons = buttons;
	}

	bool open();
	void fill_butt_coord();
	void chip_select(int chip_num);
	void read();
	bool key_read(int chip);
	
	std::vector<size_t> lst_key_down();
	
	bool add_event(device_event_t* event) {
		return lst_u2s_access(event, access_t::WRITE);
	}
	bool get_event(device_event_t* event) {
		return lst_u2s_access(event, access_t::READ);
	}
	bool delete_event() {
		return lst_u2s_access(nullptr, access_t::DELETE);
	}

	bool add_flash(flash_t* flash) {
		del_flash(flash->gpio);
		return lst_flash_access(flash, access_t::WRITE);
	}
	bool get_flash(flash_t* flash) { return lst_flash_access(flash, access_t::READ_NEXT); }
	bool del_flash(uint index) {
		flash_t flash;
		flash.gpio = index;
		return lst_flash_access(&flash, access_t::DELETE);
	}
	void cler_flash() {
		lst_flash_access(nullptr, access_t::DELETE);
	};
	void upd_flash(flash_t* flash) { lst_flash_access(flash, access_t::UPDATE); }
	void flashing();
	void led(int index, bool status);

private:
	void spi_write(char* buf, int count);
	void spi_xfer(char* buf_w, char* buf_r, int count);
};

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
/* class GPIO */
class gpio_t
{
public:

private:
	std::vector<flash_t> m_lst_flash;
	std::mutex m_mx_flash;
	bool lst_flash_access(flash_t* flash, access_t access);
	int cur;
public:
	gpio_t() {

	}

	bool open();
	void set(int gp, bool state);
	int get(int gp);
	bool add_flash(flash_t* flash) {
		del_flash(flash->gpio);
		return lst_flash_access(flash, access_t::WRITE);
	}
	bool get_flash(flash_t* flash) { return lst_flash_access(flash, access_t::READ_NEXT); }
	bool del_flash(uint index) {
		flash_t flash;
		flash.gpio = index;
		return lst_flash_access(&flash, access_t::DELETE);
	}
	void upd_flash(flash_t* flash) { lst_flash_access(flash, access_t::UPDATE); }
	void flashing();
};
/*********************************************/

class device_t
{
public:
	bool m_horn_state = false;
	bool m_opex_state = false;
	int vAEC_AMP_SEL = 0;
	int vAEC_MIC_SEL = 0;
	std::vector<button_t> m_buttons;
	gpio_t* m_gpio;
	keypad_t* m_kp;

	device_t(std::string path){
		m_path = path;
		m_buttons.resize(NumberButtons);
	}
	bool dsp_load();
	bool dsp_file_load();
	bool device_open(keypad_t* kp, gpio_t* gpio, uart_t* uart);
	void device_write(dev_cmd_t command, int index,
		gpio_t* gpio = nullptr, keypad_t* kp = nullptr);
	void device_write_aec(beg_aec_t begin, char value);
	void led_button_on(func_t function, bool functionIsID=false);
	void led_button_off(func_t function, bool functionIsID=false);
	void led_button_flash(func_t function, bool functionIsID=false);
	std::vector<int> find_button_by_phone_presents(std::string URI, bool fullUri);
	button_t get_button_by_function(func_t function);

private:
	std::string m_path;
	uart_t* m_uart;
};

