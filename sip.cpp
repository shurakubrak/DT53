#include "sip.h"
#include "sqlite_db.h"
#include <cassert> 

using namespace std;
using namespace pj;
////////////////////////////////////////////////////////////////
//***************** MySIP ***************************
//**************************************************************
///////////////////////////////////////////////////////////////

bool sip_t::init(float PBAdjust, float CaptureAdjust,
	publ_settings_t* setts, device_t* device, tones_t* tone)
{
	m_setts = setts;
	m_device = device;
	m_tone = tone;
	for (size_t i = 0; i < NumberAccount; i++) 
		m_accounts[i].init(this, &m_device->m_buttons);
	
	m_ep.libCreate();
	
	if (m_log == "Y") {
		m_ep_cfg.logConfig.filename = "./THE.log";
		m_ep_cfg.logConfig.consoleLevel = m_log_level;
		m_ep_cfg.logConfig.level = m_log_level;
		cout << "Log 'Y'" << " file: './THE.log'" << endl;
	}
	else {
		m_ep_cfg.logConfig.filename = "";
		m_ep_cfg.logConfig.consoleLevel = 0;
		m_ep_cfg.logConfig.level = 0;
		cout << "Log 'N'" << endl;
	}
	m_ep_cfg.uaConfig.maxCalls = PJSUA_MAX_CALLS;
	m_ep_cfg.uaConfig.threadCnt = 1;
	m_ep_cfg.uaConfig.mainThreadOnly = true;
	m_ep_cfg.medConfig.noVad = true;

	m_ep_cfg.medConfig.clockRate = DSP_SIMPLE_RATE;//PJSUA_DEFAULT_CLOCK_RATE;
	m_ep_cfg.medConfig.sndClockRate = DSP_SIMPLE_RATE;
	m_ep_cfg.medConfig.channelCount = 1;

	// Задать параметры эхокомпенсации
	m_ep_cfg.medConfig.ecTailLen = PJSUA_DEFAULT_EC_TAIL_LEN;
	m_ep_cfg.medConfig.ecOptions = PJMEDIA_ECHO_SPEEX
		| PJMEDIA_ECHO_NO_LOCK
		| PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR
		| PJMEDIA_ECHO_AGGRESSIVENESS_DEFAULT;
	// или выкл. эхо опции
	//ep_cfg.medConfig.ecOptions = 0;			

	m_ep.libInit(m_ep_cfg);
	codec_set(true);

	// Create SIP transport.
	m_trcfg.port = 5060;
	if (m_protocol == "UDP")
		m_ep.transportCreate(PJSIP_TRANSPORT_UDP, m_trcfg);
	else
		m_ep.transportCreate(PJSIP_TRANSPORT_TCP, m_trcfg);

	// Start library
	m_ep.libStart();

	
	AudioDevInfoVector2 aud_devs = m_ep.instance().audDevManager().enumDev2();
	
	/*для вывода аудио устройств раскоментировать*/
	//cout << "Audio devices:" << endl;
	//for (unsigned int i = 0; i < aud_devs.size(); i++)
	//	cout << aud_devs[i].name << " | "
	//	<< aud_devs[i].inputCount << " | "
	//	<< aud_devs[i].outputCount << " | "
	//	<< aud_devs[i].driver << endl;
	//cout << endl;
		// Установить нужное медиа
	//ep.instance().audDevManager().setCaptureDev(3);
	//ep.instance().audDevManager().setPlaybackDev(2);

	m_playback = m_ep.instance().audDevManager().getPlaybackDevMedia();
	m_playback.adjustRxLevel(PBAdjust);
	m_capture = m_ep.instance().audDevManager().getCaptureDevMedia();
	m_capture.adjustTxLevel(CaptureAdjust);
	return true;
}
//------------------------------------------------------------------

bool sip_t::codec_set(bool init)
{
	CodecInfoVector2 codecs = m_ep.codecEnum2();

	//all codecs priority to NULL
	for (unsigned int i = 0; i < codecs.size(); i++)
		m_ep.codecSetPriority(codecs[i].codecId, 0);

	sqlite_db_t db(m_path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from codec_priority";
	if (!db.exec_sql())
		cout << "SQLite ERROR: read table codec_priority error" << endl;

	else {
		do {
			string codec_name = db.field_by_name("codecId");
			string codec_priority = db.field_by_name("priority");
			for (unsigned int i = 0; i < codecs.size(); i++)
				if (codecs[i].codecId == codec_name)
					m_ep.codecSetPriority(codecs[i].codecId, atoi(codec_priority.c_str()));
		} while (db.next_rec());
	}
	db.close_db();

	codecs = m_ep.codecEnum2();
	/*для вывода списка кодеков раскоментировать*/
	//cout << endl;
	//cout << "CODECS LIST:" << endl;
	//for (auto c : codecs) {
	//	cout << " - " << c.codecId << " (priority: " << static_cast<int>(c.priority) << ")" << endl;
	//}
	//cout << endl;

	if (init) {
		extern struct pjsua_data pjsua_var;
		pjmedia_codec_mgr* codec_mgr;
		const pjmedia_codec_info* info;
		pjmedia_codec_param param;
		pjmedia_codec_opus_config opus_cfg;
		unsigned count = 1;
		pj_status_t status;
		pj_str_t codec_id;

		string str = "opus/48000/2";
		codec_id.ptr = strdup(str.c_str());
		codec_id.slen = 12;
		codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt);

		status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, &codec_id,
			&count, &info, NULL);
		if (status != PJ_SUCCESS) {
			cout << "pjmedia_codec_mgr_find_codecs_by_id FAIL" << endl;
			return false;
		}

		status = pjmedia_codec_mgr_get_default_param(codec_mgr, info, &param);
		if (status != PJ_SUCCESS) {
			cout << "pjmedia_codec_mgr_get_default_param FAIL" << endl;
			return false;
		}

		status = pjmedia_codec_opus_get_config(&opus_cfg);
		if (status != PJ_SUCCESS) {
			cout << "pjmedia_codec_opus_get_config FAIL" << endl;
			return false;
		}

		opus_cfg.sample_rate = OPUS_SIMPLE_RATE;
		opus_cfg.complexity = OPUS_COMPLEXITY;
		opus_cfg.channel_cnt = 1;

		status = pjmedia_codec_opus_set_default_param(&opus_cfg, &param);
		if (status != PJ_SUCCESS) {
			cout << "pjmedia_codec_opus_set_default_param FAIL" << endl;
			return false;
		}
	}
	return true;
}
//------------------------------------------------------------------

void sip_t::close()
{
	m_ep.libStopWorkerThreads();
	m_ep.libDestroy();
}
//----------------------------------------------------------------

void sip_t::fill_playlist(std::vector<std::string>* pllist, string fname)
{
	FILE* f;
	f = fopen((m_path + "VoiceIP/" + fname).c_str(), "r");
	if (f == nullptr || f == 0)
		return;

	pllist->push_back(m_path + "VoiceIP/" + fname);
}
//-------------------------------------------------------------------

