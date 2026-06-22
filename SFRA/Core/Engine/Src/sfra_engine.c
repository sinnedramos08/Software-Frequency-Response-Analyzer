/*
 * sfra_engine.c
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */


#include "stdio.h"
#include <stdbool.h>
#include <math.h>
#include "stm32g4xx_hal.h"
#include <sfra_engine.h>
#include <string.h>

#define PI_F    (3.14159265359f)

sfra_t g_sfra;

void LUT_Init(void)
{
	/* For Sine Look Up Table */
    for(int i = 0; i < DDS_LUT_SIZE; i++){
    	g_sfra.sine_lut[i] = sinf(2.0f * PI_F * ((float)i / (float)DDS_LUT_SIZE));
    }

}

void SFRA_Run(void)
{
    switch(g_sfra.state)
    {
		case SFRA_STATE_INIT:

			g_sfra.start_time_ms = HAL_GetTick();

			g_sfra.b_start_flag = true;

			g_sfra.freq_index = 0;
			SFRA_UpdateFrequency(g_sfra.freq_table[0]);
			g_sfra.settle_counter = 0;
			g_sfra.state = SFRA_STATE_SETTLING;

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
            	g_sfra.b_result_ready_flag = false;
                g_sfra.state = SFRA_STATE_DONE;
            }
            else
            {
                SFRA_UpdateFrequency(g_sfra.freq_table[g_sfra.freq_index]);
                g_sfra.settle_counter = 0;
                g_sfra.state = SFRA_STATE_SETTLING;
            }

            break;

        case SFRA_STATE_DONE:
            g_sfra.end_time_ms = HAL_GetTick();

            g_sfra.elapsed_time_ms = g_sfra.end_time_ms - g_sfra.start_time_ms;


			g_sfra.b_end_flag = true;
			g_sfra.state = SFRA_STATE_STOP;
        	break;

        case SFRA_STATE_STOP:
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

	g_sfra.b_result_ready_flag=true;


}

void SFRA_UpdateFrequency(float freq)
{
    g_sfra.current_freq = freq;
    g_sfra.phase_inc =(uint32_t)(freq *DDS_FULL_SCALE/ FLOAT_SFRA_FS_HZ);
    g_sfra.settle_samples = (uint32_t)(SFRA_SETTLING_CYCLES*FLOAT_SFRA_FS_HZ/freq);
    g_sfra.measure_samples = (uint32_t)(SFRA_MEASUREMENT_CYCLES * FLOAT_SFRA_FS_HZ/ freq);
    g_sfra.phase_acc = 0;
    g_sfra.index     = 0;
}
void SFRA_Init(void)
{
    memset(&g_sfra, 0, sizeof(g_sfra));
    SFRA_GenerateFrequencyTable();

    // Initialize Control Variables
    g_sfra.amplitude = SINE_INJECTED_AMPLITUDE_ADC;	// For 1V Amplitude Signal in Oscilloscope
    g_sfra.state = SFRA_STATE_INIT;
    g_sfra.b_result_ready_flag=false;

}

void SFRA_GenerateFrequencyTable(void){
    float decades;
    float ratio;

    // Get number of Decades - ~4 Decades from 10Hz to 40kHz
    decades = log10f((float)(FREQ_STOP_HZ)/(float)(FREQ_START_HZ)); // Number of decades = log(40000/10)

    // Get number of frequency based on number of decades and points per decade
    g_sfra.num_freqs =(uint16_t)(decades * FREQ_POINTS_PER_DECADE) + 1U; // (3.6*10)+1=37 frequencies

    // Using Logarithmic scale, we need to have common ratio (not common difference) between frequency points
    // Since we need X data points per decade, formula: ratio = 10^(1/x)
    ratio = powf(10.0f,1.0f/(float)(FREQ_POINTS_PER_DECADE));

    // Populate the Frequency Buffer

    // First Frequency at index 0 -> Start Frequency
    g_sfra.freq_table[0] = (float)FREQ_START_HZ;

    // Populate with Geometric Ratio
    for(uint16_t i = 1U;i < g_sfra.num_freqs;i++){
    	g_sfra.freq_table[i] = g_sfra.freq_table[i - 1U] * ratio;
    }

    // Last Frequency at index #frequency-1-> Stop Frequency
    g_sfra.freq_table[g_sfra.num_freqs - 1U] = (float)FREQ_STOP_HZ;

}
