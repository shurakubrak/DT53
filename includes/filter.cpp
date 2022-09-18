#include "filter.h"
#include <cstring>

filter_data_t filters_data[MAX_FILTER_COUNT];
int samples[COUNT_SAMPLES - 1];
unsigned int sample_rate_lib;

int apply_filter_one_sample(int sample)
{
	//сумма коэффициентов
	//	float tempRetvals[MAX_FILTER_COUNT] = { 0.0 };
	//количество одинаковых порядков
	//	int countRetvals[MAX_FILTER_COUNT] = { 0 };
	int retval, j, i;
	retval = 0;

	if (filters_data[0].filterType > 0)
	{
		for (j = 0; j < MAX_FILTER_COUNT; j++)
		{
			if (filters_data[j].filterType <= 0)
				break;
			if (filters_data[j].filterType < 20)
			{
				for (i = 0; i < COUNT_SAMPLES; i++)
				{

					retval += samples[i] * filters_data[j].filterKoefs[i];
				}
			}
			else //type>20
			{
				float curV = retval;
				float tempCurv = 0.0;
				int ktr = filters_data[j].filterOrder % 10;
				if (j == 0 || ktr == 0)
					curV = sample;
				tempCurv = curV * filters_data[j].AFR0 + filters_data[j].ZFR1;
				filters_data[j].ZFR1 = curV * filters_data[j].AFR1 + filters_data[j].ZFR2 - filters_data[j].BFR1 * tempCurv;
				filters_data[j].ZFR2 = curV * filters_data[j].AFR2 - filters_data[j].BFR2 * tempCurv;


				if (filters_data[j].filterOrder < 10)
					retval += tempCurv;
				else
					retval = tempCurv;
			}
		}
	}
	else
	{
		retval = sample;
	}
	for (i = 0; i < COUNT_SAMPLES - 2; i++)
		samples[i] = samples[i + 1];
	samples[COUNT_SAMPLES - 2] = sample;
	return retval;
}
/*******************************************************************************/