void sip_t::parse_one_part_addr(std::vector<std::string>* pllist, string partIP, int numberPart)
{
	string file_name = "";
	if (partIP == "000" || partIP == "00" || partIP == "0")	{
		file_name = "0.wav";
		fill_playlist(pllist, file_name);
		if (numberPart < 4)
			fill_playlist(pllist, "empty.wav");
		return;
	}
	for (size_t k = 0; k < partIP.length(); k++) {
		int pos = partIP.length() - k - 1;
		if (pos == 1 && partIP[k] == '1') {
			//nameFile = "" namefile.  + curQ[k] + curQ[k + 1] + ".wav";
			//nameFile = to_string(numberPart) + "_";
			file_name += partIP[k];
			file_name += partIP[k + 1];
			file_name += ".wav";
			k++;
		}
		else {
			if (partIP[k] != '0')
				file_name = partIP[k] + string(pos, '0') + ".wav";
		}
		if (file_name.length() > 0)
			fill_playlist(pllist, file_name);
		file_name = "";

	}
	if (numberPart < 4)
		fill_playlist(pllist, "empty.wav");
}
//-------------------------------------------------------------------


bool sip_t::lst_s2d_access(sip_cmd_t* s2d, access_t access)
{
	bool ret;

	unique_lock<mutex> locker(m_mx_s2d);
	switch (access) {
	case access_t::WRITE:
		m_lst_s2d.push_back(*s2d);
		ret = true;
		break;
	case access_t::READ:
		if (m_lst_s2d.size() > 0) {
			*s2d = m_lst_s2d.front();
			m_lst_s2d.pop_front();
			ret = true;
		}
		else
			ret = false;
		break;
	case access_t::DELETE:
		m_lst_s2d.clear();
		break;
	}
	return ret;
}
//----------------------------------------------------------------------------

void sip_t::voiceIP(string ipa)
{
	string curQ = "";
	string file_name = "";
	StringVector pllist;

	int numQ = 0;
	for (size_t i = 0; i < ipa.length(); i++) {
		if (ipa[i] == '.') {
			numQ++;
			parse_one_part_addr(&pllist, curQ, numQ);
			curQ = "";
		}
		else
			curQ += ipa[i];
	}
	parse_one_part_addr(&pllist, curQ, 4);


	if (pllist.size() > 0) {
		if (m_pl_voiceIP != nullptr) 
			m_pl_voiceIP = nullptr;
		m_pl_voiceIP = new AudioMediaPlayer();
		m_pl_voiceIP->createPlaylist(pllist, "", PJMEDIA_FILE_NO_LOOP);
		m_pl_voiceIP->startTransmit(m_playback);
	}
}
//--------------------------------------------

int sip_t::calculate_calls()
{
	int num_calls = 0;
	for (int i = 0; i < NumberAccount; i++) {
		if (m_accounts[i].m_active)
			num_calls += m_accounts[i].m_calls.size();
	}
	return num_calls;
}
//-------------------------------------------------------

crdnt_call_t sip_t::get_crdnt_call(string call_id_str)
{
	crdnt_call_t cc;

	if (!call_id_str.empty()) {
		for (int i = 0; i < NumberAccount; i++)
			for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++) {
				if (!m_accounts[i].m_calls[a]->isActive())
					continue;
				if (m_accounts[i].m_calls[a]->getInfo().callIdString == call_id_str) {
					cc.call = m_accounts[i].m_calls[a];
					cc.count_acc = i;
					cc.count_call = a;
					return cc;
				}
			}
	}
	return cc;
}
//----------------------------------------------------------

bool sip_t::find_rout(std::string dial_number)
{
	sqlite_db_t db(m_path + "settings.s3db");
	if (!db.open_db())
		return false;
	//маршрутизация
	db.m_sql_str = "select * from lcr where \
			dial_from <='" + dial_number +
		"' and (dial_to || 'я') >='" + dial_number +
		"' and length(dial_from) <=" + to_string(dial_number.length());
	if (db.exec_sql()) {
		m_route.account = atoi(db.field_by_name("account").c_str());
		m_route.num_digit = atoi(db.field_by_name("num_digits").c_str());
		m_route.status = true;
		db.close_db();
		return true;
	}
	db.close_db();
	return false;
}
/*******************************************/

bool sip_t::players_init()
{
	if (plrRing != nullptr) {
		//delete plrRing;
		plrRing = nullptr;
	}
	plrRing = new player_t();
	if (plrRing)
		if (!plrRing->init_player(m_path + m_setts->ring_ton))
			return false;

	if (plrHold != nullptr) {
		//delete plrHold;
		plrHold = nullptr;
	}
	plrHold = new player_t();
	if (plrHold)
		if (!plrHold->init_player(m_path + m_hold_tone))
			return false;

	if (plrVMsg_1 != nullptr) {
		//delete plrVMsg_1;
		plrVMsg_1 = nullptr;
	}
	plrVMsg_1 = new player_t();
	if (plrVMsg_1)
		if (!plrVMsg_1->init_player(m_path + m_setts->vmsg_1))
			return false;

	if (plrVMsg_2 != nullptr) {
		//delete plrVMsg_2;
		plrVMsg_2 = nullptr;
	}
	plrVMsg_2 = new player_t();
	if (plrVMsg_2)
		if (!plrVMsg_2->init_player(m_path + m_setts->vmsg_2))
			return false;

	if (plrVMsg_3 != nullptr) {
		//delete plrVMsg_3;
		plrVMsg_3 = nullptr;
	}
	plrVMsg_3 = new player_t();
	if (plrVMsg_3)
		if (!plrVMsg_3->init_player(m_path + m_setts->vmsg_3))
			return false;

	if (plrVMsg_4 != nullptr) {
		//delete plrVMsg_4;
		plrVMsg_4 = nullptr;
	}
	plrVMsg_4 = new player_t();
	if (plrVMsg_4)
		if (!plrVMsg_4->init_player(m_path + m_setts->vmsg_4))
			return false;

	if (plrVMsg_5 != nullptr) {
		//delete plrVMsg_5;
		plrVMsg_5 = nullptr;
	}
	plrVMsg_5 = new player_t();
	if (plrVMsg_5)
		if (!plrVMsg_5->init_player(m_path + m_setts->vmsg_5))
			return false;
	return true;
}
/*************************************************************/

void sip_t::player_destroy()
{
	plrRing = nullptr;
	plrHold = nullptr;
	plrVMsg_1 = nullptr;
	plrVMsg_2 = nullptr;
	plrVMsg_3 = nullptr;
	plrVMsg_4 = nullptr;
	plrVMsg_5 = nullptr;
}
/*********************************************************/

