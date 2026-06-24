/*
 * plant_strategy.h
 *
 *  Created on: Jun 24, 2026
 *      Author: DRamos
 */

#ifndef STRATEGIES_INC_PLANT_STRATEGY_H_
#define STRATEGIES_INC_PLANT_STRATEGY_H_

#include <stdbool.h>
#include "sfra_strategies.h"
#include "sfra_engine.h"

// Macros
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
#define FLOAT_SFRA_FS_HZ                 	(100000.0f)
#define SFRA_SETTLING_CYCLES       			(50U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(50U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"IPLANT 100KHZ"
#define FLOAT_V_TO_ADC(voltage) 			((float)(voltage) * 4095.0f / 3.3f)
#define SINE_INJECTED_AMPLITUDE_VOLTS		(0.5f)	// Amplitude of Injected Signal in Volts (0.5V to 1V)
#define SINE_INJECTED_AMPLITUDE_ADC			FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)
#define DDS_AMPLITUDE_RAMP_STEP_ADC			(0.002f)

// ADC Value Checkers

#define VOUT_R1_VALUE				(float)(14316318.0f)		//14.1Meg		//Expected Value
#define VOUT_R2_VALUE				(float)(115000.0f)			//115K
#define KI_LOOP						(float)(1.0f / (G2 * GADC))
#define GADC						(float)(1240.909091f)		//(1240.909091f)					//(1105)				//1240.909091f	//ADC Gain = 4095/3.3
#define G2							(float)(0.066f) // (0.250f)				//Hall Sensor Gain = 250mV/A
#define G3							(float)(0.00298923138f)//(0.002609997838f)//(0.00309512f)		//VIN Gain
#define G3_RMS						(float)(G3 * 100.0f)
#define G4							(float)(VOUT_R2_VALUE / (VOUT_R1_VALUE + VOUT_R2_VALUE))
#define GADC						(float)(1240.909091f)
#define VOUT_MIN_VOLTS				(float)(5.0f)
#define VOUT_VOLTS_TO_ADC(voltage)	((float)(voltage)*GADC*G4)
#define ISENSE_OFFSET_ADC			(float)(1790.0f)	// 1.5V Offset -> 1860ADC
#define ISENSE_MIN_AMPS				(float)(0.5f)
#define ISENSE_AMPS_TO_ADC(amps)	(float)(amps*G2*GADC)
#endif

// For defining the frequencies in sweep
#define FREQ_POINTS_PER_DECADE				(10U) // Can only Vary from 10 to 50 Points Per Decade

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
#define FREQ_START_HZ						(10U)
#define	FREQ_STOP_HZ						(40000U)	// Considered Nyquist Frequency: Fsampling>2Fsampled
#endif
#define SFRA_FREQ_BUFFER_MAX_POINTS(points)       	((4U * points) + 5U)	// (4 Decades*Points per Decade) + Margin


// Structs and Enum
typedef enum
{
    PLANT_STATE_PERIPH_INIT = 0,
    PLANT_STATE_CHECK_SIGNALS,
    PLANT_STATE_RAMP_UP,
    PLANT_STATE_VERIFY_DCOP,
    PLANT_STATE_SFRA_INIT,
    PLANT_STATE_SWEEP,
    PLANT_STATE_RAMP_DOWN,
    PLANT_STATE_DONE,
    PLANT_STATE_FAULT

} plant_state_t;

typedef struct
{
    plant_state_t state;

    uint32_t 	u32_pwm_duty;
    float		f_pwm_duty;
    uint32_t 	u32_duty_dc_op_latch;
    uint32_t 	u32_vout_adc;
    uint32_t 	u32_isense_adc;
    uint32_t 	u32_verify_counter;
    uint64_t 	u64_vout_verify_acc;
    bool 		b_fault_flag;
    uint8_t		fault_reason;

    uint32_t 	u32_check_counter;

    bool		b_vout_valid;
    bool		b_isense_valid;

} plant_strategy_variables_t;

extern plant_strategy_variables_t g_plant_variables;
extern const sfra_strategy_t plant_strategy;

void PlantStrategy_ISR(void);
void Plant_Run(void);
#endif /* STRATEGIES_INC_PLANT_STRATEGY_H_ */
