#pragma once

/*авто конфигурирование big/little endian*/
#define PJ_AUTOCONF	1
#include <pjsua2.hpp>

class tones_t
{
private:
	pj::ToneGenerator* m_generator_1;
	pj::ToneGenerator* m_generator_2;
	pj::ToneGenerator* m_generator_long;
	pj::ToneGenerator* m_generator_dtmf;
	pj::ToneDigitMapVector m_dv;
	pj::AudioMedia* m_playback;
	bool m_loop = false;
	bool m_ready = false;
	bool m_ring = false;
	bool m_ringback = false;

public:
	tones_t() {}
	~tones_t() {}

	void init(pj::AudioMedia* playback);
	void close();
	bool tones_stop();
	bool tones_long_stop();
	void ring_start();
	void ring_back_start();
	void ring_back_start_to(pj::AudioMedia am);
	void ring_back_stop_to(pj::AudioMedia am);
	void ready_start();
	void busy();
	void key_clik();
	void auto_answer_start();
	void ring_second_start();
};