void sip_t::ring_play()
{
	if (!plrRing->m_status)	{
		plrRing->startTransmit(m_playback);
		plrRing->setPos(0);
		plrRing->m_status = true;
	}
}
/*************************************************************/

void sip_t::ring_stop()
{
	if (plrRing->getPortId() > -1) {
		plrRing->m_status = false;
		plrRing->stopTransmit(m_playback);
	}
}
/**********************************************************/

bool sip_t::account_create(string ip_address, bool modify)
{
	sqlite_db_t db(m_path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from operators";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read table operators error" << endl;
		db.close_db();
		return false;
	}
	m_accounts[0].m_acc_set.phone = ip_address;
	m_accounts[0].m_acc_set.sipserver = "local";
	m_accounts[0].m_acc_set.sip_ver = SIP_VER_PTP;
	m_accounts[0].create_acc(modify);
	if (!m_accounts[0].m_buddy_reg && m_accounts[0].m_active)
	{
		//Загружаем BUDDY только один раз
		m_accounts[0].m_buddy_reg = true;

		//Получаем всех абонентов, которые есть под кнопкой.
		//добаляем как Buddy
		for (int i = 0; i < NumberButtons; i++)	{
			if (m_device->m_buttons[i].server == m_accounts[0].m_acc_set.sipserver 
				&& !m_device->m_buttons[i].phone.empty())
				m_accounts[0].buddy_register(m_device->m_buttons[i].phone, SIP_VER_PTP, true);
		}

		//Получаем всех абонентов,
		//кроме тех, которые под кнопкой,
		//добавляем как Buddy их системные номера
		db.m_sql_str = "select * from v_users_button \
							where button_id IS NULL";
		if (db.exec_sql()) {
			int i = 0;
			do {
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

				if (!Phone.empty())
					m_accounts[0].buddy_register(Phone, SIP_VER_PTP, true);

				i++;
			} while (db.next_rec());
		}
	}
	else {
		db.close_db();
		return false;
	}

	std::cout << "System account settings: " << "ph: " <<
		m_accounts[0].m_acc_set.phone <<
		", srv: " << m_accounts[0].m_acc_set.sipserver <<
		", sip: " << m_accounts[0].m_acc_set.sip_ver <<
		", auto-answer: " << m_accounts[0].m_acc_set.auto_answer << endl;
	std::cout << "System account registered OK. Number buddys "
		<< m_accounts[0].enumBuddies2().size() << endl;


	db.m_sql_str = "select * from operator_phones order by id";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read table error" << endl;
		db.close_db();
		return false;
	}
	int i = 1;
	do {
		m_accounts[i].m_acc_set.phone = db.field_by_name("phone");

		if (!m_accounts[i].m_acc_set.phone.empty())	{
			m_accounts[i].m_acc_set.sipserver = db.field_by_name("sipserver");
			m_accounts[i].m_acc_set.password = db.field_by_name("password");
			m_accounts[i].m_acc_set.sip_ver = atoi(db.field_by_name("sip_ver").c_str());
			m_accounts[i].m_acc_set.auto_answer = db.atob(db.field_by_name("auto_answer"));

			if (m_accounts[i].create_acc(modify))
				std::cout << "Account_" << i << " settings: " <<
				"ph: " <<
				m_accounts[i].m_acc_set.phone <<
				", srv: " << m_accounts[i].m_acc_set.sipserver <<
				", sip: " << m_accounts[i].m_acc_set.sip_ver <<
				", awr: " << m_accounts[i].m_acc_set.auto_answer << endl;
			else
				std::cout << "Account[" << i << "]-" << m_accounts[i].m_acc_set.phone
						<< " FAIL create" << endl;
		}
		i++;
	} while (db.next_rec() && i < NumberAccount);
	db.close_db();
	return true;
}
/******************************************************************/

bool sip_t::account_close()
{
	m_ep.hangupAllCalls();

	for (unsigned int a = 0; a < NumberAccount; a++) {
		for (unsigned int i = 0; i < m_accounts[a].m_buddys.size(); i++)
			delete m_accounts[a].m_buddys[i];
		m_accounts[a].m_buddys.clear();
	}

	for (int i = 0; i < NumberAccount; i++)
		m_accounts[i].m_buddy_reg = false;

	for (int i = 0; i < NumberAccount; i++) {
		m_accounts[i].close_acc();
		if (m_accounts[i].m_active && i>0)
			m_accounts[i].setRegistration(false);
	}
	//Пауза для завершения отключения
	m_ep.libHandleEvents(1000);
	return true;
}
/***************************************/

bool sip_t::reload_buddys()
{
	//Очищаем все Buddy
	for (int i = 0; i < NumberAccount; i++)	{
		for (unsigned int a = 0; a < m_accounts[i].m_buddys.size(); a++)
			delete m_accounts[i].m_buddys[a];
		m_accounts[i].m_buddys.clear();
	}

	//Добавляем Buddy для системного аккаунда
	if (m_accounts[0].m_active)	{
		//Получаем всех абонентов, которые есть под кнопкой.
		//добаляем как Buddy
		for (int i = 0; i < NumberButtons; i++)	{
			if (m_device->m_buttons[i].server == m_accounts[0].m_acc_set.sipserver &&
				!m_device->m_buttons[i].phone.empty())
				m_accounts[0].buddy_register(m_device->m_buttons[i].phone, SIP_VER_PTP, true);
		}

		//Получаем всех абонентов,
		//кроме тех, которые под кнопкой,
		//добавляем как Buddy их системные номера
		sqlite_db_t db(m_path + "settings.s3db");
		if (!db.open_db())
			return false;
		db.m_sql_str = "select * from v_users_button \
							where button_id IS NULL";
		if (db.exec_sql()){
			int i = 0;
			do {
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

				if (!Phone.empty())
					m_accounts[0].buddy_register(Phone, SIP_VER_PTP, true);

				i++;
			} while (db.next_rec());
		}
		db.close_db();
	}
	else
		return false;

	//Добавляем остальные
	for (int i = 1; i < NumberAccount; i++)
		if (m_accounts[i].m_active)
			for (int a = 0; a < NumberButtons; a++)
				if (m_device->m_buttons[a].server == m_accounts[i].m_acc_set.sipserver)
					m_accounts[i].buddy_register(m_device->m_buttons[a].phone,
						m_accounts[i].m_acc_set.sip_ver, true);
	return true;
}
/*********************************************/
crdnt_call_t sip_t::get_first_call(bool forUnhold)
{
	crdnt_call_t lc;
	time_t tm = 0;

	for (int i = 0; i < NumberAccount; i++)
		for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++) {
			if (!m_accounts[i].m_calls[a]->isActive())
				continue;
			if (forUnhold) {
				if ((m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
					m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONNECTING) &&
					m_accounts[i].m_calls[a]->m_call_status == call_status_t::OnHold) {
					if (tm) {
						if (tm > m_accounts[i].m_calls[a]->m_time_start) {
							tm = m_accounts[i].m_calls[a]->m_time_start;
							lc.call = m_accounts[i].m_calls[a];
							lc.count_acc = i;
							lc.count_call = a;
						}
					}
					else {
						tm = m_accounts[i].m_calls[a]->m_time_start;
						lc.call = m_accounts[i].m_calls[a];
						lc.count_acc = i;
						lc.count_call = a;
					}
				}
			}
			else
			{
				if (m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
					m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONNECTING) {
					if (tm)	{
						if (tm > m_accounts[i].m_calls[a]->m_time_start) {
							tm = m_accounts[i].m_calls[a]->m_time_start;
							lc.call = m_accounts[i].m_calls[a];
							lc.count_acc = i;
							lc.count_call = a;
						}
					}
					else {
						tm = m_accounts[i].m_calls[a]->m_time_start;
						lc.call = m_accounts[i].m_calls[a];
						lc.count_acc = i;
						lc.count_call = a;
					}
				}
			}
		}
	return lc;
}
/**********************************************************/

