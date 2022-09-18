#include "rc5oper.h"

using namespace std;

#define DATA_CRYPT_SIZE 1024

static const string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";
RC5Simple rc5;

static inline bool is_base64(unsigned char c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}
//------------------------------------------------------------------------------

string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len)
{
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';

	}

	return ret;
}
//-------------------------------------------------------------------------

string base64_decode(std::string const& encoded_string)
{
	size_t in_len = encoded_string.size();
	size_t i = 0;
	size_t j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++)
			char_array_4[j] = 0;

		for (j = 0; j < 4; j++)
			char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
	}

	return ret;
}
//---------------------------------------------------------------------------------

void set_key()
{
	unsigned char key[RC5_B];
	vector<unsigned char> v_key(RC5_B);
	try
	{
		// Generate key (in real, generate from password)
		for (int i = 0; i < RC5_B; i++) {
			key[i] = (i + 7) * 3;
			key[i] = (key[i] - 16) * 4;
		}
		//cout << key << endl;
		// Convert key to vector
		for (int i = 0; i < RC5_B; i++)
			v_key[i] = key[i];
		rc5.RC5_SetFormatVersionForce(RC5_FORMAT_VERSION_3);
		rc5.RC5_SetKey(v_key);
	}
	catch (...) {
		return;
	}
}
//-------------------------------------------------------

string encrypt_licence(unsigned char* data, int sz)
{
	vector<unsigned char> v_crypt_data;
	try
	{
		// Convert data array to vector
		vector<unsigned char> v_data(sz);
		for (int i = 0; i < sz; i++)
			v_data[i] = data[i];


		rc5.RC5_Encrypt(v_data, v_crypt_data);

		unsigned char bytes_to_encode[DATA_CRYPT_SIZE];
		for (unsigned int i = 0; i < v_crypt_data.size(); i++)
			bytes_to_encode[i] = v_crypt_data[i];
		string b64 = base64_encode(bytes_to_encode, v_crypt_data.size());

		//cout << "License key: " << b64.c_str() << endl;
		return b64;
	}
	catch (...) {
		return "";
	}
}
//---------------------------------------------------------------------------

string decrypt_licence(string b64)
{
	string data = "";

	try
	{
		string str = base64_decode(b64);
		vector<unsigned char> v_crypt_data(str.size());
		for (unsigned int i = 0; i < str.size(); i++)
			v_crypt_data[i] = str[i];

		vector<unsigned char> v_data;
		rc5.RC5_Decrypt(v_crypt_data, v_data);
		for (unsigned int i = 0; i < v_data.size(); i++)
			data += v_data[i];

		cout << "Serial number: " << data.c_str() << endl;
		return data;
	}
	catch (...) {
		return "";
	}
}