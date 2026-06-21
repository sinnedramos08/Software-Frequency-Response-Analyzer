/*
 * sfra.h
 *
 *  Created on: Jun 14, 2026
 *      Author: denni
 *
 *  Revision: Plant Extraction FSM + Safety Architecture
 *
 *  Design principle:
 *    - Exactly ONE toggle is active at a time.
 *    - Compensator sweeps (ILOOP, VLOOP): purely offline, no live voltage.
 *      The old FSM states (INIT→SETTLING→MEASURING→CALCULATE→NEXT_FREQ→DONE)
 *      handle these. No ramp, no ADC watchdog needed.
 *    - Plant sweep (IPLANT): live converter operating. Extended FSM with ramp-up,
 *      DC op verification, de-inject fading between frequencies, safety watchdog,
 *      and a controlled ramp-down at the end.
 */

#ifndef SFRA_H_
#define SFRA_H_

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * SWEEP MODE SELECT — only one must be 1U at a time
 * ========================================================================= */
#define TOGGLE_SWEEP_ILOOP_FS_100KHZ        (0U)
#define TOGGLE_SWEEP_VLOOP_FS_6KHZ          (0U)
#define TOGGLE_SWEEP_IPLANT_FS_100KHZ       (1U)

/* =========================================================================
 * SWEEP-MODE-SPECIFIC PARAMETERS
 * =========================================================================
 *
 * SFRA_SETTLING_CYCLES:
 *   How many full injection cycles we wait before starting IQ accumulation.
 *   Purpose: let the converter's output filter and control loop reach
 *   sinusoidal steady-state at the new injection frequency before we measure.
 *   Rule of thumb: >= 5 cycles for compensator, >= 10-50 for plant (plant
 *   has more energy storage to settle). At 10 Hz and 100 kHz sampling,
 *   50 cycles = 500,000 ISR calls = 5 seconds of settling at that one point.
 *
 * SFRA_MEASUREMENT_CYCLES:
 *   How many full injection cycles we integrate the IQ accumulators over.
 *   More cycles = better cancellation of the 2ω ripple term (recall from
 *   the math: the IQ accumulator is a discrete integrator that averages out
 *   the cos(2ωt+φ) term. The more cycles, the closer it averages to zero).
 *   Practical minimum: 5 cycles. 50 cycles gives excellent SNR.
 * ========================================================================= */
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
    #define FLOAT_SFRA_FS_HZ                (100000.0f)
    #define SFRA_SETTLING_CYCLES            (50U)
    #define SFRA_MEASUREMENT_CYCLES         (50U)
    #define STRING_MESSAGE_SWEEP_NAME       "ILOOP 100KHZ"
    #define FLOAT_V_TO_ADC(voltage)         ((float)(voltage) * 4095.0f / 3.3f)
    #define SINE_INJECTED_AMPLITUDE_VOLTS   (0.75f)
    #define SINE_INJECTED_AMPLITUDE_ADC     FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)
    #define FREQ_POINTS_PER_DECADE          (50U)

#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
    #define FLOAT_SFRA_FS_HZ                (6000.0f)
    #define SFRA_SETTLING_CYCLES            (10U)
    #define SFRA_MEASUREMENT_CYCLES         (10U)
    #define STRING_MESSAGE_SWEEP_NAME       "VLOOP 6KHZ"
    #define FLOAT_V_TO_ADC(voltage)         ((float)(voltage) * 4095.0f / 3.3f)
    #define SINE_INJECTED_AMPLITUDE_VOLTS   (0.75f)
    #define SINE_INJECTED_AMPLITUDE_ADC     FLOAT_V_TO_ADC(SINE_INJECTED_AMPLITUDE_VOLTS)
    #define FREQ_POINTS_PER_DECADE          (50U)

