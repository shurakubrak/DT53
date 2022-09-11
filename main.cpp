#include <cstdio>
#include <string>
#include <iostream>
#include <cstring>
#include "includes/sqlite_db.h"
#include "includes/main.h"
#include "includes/utils.h"

using namespace std;

int main(int argc, char** argv)
{
    vers_safe();

	bool reload = false;
	pthread_t ptDeviceRead;
	pthread_t ptShell;
	t_blink = get_time_ms();
	t_control = get_time_ms();
	path = "/home/sip/app/" /*путь к каталогу приложения*/

	if (!read_param()) {
		cout << "ReadParam fail" << endl;
		return 1;
	}

	//if (!DeviceOpen()) {
	//	cout << "Device open error" << endl;
	//	Terminated = 6;
	//}
	//digitalWrite(GLedRed, HIGH);

	//FindCPUID();
	//setMicFilters();

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
	cout << ver << endl;
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return;
	db.m_sql_str = "update operators set type='"
		+ type + "',version='"
		+ ver + "'";
	if (!db.exec_sql(true))
		cout << "SQLite ERROR: write version error" << endl;
	db.close_db();
}
/*************************************************************/

bool read_param()
{
	stNetwork eth;
	stNetwork wlan;
	sqlite_db_t db(path + "settings.s3db");
	if (!db.open_db())
		return false;
	db.m_sql_str = "select * from operators";
	if (!db.exec_sql()) {
		cout << "SQLite: read table operators error" << endl;
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
			//Log.ToLog("Invalid address 169.x.x.x");
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

	if (!read_param_ex())
	{
		cout << "ReadParamEx fail" << "\n";
		return false;
	}

	//cout << "SIP.log=" << SIP.log << " SIP.log_level=" << SIP.log_level << endl;
	return true;
}
/******************************************************/

bool read_param_ex()
{
	string param_name = "";
	sqlite_db_t db(path + "settings.s3db");
	string filter = "";
		if (!db.open_db())
			return false;
		db.m_sql_str = "select * from params";
		if (!db.exec_sql()) {
			cout << "SQLite ERROR: read table operators error" << endl;
			db.close_db();
			return false;
		}

		strFilter.clear();
		do {
			param_name = db.field_by_name("name").c_str();

			if (param_name == "ext_key") {
				KP.Ext = db.atob(db.field_by_name("value").c_str());
				continue;
			}

			if (param_name == "multiline") {
				Multiline = db.atob(db.field_by_name("value"));
				continue;
			}
			if (param_name == "autoanswer") {
				Accounts[0].stAcc->auto_answer = db.atob(db.field_by_name("value").c_str());
				continue;
			}
			if (param_name == "record") {
				flRecord = db.atob(db.field_by_name("value"));
				continue;
			}
			if (param_name == "first_digit") {
				FirstDigit = atoi(db.field_by_name("value").c_str());
				continue;
			}
			if (param_name == "inter_digit") {
				InterDigit = atoi(db.field_by_name("value").c_str());
				continue;
			}
			if (param_name == "no_answer") {
				CallNoAnswer = atoi(db.field_by_name("value").c_str());
				continue;
			}

			if (param_name == "rington") {
				RingTon = db.field_by_name("value");
				if (fopen((path + RingTon).c_str(), "r") == NULL)
					RingTon = "Tones/Ring.wav";
				continue;
			}
			if (param_name == "holdton") {
				SIP.HoldTon = db.field_by_name("value");
				if (fopen((path + SIP.HoldTon).c_str(), "r") == NULL)
					SIP.HoldTon = "Tones/Hold.wav";
				continue;
			}
			if (param_name == "vmsg_1") {
				VMsg_1 = db.field_by_name("value");
				if (fopen((path + VMsg_1).c_str(), "r") == NULL)
					VMsg_1 = "Tones/VMsg_1.wav";
				continue;
			}
			if (param_name == "vmsg_2") {
				VMsg_2 = db.field_by_name("value");
				if (fopen((path + VMsg_2).c_str(), "r") == NULL)
					VMsg_2 = "Tones/VMsg_1.wav";
				continue;
			}
			if (param_name == "vmsg_3") {
				VMsg_3 = db.field_by_name("value");
				if (fopen((path + VMsg_3).c_str(), "r") == NULL)
					VMsg_3 = "Tones/VMsg_1.wav";
				continue;
			}
			if (param_name == "vmsg_4") {
				VMsg_4 = db.field_by_name("value");
				if (fopen((path + VMsg_4).c_str(), "r") == NULL)
					VMsg_4 = "Tones/VMsg_1.wav";
				continue;
			}
			if (param_name == "vmsg_5") {
				VMsg_5 = db.field_by_name("value");
				if (fopen((path + VMsg_5).c_str(), "r") == NULL)
					VMsg_5 = "Tones/VMsg_1.wav";
				continue;
			}

			if (param_name == "capture_level") {
				CaptureAdjust = atof(db.field_by_name("value").c_str());
				continue;
			}
			if (param_name == "playback_level") {
				PBAdjust = atof(db.field_by_name("value").c_str());
				continue;
			}

			if (param_name == "eth_control") {
				flEthControl = db.atob(db.field_by_name("value"));
				continue;
			}


			if (param_name == "filter_1" ||
				param_name == "filter_2" ||
				param_name == "filter_3" ||
				param_name == "filter_4")
				strFilter.push_back(db.field_by_name("value"));
		} while (db.next_rec());
		db.close_db;


		ParseFilter();
		InitInputFilters();

		if (SIP.ep.libGetState() == PJSUA_STATE_STARTING ||
			SIP.ep.libGetState() == PJSUA_STATE_RUNNING)
		{

			PlayersInit();

			SIP.aPlayback.adjustRxLevel(PBAdjust);
			SIP.aCapture.adjustTxLevel(CaptureAdjust);
		}

		return true;
}
/***********************************************/

bool get_network(stNetwork* nw, bool wlan)
{
	string ip_addr = "";
	int psb = -1;
	int pse = -1;
	char buf[250];
	string str = "";
	FILE* f;
	bool ret = false;

	if (wlan)
		f = popen("ifconfig wlan0", "r");
	else
		f = popen("ifconfig eth0", "r");

	if (f) {
		while (fgets(buf, BUFSIZ, f)) {
			str += buf;
			for (int i = 0; i < 250; i++)
				buf[i] = 0;
		}
		/*ip адрес*/
		psb = str.find("inet ") + 5;
		if (psb != string::npos) {
			pse = str.find_first_of(' ', psb);
			if (pse != string::npos) {
				ip_addr = str.substr(psb, pse - psb);
				if (is_valid_ip_address(ip_addr)) {
					nw->ip_addr = ip_addr;
					ret = true;
				}
			}
		}
		/*маска*/
		psb = str.find("netmask ") + 8;
		if (psb != string::npos) {
			pse = str.find_first_of(' ', psb);
			if (pse != string::npos)
				nw->netmask = str.substr(psb, pse - psb);
		}
		/*MAC*/
		psb = str.find("ether ") + 6;
		if (psb != string::npos) {
			pse = str.find_first_of(' ', psb);
			if (pse != string::npos)
				nw->mac = str.substr(psb, pse - psb);
		}
	}
	pclose(f);

	/*шлюз по умолчанию*/
	f = popen("route", "r");
	if (f) {
		while (fgets(buf, BUFSIZ, f)) {
			str = buf;
			if (wlan)
				psb = str.find("wlan0");
			else
				psb = str.find("eth0");
			if (psb != string::npos) {
				psb = str.find("default") + 7;
				if (psb != string::npos) {
					for (int i = psb; i < str.length(); i++)
						if (str[i] != ' ') {
							psb = i;
							break;
						}
					pse = str.find_first_of(' ', psb);
					if (pse != string::npos) {
						nw->gateway = str.substr(psb, pse - psb);
						break;
					}
				}
			}
			for (int i = 0; i < 250; i++)
				buf[i] = 0;
		}
	}
	pclose(f);

	return ret;
}
/******************************************************/

bool is_valid_ip_address(string checkValue)
{
	int num, i, len;
	char* ch;

	//Количество блоков (четвертей) в адресе
	int blocksCount = 0;
	//  Проверяем длину
	len = checkValue.length();
	if (len < 7 || len>15)
		return false;

	//создаем новый указатель
	char* st = strdup(checkValue.c_str());
	//char* nextST = NULL;
	//получаем указатель на первый блок до точки
	ch = strtok(st, ".");

	while (ch != NULL)
	{
		blocksCount++;
		num = 0;
		i = 0;

		//  преобразовываем каждый блок к Int
		while (ch[i] != '\0')
		{
			num = num * 10;
			num = num + (ch[i] - '0');
			i++;
			if (num > 999)
				break;
		}

		if (num < 0 || num>255)
		{
			return false;
		}
		//первый и последний блок не равны нулю
		if ((blocksCount == 1 && num == 0))
		{
			return false;
		}
		//получаем указатель на следующий блок
		ch = strtok(NULL, ".");
	}

	// проверяем общее количество блоков
	if (blocksCount != 4)
	{
		return false;
	}
	free(st);
	free(ch);
	return true;
}