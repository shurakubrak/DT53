#include <cstdio>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include "sqlite_db.h"
#include "main.h"
#include "utils.h"
#include "rc5oper.h"

using namespace std;

atomic<bool> terminated(false);

void* thread_device(device_t* device)
{
	while (!terminated) {
		device->m_kp->read();
		device->m_kp->flashing();
		device->m_gpio->flashing();
		msleep(10);
	}
	return NULL;
}
/***********************************************************/

void* thread_shell(publ_settings_t* setts)
{
	uint64_t tm_talk_remove = get_time_s();
	string scmd = path + "copy.sh &";

	while (!terminated) {
		if (setts->fl_record)
			/*перенос записей разговоров*/
			if (get_time_s() - tm_talk_remove >= 180) {
				system(scmd.c_str());
				tm_talk_remove = get_time_s();
			}
		ssleep(1);
	}
	return NULL;
}
/**********************************************************/

int main(int argc, char** argv)
{
	int terminated = 0;
	
    vers_safe();

	bool reload = false;
	pthread_t ptDeviceRead;
	pthread_t ptShell;
	t_blink = get_time_ms();
	t_control = get_time_ms();
	path = "/home/sip/app/"; /*путь к каталогу приложения*/

	/*читаем настройки*/
	if(!read_param()) {
		std::cout << "ReadParam fail" << endl;
		return 1;
	}
	/*базовый класс SIP*/
	sip_t sip(path);
	if (!read_param_ex(&sip))
	{
		std::cout << "ReadParamEx fail" << "\n";
		return false;
	}
	
	/*запускаем устройство*/
	device_t device(path);
	uart_t uart;
	gpio_t gpio;
	keypad_t kp(&device.m_buttons);
	tones_t tone;

	if (!device.device_open(&kp, &gpio, &uart)) {
		std::cout << "Device open error" << endl;
		return 2;
	}
	gpio.set(GLedRed, HIGH);
	
	/*прверим лицензию*/
	find_cpu();
	
RELOAD:
	if (!sip.init(setts.pb_adjust, setts.capture_adjust, &setts, &device)) {
		std::cout << "SIP init error" << endl;
		return 3;
	}

	if (!sip.players_init()) {
		std::cout << "Player init error" << endl;
		return 4;
	}
	tone.init(&sip.m_playback);

	if (!buttons_fill(&kp, device.m_buttons)) {
		std::cout << "Fill buttons error" << endl;
		return 5;
	}

	for (int i = 0; i < NumberButtons; i++)
		device.led_button_on(static_cast<func_t>(i), true);

	if (!sip.account_create(ip_address)) {
		std::cout << "Accounts create error" << endl;
		return 6;
	}

	if (!reload && argc == 1) {
		//if (!device.dsp_load()) {
		//	std::cout << "DSP load error" << endl;
		//	return 7;
		//}
		sip.voiceIP(ip_address);
	}

	device.device_write(dev_cmd_t::Mic, static_cast<int>(mic_select_t::MIC_HF));
	device.device_write(dev_cmd_t::AMP, static_cast<int>(amp_select_t::AMP_HF));

	if (!reload)
		read_param_dsp(&device);

	initial(&sip);
	if (!reload) {
		thread thrd_device = thread(thread_device, &device);
		thrd_device.detach();

		thread thrd_shell = thread(thread_shell, &setts);
		thrd_shell.detach();
	}

	if (reload) {
		sip.del_command();
		tone.busy();
	}
	reload = false;
	std::cout << "Device phone in fly" << endl;

	while (true)
	{
		if (setts.fl_eth_control) {
			while (!eth_control()) {
				if (!reload) {
					sip.hang_phone();
					sip.ring_stop();
					for (uint8_t i = 0; i < NumberButtons; i++)
						device.led_button_off(static_cast<func_t>(i), true);
					sip.account_close();
					tone.close();
					sip.close();
					sip.player_destroy();
					device.led_button_flash(func_t::SpeeckerPhone);
					reload = true;
				}
				app_control();
				msleep(550);
			}
			if (reload) {
				ssleep(2);
				device.led_button_off(func_t::SpeeckerPhone);
				goto RELOAD;
			}
		}

		U2S(&sip);
		sip.m_ep.libHandleEvents(5);

		T2D(&sip);
		sip.m_ep.libHandleEvents(5);

		S2D(&sip);
		sip.m_ep.libHandleEvents(5);

		blink(&gpio);
		sip.m_ep.libHandleEvents(5);
	}
    return 0;
}
/*********************************************/

void vers_safe()
{
	string sub_ver = __DATE__;
	sub_ver += (string)" " + __TIME__ + ")";

	string type = "DT53";
	string ver = "v1.0 (";
#  ifndef NDEBUG
	ver += "debug: " + sub_ver;
#  else
	ver += "release: " + sub_ver;
#  endif
	std::cout << ver << endl;
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return;
	db.m_sql_str = "update operators set type='"
		+ type + "',version='"
		+ ver + "'";
	if (!db.exec_sql(true))
		std::cout << "SQLite ERROR: write version error" << endl;
	db.close_db();
}
/*************************************************************/