#elif TOGGLE_SWEEP_IPLANT_FS_100KHZ
    #define FLOAT_SFRA_FS_HZ                (100000.0f)
    #define SFRA_SETTLING_CYCLES            (50U)
    #define SFRA_MEASUREMENT_CYCLES         (50U)
    #define STRING_MESSAGE_SWEEP_NAME       "IPLANT 100KHZ"
    #define FLOAT_V_TO_ADC(voltage)         ((float)(voltage) * 4095.0f / 3.3f)
    #define FREQ_POINTS_PER_DECADE          (10U)

    /* -----------------------------------------------------------------------
     * Plant sweep injection amplitude.
     *
     * We express amplitude as a FRACTION of the latched DC duty count, not
     * an absolute number. Why? Because the duty count for a given operating
     * point depends on Vin, Vout, and converter efficiency — it is not known
     * at compile time. By computing: sine_amplitude = PERCENT * dc_op_count
     * at runtime after the ramp, we ensure the perturbation is always a
     * small signal relative to the actual operating point.
     *
     * 3% is a safe starting point for a boost converter. If your Bode plot
     * shows noise at high frequencies, increase to 5%. If you see nonlinear
     * distortion (asymmetric waveform on the scope), reduce to 1-2%.
     * ----------------------------------------------------------------------- */
    #define FLOAT_SINE_INJECTED_AMPLITUDE_PERCENT   (0.03f)

    /* -----------------------------------------------------------------------
     * DC Operating Point — Ramp Parameters
     *
     * PWM_DUTY_INC_TICKS: duty incremented by this amount each ISR call
     *   during RAMP_UP. Smaller = slower, safer ramp.
     *   At 100kHz ISR rate, 1 tick/ISR = 100,000 ticks/sec ramp rate.
     *   HRTIM_PERIOD for 100kHz switching ≈ 54400 ticks (5.44GHz/100kHz).
     *   To ramp from 0 to ~50% duty in ~1 second:
     *     0.5 * 54400 ticks / (100000 ISR/sec) ≈ 0.27 ticks/ISR → round to 1.
     *   To ramp in ~0.1 seconds: ~3 ticks/ISR.
     *   Start with 1 for safety on first bring-up, then tune faster.
     *
     * PWM_DUTY_MAX_TICKS: hard ceiling on duty cycle. 90% of HRTIM_PERIOD.
     *   Never exceed this — the boost converter needs dead time for the
     *   inductor to reset, and some HRTIM configurations require minimum
     *   off-time for gate driver bootstrap circuits.
     * ----------------------------------------------------------------------- */
    #define HRTIM_PERIOD_TICKS              (54400U)    /* Must match your HRTIM init */
    #define PWM_DUTY_MAX_PERCENT            (0.90f)
    #define PWM_DUTY_MAX_TICKS              ((uint32_t)(PWM_DUTY_MAX_PERCENT * (float)HRTIM_PERIOD_TICKS))
    #define PWM_DUTY_INC_TICKS              (3U)        /* Tune: ticks increment per ISR */

    /* -----------------------------------------------------------------------
     * Output Voltage Target and Sensing
     *
     * VOUT_TARGET: desired output voltage in volts.
     *
     * Voltage divider: VOUT ──┤R1├──┤R2├── GND, ADC measures across R2.
     *   V_adc = VOUT * R2 / (R1 + R2) = VOUT * G4
     *   ADC_count = V_adc * GADC = V_adc * 4095/3.3
     *
     * So: VOUT_TO_ADC_COUNT(v) = v * G4 * GADC
     *
     * VOUT_TOL_ADC: acceptable ADC count window around target during
     *   verification. ±0.5V * G4 * GADC gives the ADC count tolerance.
     *   Tighter tolerance = more robust operating point but may be hard to
     *   hit if VOUT has ripple. Start at ±1V tolerance.
     *
     * VOUT_NOISE_FLOOR_ADC: below this value we consider VOUT disconnected
     *   or the load not present. Used in CHECK_SIGNALS state.
     *   1V * G4 * GADC is a safe floor (converter input at 24V, output open
     *   circuit will still have some voltage on the divider from stray paths).
     *
     * VOUT_SAFE_BAND_ADC: during injection, if VOUT ADC drifts more than
     *   this many counts from target, abort to FAULT state.
     *   ±3V is reasonable for a 60V system (±5% of target).
     * ----------------------------------------------------------------------- */
    #define VOUT_R1_VALUE                   (14316318.0f)   /* 14.1 MΩ */
    #define VOUT_R2_VALUE                   (115000.0f)     /* 115 kΩ */
    #define GADC_VOUT                       (1240.909091f)  /* 4095 / 3.3 */
    #define G4                              (VOUT_R2_VALUE / (VOUT_R1_VALUE + VOUT_R2_VALUE))
    #define VOUT_TO_ADC_COUNT(v)            ((uint32_t)((float)(v) * G4 * GADC_VOUT))
    #define VOUT_TARGET                     (60.0f)
    #define VOUT_TARGET_ADC                 VOUT_TO_ADC_COUNT(VOUT_TARGET)
    #define VOUT_TOL_ADC                    VOUT_TO_ADC_COUNT(1.0f)
    #define VOUT_NOISE_FLOOR_ADC            VOUT_TO_ADC_COUNT(5.0f)
    #define VOUT_SAFE_BAND_ADC              VOUT_TO_ADC_COUNT(3.0f)

    /* -----------------------------------------------------------------------
     * DC Op Verification
     *
     * N_VERIFY_SAMPLES: how many ISR cycles we hold duty constant and
     *   accumulate VOUT to confirm stability before latching dc_op_count.
     *   At 100kHz, 5000 samples = 50ms of observation. This is long enough
     *   to average out switching ripple and see if the output is truly stable.
     *   If the verify fails (VOUT outside tolerance after averaging), we go
     *   back to ramp-up and let the duty adjust slightly before trying again.
     * ----------------------------------------------------------------------- */
    #define N_VERIFY_SAMPLES                (5000U)

    /* -----------------------------------------------------------------------
     * De-inject Fade
     *
     * SFRA_FADE_CYCLES: number of injection sine cycles over which we taper
     *   the amplitude from its operating value to zero before changing
     *   frequency. We don't hard-cut because a sudden removal of the injection
     *   signal is itself a transient — the converter's control loop will
     *   react to it. A gradual fade over a few cycles is gentle enough that
     *   the loop responds without creating an output voltage excursion.
     *
     *   The amplitude is decremented linearly each ISR:
     *     amplitude_step = amplitude_current / (FADE_SAMPLES)
     *   where FADE_SAMPLES = SFRA_FADE_CYCLES * (Fs / current_freq)
     *
     *   5 cycles is adequate for most converters. Increase to 10 if you
     *   see glitches on the output voltage during frequency transitions.
     * ----------------------------------------------------------------------- */
    #define SFRA_FADE_CYCLES                (5U)

