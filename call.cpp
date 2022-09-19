#include "sip.h"
#include "tones.h"
#include "device.h"
#include "utils.h"

using namespace std;
using namespace pj;
////////////////////////////////////////////////////////////////
//***************** MyCall**************************
//**************************************************************
/////////////////////////////////////////////////////////////
void call_t::delete_call()
{
	sip_cmd_t cmd;
	CallOpParam cop_prm;
	crdnt_call_t cc;
	string duration;

	if (m_call_status == call_status_t::xFer) {
		cc = m_acc->get_sip()->get_crdnt_call(m_pair_call_xfer);
		if (cc.call != nullptr)
			cc.call->hangup(cop_prm);
	}

	if (m_call_status == call_status_t::OnHold) {
		cmd.ev = sip_ev_t::OffHold;
		m_acc->get_sip()->add_command(&cmd);
	}

	CallInfo ci = getInfo();
	duration = time_to_string(ci.connectDuration.sec);
	cmd.ev = sip_ev_t::Disconnect;
	cmd.call = ci.callIdString;
	cmd.portA = duration;
	cmd.portB = ci.remoteUri;
	m_acc->get_sip()->add_command(&cmd);
	/*снять флаг Horn, чтобы вкл. микр.*/
	if (m_call_status == call_status_t::Horn)
		m_acc->get_sip()->m_device->m_horn_state = false;
	m_acc->remove_call(this);
	if (m_acc->m_calls.size() == 0) {
		m_acc->set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_AWAY, "");
	}

	delete this;
}
//--------------------------------------------------------------

void call_t::onCallState(OnCallStateParam& prm)
{
	sip_cmd_t cmd;
	SendInstantMessageParam prm_msg;
	CallOpParam cop_prm;
	crdnt_call_t cc;

	string cID_old = "";
	string cID_new = "";

	PJ_UNUSED_ARG(prm);
	CallInfo ci = getInfo();
	string duration = time_to_string(ci.connectDuration.sec);

		switch (ci.state) {
		case PJSIP_INV_STATE_CONNECTING:
			m_acc->set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_BUSY, 
				format_addr(ci.remoteUri));
			cmd.ev = sip_ev_t::Connect;
			cmd.call = ci.callIdString;
			cmd.portA = ci.localUri;
			cmd.portB = ci.remoteUri;
			m_acc->get_sip()->add_command(&cmd);

			if (m_tm_noanswer)
				m_tm_noanswer = false;
			break;

		case  PJSIP_INV_STATE_NULL: //Before INVITE is sent or received
			break;

		case PJSIP_INV_STATE_EARLY: //After response with To tag.
			m_tm_noanswer = true;
			m_time_start = time(NULL);

			m_acc->set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_BUSY, "");
			//only for OUT calls
			if (!m_erly_second && !m_in_call) {
				//send command-message
				if (m_acc->m_acc_set.sip_ver == SIP_VER_PTP) {
					prm_msg.content = m_call_type;
					sendInstantMessage(prm_msg);
				}
				//подаём КПВ
				m_acc->get_sip()->get_tone()->ring_back_start();
				m_erly_second = true;
			}
			//взведён xFer, для исходящих
			if (m_acc->get_sip()->m_fl_xfer && !m_in_call) { //сохраняем пары для соединения xFer
				for (int i = 0; i < NumberAccount; i++)
					for (unsigned int a = 0; a < m_acc->get_sip()->m_accounts[i].m_calls.size(); a++)
						if (m_acc->get_sip()->m_accounts[i].m_calls[a]->m_fl_wait_xfer_target) {
							m_acc->get_sip()->m_accounts[i].m_calls[a]->m_pair_call_xfer = ci.callIdString;
							m_pair_call_xfer = m_acc->get_sip()->m_accounts[i].m_calls[a]->getInfo().callIdString;
							m_fl_wait_xfer_source = true;
							m_fl_wait_xfer_target = false;
						}
			}
			break;

		case PJSIP_INV_STATE_CONFIRMED:
			break;

		case PJSIP_INV_STATE_CALLING:
			msleep(100);
			cmd.ev = sip_ev_t::Calling;
			cmd.call = ci.callIdString;
			cmd.portA = ci.localUri;
			cmd.portB = ci.remoteUri;
			m_acc->get_sip()->add_command(&cmd);
			break;

		case PJSIP_INV_STATE_INCOMING: //After INVITE is received.
			break;

		case PJSIP_INV_STATE_DISCONNECTED:
			//Sleep after last disconnect
			m_acc->get_sip()->m_calling_sleep = true;
			/* удалить звонок */
			m_acc->get_sip()->m_ep.libHandleEvents(50);
			delete_call();
			break;

		default:
			break;
		}
}
//----------------------------------------------------------

