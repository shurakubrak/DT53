#include <cassert>
#include <vector>
#include <iostream>
#include <thread>
#include "utils.h"

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