#endif  /* TOGGLE_SWEEP_IPLANT_FS_100KHZ */


/* =========================================================================
 * FREQUENCY SWEEP RANGE
 * ========================================================================= */
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
    #define FREQ_START_HZ                   (10U)
    #define FREQ_STOP_HZ                    (40000U)
#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
    #define FREQ_START_HZ                   (1U)
    #define FREQ_STOP_HZ                    (2500U)
#elif TOGGLE_SWEEP_IPLANT_FS_100KHZ
    #define FREQ_START_HZ                   (10U)
    #define FREQ_STOP_HZ                    (30000U)
#endif

/*
 * Buffer sizing: 4 decades × PPD points + 5 margin.
 * For IPLANT with PPD=10: 4*10+5 = 45 entries.
 * For ILOOP  with PPD=50: 4*50+5 = 205 entries.
 * Keep this as a macro so it auto-sizes with PPD.
 */
#define SFRA_FREQ_BUFFER_MAX_POINTS(points)     ((4U * (points)) + 5U)


/* =========================================================================
 * DDS (DIRECT DIGITAL SYNTHESIS) PARAMETERS
 *
 * The DDS engine generates the injection sine and cosine using a 32-bit
 * phase accumulator and a 13-bit LUT index (8192 samples per cycle).
 *
 * How it works:
 *   phase_inc = freq * 2^32 / Fs
 *   Each ISR: phase_acc += phase_inc  (wraps naturally at 2^32)
 *   LUT index = phase_acc >> (32 - 13) = top 13 bits of phase_acc
 *
 * The cosine is obtained by offsetting the LUT index by 2048
 * (= 8192/4 = quarter cycle = 90°):
 *   cosine_index = (lut_index + 2048) & 0x1FFF
 *
 * Why 2^32 accumulator? It gives fine phase resolution:
 *   Resolution = Fs / 2^32 ≈ 100000 / 4.295e9 ≈ 23 µHz per step.
 *   This means even at 10 Hz we have very accurate frequency synthesis.
 * ========================================================================= */
#define DDS_PHASE_BITS      32
#define DDS_LUT_BITS        13
#define DDS_LUT_SIZE        8192
#define DDS_LUT_SHIFT       (DDS_PHASE_BITS - DDS_LUT_BITS)  /* = 19 */
#define DDS_FULL_SCALE      (4294967296.0f)                   /* 2^32 */


/* =========================================================================
 * FAULT REASON CODES
 * ========================================================================= */
typedef enum
{
    SFRA_FAULT_NONE             = 0,
    SFRA_FAULT_VOUT_OVERVOLTAGE = 1,   /* VOUT > target + safe band */
    SFRA_FAULT_VOUT_UNDERVOLTAGE= 2,   /* VOUT < target - safe band */
    SFRA_FAULT_MAX_DUTY_REACHED = 3,   /* Ramp hit ceiling without reaching VOUT */
    SFRA_FAULT_NO_SIGNAL        = 4,   /* ADC sees nothing in CHECK_SIGNALS */
} sfra_fault_t;