bool read_param()
{
	network_t eth;
	network_t wlan;
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from operators";
	if (!db.exec_sql()) {
		std::cout << "SQLite: read table operators error" << endl;
		return false;
	}
	string active_addr = db.field_by_name("ip_address");

	bool fl_eth = false;
	bool fl_wlan = false;
	for (int i = 0; i < 10; i++) {
		fl_eth = get_network(&eth, false);
		fl_wlan = get_network(&wlan, true);

		if (active_addr == "eth") {
			if (fl_eth)
				ip_address = eth.ip_addr;
			else
				return false;
		}
		else if (active_addr == "wlan") {
			if (fl_wlan)
				ip_address = wlan.ip_addr;
			else
				return false;
		}
		else {
			if (fl_eth)
				ip_address = eth.ip_addr;
			else if (fl_wlan)
				ip_address = wlan.ip_addr;
			else
				return false;
		}
		/*проверка правильного адреса DHCP*/
		if (ip_address.substr(0, 3) == "169") {
			std::cout << "Invalid address 169.x.x.x" << endl;
			if (i == 9)
				return false;
			else {
				system("dhcpcd &");
				ssleep(3);
			}
		}
		else
			break;
	}

	int pos = db.field_by_name("name").find_last_of(':');
	//if (pos != string::npos)
	//	SIP.port = atoi(db.field_by_name("name").substr(pos + 1).c_str());
	//SIP.protocol = db.field_by_name("protocol");
	//SIP.log = db.field_by_name("log");
	//SIP.log_level = atoi(db.field_by_name("log_level").c_str());

	/*сохранить ip параметры*/
	db.m_sql_str = "update networks set value='" + ip_address
		+ "' where name = 'SIP_addr'";
	db.exec_sql(true);
	if (fl_eth) {
		db.m_sql_str = "update networks set value='" + eth.ip_addr
			+ "' where name = 'addressEth0'";
		db.exec_sql(true);
		db.m_sql_str = "update networks set value='" + eth.netmask
			+ "' where name = 'netmaskEth0'";
		db.exec_sql(true);
		db.m_sql_str = "update networks set value='" + eth.mac
			+ "' where name = 'macEth0'";
		db.exec_sql(true);
		db.m_dbname = "update networks set value='" + eth.gateway
			+ "' where name = 'gatewayEth0'";
		db.exec_sql(true);
	}
	if (fl_wlan) {
		db.m_sql_str = "update networks set value='" + wlan.ip_addr
			+ "' where name = 'addressWlan0'";
		db.exec_sql(true);
		db.m_sql_str = "update networks set value='" + wlan.netmask
			+ "' where name = 'netmaskWlan0'";
		db.exec_sql(true);
		db.m_sql_str = "update networks set value='" + wlan.mac
			+ "' where name = 'macWlan0'";
		db.exec_sql(true);
		db.m_sql_str = "update networks set value='" + wlan.gateway
			+ "' where name = 'gatewayWlan0'";
		db.exec_sql(true);
	}
	db.close_db();
	return true;
}
/******************************************************/

bool read_param_ex(sip_t* sip)
{
	string param_name = "";
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from params";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read table operators error" << endl;
		db.close_db();
		return false;
	}

	vector <string> v_filter;
	v_filter.clear();
	do {
		param_name = db.field_by_name("name").c_str();

		if (param_name == "ext_key") {
			//KP.Ext = db.atob(db.field_by_name("value").c_str());
			continue;
		}

		if (param_name == "multiline") {
			setts.multiline = db.atob(db.field_by_name("value"));
			continue;
		}
		if (param_name == "autoanswer") {
			//Accounts[0].stAcc->auto_answer = db.atob(db.field_by_name("value").c_str());
			continue;
		}
		if (param_name == "record") {
			setts.fl_record = db.atob(db.field_by_name("value"));
			continue;
		}
		if (param_name == "first_digit") {
			setts.first_digit = atoi(db.field_by_name("value").c_str());
			continue;
		}
		if (param_name == "inter_digit") {
			setts.inter_digit = atoi(db.field_by_name("value").c_str());
			continue;
		}
		if (param_name == "no_answer") {
			setts.call_no_answer = atoi(db.field_by_name("value").c_str());
			continue;
		}

		if (param_name == "rington") {
			setts.ring_ton = db.field_by_name("value");
			if (fopen((path + setts.ring_ton).c_str(), "r") == NULL)
				setts.ring_ton = "Tones/Ring.wav";
			continue;
		}
		if (param_name == "holdton") {
			sip->m_hold_tone = db.field_by_name("value");
			if (fopen((path + sip->m_hold_tone).c_str(), "r") == NULL)
				sip->m_hold_tone = "Tones/Hold.wav";
			continue;
		}
		if (param_name == "vmsg_1") {
			setts.vmsg_1 = db.field_by_name("value");
			if (fopen((path + setts.vmsg_1).c_str(), "r") == NULL)
				setts.vmsg_1 = "Tones/VMsg_1.wav";
			continue;
		}
		if (param_name == "vmsg_2") {
			setts.vmsg_2 = db.field_by_name("value");
			if (fopen((path + setts.vmsg_2).c_str(), "r") == NULL)
				setts.vmsg_2 = "Tones/VMsg_1.wav";
			continue;
		}
		if (param_name == "vmsg_3") {
			setts.vmsg_3 = db.field_by_name("value");
			if (fopen((path + setts.vmsg_3).c_str(), "r") == NULL)
				setts.vmsg_3 = "Tones/VMsg_1.wav";
			continue;
		}
		if (param_name == "vmsg_4") {
			setts.vmsg_4 = db.field_by_name("value");
			if (fopen((path + setts.vmsg_4).c_str(), "r") == NULL)
				setts.vmsg_4 = "Tones/VMsg_1.wav";
			continue;
		}
		if (param_name == "vmsg_5") {
			setts.vmsg_5 = db.field_by_name("value");
			if (fopen((path + setts.vmsg_5).c_str(), "r") == NULL)
				setts.vmsg_5 = "Tones/VMsg_1.wav";
			continue;
		}

		if (param_name == "capture_level") {
			setts.capture_adjust = atof(db.field_by_name("value").c_str());
			continue;
		}
		if (param_name == "playback_level") {
			setts.pb_adjust = atof(db.field_by_name("value").c_str());
			continue;
		}

		if (param_name == "eth_control") {
			setts.fl_eth_control = db.atob(db.field_by_name("value"));
			continue;
		}


		if (param_name == "filter_1" ||
			param_name == "filter_2" ||
			param_name == "filter_3" ||
			param_name == "filter_4")
			v_filter.push_back(db.field_by_name("value"));
	} while (db.next_rec());
	db.close_db();

	/*определение фильров микрофона*/
	filter_data_t filters_data[MAX_FILTER_COUNT];
	parse_filter(v_filter, filters_data);
	init_input_filters(filters_data);
	set_mic_filters();

	if (sip->m_ep.libGetState() == PJSUA_STATE_STARTING ||
		sip->m_ep.libGetState() == PJSUA_STATE_RUNNING)
	{

		sip->players_init();

		sip->m_playback.adjustRxLevel(setts.pb_adjust);
		sip->m_capture.adjustTxLevel(setts.capture_adjust);
	}

	return true;
}
/***********************************************/

