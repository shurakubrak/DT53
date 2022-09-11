#pragma once

/*Structyres*/
struct stNetwork
{
	string ip_addr;
	string netmask;
	string mac;
	string gateway;
	stNetwork() {
		ip_addr = "127.0.0.1";
		netmask = "255.255.255.0";
		mac = "00:00:00:00:00:00";
		gateway = "8.8.8.8";
	}
};

/*Variable*/
uint64_t t_blink;
uint64_t t_control;
string ip_address;
string path;

/*Functions*/
void vers_safe();
bool read_param();
bool read_param_ex();
bool get_network(stNetwork* nw, bool wlan);/*получить параметры сети*/
bool is_valid_ip_address(string checkValue);