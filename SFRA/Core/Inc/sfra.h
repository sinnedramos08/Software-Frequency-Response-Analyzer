/*
 * sfra.h
 *
 *  Created on: Jun 14, 2026
 *      Author: denni
 */

#ifndef SFRA_H_
#define SFRA_H_

#include <stdint.h>
#include <stdbool.h>

/* Sweep Mode Select - only one must be 1U at a time */
#define TOGGLE_SWEEP_ILOOP_FS_100KHZ		(0U)
#define TOGGLE_SWEEP_VLOOP_FS_6KHZ			(0U)
#define TOGGLE_SWEEP_IPLANT_FS_100KHZ		(1U)

/* ================================================
 * Sweep Mode Parameters
 * ================================================
 * FLOAT_SFRA_FS_HZ:  Sampling Frequency
 *
 * SFRA_SETTLING_CYCLES:
 * Number of cycles we wait before starting IQ accumulation.
 * Settling cycles are needed to ensure that the output of filter and compensator
 * reaches steady-state operation
 * More cycles = better settling but slower execution at lower frequencies
 *
 * SFRA_MEASUREMENT_CYCLES:
 * Number of full injection cycles we need to integrate or add to IQ accumulator
 * for proper sampling and high frequency term cancellation.
 * More cycles = better cancellation/filtering of high frequency term but
 * slower execution at lower frequencies
 *
 *
 * SINE_INJECTED_AMPLITUDE_VOLTS - for ILOOP/VLOOP sweep, amplitude of sine wave
 * we inject in volts
 *
 * SINE_INJECTED_AMPLITUDE_ADC - for ILOOP/VLOOP sweep, amplitude of sine wave
 * we inject in ADC
 *
 * FREQ_START_HZ and FREQ_STOP_HZ - start and stop frequencies in sweep (Hz)
 *
 * FREQ_POINTS_PER_DECADE - number of points per decade
 *
 * For IPLANT Injection:
 *
 * FLOAT_SINE_INJECTED_AMPLITUDE_PERCENT:
 * How many percent from the DC OP duty the amplitude of injected sine wave
 * perturbation is
 *
 * FLOAT_SINE_INJECTED_AMPLITUDE_TICKS:
 * How many ticks the amplitude of the injected perturbation is.
 * Note: 544000 - max count/ticks of the HRTIM in 100kHz
 *
 * SFRA_FREQ_BUFFER_MAX_POINTS:
 * Worst case buffer size for a certain number of points per decade.
 * Purpose: For storing frequency data points
 *
 =================================================*/


#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
#define FLOAT_SFRA_FS_HZ                 	(100000.0f)
#define SFRA_SETTLING_CYCLES       			(50U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(50U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"ILOOP 100KHZ"
#define FLOAT_V_TO_ADC(voltage) 			((float)(voltage) * 4095.0f / 3.3f)
#define SINE_INJECTED_AMPLITUDE_VOLTS		(0.75f)	// Amplitude of Injected Signal in Volts (0.5V to 1V)
#define SINE_INJECTED_AMPLITUDE_ADC			FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)

#define FREQ_START_HZ						(10U)
#define	FREQ_STOP_HZ						(40000U)	// Considered Nyquist Frequency: Fsampling>2Fsampled
#define FREQ_POINTS_PER_DECADE				(50U) // Can only Vary from 10 to 50 Points Per Decade

#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
#define FLOAT_SFRA_FS_HZ                 	(6000.0f)
#define SFRA_SETTLING_CYCLES       			(10U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    			(10U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME			"VLOOP 6KHZ"
#define FLOAT_V_TO_ADC(voltage) 			((float)(voltage) * 4095.0f / 3.3f)
#define SINE_INJECTED_AMPLITUDE_VOLTS		(0.75f)	// Amplitude of Injected Signal in Volts (0.5V to 1V)
#define SINE_INJECTED_AMPLITUDE_ADC			FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)

#define FREQ_START_HZ						(1U)
#define	FREQ_STOP_HZ						(2500U)		// Considered Nyquist Frequency: Fsampling>2Fsampled
#define FREQ_POINTS_PER_DECADE				(50U) // Can only Vary from 10 to 50 Points Per Decade