bool read_param_dsp(device_t* device)
{
	int param = -1;
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str= "select * from params";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR : read table operators error" << endl;
		db.close_db();
		return false;
	}

	do {
		string param_name = db.field_by_name("name").c_str();
		// AGC
		if (param_name == "agc_1") {
			setts.aec_agc1 = db.atob(db.field_by_name("value").c_str());
		}
		// Max PGA
		if (param_name == "agc_target_1") {
			setts.aec_agc_target_1 = atoi(db.field_by_name("value").c_str());
			if (setts.aec_agc_target_1 > 7)	setts.aec_agc_target_1 = 7;
			if (setts.aec_agc_target_1 < 0)	setts.aec_agc_target_1 = 0;
		}
		// Histeresis
		if (param_name == "his_1") {
			setts.aec_his1 = atoi(db.field_by_name("value").c_str());
			if (setts.aec_his1 > 2)	setts.aec_his1 = 2;
			if (setts.aec_his1 < 0)	setts.aec_his1 = 0;
		}
		// Gain mic
		if (param_name == "aec_gain_mic1") {
			setts.aec_gain_mic1 = atoi(db.field_by_name("value").c_str());
			if (setts.aec_gain_mic1 > 9)	setts.aec_gain_mic1 = 9;
			if (setts.aec_gain_mic1 < 0)	setts.aec_gain_mic1 = 0;
		}
		// Noise
		if (param_name == "aec_noise_mic1") {
			setts.aec_noise_mic1 = atoi(db.field_by_name("value").c_str());
			if (setts.aec_noise_mic1 > 9)	setts.aec_noise_mic1 = 9;
			if (setts.aec_noise_mic1 < 0)	setts.aec_noise_mic1 = 0;
		}
		// Gaine step
		if (param_name == "aec_gane_step_mic1") {
			setts.aec_gain_step_mic1 = atoi(db.field_by_name("value").c_str());
			if (setts.aec_gain_step_mic1 > 2)	setts.aec_gain_step_mic1 = 2;
			if (setts.aec_gain_step_mic1 < 0)	setts.aec_gain_step_mic1 = 0;
		}
	} while (db.next_rec());
	db.close_db();

	// передать в DSP для кодека
	device->device_write_aec(beg_aec_t::AEC_AGC1, setts.aec_agc1);
	device->device_write_aec(beg_aec_t::AEC_HIS1, setts.aec_his1);
	device->device_write_aec(beg_aec_t::AEC_AGC_TARGET1, setts.aec_agc_target_1);
	device->device_write_aec(beg_aec_t::AEC_GAIN_MIC1, setts.aec_gain_mic1);
	device->device_write_aec(beg_aec_t::AEC_NOISE_MIC1, setts.aec_noise_mic1);
	device->device_write_aec(beg_aec_t::AEC_GAIN_STEP_MIC1, setts.aec_gain_step_mic1);
	return true;
}
/***************************************************/

bool reload_param(sip_t* sip)
{
	pair<string, bool> sql_res;
	vector<pair<string, bool>> v_sql_res;
	string table_name = "";
	bool status = false;

	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from tables_status";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read table operators error" << endl;
		db.close_db();
		return false;
	}
	do {
		sql_res.first = db.field_by_name("table_name");
		sql_res.second = db.atob(db.field_by_name("ischange"));
		v_sql_res.push_back(sql_res);

	} while (db.next_rec());
	db.close_db();

	for (unsigned int i = 0; i < v_sql_res.size(); i++)
	{
		table_name = v_sql_res[i].first;
		status = v_sql_res[i].second;

		if (table_name == "operators" && status)
			continue;
		else if (table_name == "operator_phones" && status)	{
			sip->account_close();
			sip->account_create(ip_address, true);
			if (!db.open_db())
				return false;
			db.m_sql_str = "update tables_status set ischange = 'N' where table_name='operator_phones'";
			if (!db.exec_sql(true)) {
				std::cout << "SQLite ERROR: reload table operator_phones error" << endl;
				return false;
			}
			db.close_db();
			std::cout << "Account reload" << endl;
		}
		else if (table_name == "users" && status) {
			sip->reload_buddys();
			if (!db.open_db())
				return false;
			db.m_sql_str = "update tables_status set ischange = 'N' where table_name='users'";
			if (!db.exec_sql(true)) {
				std::cout << "SQLite ERROR: reload table users error" << endl;
				return false;
			}
			db.close_db();
			std::cout << "users reload" << endl;
		}
		else if (table_name == "buttons" && status)
		{
			buttons_fill(sip->m_device->m_kp, sip->m_device->m_buttons);
			sip->reload_buddys();
			if (!db.open_db())
				return false;
			db.m_sql_str = "update tables_status set ischange = 'N' where table_name='buttons'";
			if (!db.exec_sql(true)) {
				std::cout << "SQLite ERROR: reload table buttons error" << endl;
				return false;
			}
			db.close_db();
			std::cout << "Buttons reload" << endl;
		}
		else if (table_name == "functions" && status)
			;
		else if (table_name == "codec_priority" && status)
		{
			sip->codec_set();
			if (!db.open_db())
				return false;
			db.m_sql_str = "update tables_status set ischange = 'N' where table_name='codec_priority'";
			if (!db.exec_sql(true)) {
				std::cout << "SQLite ERROR: reload table codec_priority error" << endl;
				return false;
			}
			db.close_db();
			std::cout << "codec_priority reload" << endl;
		}
		else if (table_name == "params" && status)
		{
			read_param_ex(sip);
			read_param_dsp(sip->m_device);
			if (!db.open_db())
				return false;
			db.m_sql_str = "update tables_status set ischange = 'N' where table_name='params'";
			if (!db.exec_sql(true)) {
				std::cout << "SQLite ERROR: read table operators error" << endl;
				return false;
			}
			db.close_db();
			std::cout << "Params reload" << endl;
		}
	}
	return true;
}
/*****************************************************************/
void parse_filter(vector<string> v_filter, filter_data_t* filters_data)
{
	char separator = '|';
	int part;
	string type = "";
	string order = "";
	string Frequency = "";
	string BandWidth = "";
	
	int i = 0;
	for (string filter : v_filter)
	{
		part = 0;
		type = "";
		order = "";
		Frequency = "";
		BandWidth = "";

		for (char ch : filter)
			if (ch == separator)
				part++;
			else
				switch (part) {
				case 0:
					type += ch;
					break;
				case 1:
					order += ch;
					break;
				case 2:
					Frequency += ch;
					break;
				case 3:
					BandWidth += ch;
					break;
				}
		if (type.empty() || order.empty() || Frequency.empty() || BandWidth.empty())
		{
			filters_data[i].filterType = 0;
			filters_data[i].filterOrder = 0;
			filters_data[i].filterFrequency = 0;
			filters_data[i].filterBandWidth = 0;
		}
		else
		{
			filters_data[i].filterType = atoi(type.c_str());
			filters_data[i].filterOrder = atoi(order.c_str());
			filters_data[i].filterFrequency = atoi(Frequency.c_str());
			filters_data[i].filterBandWidth = atoi(BandWidth.c_str());
		}
		i++;
	}
}
/***********************************************************************/