void call_t::onCallMediaState(OnCallMediaStateParam & prm)
{
	call_t* source;
	CallInfo ci = getInfo();
	
	string strFullRUri = format_addr(ci.remoteUri) + "@" + format_addr_server(ci.remoteUri);
	string strFullLUri = format_addr(ci.localUri) + "@" + format_addr_server(ci.localUri);
	string record_name = "";

	for (unsigned i = 0; i < ci.media.size(); i++) {
		if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
			AudioMedia* aud_med = (AudioMedia*)getMedia(i);

			//RingBack stop
			if (m_erly_second)
				m_acc->get_sip()->get_tone()->tones_long_stop();

			//Record
			if (m_acc->get_sip()->m_setts->fl_record && m_recorder) {
				if (m_recorder->getPortId() < 0) {
					if (m_in_call)	//Incomming
						record_name = local_time_to_string() + "_i_" + strFullLUri + "_" + strFullRUri + "_" + ci.callIdString + ".wav";
					else //Outgoing
						record_name = local_time_to_string() + "_o_" + strFullLUri + "_" + strFullRUri + "_" + ci.callIdString + ".wav";
					m_recorder->createRecorder(m_acc->get_sip()->m_path + "Records/" + record_name);
				}

			}

			switch (m_call_status) {
			case call_status_t::OnHold_demand:
				aud_med->stopTransmit(m_acc->get_sip()->m_playback);
				m_acc->get_sip()->m_capture.stopTransmit(*aud_med);
				m_call_status = call_status_t::OnHold;
				break;

			case call_status_t::OnHold:
				aud_med->stopTransmit(m_acc->get_sip()->m_playback);
				m_acc->get_sip()->m_capture.stopTransmit(*aud_med);
				m_acc->get_sip()->plrHold->startTransmit(*aud_med);
				m_call_status = call_status_t::OnHold;
				break;

			case call_status_t::OffHold:
				aud_med->startTransmit(m_acc->get_sip()->m_playback);
				m_acc->get_sip()->m_capture.startTransmit(*aud_med);
				m_acc->get_sip()->plrHold->stopTransmit(*aud_med);
				m_call_status = call_status_t::OffHold;

				if (m_fl_wait_xfer_target)
					m_acc->get_sip()->mk_xfer_execute(ci.callIdString);
				break;

			case call_status_t::xFer:
				if (m_fl_wait_xfer_source){//это target
					m_fl_wait_xfer_source = false;
					source = m_acc->get_sip()->get_crdnt_call(m_pair_call_xfer).call;
					if (source)	{
						for (unsigned a = 0; a < source->getInfo().media.size(); a++)
							if (source->getInfo().media[a].type == PJMEDIA_TYPE_AUDIO &&
								source->getMedia(a)) {
								AudioMedia* source_aud_med = (AudioMedia*)source->getMedia(a);
								//отключаем ringback
								m_acc->get_sip()->get_tone()->ring_back_stop_to(*source_aud_med);
								//соединим абонентов
								source_aud_med->startTransmit(*aud_med);
								aud_med->startTransmit(*source_aud_med);
								cout << "CONN A-B" << endl;
								if (m_acc->get_sip()->m_setts->fl_record)
									aud_med->startTransmit(*m_recorder);//отметит звонки как csxFered
							}
					}
				}
				break;

			case call_status_t::Mic:
				m_acc->get_sip()->m_capture.startTransmit(*aud_med);
				if (m_acc->get_sip()->m_setts->fl_record)
					m_acc->get_sip()->m_capture.startTransmit(*m_recorder);
				break;

			case call_status_t::Horn:
				aud_med->startTransmit(m_acc->get_sip()->m_playback);
				m_acc->get_sip()->m_capture.startTransmit(*aud_med);
				if (m_acc->get_sip()->m_setts->fl_record)
				{
					aud_med->startTransmit(*m_recorder);
					m_acc->get_sip()->m_capture.startTransmit(*m_recorder);
				}
				break;

			case call_status_t::Speaker:
				aud_med->startTransmit(m_acc->get_sip()->m_playback);
				m_acc->get_sip()->m_capture.startTransmit(*aud_med);
				if (m_acc->get_sip()->m_setts->fl_record) {
					aud_med->startTransmit(*m_recorder);
					m_acc->get_sip()->m_capture.startTransmit(*m_recorder);
				}
				break;

			case call_status_t::Transfered:
				for (unsigned int z = 0; z < m_acc->m_buttons->at(m_button_conf_port_id).calls.size(); z++)
					if (m_acc->m_buttons->at(m_button_conf_port_id).calls[z] != this) {
						for (size_t m = 0; 
							m < m_acc->m_buttons->at(m_button_conf_port_id).calls[z]->getInfo().media.size(); 
							m++) {
							if (m_acc->m_buttons->at(m_button_conf_port_id).calls[z]->getInfo().media[m].type == PJMEDIA_TYPE_AUDIO 
								&& m_acc->m_buttons->at(m_button_conf_port_id).calls[z]->getMedia(m)) {
								AudioMedia* aud_med_but = (AudioMedia*)m_acc->m_buttons->at(m_button_conf_port_id).calls[z]->getMedia(m);
								aud_med_but->startTransmit(*aud_med);
								aud_med->startTransmit(*aud_med_but);
							}
						}
					}
				if (m_acc->get_sip()->m_fl_conf_port) {
					aud_med->startTransmit(m_acc->get_sip()->m_playback);
					if (m_acc->get_sip()->m_setts->fl_record)
						aud_med->startTransmit(*m_recorder);
					m_acc->get_sip()->m_capture.startTransmit(*aud_med);
					if (m_acc->get_sip()->m_setts->fl_record)
						m_acc->get_sip()->m_capture.startTransmit(*m_recorder);
				}
				break;

			default:
				aud_med->startTransmit(m_acc->get_sip()->m_playback);
				m_acc->get_sip()->m_capture.startTransmit(*aud_med);
				if (m_acc->get_sip()->m_setts->fl_record) {
					aud_med->startTransmit(*m_recorder);
					m_acc->get_sip()->m_capture.startTransmit(*m_recorder);
				}
			}
		}
	}
}
//-------------------------------------------------------------------

