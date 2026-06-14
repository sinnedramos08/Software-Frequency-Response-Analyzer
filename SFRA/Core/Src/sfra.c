/*
 * sfra.c
 *
 *  Created on: Jun 14, 2026
 *      Author: denni
 */


#include "sfra.h"

#include <math.h>
#include <string.h>

#define PI_F    (3.14159265359f)

sfra_t g_sfra;

void LUT_Init(void)
{
    for(int i = 0; i < DDS_LUT_SIZE; i++){
    	g_sfra.sine_lut[i] = sinf(2.0f * PI_F * ((float)i / (float)DDS_LUT_SIZE));
    }

}
#if 0
void SFRA_Run_Meas(void){
	switch(g_sfra.state)
	{
	case SFRA_STATE_INIT:
        g_sfra.freq_index = 0;
        g_sfra.current_freq = g_sfra.freq_table[0];
        g_sfra.settle_counter = 0;
        g_sfra.state = SFRA_STATE_SETTLING;
        break;

    case SFRA_STATE_SETTLING:
        g_sfra.settle_counter++;

        if(g_sfra.settle_counter >= g_sfra.settle_samples)
        {
        	g_sfra.measure_counter = 0;
        	g_sfra.input_I_acc = 0.0f;
        	g_sfra.input_Q_acc = 0.0f;

        	g_sfra.output_I_acc = 0.0f;
        	g_sfra.output_Q_acc = 0.0f;
            g_sfra.state = SFRA_STATE_NEXT_FREQ;
        }

        break;
	}

}
#endif

void SFRA_Run(void)
{
    switch(g_sfra.state)
    {
		case SFRA_STATE_INIT:

			g_sfra.freq_index = 0;

			SFRA_UpdateFrequency(g_sfra.freq_table[0]);

			g_sfra.settle_counter = 0;

			g_sfra.state = SFRA_STATE_SETTLING;
			printf("Start \n\n");

			break;

        case SFRA_STATE_SETTLING:

            g_sfra.settle_counter++;

            if(g_sfra.settle_counter >= g_sfra.settle_samples)
            {
                g_sfra.phase_acc = 0;
                g_sfra.index     = 0;

                g_sfra.measure_counter = 0;

                g_sfra.input_I_acc  = 0.0f;
                g_sfra.input_Q_acc  = 0.0f;

                g_sfra.output_I_acc = 0.0f;
                g_sfra.output_Q_acc = 0.0f;

                g_sfra.state = SFRA_STATE_MEASURING;
            }

            break;
        case SFRA_STATE_MEASURING:

            g_sfra.measure_counter++;

            if(g_sfra.measure_counter >= g_sfra.measure_samples)
            {
                g_sfra.state = SFRA_STATE_CALCULATE;
            }

            break;
        case SFRA_STATE_CALCULATE:

            SFRA_Calculate();

            g_sfra.state = SFRA_STATE_NEXT_FREQ;

            break;

        case SFRA_STATE_NEXT_FREQ:

            g_sfra.freq_index++;

            if(g_sfra.freq_index >= g_sfra.num_freqs)
            {
                g_sfra.state =
                    SFRA_STATE_DONE;
            }
            else
            {
                SFRA_UpdateFrequency(g_sfra.freq_table[g_sfra.freq_index]);
                g_sfra.settle_counter = 0;
                g_sfra.state = SFRA_STATE_SETTLING;
            }

            break;

        case SFRA_STATE_DONE:
        	printf("\n\nEnd");
        	HAL_Delay(1000);
        	g_sfra.state = SFRA_STATE_INIT;
        	break;
        default:
            break;
    }
}


void SFRA_Calculate(void)
{
	float input_mag;
	float input_amp;
	float input_phase;
	float output_mag;
	float output_amp;
	float output_phase;

	input_mag =sqrtf(g_sfra.input_I_acc * g_sfra.input_I_acc +g_sfra.input_Q_acc * g_sfra.input_Q_acc);
	input_amp =2.0f * input_mag /(float)g_sfra.measure_samples;
	input_phase =atan2f(g_sfra.input_Q_acc,g_sfra.input_I_acc);

	output_mag =sqrtf(g_sfra.output_I_acc * g_sfra.output_I_acc +g_sfra.output_Q_acc * g_sfra.output_Q_acc);
	output_amp =2.0f * output_mag /(float)g_sfra.measure_samples;
	output_phase =atan2f(g_sfra.output_Q_acc,g_sfra.output_I_acc);

	float gain = output_amp / input_amp;
	float gain_db = 20.0f * log10f(gain);
	float phase_deg = (output_phase - input_phase)*180.0f / PI_F;


	g_sfra.gain_db[g_sfra.freq_index] = gain_db;
	g_sfra.phase_deg[g_sfra.freq_index] = phase_deg;

	g_sfra.result_ready = 1;


}

