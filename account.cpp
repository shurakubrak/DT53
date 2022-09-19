#include "sip.h"

using namespace std;
using namespace pj;
////////////////////////////////////////////////////////////////
//***************** MyAccount**********************
//**************************************************************
/////////////////////////////////////////////////////////////////

void account_t::onRegState(OnRegStateParam& prm)
{
	sip_cmd_t cmd;
	AccountInfo ai = getInfo();

	cmd.ev = sip_ev_t::Reg;
	cmd.portA = ai.uri;
	if (ai.regStatus == PJSIP_SC_OK) {
		cmd.call = "1";
		m_active = true;

		if (!m_buddy_reg && m_acc_set.sip_ver != SIP_VER_PTP) {
			set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_AWAY, "");
			m_buddy_reg = true;

			//ѕолучаем всех абонентов, которые есть под кнопкой.
			//добал€ем как Buddy
			if (m_buttons != nullptr) {
				for (int i = 0; i < NumberButtons; i++) {
					if (m_buttons->at(i).server == m_acc_set.sipserver &&
						!m_buttons->at(i).phone.empty())
						buddy_register(m_buttons->at(i).phone, m_acc_set.sip_ver, true);
				}
			}
		}
		std::cout << "Account " << m_acc_set.sipserver << " registration: " 
			<< ai.regStatusText << ". Number buddys " << enumBuddies2().size() << endl;
	}
	else {
		cmd.call = "0";
		m_active = false;
		std::cout << "Account " << m_acc_set.sipserver << " registration ERROR: " 
			<< ai.regStatusText << endl;
	}
	m_sip->add_command(&cmd);
}
//------------------------------------------------------------------

bool account_t::create_acc(bool flModify)
{
		AccountConfig acc_cfg;
		PresenceStatus ps;
		string user;
		user = "sip:" + m_acc_set.phone;
		acc_cfg.idUri = user;

		acc_cfg.idUri = acc_cfg.idUri + "@" + m_acc_set.sipserver;
		if (m_acc_set.sip_ver != SIP_VER_PTP) {
			acc_cfg.regConfig.registrarUri = "sip:" + m_acc_set.sipserver;
			acc_cfg.sipConfig.authCreds.push_back(AuthCredInfo("digest", "*",
				m_acc_set.phone, 0, m_acc_set.password));
		}
		if (isValid())
			modify(acc_cfg);
		else
			create(acc_cfg);

		if (m_acc_set.sip_ver == SIP_VER_PTP) {
			m_active = true;
			set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_AWAY, "");
		}

		return true;
}
//-------------------------------------------------------------------------

void account_t::close_acc()
{
	if (m_active)
		set_status(PJSUA_BUDDY_STATUS_OFFLINE, PJRPID_ACTIVITY_UNKNOWN, "");
}
//----------------------------------------------------------------

void account_t::buddy_register(string phone, int sip_ver, bool monitor)
{
	BuddyConfig bud_cfg;
	SendInstantMessageParam msg_prm;

	if (!m_active)
		return;

	buddy_t* bd = new buddy_t(sip_ver, m_sip);

	if (m_acc_set.sip_ver != SIP_VER_PTP) {
		bud_cfg.uri = "sip:" + phone + "@" + m_acc_set.sipserver;
		bd->create(*this, bud_cfg);
		bd->subscribePresence(monitor);
	}
	else {
		bud_cfg.uri = "sip:" + phone;
		bd->create(*this, bud_cfg);
		bd->subscribePresence(monitor);
	}
	m_buddys.push_back(bd);
}
//--------------------------------------------------------------------

void account_t::set_status(pjsua_buddy_status status, pjrpid_activity activity, string note)
{
		if (m_acc_set.sip_ver == SIP_VER_PTP) {
			if (note == "old")
				setOnlineStatus(m_pres_st);
			else {
				m_pres_st.status = status;
				m_pres_st.activity = activity;
				m_pres_st.note = note;
				setOnlineStatus(m_pres_st);
			}
		}
		else if (note != "old" && !note.empty()){   // only status Connect
			m_pres_st.status = status;
			m_pres_st.activity = activity;
			m_pres_st.note = note;
			setOnlineStatus(m_pres_st);
		}
}
//------------------------------------------------------------------