void find_cpu()
{
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return;

	string cpuID = get_cpu_id();
	string cpuIDcrypto;
	set_key();

	db.m_sql_str = "select * from licenses";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read table licenses error" << endl;
		return;
	}
	do
	{
		cpuIDcrypto = db.field_by_name("license");
		if (cpuID == decrypt_licence(cpuIDcrypto))
		{
			setts.licence = true;
			db.m_sql_str = "update operators set license='Y'";
			if (!db.exec_sql(true)) {
				std::cout << "SQLite ERROR: write table operators error" << endl;
				return;
			}
			db.close_db();
			return;
		}
	} while (db.next_rec());

	db.m_sql_str = "update operators set license='N'";
	if (!db.exec_sql(true)) {
		std::cout << "SQLite ERROR: write table operators error" << endl;
		return;
	}
	db.close_db();
	return;
}
/**********************************************************/

bool buttons_fill(keypad_t* kp, vector<button_t> buttons)
{
	/*очистка списка кнопок*/
	buttons.clear();
	button_t emtyButtons;
	for (int i = 0; i < NumberButtons; i++)
		buttons.push_back(emtyButtons);
	kp->fill_butt_coord();

	/*заполнение кнопок*/
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from v_buttons_user";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read users error" << endl;
		return false;
	}

	int i = 0;
	do{
		string Server = "";
		string Phone = "";
		string SipVer = "";
		int def_phone = atoi(db.field_by_name("def_phone").c_str());
		switch (def_phone)
		{
		case 1:
			Phone = db.field_by_name("phone");
			Server = db.field_by_name("server");
			SipVer = db.field_by_name("sip_ver");
			break;
		case 2:
			Phone = db.field_by_name("phone_2");
			Server = db.field_by_name("server_2");
			SipVer = db.field_by_name("sip_ver_2");
			break;
		case 3:
			Phone = db.field_by_name("phone_mob");
			Server = db.field_by_name("server_mob");
			SipVer = db.field_by_name("sip_ver_mob");
			break;
		case 4:
			Phone = db.field_by_name("phone_home");
			Server = db.field_by_name("server_home");
			SipVer = db.field_by_name("sip_ver_home");
			break;
		case 5:
			Phone = db.field_by_name("phone_work");
			Server = db.field_by_name("server_work");
			SipVer = db.field_by_name("sip_ver_work");
			break;
		default:
			break;
		}

		if (SipVer == "0") {
			Phone = Server;
			Server = "local";
		}

		//Save button parameters
		buttons[i].function = static_cast<func_t>(atoi(db.field_by_name("function").c_str()));
		buttons[i].phone = Phone;
		buttons[i].server = Server;
		buttons[i].buttonID = atoi(db.field_by_name("button_id").c_str());

		if (db.field_by_name("is_mixer") == "Y")
			buttons[i].mixer = true;
		else
			buttons[i].mixer = false;
		buttons[i].status = led_button_status_t::LED_OFF;

		i++;
	} while (db.next_rec() && i < NumberButtons);

	/*координаты чипа*/
	//for (int i = 0; i < NumberButtons; i++) {
	//	for (int x = 0; x < KEY_CHIPs; x++)
	//		for (int y = 0; y < 8; y++)
	//			if (KP.keychip[x].keys[y].key_map == i) {
	//				Buttons[i].Chip = x;
	//				Buttons[i].Pin = y;
	//			}
	//}

	db.m_sql_str = "select * from users";
	if (db.exec_sql())
	{
		i = 0;
		do
		{
			string Server = "";
			string Phone = "";
			int sip_ver = atoi(db.field_by_name("sip_ver").c_str());
			int sip_ver2 = atoi(db.field_by_name("sip_ver2").c_str());
			int sip_ver_mob = atoi(db.field_by_name("sip_ver_mob").c_str());
			int sip_ver_home = atoi(db.field_by_name("sip_ver_home").c_str());
			int sip_ver_work = atoi(db.field_by_name("sip_ver_work").c_str());

			if (sip_ver == SIP_VER_PTP)
				Phone = db.field_by_name("server");
			else if (sip_ver2 == SIP_VER_PTP)
				Phone = db.field_by_name("server2");
			else if (sip_ver_mob == SIP_VER_PTP)
				Phone = db.field_by_name("server_mob");
			else if (sip_ver_home == SIP_VER_PTP)
				Phone = db.field_by_name("server_home");
			else if (sip_ver_work == SIP_VER_PTP)
				Phone = db.field_by_name("server_work");

			i++;
		} while (db.next_rec());
	}
	db.close_db();
	return true;
}
/************************************************************/