void SFRA_UpdateFrequency(float freq)
{
    g_sfra.current_freq = freq;
    g_sfra.phase_inc =(uint32_t)(freq *4294967296.0f / 100000.0f);
    g_sfra.settle_samples = (uint32_t)(SFRA_SETTLING_CYCLES*100000.0f /freq);
    g_sfra.measure_samples = (uint32_t)(SFRA_MEASUREMENT_CYCLES * 100000.0f / freq);
    g_sfra.phase_acc = 0;
    g_sfra.index     = 0;
}
void SFRA_Init(void)
{
    memset(&g_sfra, 0, sizeof(g_sfra));
#if 0
    /* 50 Hz -> 100 Hz (10 Hz steps) */
    g_sfra.freq_table[0]  = 50.0f;
    g_sfra.freq_table[1]  = 60.0f;
    g_sfra.freq_table[2]  = 70.0f;
    g_sfra.freq_table[3]  = 80.0f;
    g_sfra.freq_table[4]  = 90.0f;
    g_sfra.freq_table[5]  = 100.0f;

    /* 200 Hz -> 1 kHz (50 Hz steps) */
    g_sfra.freq_table[6]  = 200.0f;
    g_sfra.freq_table[7]  = 250.0f;
    g_sfra.freq_table[8]  = 300.0f;
    g_sfra.freq_table[9]  = 350.0f;
    g_sfra.freq_table[10] = 400.0f;
    g_sfra.freq_table[11] = 450.0f;
    g_sfra.freq_table[12] = 500.0f;
    g_sfra.freq_table[13] = 550.0f;
    g_sfra.freq_table[14] = 600.0f;
    g_sfra.freq_table[15] = 650.0f;
    g_sfra.freq_table[16] = 700.0f;
    g_sfra.freq_table[17] = 750.0f;
    g_sfra.freq_table[18] = 800.0f;
    g_sfra.freq_table[19] = 850.0f;
    g_sfra.freq_table[20] = 900.0f;
    g_sfra.freq_table[21] = 950.0f;
    g_sfra.freq_table[22] = 1000.0f;

    /* 1 kHz -> 10 kHz (500 Hz steps) */
    g_sfra.freq_table[23] = 1000.0f;
    g_sfra.freq_table[24] = 1500.0f;
    g_sfra.freq_table[25] = 2000.0f;
    g_sfra.freq_table[26] = 2500.0f;
    g_sfra.freq_table[27] = 3000.0f;
    g_sfra.freq_table[28] = 3500.0f;
    g_sfra.freq_table[29] = 4000.0f;
    g_sfra.freq_table[30] = 4500.0f;
    g_sfra.freq_table[31] = 5000.0f;
    g_sfra.freq_table[32] = 5500.0f;
    g_sfra.freq_table[33] = 6000.0f;
    g_sfra.freq_table[34] = 6500.0f;
    g_sfra.freq_table[35] = 7000.0f;
    g_sfra.freq_table[36] = 7500.0f;
    g_sfra.freq_table[37] = 8000.0f;
    g_sfra.freq_table[38] = 8500.0f;
    g_sfra.freq_table[39] = 9000.0f;
    g_sfra.freq_table[40] = 9500.0f;
    g_sfra.freq_table[41] = 10000.0f;

    /* 20 kHz -> 50 kHz (5 kHz steps) */
    g_sfra.freq_table[42] = 20000.0f;
    g_sfra.freq_table[43] = 25000.0f;
    g_sfra.freq_table[44] = 30000.0f;
    g_sfra.freq_table[45] = 35000.0f;
    g_sfra.freq_table[46] = 40000.0f;
    g_sfra.freq_table[47] = 45000.0f;
    g_sfra.freq_table[48] = 50000.0f;

    g_sfra.num_freqs = 49;
#else
    g_sfra.freq_table[0]  = 50.0f;
    g_sfra.freq_table[1]  = 63.1f;
    g_sfra.freq_table[2]  = 79.4f;
    g_sfra.freq_table[3]  = 100.0f;
    g_sfra.freq_table[4]  = 126.0f;
    g_sfra.freq_table[5]  = 158.0f;
    g_sfra.freq_table[6]  = 200.0f;
    g_sfra.freq_table[7]  = 251.0f;
    g_sfra.freq_table[8]  = 316.0f;
    g_sfra.freq_table[9]  = 398.0f;
    g_sfra.freq_table[10] = 501.0f;
    g_sfra.freq_table[11] = 631.0f;
    g_sfra.freq_table[12] = 794.0f;
    g_sfra.freq_table[13] = 1000.0f;

    g_sfra.freq_table[14] = 1260.0f;
    g_sfra.freq_table[15] = 1580.0f;
    g_sfra.freq_table[16] = 2000.0f;
    g_sfra.freq_table[17] = 2510.0f;
    g_sfra.freq_table[18] = 3160.0f;
    g_sfra.freq_table[19] = 3980.0f;
    g_sfra.freq_table[20] = 5010.0f;
    g_sfra.freq_table[21] = 6310.0f;
    g_sfra.freq_table[22] = 7940.0f;
    g_sfra.freq_table[23] = 10000.0f;

    g_sfra.freq_table[24] = 12600.0f;
    g_sfra.freq_table[25] = 15800.0f;
    g_sfra.freq_table[26] = 20000.0f;
    g_sfra.freq_table[27] = 25100.0f;
    g_sfra.freq_table[28] = 31600.0f;
    g_sfra.freq_table[29] = 39800.0f;
    g_sfra.num_freqs = 32;
#endif
    g_sfra.amplitude = 1240.9090f;

    /* Temporary value for frequency sweep verification */
    g_sfra.settle_samples = 200000;

    g_sfra.state = SFRA_STATE_INIT;
}