bool account_t::call_to(string portB, string system_message, bool to_list)
{
	/*дл€ задержки после последнего звонка (T2D раскоментировать)*/
	//if (m_sip->m_sip_sleep == true) {
	//	m_sip->m_dial_number = "";
	//	m_sip->phone_status_define();
	//	return false;
	//}
	
	Buddy bd;
	call_t* call;
	try {
		CallOpParam prm(true);
		SendInstantMessageParam msg_prm;
		//Buddy *bd;
		prm.opt.audioCount = 1;
		prm.opt.videoCount = 0;
		//msg_prm.content = speacker;
		//Find repeated dial
		for (unsigned int i = 0; i < m_calls.size(); i++) {
			if (!m_calls[i]->isActive()) 
				continue;
			
			if (format_addr(m_calls[i]->getInfo().remoteUri) == portB) {
				prm.statusCode = PJSIP_SC_OK;
				m_calls[i]->hangup(prm);
				m_sip->m_route.status = false;
				m_sip->m_ep.libHandleEvents(100);
				return false;
			} 
		}

		if (!m_sip->m_setts->multiline) //проверка многолинейности
			if (m_sip->calculate_calls() >= 1)
				return false;
			
		if (m_acc_set.sip_ver == SIP_VER_PTP) {
			if (to_list) {
				//send test
				call_queue_t stCQ;
				stCQ.call_type = system_message;
				stCQ.portB = portB;
				//lsCQ.push_back(stCQ);
				lst_call_query_access(&stCQ, "", access_t::WRITE);
				msg_prm.content = sip_msg::CallTest;
				bd = findBuddy2("sip:" + portB);
				if (bd.isValid())
					bd.sendInstantMessage(msg_prm);
			}
			else {
				// можно вызывать
				call = new call_t(*this);
				call->m_call_type = system_message;
				m_calls.push_back(call);
				call->makeCall("sip:" + portB, prm);
				if (system_message == sip_msg::Speaker ||
					system_message == sip_msg::Horn ||
					system_message == sip_msg::Mic)	{
					msg_prm.content = system_message;
					call->sendInstantMessage(msg_prm);
				}
				std::cout << "PTP call to: " << portB << " mode: " << system_message << endl;
				return true;
			}
		}
		else if (m_acc_set.sip_ver != SIP_VER_PTP) {
			call = new call_t(*this);
			portB = "sip:" + portB + "@" + m_acc_set.sipserver;
			m_calls.push_back(call);
			call->makeCall(portB, prm);
			std::cout << "Serv call to: " << portB << endl;
		}
		return true;
	}
	catch (...) {
		sip_cmd_t cmd;
		cmd.ev = sip_ev_t::CallToError;
		m_sip->add_command(&cmd);

		if (call != NULL) 
			call->delete_call();
	
		std::cout << "MyAccount::CallTo fail " << endl;
		return false;
	}
}
//-------------------------------------------------------------

void account_t::onIncomingCall(OnIncomingCallParam& iprm)
{
		call_t* call = new call_t(*this, iprm.callId);
		CallOpParam prm;
		sip_cmd_t cmd;

		int num_calls = m_sip->calculate_calls();

		CallInfo ci = call->getInfo();
		m_calls.push_back(call);
		call->m_in_call = true;

		//второй и т.д. вызов
		if (num_calls > 0) {
			if (!m_sip->m_setts->multiline)	{
				prm.statusCode = PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE;
				call->answer(prm);
				return;
			}
			//второй вызов будет звонеть
			if (m_acc_set.auto_answer) {
				prm.statusCode = PJSIP_SC_RINGING;
				call->answer(prm);
				set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_BUSY, "");
				cmd.ev = sip_ev_t::AutoAnswer;
				m_sip->add_command(&cmd);
				return;
			}
		}
		else if (num_calls == 0) //первый вызов
		{
			if (m_acc_set.auto_answer) {	//ответить
				prm.statusCode = PJSIP_SC_RINGING;
				call->answer(prm);
				set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_BUSY, "");
				cmd.ev = sip_ev_t::AutoAnswer;
				m_sip->add_command(&cmd);
				return;
			}
		}

		prm.statusCode = PJSIP_SC_RINGING;
		call->answer(prm);

		set_status(PJSUA_BUDDY_STATUS_ONLINE, PJRPID_ACTIVITY_BUSY, "");

		cmd.ev = sip_ev_t::InCall;
		cmd.call = ci.callIdString;
		cmd.portA = ci.localUri;
		cmd.portB = ci.remoteUri;
		m_sip->add_command(&cmd);
}
//---------------------------------------------------------------

