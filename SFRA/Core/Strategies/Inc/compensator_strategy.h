/*
 * compensator_strategy.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef STRATEGIES_INC_COMPENSATOR_STRATEGY_H_
#define STRATEGIES_INC_COMPENSATOR_STRATEGY_H_

#include "sfra_engine.h"
#include "compensator.h"
#include "sfra_strategies.h"


// MACROS
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
#define FLOAT_SFRA_FS_HZ                 	(100000.0f)
#define SFRA_SETTLING_CYCLES       			(50U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(50U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"ILOOP 100KHZ"
#define FLOAT_V_TO_ADC(voltage) 			((float)(voltage) * 4095.0f / 3.3f)
#define SINE_INJECTED_AMPLITUDE_VOLTS		(1.0f)	// Amplitude of Injected Signal in Volts (0.5V to 1V)
#define SINE_INJECTED_AMPLITUDE_ADC			FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)
#define DDS_AMPLITUDE_RAMP_STEP_ADC			(0.01f)

#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
#define FLOAT_SFRA_FS_HZ                 	(6000.0f)
#define SFRA_SETTLING_CYCLES       			(10U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(10U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"VLOOP 6KHZ"
#define FLOAT_V_TO_ADC(voltage) 			((float)(voltage) * 4095.0f / 3.3f)
#define SINE_INJECTED_AMPLITUDE_VOLTS		(1.0f)	// Amplitude of Injected Signal in Volts (0.5V to 1V)
#define SINE_INJECTED_AMPLITUDE_ADC			FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)
#define DDS_AMPLITUDE_RAMP_STEP_ADC			(1.0f)
#endif

// For defining the frequencies in sweep
#define FREQ_POINTS_PER_DECADE				(10U) // Can only Vary from 10 to 50 Points Per Decade

#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
#define FREQ_START_HZ						(10U)
#define	FREQ_STOP_HZ						(40000U)	// Considered Nyquist Frequency: Fsampling>2Fsampled
#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
#define FREQ_START_HZ						(1U)
#define	FREQ_STOP_HZ						(2500U)		// Considered Nyquist Frequency: Fsampling>2Fsampled
#endif
#define SFRA_FREQ_BUFFER_MAX_POINTS(points)       	((4U * points) + 5U)	// (4 Decades*Points per Decade) + Margin


typedef struct
{
    compensator_2p2z_t *p_comp;


} compensator_strategy_loop_t;

extern const sfra_strategy_t compensator_strategy;


// Function Prototypes
void CompensatorStrategy_Init(void);
void CompensatorStrategy_ISR(void);
#endif /* STRATEGIES_INC_COMPENSATOR_STRATEGY_H_ */