crdnt_call_t sip_t::get_last_call(bool forUnhold)
{
	crdnt_call_t lc;
	time_t tm = 0;

	for (int i = 0; i < NumberAccount; i++)
		for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++)	{
			if (!m_accounts[i].m_calls[a]->isActive())
				continue;
			if ((m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
				m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONNECTING) &&
				m_accounts[i].m_calls[a]->m_call_status != call_status_t::OnHold) {
				if (tm)	{
					if (tm < m_accounts[i].m_calls[a]->m_time_start) {
						tm = m_accounts[i].m_calls[a]->m_time_start;
						lc.call = m_accounts[i].m_calls[a];
						lc.count_acc = i;
						lc.count_call = a;
					}
				}
				else {
					tm = m_accounts[i].m_calls[a]->m_time_start;
					lc.call = m_accounts[i].m_calls[a];
					lc.count_acc = i;
					lc.count_call = a;
				}
			}
		}
	return lc;
}
/*******************************************************/

string sip_t::find_call(string port)
{
	for (int i = 0; i < NumberAccount; i++)	{
		for (size_t c = 0; c < m_accounts[i].m_calls.size(); c++) {
			if (!m_accounts[i].m_calls[c]->isActive())
				continue;
			CallInfo ci = m_accounts[i].m_calls[c]->getInfo();
			if (format_addr(ci.remoteUri) == port)
				return ci.callIdString;
		}
	}
	return "";
}
/**************************************************************/

void sip_t::cmd_hungup(string call)
{
	for (int i = 0; i < NumberAccount; i++)	{
		if (m_accounts[i].m_calls.size())
			if (m_accounts[i].bye_call(call))
				break;
	}
}
/***************************************************/

void sip_t::cmd_hungup_all()
{
	for (int i = 0; i < NumberAccount; i++)
		if (m_accounts[i].m_active)
			m_accounts[i].bye_call_all();
}
/*****************************************/

void sip_t::cmd_exit()
{
	for (int i = 0; i < NumberAccount; i++)
		m_accounts[i].close_acc();
}
/****************************************/

void sip_t::cmd_answer(string portB)
{
	for (int i = 0; i < NumberAccount; i++) {
		if (m_accounts[i].answer(portB))
			break;
	}
}
/*******************************************/

void sip_t::cmd_call(string portA, string portB, string type)
{
	first_digit_stop();
	for (int i = 0; i < NumberAccount; i++) {
		if (m_accounts[i].m_acc_set.sipserver == portA
			&& m_accounts[i].m_active) {
			if (type.find("opex", 0) != string::npos)
				/*вызов на Орех даём без теста*/
				m_accounts[i].call_to(portB, type, true);
			else
				m_accounts[i].call_to(portB, type);
			m_call_last.call_type = call_type_t::OUT;
			m_call_last.portA = portA;
			m_call_last.portB = portB;
			m_call_last.call_msg = type;
			m_dial_number = "";
			return;
		}
	}
	//сюда попадём только если так и не набрали
	sip_cmd_t s2d;
	s2d.ev = sip_ev_t::Disconnect;
	s2d.portB = portB;
	add_command(&s2d);
}
/******************************************************/

void sip_t::cmd_hold()
{
	crdnt_call_t cc = get_last_call();
	if (cc.call == nullptr)
		return;
	if (cc.call)
		m_accounts[cc.count_acc].hold(cc.count_call);
}
/****************************************************/

void sip_t::cmd_unhold()
{
	crdnt_call_t cc = get_first_call();
	if (cc.call == nullptr)
		return;
	if (cc.call)
		m_accounts[cc.count_acc].unhold(cc.count_call);
}
/**************************************************/

void sip_t::cmd_mute(string call)
{
	for (int i = 0; i < NumberAccount; i++)
		if (m_accounts[i].m_active) {
			if (call == "1")
				m_accounts[i].mute(true);
			else
				m_accounts[i].mute(false);
		}
}
/*************************************************/

void sip_t::cmd_simplex(string direction)
{
	m_accounts[0].simplex(direction);
}
/*********************************************************/

void sip_t::cmd_dtmf(string portB)
{
	for (int i = 0; i < NumberAccount; i++) {
		if (m_accounts[i].m_active)
			m_accounts[i].DTMF(portB);
	}
}
/***********************************************************/