void account_t::onInstantMessage(OnInstantMessageParam& prm)
{
	CallOpParam call_prm;
	string Command = "";
	SendInstantMessageParam msg_prm;
	Buddy bd;

	try
	{
		//запрос на тест
		if (prm.msgBody == sip_msg::CallTest) {
			msg_prm.content = sip_msg::CallTestResponse;

			bd = findBuddy2("sip:" + format_addr(prm.fromUri));
			if (bd.isValid())
				bd.sendInstantMessage(msg_prm);
			else {
				buddy_register(format_addr(prm.fromUri), m_acc_set.sip_ver, true);
				try {
					bd = findBuddy2("sip:" + format_addr(prm.fromUri));
					if (bd.isValid()) {
						bd.sendInstantMessage(msg_prm);
						std::cout << "Add new contact - " << prm.fromUri << endl;
					}
					else
						std::cout << "MyAccount::onInstantMessage buddy fail" << "\n";
				}
				catch (...) {
					std::cout << "MyAccount::onInstantMessage buddy fail" << "\n";
				}
			}
		}
		//тест ќ 
		else if (prm.msgBody == sip_msg::CallTestResponse) {
			call_queue_t cq;
			if (lst_call_query_access(&cq, format_addr(prm.fromUri), access_t::READ))
				call_to(cq.portB, cq.call_type, false);
		}
	}
	catch (Error err) {
		if (err.status == PJ_ENOTFOUND && prm.msgBody == sip_msg::CallTest)	{
			buddy_register(format_addr(prm.fromUri), m_acc_set.sip_ver, true);
			try {
				bd = findBuddy2("sip:" + format_addr(prm.fromUri));
				if (bd.isValid())
					bd.sendInstantMessage(msg_prm);
				std::cout << "Add new contact - " << prm.fromUri << endl;
			}
			catch (...) {
				std::cout << "MyAccount::onInstantMessage buddy fail" << "\n";
			}
		}
		else
			std::cout << "MyAccount::onInstantMessage fail" << "\n";
	}
}
//----------------------------------------------------------------

void account_t::mute(bool state)
{
	for (unsigned int a = 0; a < m_calls.size(); a++) {
		if (!m_calls[a]->isActive())
			continue;
		CallInfo ci = m_calls[a]->getInfo();
		for (unsigned i = 0; i < ci.media.size(); i++) {
			if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && m_calls[a]->getMedia(i)) {
				AudioMedia* aud_med = (AudioMedia*)m_calls[a]->getMedia(i);
				if (state)
					m_sip->m_capture.stopTransmit(*aud_med);
				else
					m_sip->m_capture.startTransmit(*aud_med);
			}
		}
	}
}
//---------------------------------------------------------------

bool account_t::answer(string portB)
{
	CallOpParam prm;

	prm.statusCode = PJSIP_SC_OK;
	for (unsigned int i = 0; i < m_calls.size(); i++) {
		if (!m_calls[i]->isActive())
			continue;
		if (m_calls[i]->getInfo().state == PJSIP_INV_STATE_EARLY &&
			m_calls[i]->m_in_call) {
			if (portB.empty()) {
				m_calls[i]->answer(prm);
				return true;
			}
			else if (format_addr(m_calls[i]->getInfo().remoteUri) == portB)	{
				m_calls[i]->answer(prm);
				return true;
			}
		}
	}
	return false;
}
//---------------------------------------------------------------

