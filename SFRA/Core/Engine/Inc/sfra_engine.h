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
#include "compensator_strategy.h"

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
void SFRA_GenerateFrequencyTable(void);


#endif /* ENGINE_INC_SFRA_ENGINE_H_ */