void sip_t::vmsg_play(player_t* plrVMsg, uint8_t numb)
{
	bool fl = false;
	AudioMedia* aud_med;
	/* Приграть сообщение */
	if (plrVMsg->m_status == OFF) {
		for (int i = 0; i < NumberAccount; i++)	{
			for (size_t a = 0; a < m_accounts[i].m_calls.size(); a++) {
				if (!m_accounts[i].m_calls[a]->isActive())
					continue;
				CallInfo ci = m_accounts[i].m_calls[a]->getInfo();
				//only for answered calls
				if (m_accounts[i].m_calls[a]->m_call_status != call_status_t::OnHold
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::OffHold_demand
					&& !m_accounts[i].m_calls[a]->m_fl_wait_xfer_target
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::xFer
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::Transfered
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::Horn
					&& !(m_accounts[i].m_calls[a]->m_in_call && ci.state == PJSIP_INV_STATE_EARLY)) {
					for (unsigned b = 0; b < ci.media.size(); b++) {
						if (ci.media[b].type == PJMEDIA_TYPE_AUDIO 
							&& m_accounts[i].m_calls[a]->getMedia(b)) {
							aud_med = (AudioMedia*)m_accounts[i].m_calls[a]->getMedia(b);
							aud_med->stopTransmit(m_playback);
							m_capture.stopTransmit(*aud_med);
							plrVMsg->startTransmit(*aud_med);
							fl = true;
						}
					}
				}

			}
		}
		if (fl) {
			plrVMsg->m_status = ON;
			plrVMsg->startTransmit(m_playback);
			plrVMsg->setPos(0);
			switch (numb) {
			case 1:
				m_device->led_button_on(func_t::VMsg_1);
				break;
			case 2:
				m_device->led_button_on(func_t::VMsg_2);
				break;
			case 3:
				m_device->led_button_on(func_t::VMsg_3);
				break;
			case 4:
				m_device->led_button_on(func_t::VMsg_4);
				break;
			case 5:
				m_device->led_button_on(func_t::VMsg_5);
				break;
			}
		}
	}
	/* Остановить сообщение*/
	else if (plrVMsg->getPortId() > -1) {
		plrVMsg->m_status = OFF;
		plrVMsg->stopTransmit(m_playback);
		switch (numb) {
		case 1:
			m_device->led_button_off(func_t::VMsg_1);
			break;
		case 2:
			m_device->led_button_off(func_t::VMsg_2);
			break;
		case 3:
			m_device->led_button_off(func_t::VMsg_3);
			break;
		case 4:
			m_device->led_button_off(func_t::VMsg_4);
			break;
		case 5:
			m_device->led_button_off(func_t::VMsg_5);
			break;
		}
		for (int i = 0; i < NumberAccount; i++)	{
			for (size_t a = 0; a < m_accounts[i].m_calls.size(); a++) {
				if (!m_accounts[i].m_calls[a]->isActive())
					continue;
				CallInfo ci = m_accounts[i].m_calls[a]->getInfo();
				//only for answered calls
				if (m_accounts[i].m_calls[a]->m_call_status != call_status_t::OnHold
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::OffHold_demand
					&& !m_accounts[i].m_calls[a]->m_fl_wait_xfer_target
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::xFer
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::Transfered
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::Horn
					&& !(m_accounts[i].m_calls[a]->m_in_call && ci.state == PJSIP_INV_STATE_EARLY)) {
					for (unsigned b = 0; b < ci.media.size(); b++) {
						if (ci.media[b].type == PJMEDIA_TYPE_AUDIO 
							&& m_accounts[i].m_calls[a]->getMedia(b)) {
							aud_med = (AudioMedia*)m_accounts[i].m_calls[a]->getMedia(b);
							aud_med->startTransmit(m_playback);
							m_capture.startTransmit(*aud_med);
							plrVMsg->stopTransmit(*aud_med);
						}
					}
				}
			}
		}
	}
	std::cout << "plrVMsg->status: " << plrVMsg->m_status << endl;
}
/*****************************************************************/

void sip_t::mk_hold()
{
	switch (get_phone_state()) {
	case phone_status_t::IDLE:
		cmd_unhold();
		break;
	case phone_status_t::CONNECT:
		cmd_hold();
		break;
	case phone_status_t::CONNECT_INCALL:
		cmd_hold();
		break;
	case phone_status_t::CONNECT_EARLY:
		cmd_hold();
		break;
	}
}
/*******************************************************************/

void sip_t::mk_xfer()
{
	if (!m_fl_xfer)	{
		crdnt_call_t lc = get_last_call();
		if (lc.call->isActive()) {
			cmd_hold();
			lc.call->m_fl_wait_xfer_target = true;
			lc.call->m_fl_wait_xfer_source = false;
			m_fl_xfer = true;
			first_digit_start();
		}
	}
	else {
		m_fl_xfer = false;
		for (int i = 0; i < NumberAccount; i++)
			for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++)
				if (m_accounts[i].m_calls[a]->m_fl_wait_xfer_target) {
					crdnt_call_t target = get_crdnt_call(m_accounts[i].m_calls[a]->m_pair_call_xfer);
					if (target.call) {
						pj::CallOpParam prm;
						prm.statusCode = PJSIP_SC_OK;
						target.call->hangup(prm);
					}

					m_accounts[i].m_calls[a]->m_fl_wait_xfer_target = false;
					m_accounts[i].m_calls[a]->m_pair_call_xfer = "";
					m_accounts[i].unhold(a);
				}
	}
}
/**************************************************************/

void sip_t::mk_xfer_execute(string source_id)
{
	CallOpParam prm;
	crdnt_call_t source;
	crdnt_call_t target;

	m_fl_xfer = false;

	source = get_crdnt_call(source_id);
	if (source.call == NULL)
		return;
	source.call->m_fl_wait_xfer_target = false;
	source.call->m_fl_wait_xfer_source = false;

	target = get_crdnt_call(source.call->m_pair_call_xfer);
	if (target.call == NULL)
		return;
	target.call->m_fl_wait_xfer_source = false;
	target.call->m_fl_wait_xfer_target = false;

	if (source.call->m_acc->m_acc_set.sip_ver == SIP_VER_PTP ||
		target.call->m_acc->m_acc_set.sip_ver == SIP_VER_PTP ||
		source.count_acc != target.count_acc) {
		for (unsigned i = 0; i < source.call->getInfo().media.size(); i++) {
			if (source.call->getInfo().media[i].type == PJMEDIA_TYPE_AUDIO &&
				source.call->getMedia(i)) {
				AudioMedia* source_aud_med = (AudioMedia*)source.call->getMedia(i);
				if (target.call->getInfo().media.size()) { // если target уже ответил
					for (unsigned a = 0; a < target.call->getInfo().media.size(); a++)
						if (target.call->getInfo().media[a].type == PJMEDIA_TYPE_AUDIO &&
							target.call->getMedia(a)) {
							AudioMedia* target_aud_med = (AudioMedia*)target.call->getMedia(a);

							//соединим абонентов
							source_aud_med->startTransmit(*target_aud_med);
							target_aud_med->startTransmit(*source_aud_med);
							cout << "CONN A-B" << endl;
							//отключаемся от абонентов
							source_aud_med->stopTransmit(m_playback);
							target_aud_med->stopTransmit(m_playback);
							m_capture.stopTransmit(*source_aud_med);
							m_capture.stopTransmit(*target_aud_med);
							cout << "DISCONN A, B" << endl;
							//отметит звонки как csxFered
							source.call->m_call_status = call_status_t::xFer;
							target.call->m_call_status = call_status_t::xFer;
							//отбиваемся
							hang_phone();
						}
				}
				else if (target.call->getInfo().state == PJSIP_INV_STATE_EARLY)	{ 
					//если target ещё не ответил
					//отключаемся от абонентa source_call
					source_aud_med->stopTransmit(m_playback);
					m_capture.stopTransmit(*source_aud_med);
					//дадим КПВ в сторону source_call
					m_tone->ring_back_start_to(*source_aud_med);
					//остановим RingBack от dest к себе
					m_tone->tones_long_stop();
					//отметит target, что бы ждать его ответ и звонки как csxFered
					target.call->m_fl_wait_xfer_source = true;
					source.call->m_call_status = call_status_t::xFer;
					target.call->m_call_status = call_status_t::xFer;
					//отбиваемся
					hang_phone();
				}
			}
		}
	}
	else {
		//prm.statusCode = PJSIP_SC_OK;
		//prm.options = PJSUA_XFER_NO_REQUIRE_REPLACES;
		//target.call->hangup(prm);
		//source.call->xfer(target.call->getInfo().remoteUri, prm);
		source.call->xferReplaces(*target.call, prm);
	}
}
/**************************************************************/

