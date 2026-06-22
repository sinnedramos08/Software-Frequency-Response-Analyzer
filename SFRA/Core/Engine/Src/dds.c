/*
 * dds.c
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */
#include <stdio.h>
#include <math.h>
#include "dds.h"
#include "sfra_engine.h"

#define PI_F    (3.14159265359f)

// Struct Instance
dds_t g_dds;

void DDS_Init(void)
{
	g_dds.f_sine_amplitude = SINE_INJECTED_AMPLITUDE_ADC;	// For 1V Amplitude Signal in Oscilloscope
	DDS_Sine_LUT_Init();

}
void DDS_Sine_LUT_Init(void)
{
	// For creating sine
    for(int i = 0; i < DDS_LUT_SIZE; i++){
    	g_dds.f_sine_lut[i] = sinf(2.0f * PI_F * ((float)i / (float)DDS_LUT_SIZE));
    }

}

void DDS_UpdateFrequency(float freq)
{
    g_sfra.current_freq = freq;

    g_sfra.settle_samples = (uint32_t)(SFRA_SETTLING_CYCLES*FLOAT_SFRA_FS_HZ/freq);
    g_sfra.measure_samples = (uint32_t)(SFRA_MEASUREMENT_CYCLES * FLOAT_SFRA_FS_HZ/ freq);

    g_dds.u32_phase_inc =(uint32_t)(freq *DDS_FULL_SCALE/ FLOAT_SFRA_FS_HZ);
    g_dds.u32_phase_acc = 0;
    g_dds.u32_LUT_index = 0;
}

void DDS_Update(void)
{
	// Create Sine Wave
	// LUT index (top 13 bits)
	g_dds.u32_LUT_index= g_dds.u32_phase_acc >> DDS_LUT_SHIFT;
	g_dds.u32_phase_acc += g_dds.u32_phase_inc;

	// Create the Injected Signal Sine Wave
	g_dds.f_sine_out = g_dds.f_sine_amplitude * g_dds.f_sine_lut[g_dds.u32_LUT_index];						// Generated sine, injected to 2p2z
	g_dds.f_cosine_out = g_dds.f_sine_amplitude * g_dds.f_sine_lut[(g_dds.u32_LUT_index+ 2048) & 0x1FFF];	// Generated for testing only, not to be processed

	// Create a reference signal sine and cosine
	g_dds.f_sine_ref = g_dds.f_sine_lut[g_dds.u32_LUT_index];
	g_dds.f_cosine_ref = g_dds.f_sine_lut[(g_dds.u32_LUT_index + 2048) & 0x1FFF];
}