void initial(sip_t* sip)
{
	sip->first_digit_stop();
	sip->inter_digit_stop();
	sip->set_phone_status(phone_status_t::IDLE);
	for (int i = 0; i < NumberButtons; i++)
		sip->m_device->led_button_off(static_cast<func_t>(i), true);
	sip->m_device->m_kp->cler_flash();
}
/********************************************************/

bool eth_control()
{
	char dev[256];
	FILE* pFile;
	string f_name = "/sys/class/net/eth0/operstate";
	string str = "";

	pFile = fopen(f_name.c_str(), "r");
	if (pFile == nullptr
		|| pFile == 0) {
		cout << "ERROR open file Etn" << endl;
		return true; /*в случае ошибки нельзя останавливаться*/
	}
	else {
		fgets(dev, 256, pFile);
		fclose(pFile);
		str = dev;
		if (str.find("up") != string::npos
			|| str.find("Up") != string::npos
			|| str.find("UP") != string::npos
			|| str.find("Y") != string::npos
			|| str.find("y") != string::npos) {
			//cout << "Eth device OK: " << str << endl;
			return true;
		}
		else {
			cout << "Eth device DOWN: " << str << endl;
			return false;
		}
	}
}
/********************************************************/

void app_control()
{
	if (get_time_ms() - t_control > 1000) {
		t_control = get_time_ms();
		if (pFile == nullptr || pFile == 0)
		{
			pFile = fopen("/tmp/phone", "w");
			if (pFile == nullptr || pFile == 0)
				return;
			else {
				fseek(pFile, 0, SEEK_SET);
				fputs(to_string(get_time_ms()).c_str(), pFile);
				fflush(pFile);
			}
		}
		else
		{
			fseek(pFile, 0, SEEK_SET);
			fputs(to_string(get_time_ms()).c_str(), pFile);
			fflush(pFile);
		}
	}
}
/*************************************************************/

