#pragma once

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <mutex>
#include <map>
#include <memory>

/*авто конфигурирование big/little endian*/
#define PJ_AUTOCONF	1
#include <pjsua2.hpp>
#include <pjsua-lib/pjsua_internal.h>

#include "classes.h"
#include "device.h"
#include "tones.h"

#define NumberAccount 		4

#define SIP_VER_PTP 		0
#define SIP_VER_1 			1
#define SIP_VER_2 			2

#define OPUS_SIMPLE_RATE	24000
#define OPUS_COMPLEXITY		8
#define DSP_SIMPLE_RATE	22050

#define TMR_CQ				3
#define WAIT_MARKER			3000
#define SEND_MARKER			700


enum class call_type_t
{
	IN,
	OUT,
	NO_ANSWER
};

enum class phone_status_t
{
	IDLE = 0,
	CONNECT,
	OUTCALL,
	EARLY,
	INCALL,
	CONNECT_EARLY,
	CONNECT_INCALL,
	OUTCALL_INCALL,
	EARLY_INCALL,
	CONNECT_EARLY_INCALL
};

/*события SIP*/
enum class sip_ev_t
{
	Early = 0,
	Calling,
	Connect,
	InCall,
	Disconnect,
	Horn,
	Speaker,
	Mic,
	Presents,
	Reg,
	XFer,
	OnHold,
	OffHold,
	Confirm,
	Mixer,
	CallToError,
	Simplex,
	AutoAnswer,
	Opex
};

enum class call_status_t
{
	Base = 0,
	OnHold_demand,
	OnHold,
	OffHold_demand,
	OffHold,
	xFer,
	Mic,
	Horn,
	Speaker,
	Transfered
};



/*сообщения (sms) SIP*/
namespace sip_msg
{
	const std::string Marker("mrk");
	const std::string MarkerResponse("mrk_res");
	const std::string CallTest("call_test");
	const std::string CallTestResponse("call_test_res");
	const std::string Base("base");
	const std::string Horn("horn");
	const std::string Speaker("speaker");
	const std::string Mic("mic");
	const std::string Mix_1("mix_1");
	const std::string Mix_2("mix_2");
	const std::string Mix_3("mix_3");
	const std::string Mix_4("mix_4");
	const std::string Mix_all("mix_all");
	const std::string Simplex_Rec("simplex_rec");
	const std::string Simplex_Tran("simplex_tran");
	const std::string Opex_1("opex_1");
	const std::string Opex_2("opex_2");
	const std::string Opex_3("opex_3");
	const std::string Opex_4("opex_4");
	const std::string Opex_5("opex_5");
	const std::string Opex_6("opex_6");
}

/*Presents status*/
namespace sip_pres_st
{
	const std::string Fail("133");
	const std::string Idle("134");
	const std::string Busy("135");

}

/*заголовки классов*/
class player_t;
class account_t;
class call_t;
class sip_t;
class buddy_t;

/* structures */
//команда от SIP к устройству
struct sip_cmd_t
{
	sip_ev_t ev;
	std::string call;
	std::string portA;
	std::string portB;
};

//Результат поиска маршрута
struct find_route_t
{
	bool status = false;
	int account = -1;
	int num_digit= 0;
};

struct crdnt_call_t
{
	call_t* call = nullptr;
	int count_acc;
	int count_call;
};

struct acc_set_t
{
	std::string phone;
	std::string sipserver;
	std::string password;
	int sip_ver;
	bool auto_answer;
};

struct call_queue_t
{
	std::string portB;
	std::string call_type;
	time_t time_test_start;
	call_queue_t() {
		time(&time_test_start);
	}
};

struct dial_num_last_t
{
	call_type_t call_type;
	std::string portA;
	std::string portB;
	std::string call_msg;
};

/*основные настройки из БД*/
struct publ_settings_t
{
	bool licence = false;
	bool multiline = true;
	bool fl_record = true;
	bool fl_eth_control = true;
	int first_digit = 10;
	int inter_digit = 6;
	int call_no_answer = 40;

	std::string ring_ton = "";
	std::string vmsg_1 = "Tones/VMsg_1.wav";
	std::string vmsg_2 = "Tones/VMsg_1.wav";
	std::string vmsg_3 = "Tones/VMsg_1.wav";
	std::string vmsg_4 = "Tones/VMsg_1.wav";
	std::string vmsg_5 = "Tones/VMsg_1.wav";

	float capture_adjust = 5.0f;
	float capture_adjust_off = 0.01f;
	float pb_adjust = 1.0f;
	float pb_adjustOff = 0.1f;

	int aec_gain_mic1 = 6;
	int aec_gain_mic2 = 6;
	int aec_noise_mic1 = -30;
	int aec_agc1 = 0;
	int aec_agc_target_1 = 3;
	int aec_his1 = 1;
	int aec_gain_step_mic1 = 0;
};



