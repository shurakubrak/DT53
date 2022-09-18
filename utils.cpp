#include <cassert>
#include <vector>
#include <iostream>
#include <cstring>
#include <thread>
#include "includes/utils.h"

using namespace std;
using namespace std::chrono;

string get_data_str()
{
	time_t t = time(NULL);
	struct tm* ti(localtime(&t));

    assert(ti);
	string mon = to_string(ti->tm_mon + 1);
	if (ti->tm_mon + 1 < 10)
		mon = "0" + to_string(ti->tm_mon + 1);
	string day = to_string(ti->tm_mday);
	if (ti->tm_mday < 10)
		day = "0" + to_string(ti->tm_mday);

	string str = to_string(ti->tm_year + 1900) + "."
		+ mon + "."
		+ day + " ";
	return str;
}

string get_time_ms_str(bool add_ms)
{
	string ms;
	Time t = system_clock::now();
	uint64_t mm = get_time_ms(t);
	time_t dt = system_clock::to_time_t(t);
	struct tm* ti(localtime(&dt));

	string mon = to_string(ti->tm_mon + 1);
	if (ti->tm_mon + 1 < 10)
		mon = "0" + to_string(ti->tm_mon + 1);
	string day = to_string(ti->tm_mday);
	if (ti->tm_mday < 10)
		day = "0" + to_string(ti->tm_mday);
	string hour = to_string(ti->tm_hour);
	if (ti->tm_hour < 10)
		hour = "0" + to_string(ti->tm_hour);
	string min = to_string(ti->tm_min);
	if (ti->tm_min < 10)
		min = "0" + to_string(ti->tm_min);
	string sec = to_string(ti->tm_sec);
	if (ti->tm_sec < 10)
		sec = "0" + to_string(ti->tm_sec);
	if (add_ms)
		ms = to_string(mm - (dt * 1000));
	else
		ms = to_string(mm - (dt * 1000) + 1);

	string str = to_string(ti->tm_year + 1900) + "."
		+ mon + "."
		+ day + " "
		+ hour + ":"
		+ min + ":"
		+ sec + "."
		+ ms;
	return str;
}

uint64_t get_time_ms()
{
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

uint64_t get_time_s()
{
	return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

uint64_t get_time_ms(Time dt)
{
	return duration_cast<milliseconds>(dt.time_since_epoch()).count();
}
uint64_t get_time_s(Time dt)
{
	return duration_cast<seconds>(dt.time_since_epoch()).count();
}

int32_t twoint16_oneint32(int16_t h, int16_t l)
{
    return static_cast<int32_t>(((static_cast<int32_t>(l)) & 0xffff) 
            | (static_cast<int32_t>(h) << 16));
}

void oneint32_twoint16(int16_t &h, int16_t &l, int32_t val)
{
	l = (int16_t)(val & 0xffff);
	h = (int16_t)((val >> 16) & 0xffff);
}

int get_random_number(int min, int max)
{
    static const double fraction = 1.0 / (static_cast<double>(RAND_MAX) + 1.0);
	return static_cast<int>(rand() * fraction * (max - min + 1) + min);
}

string pi_calc(int n)
{
	int M = (n * 10 + 2) / 3;
	vector<int> r(M, 2);
	string pi;
	pi.reserve(n + 1);
	for (int i = 0; i < n; ++i)
	{
		int carry = 0;
		int sum = 0;
		for (int j = M - 1; j >= 0; --j)
		{
			r[j] *= 10;
			sum = r[j] + carry;
			int q = sum / (2 * j + 1);
			r[j] = sum % (2 * j + 1);
			carry = q * j;
		}
		r[0] = sum % 10;
		int q = sum / 10;
		if (q >= 10)
		{
			q = q - 10;
			for (int j = pi.length() - 1;; --j)
			{
				if (pi[j] == '9')
					pi[j] = '0';
				else
				{
					++pi[j];
					break;
				}
			}
		}
		pi += ('0' + q);
		if (i == 0) pi += '.';
	}
	return pi;
}

void ssleep(size_t sec)
{
	this_thread::sleep_for(milliseconds(sec * 1000));
}

void msleep(size_t msec)
{
	this_thread::sleep_for(milliseconds(msec));
}

bool wait4(bool* flag, uint64_t msec)
{
	uint64_t start = get_time_ms();
	bool fl_start = *flag;
	while ((get_time_ms() - start) < msec)
		if (*flag != fl_start)
			return true;
		else
			msleep(5);
	return false;
}

bool wait4(atomic<bool>* flag, uint64_t msec)
{
	uint64_t start = get_time_ms();
	bool fl_start = flag->load();
	while ((get_time_ms() - start) < msec)
		if (flag->load() != fl_start)
			return true;
		else
			msleep(5);
	return false;
}
/**************************************************/

bool get_network(network_t* nw, bool wlan)
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
/************************************************/

string get_cpu_id()
{
	FILE* cpuinfo = fopen("/proc/cpuinfo", "rb");
	char* arg = 0;
	size_t size = 0;
	std::string ID;
	while (getdelim(&arg, &size, 0, cpuinfo) != -1)
	{
		std::string str(arg);
		ID = str.substr(str.size() - 17, 16);
	}
	fclose(cpuinfo);
	return ID;
}
/**********************************************************************/

string format_addr(string addr)
{
	string str = "";
	bool write = false;

	for (unsigned int i = 0; i < addr.length(); i++)
	{
		if (addr[i] == ':') write = true;
		else if ((addr[i] == '>') || (addr[i] == '@')) break;
		else if (write)	str += addr[i];
	}
	return str;
}
//--------------------------------------------------------------------

string format_addr_server(string addr)
{
	string str = "";
	bool write = false;

	for (unsigned int i = 0; i < addr.length(); i++)
	{
		if (addr[i] == '@') write = true;
		else if ((addr[i] == '>') || (i == addr.length())) break;
		else if (write)	str += addr[i];
	}
	if (str.empty())
		return "local";
	else
		return str;
}
//-----------------------------------------------------