void account_t::hold(int callID)
{
	CallOpParam prm;
	AudioMedia* aud_med;
	sip_cmd_t cmd;

	//если переданный 'callID' €вл€етс€ 'id' звонка
	for (size_t i = 0; i < m_calls.size(); i++) {
		if (!m_calls[i]->isActive())
			continue;
		if (m_calls[i]->getInfo().id == callID)
			callID = i;
	}

	CallInfo ci = m_calls[callID]->getInfo();
	switch (m_acc_set.sip_ver) {
	case SIP_VER_PTP:
		for (unsigned a = 0; a < ci.media.size(); a++) {
			if (ci.media[a].type == PJMEDIA_TYPE_AUDIO && m_calls[callID]->getMedia(a)) {
				aud_med = (AudioMedia*)m_calls[callID]->getMedia(a);
				aud_med->stopTransmit(m_sip->m_playback);
				m_sip->m_capture.stopTransmit(*aud_med);
				if(m_sip->plrHold != nullptr)
					m_sip->plrHold->startTransmit(*aud_med);
				m_calls[callID]->m_call_status = call_status_t::OnHold;
			}
		}
		break;
	case SIP_VER_1:
		m_calls[callID]->m_call_status = call_status_t::OnHold_demand;
		if (pjsua_call_set_hold(ci.id, NULL) != PJ_SUCCESS)
			return;
		break;
	case SIP_VER_2:
		m_calls[callID]->m_call_status = call_status_t::OnHold_demand;
		m_calls[callID]->setHold(prm);
		break;
	}

	cmd.ev = sip_ev_t::OnHold;
	m_sip->add_command(&cmd);
}
//---------------------------------------------------------------

void account_t::unhold(int callID)
{
	CallOpParam prm;
	AudioMedia* aud_med;
	sip_cmd_t cmd;

	if (m_calls[callID]->m_call_status == call_status_t::OnHold
		&& m_calls[callID]->isActive())	{
		CallInfo ci = m_calls[callID]->getInfo();
		switch (m_acc_set.sip_ver) {
		case SIP_VER_PTP:
			for (unsigned a = 0; a < ci.media.size(); a++) {
				if (ci.media[a].type == PJMEDIA_TYPE_AUDIO && m_calls[callID]->getMedia(a)) {
					aud_med = (AudioMedia*)m_calls[callID]->getMedia(a);

					ConfPortInfo pi = m_sip->m_playback.getPortInfo();
					aud_med->startTransmit(m_sip->m_playback);
					m_sip->m_capture.startTransmit(*aud_med);
					if(m_sip->plrHold != nullptr)
						m_sip->plrHold->stopTransmit(*aud_med);

					m_calls[callID]->m_call_status = call_status_t::OffHold;
					if (m_calls[callID]->m_fl_wait_xfer_target)
						m_sip->mk_xfer_execute(ci.callIdString);
				}
			}
			break;
		case SIP_VER_1:
			m_calls[callID]->m_call_status = call_status_t::OffHold_demand;
			if (pjsua_call_reinvite(ci.id, PJSUA_CALL_UNHOLD, NULL) != PJ_SUCCESS)
				return;
			break;
		case SIP_VER_2:
			m_calls[callID]->m_call_status = call_status_t::OffHold_demand;
			prm.opt.flag = PJSUA_CALL_UNHOLD;
			m_calls[callID]->reinvite(prm);
			break;
		}

		std::cout << "UnHold--------------------------" << endl;
		std::cout << "source: " << ci.callIdString <<
			" / " << m_calls[callID]->m_fl_wait_xfer_source <<
			" / " << m_calls[callID]->m_fl_wait_xfer_target <<
			" / " << static_cast<int>(m_calls[callID]->m_call_status) << endl;
		std::cout << "--------------------------------" << endl;

		cmd.ev = sip_ev_t::OffHold;
		cmd.call = ci.callIdString;
		cmd.portA = ci.localUri;
		cmd.portB = ci.remoteUri;
		m_sip->add_command(&cmd);

		cmd.ev = sip_ev_t::Connect;
		m_sip->add_command(&cmd);
	}
}
//--------------------------------------------------------------
void account_t::remove_call(call_t* call)
{
	for (vector<call_t*>::iterator it = m_calls.begin();
		it != m_calls.end(); ++it) {
		if (*it == call) {
			m_calls.erase(it);
			break;
		}
	}

	for (int ButtonID = 0; ButtonID < NumberButtons; ButtonID++)
		if (m_buttons->at(ButtonID).function == func_t::ConfPort)
			for (auto it = m_buttons->at(ButtonID).calls.begin(); 
				it != m_buttons->at(ButtonID).calls.end(); it++)
				if (*it == call) {
					m_buttons->at(ButtonID).calls.erase(it);
					if (m_buttons->at(ButtonID).calls.size() == 0) {
						m_sip->m_device->led_button_off(static_cast<func_t>(ButtonID), true);
						m_sip->m_fl_conf_port = false;
					}
					break;
				}
}
//--------------------------------------------------------------