#elif TOGGLE_SWEEP_IPLANT_FS_100KHZ
#define FLOAT_SFRA_FS_HZ                 			(100000.0f)
#define SFRA_SETTLING_CYCLES       					(50U)	// Number of cycles to be waited before it settles
#define SFRA_MEASUREMENT_CYCLES    					(50U)	// Number of Cycles to be calculated
#define STRING_MESSAGE_SWEEP_NAME					"IPLANT 100KHZ"

#define FLOAT_SINE_INJECTED_AMPLITUDE_PERCENT		(0.01f) // Percent of injected sine wave amplitude from DC OP
#define FLOAT_SINE_INJECTED_AMPLITUDE_TICKS			(300.0f)

#define FREQ_START_HZ								(10U)
#define	FREQ_STOP_HZ								(30000U)	// Considered Nyquist Frequency: Fsampling>2Fsampled
#define FREQ_POINTS_PER_DECADE						(10U) // Can only Vary from 10 to 50 Points Per Decade
#endif

#define SFRA_FREQ_BUFFER_MAX_POINTS(points)       	((4U * points) + 5U)	// (4 Decades*Points per Decade) + Margin

/* ================================================
 * DC Operating Points
 * ================================================
 * HRTIM_PERIOD_TICKS:
 * HRTIM Up counter max ticks. For 100kHz, 54400 is used.
 * Computation: 170MHz System Clock * 32 Prescaler = 5.44GHz HRTIM Clock
 * 5.44 GHz HRTIM Clock/100kHz Sampling Freq = 54400 Ticks
 *
 * PWM_DUTY_MAX_PERCENT - Percentage of maximum allowable duty
 *
 * PWM_DUTY_MAX_TICKS - PWM_DUTY_MAX_PERCENT*HRTIM_PERIOD_TICKS
 *
 * PWM_DUTY_INC_TICKS - Increment
 *
 */

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
#define HRTIM_PERIOD_TICKS              (54400U)    /* Must match your HRTIM init */
#define PWM_DUTY_MAX_PERCENT            (0.90f)
#define PWM_DUTY_MAX_TICKS              ((uint32_t)(PWM_DUTY_MAX_PERCENT * (float)HRTIM_PERIOD_TICKS))
#define PWM_DUTY_INC_TICKS              (1U)        /* Tune: ticks increment per ISR */
#endif

/* ================================================
 * Output Voltage Sensing and Target
 * ================================================
 * VOUT_RX_VALUE - Based on hardware divider for output sensing
 *
 * G4 - Gain for attenuating the output signal to a 0 to 3.3V signal
 *
 * GADC - Conversion of output attenuated signal to ADC values
 *
 * VOUT_TARGET_VOLTS - target output voltage in volts
 *
 * VOUT_TARGET_ADC - target output voltage in ADC
 *
 * VOUT_TOL_ADC - acceptable ripple on the output in ADC
 *
 * VOUT_NOISE_FLOOR_ADC:
 * Ensure that live voltage input and output is enabled. During Check Signals state,
 * the output is checked to ensure that there is a live operation and ready to be tested
 *
 * VOUT_SAFE_BAND_ADC:
 * During injection, if VOUT drifts more that this many voltage/ADC, FAULT occurs.
 * Signifies the OVP or UVP
 *
 * N_VERIFY_SAMPLES:
 * How many ISR Cycles we hold duty constant and accumulate VOUT to confirm that output
 * is stable before assigning the DC OP duty. At 100kHz, 5000 samples = 50ms of observation.
 * this is long enough to average out switching ripple and see if the output is truly stable.
 *
 * SFRA_FADE_CYCLES:
 * Number of injection sine cycles we inject the perturbation from starting amplitude to 0
 * before we change the frequency. Note that to ensure safety transition to another frequency
 * we should consider the hardware stability and remove all the transient to ensure
 * steady-state operation.
 *   The amplitude is decremented linearly each ISR:
 *     amplitude_step = amplitude_current / (FADE_SAMPLES)
 *   where FADE_SAMPLES = SFRA_FADE_CYCLES * (Fs / current_freq)
 *
 *   5 cycles is adequate for most converters. Increase to 10 if you
 *   see glitches on the output voltage during frequency transitions.
 */

