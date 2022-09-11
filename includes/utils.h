#pragma once
#include <string>
#include <atomic>
#include <chrono>

typedef std::chrono::time_point<std::chrono::system_clock> Time;

/* Пролучить дату в строке */
std::string get_data_str();
std::string get_time_ms_str(bool add_ms = false);

/*получить время в uint64_t*/
uint64_t get_time_ms();
uint64_t get_time_s();
uint64_t get_time_ms(Time);
uint64_t get_time_s(Time);

/*манипуляции с числами*/
int32_t twoint16_oneint32(int16_t h, int16_t l);
void oneint32_twoint16(int16_t &h, int16_t &l, int32_t val);
int get_random_number(int min, int max);
std::string pi_calc(int n);

/*ожидание*/
void ssleep(size_t sec);
void msleep(size_t msec);
bool wait4(bool* flag, uint64_t msec);
bool wait4(std::atomic<bool>* flag, uint64_t msec);