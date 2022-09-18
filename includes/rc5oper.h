#pragma once
#include "RC5Simple.h"


static inline bool is_base64(unsigned char c);
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const& encoded_string);
void set_key();
std::string encrypt_licence(unsigned char* data, int sz);
std::string decrypt_licence(std::string b64);