#define VOUT_R1_VALUE                   (14316318.0f)   /* 14.1 MΩ */
#define VOUT_R2_VALUE                   (115000.0f)     /* 115 kΩ */
#define GADC_VOUT                       (1240.909091f)  /* 4095 / 3.3 */
#define G4                              (VOUT_R2_VALUE / (VOUT_R1_VALUE + VOUT_R2_VALUE))
#define VOUT_TO_ADC_COUNT(v)            ((uint32_t)((float)(v) * G4 * GADC_VOUT))
#define VOUT_TARGET_VOLTS               (60.0f)
#define VOUT_TARGET_ADC                 VOUT_TO_ADC_COUNT(VOUT_TARGET)
#define VOUT_TOL_ADC                    VOUT_TO_ADC_COUNT(1.0f)
#define VOUT_MIN_ADC		            VOUT_TO_ADC_COUNT(5.0f)
#define VOUT_SAFE_BAND_ADC              VOUT_TO_ADC_COUNT(3.0f)
#define N_VERIFY_SAMPLES                (5000U)
#define SFRA_FADE_CYCLES                (5U)



/* ================================================
 * Direct Digital Synthesis
 * ================================================
 */
#define DDS_PHASE_BITS    					32
#define DDS_LUT_BITS      					13
#define DDS_LUT_SIZE      					8192
#define DDS_LUT_SHIFT     					(DDS_PHASE_BITS - DDS_LUT_BITS)
#define DDS_FULL_SCALE						(4294967296.0f)	// 2^32


/* =========================================================================
 * FAULT REASON CODES
 * =========================================================================
 */
typedef enum
{
    SFRA_FAULT_NONE             = 0,
    SFRA_FAULT_VOUT_OVERVOLTAGE = 1,   /* VOUT > target + safe band */
    SFRA_FAULT_VOUT_UNDERVOLTAGE= 2,   /* VOUT < target - safe band */
    SFRA_FAULT_MAX_DUTY_REACHED = 3,   /* Ramp hit ceiling without reaching VOUT */
    SFRA_FAULT_NO_SIGNAL        = 4,   /* ADC sees nothing in CHECK_SIGNALS */
} sfra_fault_t;



/* =========================================================================
 * FAULT REASON CODES
 * =========================================================================
 * For Compensator Sweep:
 * INIT -> SETTLING -> MEASURING -> CALCULATE -> NEXT_FREQ -> DONE -> STOP
 *
 * For Plant Sweep:
 * SFRA_STATE_PERIPH_INIT -> CHECK_SIGNALS -> RAMP_UP -> VERIFY_DCOP
 * -> SFRA_INIT -> DEJECT_WAIT -> SETTLING -> MEASURING -> CALCULATE
 * -> NEXT_FREQ -> RAMP_DOWN -> DONE -> STOP
 * (any state → FAULT on safety trip)
 */
typedef enum
{
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    /* ---- Plant-only states: live voltage bring-up ---- */
    SFRA_STATE_PERIPH_INIT,         /* Enable relay GPIO, start ADC */
    SFRA_STATE_CHECK_SIGNALS,       /* Verify VOUT + ISENSE ADC are non-zero */
    SFRA_STATE_RAMP_UP,             /* Slowly increment duty until VOUT=target */
    SFRA_STATE_VERIFY_DCOP,         /* Hold duty, accumulate VOUT, confirm stable */
    SFRA_STATE_SFRA_INIT,           /* Latch dc_op, set amplitude, arm DDS */

    /* ---- Shared injection states (both paths use these) ---- */
    SFRA_STATE_DEJECT_WAIT,         /* Fade amplitude→0, reset DDS, update freq */
    SFRA_STATE_SETTLING,            /* Injection active, wait for steady-state */
    SFRA_STATE_MEASURING,           /* Accumulate IQ data */
    SFRA_STATE_CALCULATE,           /* Compute gain_dB and phase_deg */
    SFRA_STATE_NEXT_FREQ,           /* Advance freq_index, branch or finish */

    /* ---- Plant-only states: live voltage tear-down ---- */
    SFRA_STATE_RAMP_DOWN,           /* Fade amplitude→0, then ramp duty→0 */

    /* ---- Terminal states ---- */
    SFRA_STATE_DONE,
    SFRA_STATE_STOP,
    SFRA_STATE_FAULT,               /* Safety trip — duty→0, HRTIM off */

#elif TOGGLE_SWEEP_ILOOP_FS_100KHZ || TOGGLE_SWEEP_VLOOP_FS_6KHZ
    SFRA_STATE_INIT = 0,
    SFRA_STATE_SETTLING,
    SFRA_STATE_MEASURING,
    SFRA_STATE_CALCULATE,
    SFRA_STATE_NEXT_FREQ,
    SFRA_STATE_DONE,
	SFRA_STATE_STOP
#endif
} sfra_state_t;