void function_dial(int button_id, sip_t* sip)
{
	string strCommand = "";
	string digit = "";

	if (sip->m_device->m_buttons[button_id].function != func_t::ConfPort)
		sip->m_fl_conf_port = false;


	switch (sip->m_device->m_buttons[button_id].function)
	{
	case func_t::NoFunction:
		break;
	case func_t::OK:
		if (!sip->m_dial_number.empty())
		{
			sip->first_digit_stop();
			sip->inter_digit_stop();

			if (sip->find_user_phone_by_dial_number(sip->m_dial_number)) {
				sip->m_device->led_button_on(func_t::OK);
				sip->m_device->led_button_off(func_t::SpeedDial);
			}
			else
				sip->hang_phone();
		}
		break;

	case func_t::SpeedDial:
		if (sip->m_device->m_buttons[button_id].status == led_button_status_t::LED_OFF)
		{
			sip->m_device->led_button_on(func_t::SpeedDial);
			sip->first_digit_start();
		}
		else
		{
			sip->m_device->led_button_on(func_t::SpeedDial);
			sip->first_digit_stop();
		}
		break;

	case func_t::SpeeckerPhone:
		if (sip->m_device->m_buttons[button_id].status == led_button_status_t::LED_ON)
		{
			if (sip->m_horn_state)/*есть вызов на рупор, вкл. микр. (см. SetPhoneStatus())*/
				sip->m_horn_state = false;
			else {
				/* выключить */
				sip->hang_phone();
				for (int i = 0; i < NumberAccount; i++)
					for (unsigned int a = 0; a < sip->m_accounts[i].m_calls.size(); a++)
						if (sip->m_accounts[i].m_calls[a]->m_call_status == call_status_t::Horn)
							sip->m_accounts[i].m_calls[a]->m_call_status = call_status_t::Base;
				sip->cmd_hungup_all();
			}
		}
		else
		{	/* включить спикерфон */
			switch (sip->get_phone_state())
			{
			case phone_status_t::IDLE:
				sip->first_digit_start();
				break;
			case phone_status_t::INCALL:
				sip->first_digit_stop();
				sip->m_device->led_button_off(func_t::SpeedDial);
				sip->cmd_answer();
				break;
			}
			sip->m_device->led_button_on(func_t::SpeeckerPhone);
		}
		break;

	case func_t::Mic:
		if (sip->m_device->get_button_by_function(func_t::Mic).status == led_button_status_t::LED_OFF)
			//Мик. вкл.
			sip->m_device->led_button_on(func_t::Mic);
		else //Мик. выкл.
			sip->m_device->led_button_off(func_t::Mic);
		break;

	case func_t::Simplex_rec_tran:
		//отправляем на микшер Пр.
		sip->m_device->led_button_off(func_t::Simplex_rec_tran);
		sip->cmd_simplex("simplex_rec");
		break;

	case func_t::Call_Horn:
		if (sip->m_device->m_buttons[button_id].status == led_button_status_t::LED_OFF)
		{
			sip->first_digit_start();
			sip->m_device->led_button_on(func_t::Call_Horn);
			sip->m_device->led_button_off(func_t::Call_Speecker);
			sip->m_device->led_button_off(func_t::Call_Mic);
			sip->m_md_call_horn = true;
			sip->m_md_call_speecker = false;
			sip->m_md_call_mic = false;
		}
		else
		{
			sip->first_digit_stop();
			sip->m_device->led_button_off(func_t::Call_Horn);
			sip->m_md_call_horn = false;
		}
		break;

	case func_t::Call_Speecker:
		if (sip->m_device->m_buttons[button_id].status == led_button_status_t::LED_OFF)
		{
			sip->first_digit_start();
			sip->m_device->led_button_on(func_t::Call_Speecker);
			sip->m_device->led_button_off(func_t::Call_Horn);
			sip->m_device->led_button_off(func_t::Call_Mic);
			sip->m_md_call_horn = false;
			sip->m_md_call_speecker = true;
			sip->m_md_call_mic = false;
		}
		else
		{
			sip->first_digit_stop();
			sip->m_device->led_button_off(func_t::Call_Speecker);
			sip->m_md_call_speecker = false;
		}
		break;

	case func_t::Call_Mic:
		if (sip->m_device->m_buttons[button_id].status == led_button_status_t::LED_OFF)
		{
			sip->first_digit_start();
			sip->m_device->led_button_on(func_t::Call_Mic);
			sip->m_device->led_button_off(func_t::Call_Horn);
			sip->m_device->led_button_off(func_t::Call_Speecker);
			sip->m_md_call_horn = false;
			sip->m_md_call_speecker = false;
			sip->m_md_call_mic = true;
		}
		else
		{
			sip->first_digit_stop();
			sip->m_device->led_button_off(func_t::Call_Speecker);
			sip->m_md_call_mic = false;
		}
		break;

	case func_t::Hold:
		sip->mk_hold();
		break;

	case func_t::xFer:
		sip->mk_xfer();
		break;

	case func_t::Conf:
		sip->first_digit_stop();
		sip->inter_digit_stop();
		sip->mk_conf();
		break;

	case func_t::ConfSpkr:
		sip->first_digit_stop();
		sip->inter_digit_stop();

		sip->mk_conf(true);
		break;


	case func_t::ConfPort:
		if (!sip->m_fl_conf_port)
		{
			if (sip->mk_conf_port(button_id)) {
				sip->m_fl_conf_port = true;
				sip->m_device->led_button_on(static_cast<func_t>(button_id), true);
			}
		}
		else
		{
			for (unsigned int i = 0; i < sip->m_device->m_buttons[button_id].calls.size(); i++)
				sip->cmd_hungup(sip->m_device->m_buttons[button_id].calls[i]->getInfo().callIdString);
		}
		break;

	case func_t::DTMF:
		if (sip->m_device->m_buttons[button_id].status == led_button_status_t::LED_OFF)
		{
			sip->m_fl_dial_dtmf = true;
			sip->m_device->led_button_on(func_t::DTMF);
		}
		else
		{
			sip->m_fl_dial_dtmf = false;
			sip->m_device->led_button_off(func_t::DTMF);
		}
		break;

	case func_t::VoiceIP:
		sip->voiceIP(ip_address);
		break;

	case func_t::Mixer1:
		sip->find_user_phone_by_button_call(button_id, "mix_1");
		break;

	case func_t::Mixer2:
		sip->find_user_phone_by_button_call(button_id, "mix_2");
		break;

	case func_t::Mixer3:
		sip->find_user_phone_by_button_call(button_id, "mix_3");
		break;

	case func_t::Mixer4:
		sip->find_user_phone_by_button_call(button_id, "mix_4");
		break;

	case func_t::MixerAll:
		sip->find_user_phone_by_button_call(button_id, "mix_all");
		break;

	case func_t::VMsg_1:
		sip->vmsg_play(sip->plrVMsg_1, 1);
		break;
	case func_t::VMsg_2:
		sip->vmsg_play(sip->plrVMsg_2, 2);
		break;
	case func_t::VMsg_3:
		sip->vmsg_play(sip->plrVMsg_3, 3);
		break;
	case func_t::VMsg_4:
		sip->vmsg_play(sip->plrVMsg_4, 4);
		break;
	case func_t::VMsg_5:
		sip->vmsg_play(sip->plrVMsg_5, 5);
		break;

		/*Орех*/
	case func_t::Opex_0:
		sip->find_user_phone_by_button_call(button_id, "opex_0");
		sip->m_opex_state = true;
		break;
	case func_t::Opex_1:
		sip->find_user_phone_by_button_call(button_id, "opex_1");
		sip->m_opex_state = true;
		break;
	case func_t::Opex_2:
		sip->find_user_phone_by_button_call(button_id, "opex_2");
		sip->m_opex_state = true;
		break;
	case func_t::Opex_3:
		sip->find_user_phone_by_button_call(button_id, "opex_3");
		sip->m_opex_state = true;
		break;
	case func_t::Opex_4:
		sip->find_user_phone_by_button_call(button_id, "opex_4");
		sip->m_opex_state = true;
		break;
	case func_t::Opex_5:
		sip->find_user_phone_by_button_call(button_id, "opex_5");
		sip->m_opex_state = true;
		break;
	case func_t::Opex_6:
		sip->find_user_phone_by_button_call(button_id, "opex_6");
		sip->m_opex_state = true;
		break;


	default:	//digit '0' - '9'
		sip->first_digit_stop();
		sip->inter_digit_stop();

		if (sip->m_device->m_buttons[button_id].function == func_t::d0)
			digit = to_string(0);
		else
			digit = to_string(static_cast<int>(sip->m_device->m_buttons[button_id].function));

		//набор DTMF или собираем цифры для вызова
		if (!sip->m_fl_dial_dtmf)
		{
			sip->m_dial_number += digit;
			cout << "dial: " << sip->m_dial_number << endl;
			if (sip->find_user_phone_by_dial_number(sip->m_dial_number, true)) {
				sip->m_device->led_button_off(func_t::OK);
				sip->m_device->led_button_off(func_t::SpeedDial);
			}
			else
			{
				if (sip->m_route.status)
				{
					if (sip->m_dial_number.length() == (unsigned)sip->m_route.num_digit)
					{
						if (sip->m_route.account)
							if (sip->m_accounts[sip->m_route.account].m_acc_set.sip_ver != SIP_VER_PTP)
								sip->cmd_call(sip->m_accounts[sip->m_route.account].m_acc_set.sipserver, sip->m_dial_number, "base");
						sip->m_device->led_button_off(func_t::OK);
						sip->m_device->led_button_off(func_t::SpeedDial);
					}
					else
						sip->inter_digit_start();
				}
				else
				{
					if (sip->find_rout(sip->m_dial_number))
					{
						if (sip->m_dial_number.length() == (unsigned)sip->m_route.num_digit)
						{
							if (sip->m_route.account)
								if (sip->m_accounts[sip->m_route.account].m_acc_set.sip_ver != SIP_VER_PTP)
									sip->cmd_call(sip->m_accounts[sip->m_route.account].m_acc_set.sipserver, sip->m_dial_number, "base");
							sip->m_device->led_button_off(func_t::OK);
							sip->m_device->led_button_off(func_t::SpeedDial);
						}
						else
							sip->inter_digit_start();
					}
					else
						sip->inter_digit_start();
				}
			}
		}
		else
			sip->cmd_dtmf(digit);
		break;
	}
}
/************************************************************************/

