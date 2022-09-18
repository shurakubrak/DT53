#pragma once

#include "sip.h"
#include "filter.h"
#include "device.h"

#define REQUEST_DB	3/*сек*/

/*Variable*/
bool f_blink = OFF;
uint64_t t_blink;
uint64_t t_control;
std::string ip_address;
std::string path;
FILE* pFile;
publ_settings_t setts;
int stars = 0;

bool md_call_horn = false;
bool md_call_speecker = false;
bool md_call_mic = false;



/*Functions*/
void vers_safe();
bool read_param();
bool read_param_ex(sip_t* sip);
bool read_param_dsp(device_t* device);
bool reload_param(sip_t* sip);
void parse_filter(std::vector<std::string> v_filter, filter_data_t* filters_data);
void find_cpu();
bool buttons_fill(keypad_t* kp, std::vector<button_t> buttons);
void app_control();
void initial(sip_t* sip);
bool eth_control();
void function_dial(int button_id, sip_t* sip);
void blink(gpio_t* gpio);

std::atomic<bool> button_in_down;
int64_t requestDB = get_time_ms();

void U2S(sip_t* sip);
void S2D(sip_t* sip);
void T2D(sip_t* sip);
void blink(gpio_t* gpio);