bool account_t::bye_call(string callID)
{
	CallOpParam prm;
	crdnt_call_t cc = m_sip->get_crdnt_call(callID);
	prm.statusCode = PJSIP_SC_OK;

	//если на отбиваемое соединение 'target' надо перевести,
	//то соединение 'source' снимаем с удержани€
	if (cc.call->m_fl_wait_xfer_source && m_sip->m_fl_xfer) {
		crdnt_call_t cc_source = m_sip->get_crdnt_call(cc.call->m_pair_call_xfer);
		if (cc_source.call) 
			cc_source.call->m_acc->unhold(cc_source.count_call);
		else
			cc.call->hangup(prm);
	}
	//иначе просто прерываем
	else
		cc.call->hangup(prm);

	if (cc.call->getInfo().state == PJSIP_INV_STATE_CALLING) {
		sip_cmd_t cmd;
		cmd.ev = sip_ev_t::Disconnect;
		cmd.call = cc.call->getInfo().callIdString;
		cmd.portA = call_t::time_to_string(cc.call->getInfo().connectDuration.sec);
		cmd.portB = cc.call->getInfo().remoteUri;
		m_sip->add_command(&cmd);
		remove_call(cc.call);
	}
	return true;
}
//----------------------------------------------------------

void account_t::bye_call_all()
{
	CallOpParam prm;
	list<call_t*> lCalls;
	prm.statusCode = PJSIP_SC_OK;
	unsigned int count = m_calls.size();

	for (unsigned int i = 0; i < m_calls.size(); i++)
		lCalls.push_back(m_calls[i]);

	for (unsigned int i = 0; i < count; i++) {
		call_t* call = lCalls.front();
		if (!call->isActive())
			continue;
		CallInfo ci = call->getInfo();
		//only for answered calls
		if (call->m_call_status != call_status_t::OnHold
			&& call->m_call_status != call_status_t::OffHold_demand
			&& !call->m_fl_wait_xfer_target
			&& call->m_call_status != call_status_t::xFer
			&& call->m_call_status != call_status_t::Transfered
			&& call->m_call_status != call_status_t::Horn
			&& !(call->m_in_call && ci.state == PJSIP_INV_STATE_EARLY))
			bye_call(ci.callIdString);
		lCalls.pop_front();
	}
}
//-------------------------------------------------------------

bool account_t::lst_call_query_access(call_queue_t* cq, string portB, access_t acc)
{
	vector<vector<call_queue_t>::iterator> rem;
	bool ret = false;
	//unique_lock<mutex> locker(m_mxCQ);
	switch (acc) {
	case access_t::WRITE:
		lst_CQ.push_back(*cq);
		ret = true;
		break;
	case access_t::READ:
		for (auto it = lst_CQ.begin(); it != lst_CQ.end(); ++it) {
			if (it->portB == portB) {
				*cq = *it;
				lst_CQ.erase(it);
				return true;
			}
		}
		break;
	case access_t::DELETE:
		for (auto it = lst_CQ.begin(); it != lst_CQ.end(); ++it)
			if ((time(NULL) - it->time_test_start) >= TMR_CQ)
				rem.push_back(it);
		for (unsigned int i = 0; i < rem.size(); i++) {
			lst_CQ.erase(rem[i]);
			ret = true;
		}
		rem.clear();
		break;
	}
	return ret;
}
//--------------------------------------------------------------------

void account_t::DTMF(string digit)
{
	for (unsigned int i = 0; i < m_calls.size(); i++) {
		if (!m_calls[i]->isActive())
			continue;
		if (m_calls[i]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
			m_calls[i]->getInfo().state == PJSIP_INV_STATE_CONNECTING)
			m_calls[i]->dialDtmf(digit);
	}
}
//-------------------------------------------------------------

void account_t::simplex(string direction)
{
	SendInstantMessageParam msg_prm;
	if (m_active)
		for (unsigned int i = 0; i < m_calls.size(); i++)
			if (m_calls[i]->getInfo().state == PJSIP_INV_STATE_CONFIRMED ||
				m_calls[i]->getInfo().state == PJSIP_INV_STATE_CONNECTING) {
				msg_prm.content = direction;
				m_calls[i]->sendInstantMessage(msg_prm);
			}
}
//---------------------------------------------------------------