void U2S(sip_t* sip)
{
	device_event_t event;

	if (sip->m_device->m_kp->get_event(&event))	{
		switch (event.ev)
		{
		case dcButton:
			/*функция Орех работает симплексно,
			для других операций событие
			UP(flag==false) не обрабатывается*/
			if (event.flag == false) {/*отпустили кнопку*/
				if (sip->m_device->m_buttons[event.index].function >= func_t::Opex_0
					&& sip->m_device->m_buttons[event.index].function <= func_t::Opex_6) {
					/*Орех: делаем отбой*/
					sip->cmd_hungup(sip->find_call(sip->m_device->m_buttons[event.index].phone));
					sip->m_opex_state = false;
				}
				return;
			}

			/*clik (кнопка нажата: flag == true)*/
			if (sip->m_device->m_buttons[event.index].function < func_t::Opex_0
				|| sip->m_device->m_buttons[event.index].function > func_t::Opex_6)
				sip->get_tone()->key_clik();

			if (sip->m_device->m_buttons[event.index].function != func_t::SpeedDial)
				sip->first_digit_stop();
			sip->inter_digit_stop();

			if (sip->m_device->m_buttons[event.index].status == led_button_status_t::SUBS_CALL)
			{	//answer
				sip->m_device->led_button_off(func_t::SpeedDial);
				sip->cmd_answer(sip->m_device->m_buttons[event.index].phone);
			}
			else
			{
				if (sip->m_device->get_button_by_function(func_t::SpeedDial).status == led_button_status_t::LED_ON)
				{	//если ПА вкл., набираем прямого.
					//если нет прямого, пробуем спикер
					if (!sip->find_user_phone_by_button_call(event.index))
						if (sip->m_device->m_buttons[event.index].function == func_t::SpeeckerPhone)
							function_dial(event.index, sip);
					sip->m_device->led_button_off(func_t::SpeedDial);
				}
				else
				{	//сначала функцию, потом прямого
					if (sip->m_device->m_buttons[event.index].function == func_t::NoFunction)
						function_dial(event.index, sip);
					else
						sip->find_user_phone_by_button_call(event.index);
				}
			}
			break;
		}
		sip->phone_status_define();
	}
}
/************************************************************************************/