int init_one_filter(int number_filter)
{

	if (number_filter < 0 || number_filter >= MAX_FILTER_COUNT)
		return -1;
	if (sample_rate_lib == 0)
		sample_rate_lib = 8000;

	//безразмерная частота среза
	//float curfilterBandWidth = 50;

	float Fc = (float)filters_data[number_filter].filterFrequency / sample_rate_lib;
	float Fc1 = (float)(filters_data[number_filter].filterFrequency - filters_data[number_filter].filterBandWidth) / sample_rate_lib;
	float Fc2 = (float)(filters_data[number_filter].filterFrequency + filters_data[number_filter].filterBandWidth) / sample_rate_lib;
	//безразмерный коэффициент
	float alfaK = 2 * M_PI * Fc;
	float alfaK1 = 2 * M_PI * Fc1;
	float alfaK2 = 2 * M_PI * Fc2;

	//Биквадратичные коэффициенты
	//float tanK = tan(M_PI * Fc);

	float sumKoef = 0.0;
	int i, center;
	filters_data[number_filter].countKoefs = COUNT_SAMPLES;
	center = (COUNT_SAMPLES - 1) / 2;
	filters_data[number_filter].calculatedSampleRate = sample_rate_lib;

	switch (filters_data[number_filter].filterType)
	{
	case 1:
		filters_data[number_filter].filterKoefs[center] = 2 * Fc;
		sumKoef = filters_data[number_filter].filterKoefs[center];
		for (i = 1; i < center; i++)
		{
			filters_data[number_filter].filterKoefs[center - i] = sin(alfaK * i) / (M_PI * i);
			filters_data[number_filter].filterKoefs[center + i] = filters_data[number_filter].filterKoefs[center - i];
			sumKoef += 2 * filters_data[number_filter].filterKoefs[center - i];
		}
		//нормализация
		for (i = 0; i < COUNT_SAMPLES; i++)
			filters_data[number_filter].filterKoefs[i] /= sumKoef;
		break;
	case 2:


		filters_data[number_filter].filterKoefs[center] = 1 - (2 * Fc);
		sumKoef = filters_data[number_filter].filterKoefs[center];
		for (i = 1; i < center; i++)
		{
			filters_data[number_filter].filterKoefs[center - i] = -sin(alfaK * i) / (M_PI * i);
			filters_data[number_filter].filterKoefs[center + i] = filters_data[number_filter].filterKoefs[center - i];
			sumKoef += 2 * filters_data[number_filter].filterKoefs[center - i];
		}
		break;
	case 3:
		filters_data[number_filter].filterKoefs[center] = 2 * (Fc2 - Fc1);
		sumKoef = filters_data[number_filter].filterKoefs[center];
		for (i = 1; i < center; i++)
		{
			filters_data[number_filter].filterKoefs[center - i] = (sin(alfaK2 * i) - sin(alfaK1 * i)) / (M_PI * i);
			filters_data[number_filter].filterKoefs[center + i] = filters_data[number_filter].filterKoefs[center - i];
			sumKoef += 2 * filters_data[number_filter].filterKoefs[center - i];
		}
		break;
	case 4:

		filters_data[number_filter].filterKoefs[center] = 1 - (2 * (Fc2 - Fc1));
		sumKoef = filters_data[number_filter].filterKoefs[center];
		for (i = 1; i < center; i++)
		{
			filters_data[number_filter].filterKoefs[center - i] = (sin(alfaK1 * i) - sin(alfaK2 * i)) / (M_PI * i);
			filters_data[number_filter].filterKoefs[center + i] = filters_data[number_filter].filterKoefs[center - i];
			sumKoef += 2 * filters_data[number_filter].filterKoefs[center - i];
		}
		////нормализация
		//for (i = 0; i < COUNT_SAMPLES; i++)
		//{
		//	filters_data[number_filter].filterKoefs[i] /= sumKoef;
		//}
		break;
	case 21: //biquad_lowPass
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterBustCutDB) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 0.7071;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].NORM_FR = 1 / (1 + filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
		filters_data[number_filter].AFR0 = filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].AFR1 = 2 * filters_data[number_filter].AFR0;
		filters_data[number_filter].AFR2 = filters_data[number_filter].AFR0;
		filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].BFR2 = (1 - filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		break;
	case 22: //biquad_HighPass
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterBustCutDB) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 3.0;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].NORM_FR = 1 / (1 + filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
		filters_data[number_filter].AFR0 = 1 * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].AFR1 = -2 * filters_data[number_filter].AFR0;
		filters_data[number_filter].AFR2 = filters_data[number_filter].AFR0;
		filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].BFR2 = (1 - filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		break;
	case 23: //biquad_BandPass
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterBustCutDB) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 20.0;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].NORM_FR = 1 / (1 + filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
		filters_data[number_filter].AFR0 = filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].AFR1 = 0;
		filters_data[number_filter].AFR2 = -1 * filters_data[number_filter].AFR0;
		filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].BFR2 = (1 - filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		break;
	case 24: //biquad_notch
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterOrder) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 0.7071;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].NORM_FR = 1 / (1 + filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
		filters_data[number_filter].AFR0 = (1 + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].AFR2 = filters_data[number_filter].AFR0;
		filters_data[number_filter].BFR1 = filters_data[number_filter].AFR1;
		filters_data[number_filter].BFR2 = (1 - filters_data[number_filter].TAN_FR / filters_data[number_filter].Q_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		break;
	case 25: //biquad_PeakEQ
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterBustCutDB) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 10.0;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		if (filters_data[number_filter].filterBustCutDB >= 0)
		{
			filters_data[number_filter].NORM_FR = 1 / (1 + 1 / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
			filters_data[number_filter].AFR0 = (1 + filters_data[number_filter].VGAIN_FR / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR2 = (1 - filters_data[number_filter].VGAIN_FR / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR1 = filters_data[number_filter].AFR1;
			filters_data[number_filter].BFR2 = (1 - 1 / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		}
		else
		{
			filters_data[number_filter].NORM_FR = 1 / (1 + filters_data[number_filter].VGAIN_FR / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
			filters_data[number_filter].AFR0 = (1 + 1 / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR2 = (1 - 1 / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR1 = filters_data[number_filter].AFR1;
			filters_data[number_filter].BFR2 = (1 - filters_data[number_filter].VGAIN_FR / filters_data[number_filter].Q_FR * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		}
		break;
	case 26: //biquad_lowshelfEQ
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterBustCutDB) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 1.0;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		if (filters_data[number_filter].filterBustCutDB >= 0)
		{
			filters_data[number_filter].NORM_FR = 1 / (1 + sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
			filters_data[number_filter].AFR0 = (1 + sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].VGAIN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].VGAIN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR2 = (1 - sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].VGAIN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR2 = (1 - sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		}
		else
		{
			filters_data[number_filter].NORM_FR = 1 / (1 + sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].VGAIN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
			filters_data[number_filter].AFR0 = (1 + sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR2 = (1 - sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].VGAIN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR2 = (1 - sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].VGAIN_FR * filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		}
		break;
	case 27: //biquad_highshelfEQ
		filters_data[number_filter].TAN_FR = tan(M_PI * Fc);
		filters_data[number_filter].VGAIN_FR = pow(10, (float)fabs(filters_data[number_filter].filterBustCutDB) / 20.0);
		if (filters_data[number_filter].filterPitch < 0.01)
			filters_data[number_filter].Q_FR = 1.0;
		else
			filters_data[number_filter].Q_FR = filters_data[number_filter].filterPitch;
		filters_data[number_filter].ZFR1 = 0.0;
		filters_data[number_filter].ZFR2 = 0.0;
		if (filters_data[number_filter].filterBustCutDB >= 0)
		{
			filters_data[number_filter].NORM_FR = 1 / (1 + sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
			filters_data[number_filter].AFR0 = (filters_data[number_filter].VGAIN_FR + sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR2 = (filters_data[number_filter].VGAIN_FR - sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR2 = (1 - sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		}
		else
		{
			filters_data[number_filter].NORM_FR = 1 / (filters_data[number_filter].VGAIN_FR + sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR);
			filters_data[number_filter].AFR0 = (1 + sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - 1) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].AFR2 = (1 - sqrt(2) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR1 = 2 * (filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR - filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].NORM_FR;
			filters_data[number_filter].BFR2 = (filters_data[number_filter].VGAIN_FR - sqrt(2 * filters_data[number_filter].VGAIN_FR) * filters_data[number_filter].TAN_FR + filters_data[number_filter].TAN_FR * filters_data[number_filter].TAN_FR) * filters_data[number_filter].NORM_FR;
		}
		break;


	}

	return 0;
}
/*********************************************************************/

int  init_input_filters(filter_data_t* new_filters_data)
{
	int i, j;
	int countFilters = 0;

	for (int i = 0; i < MAX_FILTER_COUNT; i++) {
		filters_data[i].calculatedSampleRate = new_filters_data[i].calculatedSampleRate;
		filters_data[i].countKoefs = new_filters_data[i].countKoefs;
		filters_data[i].filterBandWidth = new_filters_data[i].filterBandWidth;
		filters_data[i].filterBustCutDB = new_filters_data[i].filterBustCutDB;
		filters_data[i].filterFrequency = new_filters_data->filterFrequency;
		filters_data[i].filterOrder = new_filters_data[i].filterOrder;
		filters_data[i].filterPitch = new_filters_data->filterPitch;
		filters_data[i].filterType = new_filters_data[i].filterType;
		memcpy(filters_data[i].filterKoefs, new_filters_data[i].filterKoefs, MAX_FILTER_KOEFS_COUNT);
	}

	for (j = 0; j < MAX_FILTER_COUNT; j++)
	{
		filters_data[j].countKoefs = 0;
		for (i = 0; i < MAX_FILTER_KOEFS_COUNT; i++)
		{
			filters_data[j].filterKoefs[i] = 0.0;
		}
		filters_data[j].calculatedSampleRate = 0;
		if (filters_data[j].filterType > 0)
		{
			init_one_filter(j);
			countFilters++;
		}
	}
	return countFilters;
}
/**********************************************************************/

int check_mic_frame(pjmedia_snd_port* snd_port, pjmedia_frame* frame)
{
	int retVal = 0;
	if (sample_rate_lib != snd_port->clock_rate)
	{
		sample_rate_lib = snd_port->clock_rate;
		init_input_filters(filters_data);
	}
	
	for (unsigned int k1 = 0; k1 < (snd_port->samples_per_frame); k1++)
		((pj_int16_t*)(frame->buf))[k1] = 
			apply_filter_one_sample(((pj_int16_t*)(frame->buf))[k1]);
	return retVal;
}
/**********************************************************************/

void set_mic_filters()
{
	pointApplyFilter = POINT_TO_FILTER; //0-после эхоподавления, 1- до эхоподавления
	//	otherFilter = 0; //в этом случае процедура фильтрации не вызывается
	otherFilter = check_mic_frame; //имя функции, которая будет вызываться после каждого заполнения буфера
}
