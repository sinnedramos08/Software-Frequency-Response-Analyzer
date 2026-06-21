/*
 * sfra.c
 *
 *  Created on: Jun 14, 2026
 *      Author: denni
 */


#include "sfra.h"
#include "stdio.h"
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define PI_F    (3.14159265359f)

sfra_t g_sfra;

void LUT_Init(void)
{
	/* For Sine Look Up Table */
    for(int i = 0; i < DDS_LUT_SIZE; i++){
    	g_sfra.sine_lut[i] = sinf(2.0f * PI_F * ((float)i / (float)DDS_LUT_SIZE));
    }

#if 0
     for(uint32_t i = 0; i < 8192; i++)
	{
    	sine_lut[i] = sinf(2.0f * PI_F * i / 8192.0f);
    }
#endif

}

void SFRA_UpdateFrequency(float freq)
{
    g_sfra.current_freq = freq;

    g_sfra.phase_inc =(uint32_t)(freq *DDS_FULL_SCALE/ FLOAT_SFRA_FS_HZ);

    g_sfra.settle_samples = (uint32_t)(SFRA_SETTLING_CYCLES*FLOAT_SFRA_FS_HZ/freq);
    g_sfra.measure_samples = (uint32_t)(SFRA_MEASUREMENT_CYCLES * FLOAT_SFRA_FS_HZ/ freq);

    g_sfra.phase_acc = 0;
    g_sfra.index     = 0;

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    /* During DEJECT_WAIT, amplitude is decremented by amplitude_step
     *
     */
    g_sfra.fade_samples = (uint32_t)((float)SFRA_FADE_CYCLES * FLOAT_SFRA_FS_HZ / freq);
#endif
}

void SFRA_Calculate(void)
{
    float input_mag   = sqrtf(g_sfra.input_I_acc  * g_sfra.input_I_acc  + g_sfra.input_Q_acc  * g_sfra.input_Q_acc);
    float input_amp   = 2.0f * input_mag / (float)g_sfra.measure_samples;
    float input_phase = atan2f(g_sfra.input_Q_acc, g_sfra.input_I_acc);


    float output_mag   = sqrtf(g_sfra.output_I_acc * g_sfra.output_I_acc + g_sfra.output_Q_acc * g_sfra.output_Q_acc);
    float output_amp   = 2.0f * output_mag / (float)g_sfra.measure_samples;
    float output_phase = atan2f(g_sfra.output_Q_acc, g_sfra.output_I_acc);

	float gain = output_amp / input_amp;
	float gain_db = 20.0f * log10f(gain);
	float phase_deg = (output_phase - input_phase)*180.0f / PI_F;


	g_sfra.gain_db[g_sfra.freq_index] = gain_db;
	g_sfra.phase_deg[g_sfra.freq_index] = phase_deg;

	g_sfra.b_result_ready_flag=true;


}

void SFRA_GenerateFrequencyTable(void){
    // Get number of Decades - ~4 Decades from 10Hz to 40kHz
    float decades= log10f((float)(FREQ_STOP_HZ)/(float)(FREQ_START_HZ)); // Number of decades = log(40000/10)

    // Get number of frequency based on number of decades and points per decade
    g_sfra.num_freqs =(uint16_t)(decades * FREQ_POINTS_PER_DECADE) + 1U; // (3.6*10)+1=37 frequencies

    // Using Logarithmic scale, we need to have common ratio (not common difference) between frequency points
    // Since we need X data points per decade, formula: ratio = 10^(1/x)
    float ratio = powf(10.0f,1.0f/(float)(FREQ_POINTS_PER_DECADE));

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

void SFRA_Init(void)
{
    memset(&g_sfra, 0, sizeof(g_sfra));
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    g_sfra.state = SFRA_STATE_PERIPH_INIT;
#else
    SFRA_GenerateFrequencyTable();
    g_sfra.amplitude = SINE_INJECTED_AMPLITUDE_ADC;
    g_sfra.state = SFRA_STATE_INIT;
#endif

    g_sfra.b_result_ready_flag = false;
    g_sfra.b_fault_flag        = false;
}



#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
void SFRA_Plant_TriggerFault(sfra_fault_t reason)
{
    g_sfra.amplitude            = 0.0f;
    g_sfra.amplitude_target     = 0.0f;
    g_sfra.amplitude_step       = 0.0f;
    g_sfra.u32_duty_dc_op_latch = 0U;
    g_sfra.u32_pwm_duty_count   = 0U;
    g_sfra.fault_reason         = reason;
    g_sfra.b_fault_flag         = true;
    g_sfra.state                = SFRA_STATE_FAULT;
}

void SFRA_Plant_CheckSafetyWatchdog(void)
{
    if(g_sfra.state != SFRA_STATE_SETTLING &&
       g_sfra.state != SFRA_STATE_MEASURING)
    {
        return;  /* Not in an injection state — watchdog inactive */
    }

    int32_t vout_error = (int32_t)g_sfra.u32_vout_adc - (int32_t)VOUT_TARGET_ADC;
    if(vout_error < 0) vout_error = -vout_error;  /* abs() */

    if((uint32_t)vout_error > VOUT_SAFE_BAND_ADC)
    {
        sfra_fault_t reason = (g_sfra.u32_vout_adc > VOUT_TARGET_ADC) ?
                               SFRA_FAULT_VOUT_OVERVOLTAGE :
                               SFRA_FAULT_VOUT_UNDERVOLTAGE;
        SFRA_Plant_TriggerFault(reason);
    }
}
#endif




void SFRA_Run(void)
{
    switch(g_sfra.state)
    {
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ

#elif TOGGLE_SWEEP_ILOOP_FS_100KHZ || TOGGLE_SWEEP_VLOOP_FS_6KHZ
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
#endif
}



