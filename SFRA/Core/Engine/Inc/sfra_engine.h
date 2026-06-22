/*
 * sfra_engine.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef ENGINE_INC_SFRA_ENGINE_H_
#define ENGINE_INC_SFRA_ENGINE_H_

#include <stdint.h>
#include <stdbool.h>

#define TOGGLE_SWEEP_ILOOP_FS_100KHZ		(1U)
#define TOGGLE_SWEEP_VLOOP_FS_6KHZ			(0U)

#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
#define FLOAT_SFRA_FS_HZ                 	(100000.0f)
#define SFRA_SETTLING_CYCLES       			(50U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(50U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"ILOOP 100KHZ"

#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
#define FLOAT_SFRA_FS_HZ                 	(6000.0f)
#define SFRA_SETTLING_CYCLES       			(10U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(10U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"VLOOP 6KHZ"

#endif


#define FLOAT_V_TO_ADC(voltage) 			((float)(voltage) * 4095.0f / 3.3f)
#define SINE_INJECTED_AMPLITUDE_VOLTS		(0.75f)	// Amplitude of Injected Signal in Volts (0.5V to 1V)
#define SINE_INJECTED_AMPLITUDE_ADC			FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)


// For defining the frequencies in sweep
#define FREQ_POINTS_PER_DECADE				(50U) // Can only Vary from 10 to 50 Points Per Decade
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
#define FREQ_START_HZ						(10U)
#define	FREQ_STOP_HZ						(40000U)	// Considered Nyquist Frequency: Fsampling>2Fsampled
#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
#define FREQ_START_HZ						(1U)
#define	FREQ_STOP_HZ						(2500U)		// Considered Nyquist Frequency: Fsampling>2Fsampled
#endif
#define SFRA_FREQ_BUFFER_MAX_POINTS(points)       	((4U * points) + 5U)	// (4 Decades*Points per Decade) + Margin




// Structs and Enum
typedef enum
{
    SFRA_STATE_INIT = 0,
    SFRA_STATE_SETTLING,
    SFRA_STATE_MEASURING,
    SFRA_STATE_CALCULATE,
    SFRA_STATE_NEXT_FREQ,
    SFRA_STATE_DONE,
	SFRA_STATE_STOP

} sfra_state_t;


typedef struct
{
    /* FSM */

    sfra_state_t state; // States for FSM
    /* Sweep */
    uint16_t freq_index; 	// Frequency table index
    uint16_t num_freqs;		// Number of frequencies to sweep. When freq_index>=num_freqs, then done sweep
    float current_freq;		// Current frequency tested
    float freq_table[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)]; // Array for storing frequencies to test



    uint32_t settle_counter;
    uint32_t settle_samples;		// Samples = #cycles * (Fsampling/current_freq)

    uint32_t measure_counter;
    uint32_t measure_samples;		// Samples = #cycles * (Fsampling/current_freq)

    /* Results */

    float debug_amp;
	float debug_phase;

    float gain_db[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];
    float phase_deg[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];

    /* Flag Reporting */
    bool b_start_flag;
	bool b_end_flag;
    bool b_result_ready_flag;

    /* Total Time Elapsed Measurement */
    uint32_t start_time_ms;
    uint32_t end_time_ms;
    uint32_t elapsed_time_ms;

} sfra_t;

extern sfra_t g_sfra;



// Function Prototypes
void SFRA_Init(void);
void SFRA_Run(void);
void SFRA_Calculate(void);
void SFRA_GenerateFrequencyTable(void);


#endif /* ENGINE_INC_SFRA_ENGINE_H_ */