void call_t::onInstantMessage(OnInstantMessageParam& prm)
{
	CallOpParam call_prm;
	string Command = "";
	SendInstantMessageParam msg_prm;
	sip_cmd_t cmd;

	call_prm.statusCode = PJSIP_SC_OK;
	CallInfo ci = getInfo();

	if (prm.msgBody == sip_msg::Marker)	{
		m_time_marker_response = get_time_ms();
		msg_prm.content = sip_msg::MarkerResponse;
		sendInstantMessage(msg_prm);
	}
	else if (prm.msgBody == sip_msg::MarkerResponse)
		m_time_marker_response = get_time_ms();

	else if (prm.msgBody == sip_msg::Base)
		return;

	else if (prm.msgBody == sip_msg::Horn) {
		if (m_call_status != call_status_t::Horn) {
			m_call_status = call_status_t::Horn;
			m_acc->get_sip()->cmd_answer();
			cmd.ev = sip_ev_t::Horn;
			m_acc->get_sip()->add_command(&cmd);
		}
	}

	else if (prm.msgBody == sip_msg::Speaker) {
		if (m_call_status != call_status_t::Speaker) {
			m_call_status = call_status_t::Speaker;
			m_acc->get_sip()->cmd_answer();
			cmd.ev = sip_ev_t::Speaker;
			m_acc->get_sip()->add_command(&cmd);
		}
	}

	else if (prm.msgBody == sip_msg::Mic) {
		if (m_call_status != call_status_t::Mic) {
			m_call_status = call_status_t::Mic;
			m_acc->get_sip()->cmd_answer();
			cmd.ev = sip_ev_t::Mic;
			m_acc->get_sip()->add_command(&cmd);
		}
	}

	else if (prm.msgBody == sip_msg::Simplex_Rec) {
		cmd.ev = sip_ev_t::Simplex;
		cmd.call = "recive";
		m_acc->get_sip()->add_command(&cmd);
	}
	else if (prm.msgBody == sip_msg::Simplex_Tran) {
		cmd.ev = sip_ev_t::Simplex;
		cmd.call = "transmit";
		m_acc->get_sip()->add_command(&cmd);
	}

	/*Орех*/
	else if (prm.msgBody == sip_msg::Opex_1){
		cmd.ev = sip_ev_t::Opex;
		cmd.call = "0";
		cmd.portA = ci.remoteUri;
		m_acc->get_sip()->add_command(&cmd);
	}
	else if (prm.msgBody == sip_msg::Opex_2) {
		cmd.ev = sip_ev_t::Opex;
		cmd.call = "1";
		cmd.portA = ci.remoteUri;
		m_acc->get_sip()->add_command(&cmd);
	}
	else if (prm.msgBody == sip_msg::Opex_3) {
		cmd.ev = sip_ev_t::Opex;
		cmd.call = "2";
		cmd.portA = ci.remoteUri;
		m_acc->get_sip()->add_command(&cmd);
	}
	else if (prm.msgBody == sip_msg::Opex_4) {
		cmd.ev = sip_ev_t::Opex;
		cmd.call = "3";
		cmd.portA = ci.remoteUri;
		m_acc->get_sip()->add_command(&cmd);
	}
	else if (prm.msgBody == sip_msg::Opex_5) {
		cmd.ev = sip_ev_t::Opex;
		cmd.call = "4";
		cmd.portA = ci.remoteUri;
		m_acc->get_sip()->add_command(&cmd);
	}
	else if (prm.msgBody == sip_msg::Opex_6) {
		cmd.ev = sip_ev_t::Opex;
		cmd.call = "5";
		cmd.portA = ci.remoteUri;
		m_acc->get_sip()->add_command(&cmd);
	}
}
//-------------------------------------------------------------------

