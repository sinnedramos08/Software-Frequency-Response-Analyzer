/*
 * sfra.h
 *
 *  Created on: Jun 14, 2026
 *      Author: denni
 */

#ifndef SFRA_H_
#define SFRA_H_

#include <stdint.h>

#define SFRA_FS_HZ                 	(100000.0f)

#define SFRA_SETTLING_CYCLES       	(500U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    	(500U)	// Number of Cycles to be calculated

#define SFRA_MAX_FREQ_POINTS       	(50U)	// For higher number of frequency points per decade

#define DDS_PHASE_BITS    32
#define DDS_LUT_BITS      13
#define DDS_LUT_SIZE      8192
#define DDS_LUT_SHIFT     (DDS_PHASE_BITS - DDS_LUT_BITS)


typedef enum
{
    SFRA_STATE_INIT = 0,
    SFRA_STATE_SETTLING,
    SFRA_STATE_MEASURING,
    SFRA_STATE_CALCULATE,
    SFRA_STATE_NEXT_FREQ,
    SFRA_STATE_DONE

} sfra_state_t;

typedef struct
{
    /* FSM */

    sfra_state_t state; // States for FSM
    /* Sweep */
    uint16_t freq_index; 	// Frequency table index
    uint16_t num_freqs;		// Number of frequencies to sweep. When freq_index>=num_freqs, then done sweep
    float current_freq;		// Current frequency tested
    float freq_table[SFRA_MAX_FREQ_POINTS]; // Array for storing frequencies to test

    /* Signal Generator */

    uint32_t phase_inc;
    uint32_t phase_acc;
    uint32_t index;

    float amplitude;	// 1240.9090f
    float sine_out;		// Output sine: Amplitude*sinf(theta)
    float cosine_out;	// for cosine

    /* For Reference Signal with Unity Amplitude */
    float sine_ref;
    float cosine_ref;

    /* Measurement */
    float input_I_acc;
    float input_Q_acc;

    float output_I_acc;
    float output_Q_acc;

    uint32_t settle_counter;
    uint32_t settle_samples;		// Samples = #cycles * (Fsampling/current_freq)

    uint32_t measure_counter;
    uint32_t measure_samples;		// Samples = #cycles * (Fsampling/current_freq)

    /* Results */

    float debug_amp;
	float debug_phase;

    float gain_db[SFRA_MAX_FREQ_POINTS];
    float phase_deg[SFRA_MAX_FREQ_POINTS];

    /* Flag Reporting */
    uint8_t result_ready;

    /* Look Up Table Sine */
    float	sine_lut[DDS_LUT_SIZE];


} sfra_t;

extern sfra_t g_sfra;

void SFRA_Init(void);
void SFRA_Run(void);
void SFRA_Run_Meas(void);
void SFRA_Calculate(void);
void SFRA_UpdateFrequency(float freq);
void LUT_Init(void);
#endif