void sip_t::mk_conf(bool spkr)
{
	sqlite_db_t db(m_path + "settings.s3db");
	if (!db.open_db())
		return;
	db.m_sql_str = "select * from v_conf_members_users";
	if (!db.exec_sql()) {
		std::cout << "SQLite ERROR: read table error" << endl;
		db.close_db();
		return;
	}

	string Command = "";
	int i = 0;
	do {
		string dial = "";
		int sip_ver = atoi(db.field_by_name("sip_ver").c_str());
		int sip_ver2 = atoi(db.field_by_name("sip_ver2").c_str());
		int sip_ver_mob = atoi(db.field_by_name("sip_ver_mob").c_str());
		int sip_ver_home = atoi(db.field_by_name("sip_ver_home").c_str());
		int sip_ver_work = atoi(db.field_by_name("sip_ver_work").c_str());

		if (sip_ver == SIP_VER_PTP)
			dial = db.field_by_name("server");
		else if (sip_ver2 == SIP_VER_PTP)
			dial = db.field_by_name("server2");
		else if (sip_ver_mob == SIP_VER_PTP)
			dial = db.field_by_name("server_mob");
		else if (sip_ver_home == SIP_VER_PTP)
			dial = db.field_by_name("server_home");
		else if (sip_ver_work == SIP_VER_PTP)
			dial = db.field_by_name("server_work");

		if (!dial.empty()) {
			if (spkr)
				cmd_call(m_accounts[0].m_acc_set.sipserver, dial, "horn");
			else
				cmd_call(m_accounts[0].m_acc_set.sipserver, dial, "base");
		}

		i++;
	} while (db.next_rec());
	db.close_db();
}
/*****************************************************************************/

bool sip_t::mk_conf_port(int button_id)
{
	bool alow_add = false;

	phone_status_t ps = get_phone_state();
	if (ps == phone_status_t::CONNECT ||
		ps == phone_status_t::CONNECT_EARLY ||
		ps == phone_status_t::CONNECT_EARLY_INCALL ||
		ps == phone_status_t::OUTCALL ||
		ps == phone_status_t::EARLY ||
		ps == phone_status_t::OUTCALL_INCALL ||
		ps == phone_status_t::EARLY_INCALL)	{
		for (int i = 0; i < NumberAccount; i++)	{
			for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++)	{
				if (!m_accounts[i].m_calls[a]->isActive())
					continue;
				if (m_accounts[i].m_calls[a]->m_call_status != call_status_t::OnHold
					&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::Transfered) {
					if (m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
						m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONNECTING ||
						(m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_EARLY
							&& !m_accounts[i].m_calls[a]->m_in_call)) {
						alow_add = true;
						for (unsigned int y = 0; y < m_device->m_buttons[button_id].calls.size(); y++)
							if (m_device->m_buttons[button_id].calls[y] == m_accounts[i].m_calls[a]) {
								alow_add = false;
								break;
							}
						if (alow_add) {
							for (size_t l = 0; l < m_accounts[i].m_calls[a]->getInfo().media.size(); l++) {
								if (m_accounts[i].m_calls[a]->getInfo().media[l].type == PJMEDIA_TYPE_AUDIO 
									&& m_accounts[i].m_calls[a]->getMedia(l)) {
									AudioMedia* aud_med_lastadd = (AudioMedia*)m_accounts[i].m_calls[a]->getMedia(l);
									for (size_t z = 0; z < m_device->m_buttons[button_id].calls.size(); z++)
										if (m_device->m_buttons[button_id].calls[z] != m_accounts[i].m_calls[a]) {
											for (size_t m = 0; m < m_device->m_buttons[button_id].calls[z]->getInfo().media.size(); m++) {
												if (m_device->m_buttons[button_id].calls[z]->getInfo().media[m].type == PJMEDIA_TYPE_AUDIO
													&& m_device->m_buttons[button_id].calls[z]->getMedia(m)) {
													AudioMedia* aud_med = (AudioMedia*)m_device->m_buttons[button_id].calls[z]->getMedia(m);
													aud_med->startTransmit(*aud_med_lastadd);
													aud_med_lastadd->startTransmit(*aud_med);

													aud_med->startTransmit(m_playback);
													m_capture.startTransmit(*aud_med);
												}
											}
										}
								}
							}
							m_device->m_buttons[button_id].calls.push_back(m_accounts[i].m_calls[a]);
							m_accounts[i].m_calls[a]->m_call_status = call_status_t::Transfered;
							m_accounts[i].m_calls[a]->m_button_conf_port_id = button_id;
						}
					}
				}
			}
		}
	}
	if (ps == phone_status_t::IDLE ||
		ps == phone_status_t::OUTCALL ||
		ps == phone_status_t::EARLY ||
		ps == phone_status_t::OUTCALL_INCALL ||
		ps == phone_status_t::EARLY_INCALL)
	{
		alow_add = false;
		for (unsigned int z = 0; z < m_device->m_buttons[button_id].calls.size(); z++)
			for (unsigned m = 0; m < m_device->m_buttons[button_id].calls[z]->getInfo().media.size(); m++) {
				if (m_device->m_buttons[button_id].calls[z]->getInfo().media[m].type == PJMEDIA_TYPE_AUDIO 
					&& m_device->m_buttons[button_id].calls[z]->getMedia(m)) {
					AudioMedia* aud_med = (AudioMedia*)m_device->m_buttons[button_id].calls[z]->getMedia(m);
					aud_med->startTransmit(m_playback);
					m_capture.startTransmit(*aud_med);
					alow_add = true;
				}
			}
	}
	return alow_add;
}
/***********************************************************************/

