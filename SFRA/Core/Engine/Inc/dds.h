/*
 * dds.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef ENGINE_INC_DDS_H_
#define ENGINE_INC_DDS_H_

// Includes
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Macros


// For Bit Manipulation in Sine Wave Generation
#define DDS_PHASE_BITS    					32
#define DDS_LUT_BITS      					13
#define DDS_LUT_SIZE      					8192
#define DDS_LUT_SHIFT     					(DDS_PHASE_BITS - DDS_LUT_BITS)
#define DDS_FULL_SCALE						(4294967296.0f)	// 2^32

// Structs
typedef struct{

    // Direct digital synthesis

    uint32_t u32_phase_inc;
    uint32_t u32_phase_acc;
    uint32_t u32_LUT_index;

    float f_sine_amplitude;	// 1240.9090f
    float f_sine_amplitude_target;

    float f_sine_out;		// Output sine: Amplitude*sinf(theta)
    float f_cosine_out;	// for cosine

    // For Reference Signal with Unity Amplitude
    float f_sine_ref;
    float f_cosine_ref;

    /* Look Up Table Sine */
    float	f_sine_lut[DDS_LUT_SIZE];


} dds_t;
extern dds_t g_dds;

// Function Prototypes
void DDS_Init(void);
void DDS_Sine_LUT_Init(void);
void DDS_UpdateFrequency(float freq);
void DDS_Update(void);
void DDS_AmplitudeRamp(void);
bool DDS_IsAmplitudeReached(void);
#endif /* ENGINE_INC_DDS_H_ */