void S2D(sip_t* sip)
{
	vector<int> vButton;
	sip_cmd_t cmd;
	bool tp = false;

	while (true) {
		if (sip->get_command(&cmd))
		{
			switch (cmd.ev)
			{
			case sip_ev_t::Calling:
				sip->phone_status_define();
				sip->m_route.status = false;
				sip->m_device->led_button_off(func_t::SpeedDial);
				sip->m_device->led_button_off(func_t::Call_Horn);
				sip->m_device->led_button_off(func_t::Call_Speecker);
				sip->m_device->led_button_off(func_t::Call_Mic);

				vButton = sip->m_device->find_button_by_phone_presents(cmd.portB, true);
				for (size_t b = 0; b < vButton.size(); b++)
				{
					if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0	/*не для кнопок с Орех*/
						|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6) {
						sip->m_device->led_button_off(static_cast<func_t>(vButton[b]), true);
						sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_BUSY;
					}
				}
				break;
			case sip_ev_t::Early:
				sip->phone_status_define();

				vButton = sip->m_device->find_button_by_phone_presents(cmd.portB, true);
				for (size_t b = 0; b < vButton.size(); b++)
				{
					if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0	/*не для кнопок с Орех*/
						|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6) {
						sip->m_device->led_button_on(static_cast<func_t>(vButton[b]), true);
						sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_BUSY;
					}
				}
				break;
			case sip_ev_t::InCall:
				sip->phone_status_define();

				vButton = sip->m_device->find_button_by_phone_presents(cmd.portB, true);
				for (size_t b = 0; b < vButton.size(); b++)
				{
					if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0	/*не для кнопок с Орех*/
						|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6) {
						sip->m_device->led_button_flash(static_cast<func_t>(vButton[b]), true);
						sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_CALL;
					}
				}
				break;

			case sip_ev_t::Connect:
				sip->m_call_last.call_type = call_type_t::IN;
				if (format_addr(cmd.portA) == ip_address)/*PTP server*/
					sip->m_call_last.portA = "local";
				else
					sip->m_call_last.portA = format_addr(cmd.portA);
				sip->m_call_last.portB = format_addr(cmd.portB);
				sip->m_call_last.call_msg = "base";
				sip->phone_status_define();

				vButton = sip->m_device->find_button_by_phone_presents(cmd.portB, true);
				for (size_t b = 0; b < vButton.size(); b++)
				{
					if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0	/*не для кнопок с Орех*/
						|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6) {
						sip->m_device->led_button_on(static_cast<func_t>(vButton[b], true));
						sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_BUSY;
					}
				}
				break;

			case sip_ev_t::Disconnect:
				sip->phone_status_define();

				vButton = sip->m_device->find_button_by_phone_presents(cmd.portB, true);
				for (size_t b = 0; b < vButton.size(); b++)
				{
					sip->m_device->led_button_off(static_cast<func_t>(vButton[b], true));
					sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::LED_OFF;
					/*для Ореха сигнал Busy ре подаётся*/
					if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0	/*не для кнопок с Орех*/
						|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6)
						tp = true;;
				}
				if (tp)
					sip->get_tone()->busy();
				break;

			case sip_ev_t::CallToError:
				sip->get_tone()->busy();
				sip->m_route.status = false;
				sip->m_device->led_button_off(func_t::SpeedDial);
				sip->m_device->led_button_off(func_t::Call_Horn);
				sip->m_device->led_button_off(func_t::Call_Speecker);
				sip->m_device->led_button_off(func_t::Call_Mic);

				sip->phone_status_define();

				vButton = sip->m_device->find_button_by_phone_presents(cmd.portB, true);
				for (size_t b = 0; b < vButton.size(); b++)
				{
					sip->m_device->led_button_off(static_cast<func_t>(vButton[b], true));
					sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::LED_OFF;
				}
				break;

			case sip_ev_t::OnHold:
				sip->phone_status_define();
				sip->m_device->led_button_flash(func_t::Hold);
				sip->m_count_hold++;
				break;

			case sip_ev_t::OffHold:
				sip->phone_status_define();
				sip->m_count_hold--;
				if (sip->m_count_hold == 0)
					sip->m_device->led_button_off(func_t::Hold);
				break;

			case sip_ev_t::AutoAnswer:
				sip->cmd_answer();
				sip->phone_status_define();
				break;

			case sip_ev_t::Mic:
				sip->phone_status_define();
				break;

			case sip_ev_t::Horn:
				sip->cmd_hungup_all();
				sip->m_horn_state = true;
				sip->phone_status_define();
				break;

			case sip_ev_t::Speaker:
				sip->phone_status_define();
				break;

			case sip_ev_t::Opex:
				vButton = sip->m_device->find_button_by_phone_presents(cmd.portA, true);
				for (size_t b = 0; b < vButton.size(); b++)
					if (static_cast<int>(sip->m_device->m_buttons[vButton[b]].function) ==
						static_cast<int>(func_t::Opex_0) + atoi(cmd.portB.c_str()) + 1) {
						sip->m_device->led_button_flash(static_cast<func_t>(vButton[b], true));
						sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_CALL;
					}
				break;
			case sip_ev_t::Presents:
				//CallID - present status
				vButton = sip->m_device->find_button_by_phone_presents(cmd.portA, true);

				for (size_t b = 0; b < vButton.size(); b++)
				{
					if (cmd.call == sip_pres_st::Fail) {
						sip->m_device->led_button_off(static_cast<func_t>(vButton[b], true));
						sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::LED_OFF;
					}
					else if (cmd.call == sip_pres_st::Busy
						&& sip->m_device->m_buttons[vButton[b]].status != led_button_status_t::SUBS_CALL
						&& sip->m_device->m_buttons[vButton[b]].status != led_button_status_t::LED_FLASH) {
						/*не для кнопок с Орех*/
						if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0
							|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6) {
							sip->m_device->led_button_on(static_cast<func_t>(vButton[b], true));
							sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_BUSY;
						}
						/*Орех, cmd.portB - это номер канала*/
						else if (static_cast<int>(sip->m_device->m_buttons[vButton[b]].function) ==
							static_cast<int>(func_t::Opex_0) + atoi(cmd.portB.c_str()) + 1) {
							sip->m_device->led_button_on(static_cast<func_t>(vButton[b], true));
							sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::SUBS_BUSY;
						}
					}
					else if (cmd.call == sip_pres_st::Idle) { //PRES_STATUS_IDLE
						if (sip->m_device->m_buttons[vButton[b]].function < func_t::Opex_0
							|| sip->m_device->m_buttons[vButton[b]].function > func_t::Opex_6) {
							sip->m_device->led_button_off(static_cast<func_t>(vButton[b], true));
							sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::LED_OFF;
						}
						else if (static_cast<int>(sip->m_device->m_buttons[vButton[b]].function) == 
							static_cast<int>(func_t::Opex_0) + atoi(cmd.portB.c_str()) + 1) {
							sip->m_device->led_button_off(static_cast<func_t>(vButton[b], true));
							sip->m_device->m_buttons[vButton[b]].status = led_button_status_t::LED_OFF;
						}
					}
				}
				break;
			}
		}
		else break;
	}
}
/************************************************************************************************************/

void T2D(sip_t* sip)
{
		string CallID = "";
		string Message = "";
		string strCommand = "";
		
		/*очистить очередь тестов*/
		sip->cq_timeout_remove();

		/*работа с маркерами и удаление неотвеченных*/
		sip->marker_work();

		//FirstDigit
		sip->first_digit_exec();

		//InterDigit
		sip->inter_digit_exec();

		//Опрос базы
		if (get_time_ms() - requestDB >= REQUEST_DB)
		{
			if (!setts.licence)
			{
				std::cout << "**************** LICENSE FAIL! ****************" << endl;
				terminated = 12;
			}
			requestDB = time(NULL);
			reload_param(sip);
		}

		/*для задержки после последнего звонка (сфдд_ещ раскоментировать)*/
		//if (sip->m_sip_sleep == true) {
		//	if (sip->m_last_disconnect > 0)
		//		sip->m_last_disconnect--;
		//	else {
		//		sip->m_last_disconnect = 10;
		//		sip->m_sip_sleep = false;
		//	}
		//}

}
/******************************************************************/

void blink(gpio_t* gpio)
{
	if (!f_blink) {
		if (get_time_ms() - t_blink > 50) {
			gpio->set(GLedRed, LOW);
			t_blink = get_time_ms();
			f_blink = ON;
		}
	}
	else {
		if (get_time_ms() - t_blink > 2500) {
			gpio->set(GLedRed, HIGH);
			t_blink = get_time_ms();
			f_blink = OFF;
		}
	}
	app_control();
}