bool sip_t::find_user_phone_by_dial_number(string dial_number, bool half)
{
	string Server = "";
	string strCommand = "";
	string SipVer = "";
	string speaker_status = "";
	string Mixer = "N";

	if (dial_number.empty())
		return false;
	sqlite_db_t db(m_path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from users where \
			phone='" + dial_number +
		"' or phone_2='" + dial_number +
		"' or phone_mob='" + dial_number +
		"' or phone_home='" + dial_number +
		"' or phone_work='" + dial_number + "'";

	if (!db.exec_sql())	{	//	OUTSIDE
		db.close_db();
		if (half)
			return false; //если smart-dial и не нашел

		if (m_accounts[1].m_active)
			Server = m_accounts[1].m_acc_set.sipserver;
		else if (m_accounts[2].m_active)
			Server = m_accounts[2].m_acc_set.sipserver;
		else if (m_accounts[3].m_active)
			Server = m_accounts[3].m_acc_set.sipserver;
		else {
			std::cout << "SIP ERROR: no active account" << endl;
			return false;
		}
		cmd_call(Server, dial_number, "base");
		return true;
	}

	if (db.field_by_name("phone") == dial_number) {
		Server = db.field_by_name("server");
		SipVer = db.field_by_name("sip_ver");
		Mixer = db.field_by_name("is_mixer");
	}
	else if (db.field_by_name("phone_2") == dial_number) {
		Server = db.field_by_name("server_2");
		SipVer = db.field_by_name("sip_ver_2");
	}
	else if (db.field_by_name("phone_mob") == dial_number) {
		Server = db.field_by_name("server_mob");
		SipVer = db.field_by_name("sip_ver_mob");
	}
	else if (db.field_by_name("phone_home") == dial_number) {
		Server = db.field_by_name("server_home");
		SipVer = db.field_by_name("sip_ver_home");
	}
	else if (db.field_by_name("phone_work") == dial_number) {
		Server = db.field_by_name("server_work");
		SipVer = db.field_by_name("sip_ver_work");
	}
	db.close_db();

	if (SipVer == "0") { //SIP_VER_PTP
		dial_number = Server;
		Server = m_accounts[0].m_acc_set.sipserver;
	}
	else if (Server == "outside") {
		if (m_accounts[1].m_active)
			Server = m_accounts[1].m_acc_set.sipserver;
		else if (m_accounts[2].m_active)
			Server = m_accounts[2].m_acc_set.sipserver;
		else
			return false;
	}

	if (Mixer == "Y")
		speaker_status = "mix_all";
	else if (m_md_call_horn)
		speaker_status = "horn";
	else if (m_md_call_speecker)
		speaker_status = "speaker";
	else if (m_md_call_mic)
		speaker_status = "mic";
	else
		speaker_status = "base";

	if (!dial_number.empty()) {
		cmd_call(Server, dial_number, speaker_status);
		return true;
	}
	else
		return false;
}
/*************************************************************/

bool sip_t::find_user_phone_by_button_call(int button_id, string mixer)
{
	string Phone = "";
	string server = "";
	string strCommand = "";
	string SipVer = "";
	string speaker_status = "";

	try {
		for (int i = 0; i < NumberButtons; i++)
			if (m_device->m_buttons[i].buttonID == button_id) {
				Phone = m_device->m_buttons[i].phone;
				server = m_device->m_buttons[i].server;
			}

		if (server == "outside") {
			if (m_accounts[1].m_active)
				server = m_accounts[1].m_acc_set.sipserver;
			else if (m_accounts[2].m_active)
				server = m_accounts[2].m_acc_set.sipserver;
			else if (m_accounts[3].m_active)
				server = m_accounts[3].m_acc_set.sipserver;
			else {
				return false;
			}
		}

		if (!mixer.empty())
			speaker_status = mixer;
		else if (m_md_call_horn)
			speaker_status = "horn";
		else if (m_md_call_speecker)
			speaker_status = "speaker";
		else if (m_md_call_mic)
			speaker_status = "mic";
		else
			speaker_status = "base";

		if (!Phone.empty()) {
			m_dial_number = Phone;
			cmd_call(server, Phone, speaker_status);
			return true;
		}
		//cout<<"FindUserPhoneByButtonCall stop"<<endl;
		return false;
	}
	catch (...) {
		cout << "Driver::FindUserPhoneByButtonCall fail" << "\n";
		return "";
	}
}
/********************************************************/

void sip_t::hang_phone()
{
	first_digit_stop();
	inter_digit_stop();

	for (int i = 0; i < NumberAccount; i++)	{
		for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++) 	{
			if (!m_accounts[i].m_calls[a]->isActive())
				continue;
			if (m_accounts[i].m_calls[a]->m_call_status == call_status_t::Transfered) {
				for (unsigned l = 0; l < m_accounts[i].m_calls[a]->getInfo().media.size(); l++)	{
					if (m_accounts[i].m_calls[a]->getInfo().media[l].type == PJMEDIA_TYPE_AUDIO 
						&& m_accounts[i].m_calls[a]->getMedia(l)){
						AudioMedia* aud_med = (AudioMedia*)m_accounts[i].m_calls[a]->getMedia(l);
						aud_med->stopTransmit(m_playback);
						m_capture.stopTransmit(*aud_med);

					}
				}
			}
		}
	}

	m_device->m_horn_state = false;
	if (m_device->m_kp->lst_key_down().size() == 0)
		m_device->m_opex_state = false;

	m_device->led_button_off(func_t::SpeeckerPhone);// led "SPEAKER" off
	m_device->led_button_off(func_t::OK);
	m_device->m_gpio->set(GLedGreen, LOW);

	m_dial_number = "";
	m_fl_conf_port = false;
	m_route.status = false;
	m_fl_dial_dtmf = false;

	m_device->led_button_off(func_t::SpeedDial);
	m_device->led_button_off(func_t::DTMF);
	m_device->led_button_off(func_t::Call_Horn);
	m_device->led_button_off(func_t::Call_Speecker);
	m_device->led_button_off(func_t::Call_Mic);
}
/*********************************************************************/

void sip_t::phone_status_define()
{
	int number_calls = 0;
	int conn = 0;
	int in = 0;
	int out = 0;
	int early = 0;
	for (int i = 0; i < NumberAccount; i++)	{
		for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++)	{
			if (!m_accounts[i].m_calls[a]->isActive())
				continue;

			if (m_accounts[i].m_calls[a]->m_call_status != call_status_t::OnHold
				&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::xFer
				&& m_accounts[i].m_calls[a]->m_call_status != call_status_t::Transfered) {
				number_calls++;

				if (m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
					m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CONNECTING)
					conn++;
				else if (m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_EARLY) {
					if (m_accounts[i].m_calls[a]->m_in_call)
						in++;
					else
						early++;
				}
				else if (m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_INCOMING)
					in++;
				else if (m_accounts[i].m_calls[a]->getInfo().state == PJSIP_INV_STATE_CALLING) {
					if (!m_accounts[i].m_calls[a]->m_in_call)
						out++;
				}
			}
		}
	}


	//no calls
	if (number_calls == 0 && !m_firstdigit_start && m_dial_number.empty() && !m_fl_conf_port)
		set_phone_status(phone_status_t::IDLE);
	else {
		// Easy statuses
		if (!conn && !out && !early && !in && !m_fl_conf_port)
			set_phone_status(phone_status_t::IDLE);

		else if (!conn && out && !early && !in)
			set_phone_status(phone_status_t::OUTCALL);

		else if (!conn && !out && early && !in)
			set_phone_status(phone_status_t::EARLY);

		else if (!conn && !out && !early && in == 1)
			set_phone_status(phone_status_t::INCALL);

		else if ((conn || m_fl_conf_port) && !out && !early && !in)
			set_phone_status(phone_status_t::CONNECT);

		// Composite statuses
		else if ((conn || m_fl_conf_port) && early && !in)
			set_phone_status(phone_status_t::CONNECT_EARLY);

		else if ((conn || m_fl_conf_port) && !early && in)
			set_phone_status(phone_status_t::CONNECT_INCALL);

		else if ((conn || m_fl_conf_port) && early && in)
			set_phone_status(phone_status_t::CONNECT_EARLY_INCALL);

		else if (!conn && out > 0 && !early && in)
			set_phone_status(phone_status_t::OUTCALL_INCALL);

		else if (!conn && early && in)
			set_phone_status(phone_status_t::EARLY_INCALL);
	}
}
/***********************************************************************/