/////////////////////////////////////////////////////////////////
///////////////// myPlayer ///////////////////////////////////////
class player_t : public pj::AudioMediaPlayer
{
public:
	bool m_status = false;
	bool m_noloop = false;
	AudioMedia* m_aud_med = nullptr;
	bool m_autoanswer = false;

	player_t() {}
	~player_t() {}

	bool init_player(std::string wav_path, bool no_loop = false)
	{
		m_noloop = no_loop;
		if (m_noloop)
			createPlayer(wav_path, PJMEDIA_FILE_NO_LOOP);
		else
			createPlayer(wav_path);
		return true;
	}

	virtual void onEof2()
	{
		if (m_noloop)
		{
			stopTransmit(pj::Endpoint::instance().audDevManager().getPlaybackDevMedia());
			if (m_aud_med)
				if (m_aud_med->getPortId() > -1)
					stopTransmit(*m_aud_med);
			m_status = false;
			setPos(0);
		}
		return;
	}
};

////////////////////////////////////////////////////////////////
//////////////// MyAccount ////////////////////////////////////
class account_t : public pj::Account
{
public:
	std::vector<call_t*>		m_calls;
	std::vector<buddy_t*>		m_buddys;
	acc_set_t					m_acc_set;
	bool						m_active = false;
	bool						m_buddy_reg = false;
	pj::PresenceStatus			m_pres_st;
	std::vector<button_t>* m_buttons;

public:
	account_t()	{}
	~account_t() {}

	void init(sip_t* sip, std::vector<button_t>* buttons) {
		m_sip = sip;
		m_buttons = buttons;
	}
	bool create_acc(bool flModify = false);
	void close_acc();
	void buddy_register(std::string phone, int sip_ver, bool monitor);
	void set_status(pjsua_buddy_status status = (pjsua_buddy_status)0,
		pjrpid_activity activity = (pjrpid_activity)0, std::string note = "old");
	bool call_to(std::string portB, std::string system_message, bool to_list = true);
	bool answer(std::string portB);
	void hold(int callID);
	void unhold(int callID);
	void remove_call(call_t* call);
	bool bye_call(std::string callID);
	void bye_call_all();
	void mute(bool state);
	void DTMF(std::string digit);
	void simplex(std::string direction);
	sip_t* get_sip() { return m_sip; }

	virtual void onRegState(pj::OnRegStateParam& prm);
	virtual void onIncomingCall(pj::OnIncomingCallParam& iprm);
	void onInstantMessage(pj::OnInstantMessageParam& prm);

	bool lst_call_query_access(call_queue_t* cq, std::string portB, access_t acc);

private:
	sip_t* m_sip;
	std::vector<call_queue_t>	lst_CQ;
	std::mutex					m_mxCQ;
};

////////////////////////////////////////////////////////////////
///////////// MySIP ///////////////////////////////////////////
class sip_t
{
public:
	pj::Endpoint				m_ep;
	pj::EpConfig				m_ep_cfg;
	pj::TransportConfig			m_trcfg;
	pj::AudioMedia&				m_playback = pj::Endpoint::instance().audDevManager().getPlaybackDevMedia();
	pj::AudioMedia&				m_capture = pj::Endpoint::instance().audDevManager().getCaptureDevMedia();
	
	std::array<account_t, NumberAccount> m_accounts;
	device_t*					m_device;
	uint32_t					m_port = 5060;
	std::string					m_path = "";
	std::string					m_protocol;
	std::string					m_log;
	int							m_log_level = 0;
	std::string					m_hold_tone;
	bool						m_fl_xfer = false;
	bool						m_fl_conf_port = false;
	bool						m_fl_dial_dtmf = false;
	std::atomic <bool>			m_calling_sleep;
	find_route_t				m_route;
	publ_settings_t*			m_setts;
	bool						m_md_call_horn = false;
	bool						m_md_call_speecker = false;
	bool						m_md_call_mic = false;
	std::string					m_dial_number = "";
	dial_num_last_t				m_call_last;
	bool						m_horn_state = false;
	bool						m_opex_state = false;
	int							m_count_hold = 0;
	std::atomic <bool>			m_sip_sleep;
	int							m_last_disconnect = 25;
	
	player_t* plrHold;
	player_t* plrRing;
	player_t* plrVMsg_1;
	player_t* plrVMsg_2;
	player_t* plrVMsg_3;
	player_t* plrVMsg_4;
	player_t* plrVMsg_5;
	