/* =========================================================================
 * FSM STATE ENUM
 *
 * Compensator path (ILOOP / VLOOP):
 *   SFRA_STATE_INIT → SETTLING → MEASURING → CALCULATE → NEXT_FREQ
 *   → DONE → STOP
 *
 * Plant path (IPLANT) — full sequence:
 *   SFRA_STATE_PERIPH_INIT → CHECK_SIGNALS → RAMP_UP → VERIFY_DCOP
 *   → SFRA_INIT → DEJECT_WAIT → SETTLING → MEASURING → CALCULATE
 *   → NEXT_FREQ → RAMP_DOWN → DONE → STOP
 *   (any state → FAULT on safety trip)
 * ========================================================================= */
typedef enum
{
    /* ---- Shared entry point (compensator path starts here) ---- */
    SFRA_STATE_INIT         = 0,

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

} sfra_state_t;


/* =========================================================================
 * MAIN SFRA DATA STRUCTURE
 * ========================================================================= */
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
    float       input_I_acc;    /* Σ x[n] * sin_ref[n] */
    float       input_Q_acc;    /* Σ x[n] * cos_ref[n] */
    float       output_I_acc;   /* Σ y[n] * sin_ref[n] */
    float       output_Q_acc;   /* Σ y[n] * cos_ref[n] */

    /* ---- Timing counters ---- */
    uint32_t    settle_counter;
    uint32_t    settle_samples;     /* = SETTLING_CYCLES  * Fs / freq */
    uint32_t    measure_counter;
    uint32_t    measure_samples;    /* = MEASUREMENT_CYCLES * Fs / freq */

    /* ---- De-inject fade counter ---- */
    uint32_t    fade_counter;       /* ISR cycles spent in DEJECT_WAIT */
    uint32_t    fade_samples;       /* Total ISR cycles for one fade = FADE_CYCLES * Fs/freq */

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    /* ---- ADC Readings (written by ISR callbacks) ---- */
    uint32_t    u32_isense_ave_adc;     /* ISENSE ADC — injected at average-current point */
    uint32_t    u32_vout_adc;           /* VOUT ADC — read each ISR for monitoring */

    /* ---- PWM Duty Counts ---- */
    uint32_t    u32_duty_dc_op_count;   /* Current duty during ramp (counts) */
    uint32_t    u32_duty_dc_op_latch;   /* Latched duty at verified DC op point */
    uint32_t    u32_pwm_duty_count;     /* Live duty = dc_op_latch + sine perturbation */

    /* ---- DC Op Verification ---- */
    uint64_t    vout_verify_acc;        /* u64 to prevent overflow: 5000 * 4095 = 20.5M */
    uint32_t    u32_verify_counter;

    /* ---- ISENSE measurement accumulator ---- */
    /*
     * During MEASURING state, we accumulate ISENSE each ISR *before*
     * doing the IQ multiply. This gives us a per-sample average that
     * rejects high-frequency ADC noise without adding latency.
     * The IQ demodulation then acts as a narrow-band filter at the
     * injection frequency, further rejecting all other noise components.
     */
    float       isense_for_iq;          /* Per-ISR ISENSE value fed into IQ */

    /* ---- Fault ---- */
    sfra_fault_t    fault_reason;
#endif /* TOGGLE_SWEEP_IPLANT_FS_100KHZ */

    /* ---- Results ---- */
    float   gain_db[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];
    float   phase_deg[SFRA_FREQ_BUFFER_MAX_POINTS(FREQ_POINTS_PER_DECADE)];

    /* ---- Status Flags (written by FSM, read by main loop) ---- */
    bool    b_start_flag;           /* Set once at sweep start — triggers UART header */
    bool    b_end_flag;             /* Set once at sweep end */
    bool    b_result_ready_flag;    /* Set each time a frequency result is ready */
    bool    b_fault_flag;           /* Set on safety trip */

    /* ---- Elapsed time ---- */
    uint32_t    start_time_ms;
    uint32_t    end_time_ms;
    uint32_t    elapsed_time_ms;

    /* ---- Sine LUT ---- */
    float   sine_lut[DDS_LUT_SIZE];

} sfra_t;


/* =========================================================================
 * GLOBAL INSTANCE
 * ========================================================================= */
extern sfra_t g_sfra;


/* =========================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================= */
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

#endif /* SFRA_H_ */