void sip_t::set_phone_status(phone_status_t status)
{
	set_phone_state(status);
	//cout << "PHONE STATUS: " << itoa(status) << endl;

	/*для звонка на рупор выкл. мик.*/
	if (m_horn_state)
		m_device->device_write(dev_cmd_t::Mic, static_cast<int>(mic_select_t::MIC_OFF));
	else
		m_device->device_write(dev_cmd_t::Mic, static_cast<int>(mic_select_t::MIC_HF));

	/*при вызове на Орех откл. динамик*/
	if (m_opex_state)
		m_device->device_write(dev_cmd_t::AMP, static_cast<int>(amp_select_t::AMP_OFF));
	else
		m_device->device_write(dev_cmd_t::AMP, static_cast<int>(amp_select_t::AMP_HF));

	switch (status)	{
	case phone_status_t::IDLE:
		if (!m_firstdigit_start && !m_interdigit_start)	{
			get_tone()->tones_long_stop();
			ring_stop();
			hang_phone();
			m_device->led_button_off(func_t::SpeeckerPhone);
		}
		else {
			if (m_firstdigit_start)
				get_tone()->ready_start();
			else
				get_tone()->tones_long_stop();
			m_device->led_button_on(func_t::SpeeckerPhone);
		}
		break;
 
	case phone_status_t::OUTCALL:
		ring_stop();
		get_tone()->ring_back_start();
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;

	case phone_status_t::EARLY:
		ring_stop();
		get_tone()->ring_back_start();
		m_device->led_button_off(func_t::OK);
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;

	case phone_status_t::INCALL:
		first_digit_stop();
		get_tone()->tones_long_stop();
		ring_play();
		m_device->led_button_flash(func_t::SpeeckerPhone);
		break;

	case phone_status_t::CONNECT:
		ring_stop();
		get_tone()->tones_long_stop();
		m_device->led_button_on(func_t::SpeeckerPhone);
		m_device->m_gpio->set(GLedGreen, HIGH);
		break;

		////// Composite statuses
	case phone_status_t::CONNECT_EARLY:
		ring_stop();
		get_tone()->ring_back_start();
		m_device->led_button_off(func_t::OK);
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;

	case phone_status_t::CONNECT_INCALL:
		ring_stop();
		get_tone()->ring_second_start();
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;

	case phone_status_t::CONNECT_EARLY_INCALL:
		ring_stop();
		get_tone()->ring_back_start();
		get_tone()->ring_second_start();
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;

	case phone_status_t::OUTCALL_INCALL:
		ring_stop();
		get_tone()->ring_second_start();
		m_device->led_button_off(func_t::OK);
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;

	case phone_status_t::EARLY_INCALL:
		ring_stop();
		get_tone()->ring_second_start();
		get_tone()->ring_back_start();
		m_device->led_button_off(func_t::OK);
		m_device->led_button_on(func_t::SpeeckerPhone);
		break;
	}

	//	cout << "HR|HF|HS     " << HornStatus << "|"
	//			<< SpeakerStatus << "|" << HSStatus << endl;
}
/***********************************************************/

void sip_t::cq_timeout_remove()
{
	if (m_accounts[0].lst_call_query_access(nullptr, "", access_t::DELETE)) {
		get_tone()->busy();
		phone_status_define();
	}
}
/***********************************************************/

void sip_t::marker_work()
{
	SendInstantMessageParam msg_prm;
	for (int i = 0; i < NumberAccount; i++)	{
		for (unsigned int a = 0; a < m_accounts[i].m_calls.size(); a++)	{
			if (!m_accounts[i].m_calls[a]->isActive())
				continue;
			//Work with of marker
			if (m_accounts[i].m_acc_set.sip_ver == SIP_VER_PTP)	{
				//send marker (only outgoing side)
				if (!m_accounts[i].m_calls[a]->m_in_call
					&& (get_time_ms() - m_accounts[i].m_calls[a]->m_time_marker)
					>= SEND_MARKER)	{
					m_accounts[i].m_calls[a]->m_time_marker = get_time_ms();
					msg_prm.content = sip_msg::Marker;
					m_accounts[i].m_calls[a]->sendInstantMessage(msg_prm);
				}
				//control marker
				//for outgoing side - reset 'time_marker' in 'onInstantMessageStatus'
				//for incomming side - reset in 'onInstantMessage'
				if (get_time_ms() - m_accounts[i].m_calls[a]->m_time_marker_response 
					>= WAIT_MARKER)	{
					cmd_hungup(m_accounts[i].m_calls[a]->getInfo().callIdString);
					continue;
				}
			}

			/*удаление неотвеченных ИСХОДЯЩИХ звонков*/
			if (!m_accounts[i].m_calls[a]->isActive())
				continue;
			if (!m_accounts[i].m_calls[a]->m_in_call 
				&& m_accounts[i].m_calls[a]->m_tm_noanswer)	{
				if ((time(NULL) - m_accounts[i].m_calls[a]->m_time_start) 
					>= m_setts->call_no_answer)	{
					m_accounts[i].m_calls[a]->m_tm_noanswer = false;
					m_dial_number = "";
					m_route.status = false;
					cmd_hungup(m_accounts[i].m_calls[a]->getInfo().callIdString);
				}
			}
		}
	}
}
/*************************************************/

void sip_t::first_digit_exec()
{
	if (m_firstdigit_start)	{
		if ((time(NULL) - m_firstdigit_begin) >= m_setts->first_digit) {
			first_digit_stop();
			get_tone()->busy();
			hang_phone();
			get_tone()->tones_stop();
			phone_status_define();
		}
	}
}
/****************************************************/

void sip_t::inter_digit_exec()
{
	if (m_interdigit_start)
		if ((time(NULL) - m_interdigit_begin) >= m_setts->inter_digit) {
			inter_digit_stop();
			if (!m_dial_number.empty())	{ 	//	dial this number
				first_digit_stop();

				if (find_user_phone_by_dial_number(m_dial_number)) {
					m_device->led_button_off(func_t::OK);
					m_device->led_button_off(func_t::SpeedDial);
				}
				else {
					get_tone()->busy();
					hang_phone();
					phone_status_define();
				}
			}
			else {
				hang_phone();
				get_tone()->tones_long_stop();
				phone_status_define();
			}
		}
}