typedef struct
{
	/* ---- FSM ---- */
    sfra_state_t state;

    /* ---- Frequency sweep ---- */
    uint16_t    freq_index;
    uint16_t    num_freqs;
    float       current_freq;
    float       freq_table[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];

    /* ---- DDS Signal Generator ---- */
    uint32_t    phase_inc;      /* Phase increment per ISR = freq * 2^32 / Fs */
    uint32_t    phase_acc;      /* 32-bit accumulator, wraps at 2^32 */
    uint32_t    index;          /* Current LUT index (top 13 bits of phase_acc) */

    float       amplitude;          /* Current injection amplitude (ramps during fade) */
    float       amplitude_target;   /* What amplitude is converging toward */
    float       amplitude_step;     /* Per-ISR decrement during fade */
    float       sine_out;           /* amplitude * sin(theta) — injected signal */
    float       cosine_out;         /* amplitude * cos(theta) — debug/testing only */

    /* ---- Reference signals (unity amplitude) ---- */
    float       sine_ref;       /* sin(theta) at current phase */
    float       cosine_ref;     /* cos(theta) at current phase */

    /* ---- IQ Accumulators ---- */
    float       input_I_acc;
    float       input_Q_acc;
    float       output_I_acc;
    float       output_Q_acc;

    /* ---- Timing counters ---- */
    uint32_t    settle_counter;
    uint32_t    settle_samples;     /* = SETTLING_CYCLES  * Fs / freq */
    uint32_t    measure_counter;
    uint32_t    measure_samples;    /* = MEASUREMENT_CYCLES * Fs / freq */

    /* ---- De-inject fade counter ---- */
    uint32_t    fade_counter;       /* ISR cycles spent in DEJECT_WAIT */
    uint32_t    fade_samples;       /* Total ISR cycles for one fade = FADE_CYCLES * Fs/freq */

    /* ---- ADC Readings (written by ISR callbacks) ---- */
    uint32_t    u32_isense_ave_adc;     /* ISENSE ADC — injected at average-current point */
    uint32_t    u32_voutsense_adc;           /* VOUT ADC — read each ISR for monitoring */

    /* ---- PWM Duty Counts ---- */
    uint32_t    u32_duty_dc_op_count;   /* Current duty during ramp (counts) */
    uint32_t    u32_duty_dc_op_latch;   /* Latched duty at verified DC op point */
    uint32_t    u32_pwm_duty_count;     /* Live duty = dc_op_latch + sine perturbation */

    /* ---- DC Op Verification ---- */
    uint64_t    vout_verify_acc;        /* u64 to prevent overflow: 5000 * 4095 = 20.5M */
    uint32_t    u32_verify_counter;

    /* ---- ISENSE measurement accumulator ---- */
    float       isense_for_iq;          /* Per-ISR ISENSE value fed into IQ */



    /* ---- Results ---- */
    float 	debug_amp;
	float 	debug_phase;
    float   gain_db[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];
    float   phase_deg[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];


    /* ---- Status Flags (written by FSM, read by main loop) ---- */
    bool    b_start_flag;           /* Set once at sweep start — triggers UART header */
    bool    b_end_flag;             /* Set once at sweep end */
    bool    b_result_ready_flag;    /* Set each time a frequency result is ready */
    bool    b_fault_flag;           /* Set on safety trip */

    /* ---- Fault ---- */
    sfra_fault_t    fault_reason;

    /* ---- Elapsed time ---- */
    uint32_t    start_time_ms;
    uint32_t    end_time_ms;
    uint32_t    elapsed_time_ms;

    /* ---- Sine LUT ---- */
    float   sine_lut[DDS_LUT_SIZE];


} sfra_t;

extern sfra_t g_sfra;

void SFRA_Init(void);
void SFRA_Run(void);
void SFRA_Calculate(void);
void SFRA_UpdateFrequency(float freq);
void SFRA_GenerateFrequencyTable(void);
void LUT_Init(void);

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
void SFRA_Plant_TriggerFault(sfra_fault_t reason);
void SFRA_Plant_CheckSafetyWatchdog(void);
#endif

#endif