	sip_t(std::string path) {
		m_calling_sleep = false;
		m_path = path;
	}
	bool init(float PBAdjust, float CaptureAdjust,
		publ_settings_t* setts, device_t* device, tones_t* tone);
	bool codec_set(bool init = false);
	void close();
	bool add_command(sip_cmd_t* s2d) { return lst_s2d_access(s2d, access_t::WRITE); }
	bool get_command(sip_cmd_t* s2d) { return lst_s2d_access(s2d, access_t::READ); }
	bool del_command() { return lst_s2d_access(nullptr, access_t::DELETE); }
	void voiceIP(std::string ipa);
	int calculate_calls();
	crdnt_call_t get_crdnt_call(std::string call_id_str);
	bool find_rout(std::string dial_number);
	tones_t* get_tone() { return m_tone; }
	bool players_init();
	void player_destroy();
	void ring_play();
	void ring_stop();
	bool account_create(std::string ip_address, bool modify = false);
	bool account_close();
	bool reload_buddys();
	crdnt_call_t get_first_call(bool forUnhold = true);
	crdnt_call_t get_last_call(bool forUnhold = true);
	std::string find_call(std::string port);
	void hang_phone();
	void cq_timeout_remove();
	void marker_work();
	void first_digit_exec();
	void inter_digit_exec();

	void cmd_call(std::string portA, std::string portB, std::string type);
	void cmd_hold();
	void cmd_unhold();
	void cmd_hungup(std::string call);
	void cmd_hungup_all();
	void cmd_answer(std::string portB="");
	void cmd_exit();
	void cmd_mute(std::string call);
	void cmd_simplex(std::string direction);
	void cmd_dtmf(std::string portB);

	void mk_hold();
	void mk_xfer();
	void mk_xfer_execute(std::string source_id);
	void mk_conf(bool spkr = false);
	bool mk_conf_port(int button_id);

	void vmsg_play(player_t* plrVMsg, uint8_t numb);
	bool find_user_phone_by_dial_number(std::string dial_number, bool half = false);
	bool find_user_phone_by_button_call(int button_id, std::string mixer = "");
	
	void first_digit_start() { m_firstdigit_start = true; time(&m_firstdigit_begin); }
	void first_digit_stop() { m_firstdigit_start = false; }
	void inter_digit_start() { m_interdigit_start = true; time(&m_interdigit_begin); }
	void inter_digit_stop() { m_interdigit_start = false; }

	void phone_status_define();
	void set_phone_status(phone_status_t status);
	phone_status_t get_phone_state() { return m_phone_state.load(); }
	void set_phone_state(phone_status_t status) { m_phone_state.store(status); }

private:
	std::atomic<phone_status_t>	m_phone_state;
	tones_t*					m_tone;
	std::list<sip_cmd_t>		m_lst_s2d;
	std::mutex					m_mx_s2d;
	bool						lst_s2d_access(sip_cmd_t* s2d, access_t access);
	pj::AudioMediaPlayer*		m_pl_voiceIP;
	std::atomic<bool>			m_firstdigit_start;
	std::atomic<bool>			m_interdigit_start;
	time_t						m_firstdigit_begin = time(NULL);
	time_t						m_interdigit_begin = time(NULL);
	
	void fill_playlist(std::vector<std::string>* pllist, std::string fname);
	void parse_one_part_addr(std::vector<std::string>* pllist, std::string partIP, int numberPart);
};


////////////////////////////////////////////////////////////////
//***************** MyCall********************************
class call_t : public pj::Call
{
public:
	account_t*					m_acc;
	std::string					m_mix_channel_number = "00";
	call_status_t				m_call_status = call_status_t::Base;
	std::string					m_call_type = sip_msg::Base;
	std::string					m_xfer_CallID = "";
	std::string					m_port_to = "";
	std::atomic<int>			m_button_conf_port_id;
	uint64_t					m_time_marker = get_time_ms();
	uint64_t					m_time_marker_response = get_time_ms();
	time_t						m_time_start = time(nullptr);
	bool						m_in_call = false;
	bool						m_erly_second = false;
	bool						m_tm_noanswer = false;
	bool						m_fl_wait_xfer_source = false;
	bool						m_fl_wait_xfer_target = false;
	std::string					m_pair_call_xfer = "";

	call_t(account_t& acc, int call_id = PJSUA_INVALID_ID)
		: Call(acc, call_id)
	{
		m_acc = static_cast<account_t*>(&acc);
		m_button_conf_port_id = -1;
	}
	~call_t(){}

	virtual void onCallState(pj::OnCallStateParam& prm);
	virtual void onCallMediaState(pj::OnCallMediaStateParam& prm);
	void onInstantMessage(pj::OnInstantMessageParam& prm);
	void onCallTransferRequest(pj::OnCallTransferRequestParam& prm);
	void onDtmfDigit(pj::OnDtmfDigitParam& prm);
	static std::string time_to_string(int sec);
	std::string local_time_to_string();
	void delete_call();

private:
	std::unique_ptr<pj::AudioMediaRecorder> m_recorder =
					std::make_unique<pj::AudioMediaRecorder>();
};

////////////// MyBody //////////////////////////////////////
class buddy_t : public pj::Buddy
{
public:
	int m_sip_ver;
	buddy_t(int SIPver, sip_t* sip)
		: pj::Buddy()
	{
		m_sip_ver = SIPver;
		m_sip = sip;
	}
	~buddy_t() {}
	virtual void onBuddyState();

private:
	sip_t* m_sip;
};


//---------------------------------------------------------------