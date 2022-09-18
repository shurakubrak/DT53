/*
 * filter.h
 *
 *  Created on: 8 дек. 2017 г.
 *      Author: shura
 */

#ifndef FILTER_H_
#define FILTER_H_

#include <math.h>
 /*авто конфигурирование big/little endian*/
#define PJ_AUTOCONF	1
#include <pjsua2.hpp>

#define MAX_FILTER_COUNT 50
#define COUNT_SAMPLES 31 //минимум
#define MAX_FILTER_KOEFS_COUNT COUNT_SAMPLES*3
#define M_PI 3.14159265358979323846
#define POINT_TO_FILTER 0
#define AFR0 filterKoefs[COUNT_SAMPLES+1]
#define AFR1 filterKoefs[COUNT_SAMPLES+2]
#define AFR2 filterKoefs[COUNT_SAMPLES+3]
#define BFR1  filterKoefs[COUNT_SAMPLES+4]
#define BFR2  filterKoefs[COUNT_SAMPLES+5]
#define ZFR1  filterKoefs[COUNT_SAMPLES+6]
#define ZFR2  filterKoefs[COUNT_SAMPLES+7]
#define NORM_FR filterKoefs[COUNT_SAMPLES+8]
#define Q_FR filterKoefs[COUNT_SAMPLES+9]
#define TAN_FR filterKoefs[COUNT_SAMPLES+10]
#define VGAIN_FR filterKoefs[COUNT_SAMPLES+11]

extern "C" int(*otherFilter) (pjmedia_snd_port *snd_port, pjmedia_frame *frame);
extern "C" int pointApplyFilter;

struct filter_data_t
{
	//тип фильтра
	// 1  - FIR_lowPass;
	// 2  - FIR_highPass;
	// 3  - FIR_bandPass;
	// 4  - FIR_bandCut; (ФПЗ)
	// 21 - biQuad_lowPass;
	// 22 - biQuad_highPass;
	// 23 - biQuad_BandPass;
	// 24 - biQuad_notch; (ФПЗ)
	// 25 - biQuad_PeakEQ;
	// 26 - biQuad_lowShelfEQ;
	// 27 - biQuad_highShelfEQ;
	int filterType;
	// порядок применения для FIR
	// для biQuad: 1 - применить к результату предыдущих фильтров,  0 - применить к оригинальному значению
	//             +0 - накопить результат, + 10 - обновить результат
	//             т.е. 0,1,10,11
	int filterOrder;
	//частота
	int filterFrequency;
	//ширина полосы для FIR
	int filterBandWidth;
	// крутизна перехода для BiQuad
	// default: 0.7071 для biQuad_lowPass И biQuad_notch
	//	        3.0   для biQuad_highPass
	//         20.0   для biQuad_BandPass
	//         10.0   для biQuad_PeakEQ
	//          1.0   для biQuad_lowShelfEQ и biQuad_highShelfEQ;
	float filterPitch;
	// коэффициет усиления-ослабления в Дб
	int filterBustCutDB;
	// расчетные коэффициенты для фильтров
	float filterKoefs[MAX_FILTER_KOEFS_COUNT];
	// количество расчетных коэффициентов в фильтре
	int countKoefs;
	// частота дискретизации
	int calculatedSampleRate;
};

struct pjmedia_snd_port
{
	int			 rec_id;
	int			 play_id;
	pj_uint32_t		 aud_caps;
	pjmedia_aud_param	 aud_param;
	pjmedia_aud_stream	*aud_stream;
	pjmedia_dir		 dir;
	pjmedia_port	*port;

	pjmedia_clock_src    cap_clocksrc,
		play_clocksrc;

	unsigned		 clock_rate;
	unsigned		 channel_count;
	unsigned		 samples_per_frame;
	unsigned		 bits_per_sample;
	unsigned		 options;
	unsigned		 prm_ec_options;

	/* software ec */
	pjmedia_echo_state	*ec_state;
	unsigned		 ec_options;
	unsigned		 ec_tail_len;
	pj_bool_t		 ec_suspended;
	unsigned		 ec_suspend_count;
	unsigned		 ec_suspend_limit;

	/* audio frame preview callbacks */
	void		*user_data;
	pjmedia_aud_play_cb  on_play_frame;
	pjmedia_aud_rec_cb   on_rec_frame;
};

int  init_input_filters(filter_data_t* new_filters_data);
void set_mic_filters();

#endif /* FILTER_H_ */