void call_t::onCallTransferRequest(OnCallTransferRequestParam& prm)
{
	prm.opt.videoCount = 0;
	prm.statusCode = PJSIP_SC_VERSION_NOT_SUPPORTED;

	cout << "Recive REFER  " << prm.dstUri << "\n";
	return;
}
//------------------------------------------------------------

void call_t::onDtmfDigit(OnDtmfDigitParam& prm)
{
	sip_cmd_t cmd;
	string Command = "";

	// только для микшера
	int digit = atoi(prm.digit.c_str());
	switch (digit) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		break;
	case 6:
		break;
	case 7:
		break;
	case 8:
		cmd.ev = sip_ev_t::Simplex;
		cmd.call = "recive";
		break;
	case 9:
		cmd.ev = sip_ev_t::Simplex;
		cmd.call = "transmit";
		break;
	}
	m_acc->get_sip()->add_command(&cmd);
}
//--------------------------------------------------------------------

string call_t::time_to_string(int sec)
{
	int min = sec / 60;
	int hh = min / 60;
	sec = sec - min * 60;
	min = min - hh * 60;
	string buf;
	buf = to_string(sec);
	string dt = buf;
	buf = to_string(min);
	dt = ":" + dt;
	dt = buf + dt;
	buf = to_string(hh);
	dt = ":" + dt;
	dt = buf + dt;
	return dt;
}
//------------------------------------------------------------------------

string call_t::local_time_to_string()
{
	time_t call_start;
	struct tm* Local_time = { 0 };
	string str = "";

	time(&call_start);
	Local_time = localtime(&call_start);
	str = to_string(Local_time->tm_mday) + "-" +
		to_string(Local_time->tm_mon + 1) + "-" +
		to_string(Local_time->tm_year + 1900) + "-" +
		to_string(Local_time->tm_hour) + "-" +
		to_string(Local_time->tm_min) + "-" +
		to_string(Local_time->tm_sec);

	return str;
}
//----------------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////
//****************** MyBody***************************
//**************************************************************
////////////////////////////////////////////////////////////////

void buddy_t::onBuddyState()
{
	sip_cmd_t cmd;
	BuddyInfo bi = getInfo();
	string b_uri = "";

	if (m_sip_ver == SIP_VER_PTP)
		b_uri = bi.uri + "@local";
	else
		b_uri = bi.uri;

	cmd.ev = sip_ev_t::Presents;
	cmd.portA = b_uri;
	cmd.portB = bi.presStatus.note;

	switch (bi.presStatus.status) {
	case PJSUA_BUDDY_STATUS_UNKNOWN:
		if (m_sip_ver == SIP_VER_PTP)
			cmd.call = sip_pres_st::Fail;
		else
			cmd.call = sip_pres_st::Idle;
		m_sip->add_command(&cmd);
		break;

	case PJSUA_BUDDY_STATUS_ONLINE:
		switch (bi.presStatus.activity) {
		case PJRPID_ACTIVITY_UNKNOWN:
			if (bi.presStatus.statusText == "Online")
				cmd.call = sip_pres_st::Idle;
			else if (bi.presStatus.statusText == "Busy")
				cmd.call = sip_pres_st::Busy;
			else
				cmd.call = sip_pres_st::Idle;
			m_sip->add_command(&cmd);
			break;

		case PJRPID_ACTIVITY_AWAY:
			cmd.call = sip_pres_st::Idle;
			m_sip->add_command(&cmd);
			break;

		case PJRPID_ACTIVITY_BUSY:
			cmd.call = sip_pres_st::Busy;
			m_sip->add_command(&cmd);
			break;
		}
		break;

	case PJSUA_BUDDY_STATUS_OFFLINE:
		cmd.call = sip_pres_st::Fail;
		m_sip->add_command(&cmd);
		break;
	}
}
//---------------------------------------------------------------------
