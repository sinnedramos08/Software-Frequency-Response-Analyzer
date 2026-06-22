/*
 * sfra_engine.c
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

// Includes
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "main.h"
//#include "stm32g4xx_hal.h"
#include "sfra_engine.h"
#include "dds.h"
#include "iq.h"

// Macro
#define PI_F    (3.14159265359f)


// Struct Instance
sfra_t g_sfra;

void SFRA_Run(void)
{
    switch(g_sfra.state)
    {
		case SFRA_STATE_INIT:
			SFRA_StateInit_Handler();
			break;
        case SFRA_STATE_SETTLING:
			SFRA_StateSettling_Handler();
            break;
        case SFRA_STATE_MEASURING:
        	SFRA_StateMeasuring_Handler();
            break;
        case SFRA_STATE_CALCULATE:
        	SFRA_StateCalculate_Handler();
            break;
        case SFRA_STATE_NEXT_FREQ:
        	SFRA_StateNextFreq_Handler();
            break;
        case SFRA_STATE_DONE:
        	SFRA_StateDone_Handler();\
        	break;
        case SFRA_STATE_STOP:
        	break;
        default:
            break;
    }
}

// FSM Handlers
static void SFRA_StateInit_Handler(void)
{
	g_sfra.start_time_ms = HAL_GetTick();
	g_sfra.b_start_flag = true;
	g_sfra.freq_index = 0;
	DDS_UpdateFrequency(g_sfra.freq_table[0]);
	g_sfra.settle_counter = 0;
	g_sfra.state = SFRA_STATE_SETTLING;

}

static void SFRA_StateSettling_Handler(void)
{
    g_sfra.settle_counter++;

    if(g_sfra.settle_counter >= g_sfra.settle_samples)
    {
        g_dds.u32_phase_acc = 0;
        g_dds.u32_LUT_index     = 0;

        g_sfra.measure_counter = 0;

        IQ_Reset();
        g_sfra.state = SFRA_STATE_MEASURING;
    }

}

static void SFRA_StateMeasuring_Handler(void)
{
    g_sfra.measure_counter++;

    if(g_sfra.measure_counter >= g_sfra.measure_samples)
    {
        g_sfra.state = SFRA_STATE_CALCULATE;
    }

}

static void SFRA_StateCalculate_Handler(void)
{
	iq_result_t g_iq_result_t;
	float gain;

	IQ_Calculate(g_sfra.measure_samples, &g_iq_result_t);

	// Guard for input_amp ~= 0
	if(g_iq_result_t.f_input_amp > 1e-12f)
	{
	    gain = g_iq_result_t.f_output_amp / g_iq_result_t.f_input_amp;

	}
	else
	{
	    gain = 0.0f;

	}

	float gain_db = 20.0f * log10f(gain);
	float phase_deg = (g_iq_result_t.f_output_phase - g_iq_result_t.f_input_phase)*180.0f / PI_F;

	g_sfra.gain_db[g_sfra.freq_index] = gain_db;
	g_sfra.phase_deg[g_sfra.freq_index] = phase_deg;

	g_sfra.b_result_ready_flag=true;
    g_sfra.state = SFRA_STATE_NEXT_FREQ;
}

static void SFRA_StateNextFreq_Handler(void)
{
    g_sfra.freq_index++;

    if(g_sfra.freq_index >= g_sfra.num_freqs)
    {
        g_sfra.b_result_ready_flag = false;

        g_sfra.state =SFRA_STATE_DONE;
    }
    else
    {
        DDS_UpdateFrequency(
            g_sfra.freq_table[g_sfra.freq_index]);

        g_sfra.settle_counter = 0;

        g_sfra.state = SFRA_STATE_SETTLING;
    }

}

static void SFRA_StateDone_Handler(void)
{
    g_sfra.end_time_ms = HAL_GetTick();

    g_sfra.elapsed_time_ms = g_sfra.end_time_ms - g_sfra.start_time_ms;


	g_sfra.b_end_flag = true;
	g_sfra.state = SFRA_STATE_STOP;
}
// Initializations

void SFRA_Init(void)
{
    memset(&g_sfra, 0, sizeof(g_sfra));
    SFRA_GenerateFrequencyTable();
    // Initialize Control Variables
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
