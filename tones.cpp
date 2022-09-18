#include "tones.h"

using namespace std;
using namespace pj;

void tones_t::init(AudioMedia* playback)
{
	m_playback = playback;

	m_generator_1 = new ToneGenerator();
	m_generator_long = new ToneGenerator();
	m_generator_2 = new ToneGenerator();
	m_generator_dtmf = new ToneGenerator();

	if (m_generator_1 != nullptr)
		m_generator_1->createToneGenerator(44100, 1);
	if (m_generator_long != nullptr)
		m_generator_long->createToneGenerator(44100, 1);
	if (m_generator_2 != nullptr)
		m_generator_2->createToneGenerator(44100, 1);
	if (m_generator_dtmf != nullptr)
		m_generator_dtmf->createToneGenerator(44100, 1);

	m_loop = false;
	m_ready = false;
	m_ring = false;
	m_ringback = false;
}

void tones_t::close()
{
	if (m_generator_1 != nullptr) {
		if (m_generator_1->isBusy())
			m_generator_1->stop();
		delete m_generator_1;
	}

	if (m_generator_long != nullptr) {
		if (m_generator_long->isBusy())
			m_generator_long->stop();
		delete m_generator_long;
	}

	if (m_generator_2 != nullptr) {
		if (m_generator_2->isBusy())
			m_generator_2->stop();
		delete m_generator_2;
	}

	if (m_generator_dtmf != nullptr) {
		if (m_generator_dtmf->isBusy())
			m_generator_dtmf->stop();
		delete m_generator_dtmf;
	}
}
//-------------------------------------------------------

bool tones_t::tones_long_stop()
{
	if (m_generator_long == nullptr)
		return false;
	if (m_generator_long->isBusy()) {
		m_generator_long->stop();
		m_ready = false;
		m_ring = false;
		m_ringback = false;
	}
	return true;
}
//----------------------------------------------------------------

bool tones_t::tones_stop()
{
	if (m_generator_1 == nullptr)
		return false;
	if (m_generator_1->isBusy()) {
		m_generator_1->stop();
	}
	return true;
}
//-------------------------------------------------------

void tones_t::ready_start()
{
	if (m_ready)
		return;

	ToneDesc td;
	ToneDescVector v_td;

	td.freq1 = 400;
	td.freq2 = 440;
	td.on_msec = 1000;
	v_td.push_back(td);

	if (tones_long_stop()) {
		m_generator_long->play(v_td, true);
		m_generator_long->startTransmit(*m_playback);
		m_ready = true;
	}
}
//-------------------------------------------------------------------

void tones_t::ring_start()
{
		ToneDesc td;
		ToneDescVector v_td;

		td.freq1 = 600;
		td.freq2 = 1200;
		td.on_msec = 1000;
		td.off_msec = 2500;
		v_td.push_back(td);

		if (tones_long_stop()) {
			m_generator_long->play(v_td, true);
			m_generator_long->startTransmit(*m_playback);
			m_ring = true;
		}
}
//----------------------------------------------------------------

void tones_t::ring_back_start()
{
		if (m_ringback)
			return;

		ToneDesc td;
		ToneDescVector v_td;

		td.freq1 = 400;
		td.freq2 = 440;
		td.on_msec = 1000;
		td.off_msec = 2500;
		v_td.push_back(td);

		if (tones_long_stop()) {
			m_generator_long->play(v_td, true);
			m_generator_long->startTransmit(*m_playback);
			m_ringback = true;
		}
}
//----------------------------------------------------------------

void tones_t::ring_back_start_to(AudioMedia am)
{
	if (!m_generator_2->isBusy()) {
		ToneDesc td;
		ToneDescVector v_td;

		td.freq1 = 400;
		td.freq2 = 440;
		td.on_msec = 1000;
		td.off_msec = 2500;
		v_td.push_back(td);

		m_generator_2->play(v_td, true);
	}
	m_generator_2->startTransmit(am);
}
//----------------------------------------------------------

void tones_t::ring_back_stop_to(AudioMedia am)
{
	m_generator_2->stopTransmit(am);
}
//----------------------------------------------------------------
 
void tones_t::busy()
{
		ToneDesc td;
		ToneDescVector v_td;

		td.freq1 = 625;
		td.on_msec = 80;
		td.off_msec = 30;
		v_td.push_back(td);
		td.freq1 = 525;
		td.on_msec = 60;
		td.off_msec = 20;
		v_td.push_back(td);
		td.freq1 = 425;
		td.on_msec = 40;
		td.off_msec = 20;
		v_td.push_back(td);
		td.freq1 = 325;
		td.on_msec = 30;
		td.off_msec = 0;
		v_td.push_back(td);

		if (tones_stop()) {
			m_generator_1->play(v_td, false);
			m_generator_1->startTransmit(*m_playback);
		}
}
//----------------------------------------------------------------

void tones_t::key_clik()
{
	ToneDesc td;
	ToneDescVector v_td;

	td.freq1 = 697;
	td.freq2 = 1209;
	td.on_msec = 200;
	td.off_msec = 0;
	v_td.push_back(td);

	if (tones_stop()) {
		m_generator_1->play(v_td, false);
		m_generator_1->startTransmit(*m_playback);
	}
}
//--------------------------------------------------------------------

void tones_t::auto_answer_start()
{
		ToneDesc td;
		ToneDescVector v_td;

		td.freq1 = 941;
		td.freq2 = 163;
		td.on_msec = 400;
		td.off_msec = 0;
		v_td.push_back(td);

		if (tones_stop()) {
			m_generator_1->play(v_td, false);
			m_generator_1->startTransmit(*m_playback);
		}
}
//-------------------------------------------------------------------
void tones_t::ring_second_start()
{
	ToneDesc td;
	ToneDescVector v_td;

	td.freq1 = 770;
	td.freq2 = 1336;
	td.on_msec = 100;
	td.off_msec = 300;
	v_td.push_back(td);
	td.freq1 = 770;
	td.freq2 = 1336;
	td.on_msec = 100;
	td.off_msec = 2000;
	v_td.push_back(td);
	td.freq1 = 770;
	td.freq2 = 1336;
	td.on_msec = 100;
	td.off_msec = 300;
	v_td.push_back(td);
	td.freq1 = 770;
	td.freq2 = 1336;
	td.on_msec = 100;
	td.off_msec = 0;
	v_td.push_back(td);

	if (tones_stop()) {
		m_generator_1->play(v_td, false);
		m_generator_1->startTransmit(*m_playback);
	}
}
