/*
 * sfra.c
 *
 *  Created on: Jun 14, 2026
 *      Author: denni
 *
 *  Revision: Plant Extraction FSM + Safety Architecture
 *
 * =============================================================================
 * ARCHITECTURE OVERVIEW
 * =============================================================================
 *
 * Two separate FSM paths, compile-time selected:
 *
 * [COMPENSATOR PATH] (ILOOP / VLOOP)
 *   Runs entirely offline — no live voltage, no ADC-sensed plant signals.
 *   The "plant" is just the digital compensator (2p2z filter) running in the
 *   HRTIM ISR. We inject a sine into its reference input and accumulate the
 *   output. The states used are the original ones: INIT → SETTLING →
 *   MEASURING → CALCULATE → NEXT_FREQ → DONE → STOP.
 *
 * [PLANT PATH] (IPLANT)
 *   Live converter operation. Must safely:
 *     1. Enable hardware (relay, PWM, ADC)
 *     2. Slowly ramp duty until VOUT reaches the desired operating point
 *     3. Verify the operating point is stable (not just a transient)
 *     4. Lock in that duty as the DC bias (dc_op_latch)
 *     5. Sweep frequencies with small sine perturbation on top of dc_op_latch
 *     6. Between each frequency: fade amplitude to zero (de-inject)
 *        BEFORE changing DDS frequency — prevents transients
 *     7. Monitor VOUT throughout — fault trip if it leaves the safe band
 *     8. After sweep: fade amplitude to zero, then ramp duty back to zero
 *     9. Disable hardware cleanly
 *
 * =============================================================================
 * ISR CONTRACT
 * =============================================================================
 *
 * The HRTIM ISR (HAL_HRTIM_CounterResetCallback in main.c) is responsible for:
 *   - Advancing the DDS (phase_acc += phase_inc, compute index)
 *   - Computing sine_out = amplitude * sine_lut[index]
 *   - Computing sine_ref and cosine_ref (unity amplitude references)
 *   - Reading ADC values (VOUT, ISENSE) into the struct
 *   - Computing u32_pwm_duty_count = dc_op_latch + (uint32_t)sine_out
 *   - Writing to HRTIM compare register
 *   - During MEASURING: accumulating IQ
 *   - Calling SFRA_Plant_CheckSafetyWatchdog() each ISR
 *   - Calling SFRA_Run() each ISR (FSM tick)
 *
 * The ISR does NOT make state transition decisions.
 * All decisions live in SFRA_Run() / the state handler functions below.
 *
 * =============================================================================
 */

#include "sfra.h"
#include "stm32g4xx_hal.h"    /* HAL_GetTick(), GPIO */
#include "main.h"             /* HRTIM handle, DAC handle, GPIO defines */
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define PI_F    (3.14159265359f)

/* Global instance — zero-initialised at startup by C runtime */
sfra_t g_sfra;


/* =============================================================================
 * LUT INITIALISATION
 *
 * Pre-compute 8192 sine values at startup. This trades 8192*4 = 32 kB of RAM
 * for eliminating sinf() calls inside the 100kHz ISR (sinf on Cortex-M4 FPU
 * takes ~20-60 cycles depending on argument).
 *
 * Note: the LUT stores ONE full cycle (0 to 2π). The cosine is obtained by
 * reading with a 90° offset (index + 2048) & 0x1FFF, exploiting:
 *   cos(θ) = sin(θ + π/2)
 * ============================================================================= */
void LUT_Init(void)
{
    for(int i = 0; i < DDS_LUT_SIZE; i++){
        g_sfra.sine_lut[i] = sinf(2.0f * PI_F * ((float)i / (float)DDS_LUT_SIZE));
    }
}


/* =============================================================================
 * SFRA_UpdateFrequency
 *
 * Called whenever we move to a new injection frequency.
 * Computes:
 *   phase_inc  — the per-ISR phase step for the DDS
 *   settle_samples — how many ISR cycles to wait before measuring
 *   measure_samples — how many ISR cycles to integrate IQ over
 *
 * IMPORTANT: phase_acc and index are NOT reset here. They are reset
 * in the transition from SETTLING → MEASURING (after the transient has
 * settled). Resetting here would cause a phase discontinuity mid-injection
 * on the first ISR after frequency update.
 * ============================================================================= */
void SFRA_UpdateFrequency(float freq)
{
    g_sfra.current_freq = freq;

    /*
     * DDS phase increment.
     * The 32-bit accumulator wraps at 2^32. We need phase_acc to complete
     * exactly (freq/Fs) of a full cycle per ISR step:
     *   phase_inc = freq * (2^32) / Fs
     *
     * Example: freq=1000Hz, Fs=100000Hz
     *   phase_inc = 1000 * 4294967296 / 100000 = 42949673 (≈ 0x28F5C28)
     *   Each ISR the accumulator advances 1000/100000 = 1% of full scale.
     *   After 100 ISR calls → phase_acc wraps once → one complete cycle. ✓
     */
    g_sfra.phase_inc = (uint32_t)(freq * DDS_FULL_SCALE / FLOAT_SFRA_FS_HZ);

    /*
     * Settling window = SFRA_SETTLING_CYCLES full cycles of the injection freq.
     * In samples: N_settle = SFRA_SETTLING_CYCLES * (Fs / freq)
     *
     * At 10Hz, Fs=100kHz: N_settle = 50 * 10000 = 500,000 samples (5 seconds).
     * At 30kHz, Fs=100kHz: N_settle = 50 * 3.33 ≈ 167 samples (1.67 ms).
     * This is intentional — low-frequency points need long settling times
     * because the converter's output filter (LC) has a large time constant
     * relative to the injection period.
     */
    g_sfra.settle_samples  = (uint32_t)((float)SFRA_SETTLING_CYCLES  * FLOAT_SFRA_FS_HZ / freq);
    g_sfra.measure_samples = (uint32_t)((float)SFRA_MEASUREMENT_CYCLES * FLOAT_SFRA_FS_HZ / freq);

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    /*
     * Fade window = SFRA_FADE_CYCLES full cycles.
     * During DEJECT_WAIT, amplitude is decremented by amplitude_step each ISR.
     * amplitude_step is set when entering DEJECT_WAIT (see state handler below).
     */
    g_sfra.fade_samples = (uint32_t)((float)SFRA_FADE_CYCLES * FLOAT_SFRA_FS_HZ / freq);

#endif
}


/* =============================================================================
 * SFRA_Calculate
 *
 * Called once per frequency point after the IQ accumulators are full.
 *
 * Math recap (from your presentation):
 *   After accumulating N samples:
 *     I_acc = Σ y[n]*sin(ωn) ≈ (B*N/2)*cos(φ)
 *     Q_acc = Σ y[n]*cos(ωn) ≈ (B*N/2)*sin(φ)
 *   where B = amplitude of output signal, φ = phase of output signal.
 *
 *   Magnitude: R = sqrt(I² + Q²) = B*N/2
 *   So: B = 2*R/N
 *
 *   Phase: φ = atan2(Q, I)
 *
 * Applied separately to input and output, then ratioed:
 *   Gain  = B_output / B_input
 *   Phase = φ_output - φ_input  (then wrapped to ±180°)
 * ============================================================================= */
void SFRA_Calculate(void)
{
    /* ---- Input signal extraction ---- */
    float input_mag   = sqrtf(g_sfra.input_I_acc  * g_sfra.input_I_acc  +
                               g_sfra.input_Q_acc  * g_sfra.input_Q_acc);
    float input_amp   = 2.0f * input_mag / (float)g_sfra.measure_samples;
    float input_phase = atan2f(g_sfra.input_Q_acc, g_sfra.input_I_acc);

    /* ---- Output signal extraction ---- */
    float output_mag   = sqrtf(g_sfra.output_I_acc * g_sfra.output_I_acc +
                                g_sfra.output_Q_acc * g_sfra.output_Q_acc);
    float output_amp   = 2.0f * output_mag / (float)g_sfra.measure_samples;
    float output_phase = atan2f(g_sfra.output_Q_acc, g_sfra.output_I_acc);

    /* ---- Guard: don't divide by near-zero input ----
     * This can happen at very high frequencies where the injection sine is
     * heavily attenuated by the converter's input filter, or if the DDS
     * produces a very small signal due to integer truncation in phase_inc.
     * In these cases, the measurement is meaningless — store NaN so the
     * post-processing tool knows to flag this point.
     */
    if(input_amp < 1e-6f)
    {
        g_sfra.gain_db[g_sfra.freq_index]   = -999.0f;  /* Sentinel for invalid */
        g_sfra.phase_deg[g_sfra.freq_index] = -999.0f;
        g_sfra.b_result_ready_flag = true;
        return;
    }

    float gain     = output_amp / input_amp;
    float gain_db  = 20.0f * log10f(gain);

    /* ---- Phase difference with wrapping ----
     * atan2f returns [-π, +π]. Subtraction can produce values in [-2π, +2π].
     * We wrap to [-180°, +180°] so the Bode plot phase trace is continuous
     * when plotted by a tool. For a boost converter you expect phase going
     * more negative as frequency increases past the resonance peak, so you
     * will often see values between -180° and -360° — unwrap if needed in
     * post-processing (Python/MATLAB).
     */
    float phase_deg = (output_phase - input_phase) * 180.0f / PI_F;
    while(phase_deg >  180.0f) phase_deg -= 360.0f;
    while(phase_deg < -180.0f) phase_deg += 360.0f;

    g_sfra.gain_db[g_sfra.freq_index]   = gain_db;
    g_sfra.phase_deg[g_sfra.freq_index] = phase_deg;
    g_sfra.b_result_ready_flag = true;
}


/* =============================================================================
 * SAFETY FUNCTIONS (plant path only)
 * ============================================================================= */
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ

/*
 * SFRA_Plant_TriggerFault
 *
 * Hard fault entry. Called from the safety watchdog (in ISR context) or from
 * any state handler that detects an unrecoverable condition.
 *
 * Actions:
 *   1. Set amplitude and amplitude_target to 0 immediately.
 *      The ISR will compute sine_out = 0 * lut = 0, so duty returns to dc_op_latch.
 *      But we also zero dc_op_latch next:
 *   2. Zero dc_op_latch so the ISR's duty computation gives 0.
 *      The HRTIM output goes to 0% duty on the next compare update.
 *   3. Record fault reason and set flag for main loop to report via UART.
 *   4. Transition to FAULT state.
 *
 * NOTE: We do NOT directly write to the HRTIM register here. The ISR does
 * that. By zeroing the values the ISR reads, we ensure the ISR itself applies
 * 0 duty on its very next execution — this is the safe, race-condition-free
 * approach.
 */
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

/*
 * SFRA_Plant_CheckSafetyWatchdog
 *
 * Called from the HRTIM ISR every switching cycle.
 * Monitors VOUT ADC against the safe operating band.
 *
 * Only active during injection states (SETTLING, MEASURING) because:
 *   - During RAMP_UP: VOUT changing is expected.
 *   - During VERIFY_DCOP: VOUT may be slightly off — that's the whole point.
 *   - During DEJECT_WAIT / RAMP_DOWN: amplitude is going to zero, VOUT
 *     may transiently deviate — we can tolerate this briefly.
 *   - During SETTLING / MEASURING: VOUT should be held near target by the
 *     DC operating point. Any excursion here is unexpected and dangerous.
 *
 * The VOUT_SAFE_BAND_ADC is ±3V (in ADC counts). If VOUT drifts outside
 * [TARGET - BAND, TARGET + BAND], something is wrong (load change, oscillation,
 * overcurrent trip, etc.) and we abort.
 */
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

#endif /* TOGGLE_SWEEP_IPLANT_FS_100KHZ */


/* =============================================================================
 * FSM STATE HANDLER FUNCTIONS
 *
 * Each state is handled by a dedicated static function.
 * Pattern: read inputs → decide transition → write outputs.
 * No state handler calls another state handler directly — they only set
 * g_sfra.state. The dispatch in SFRA_Run() calls the new handler next tick.
 * ============================================================================= */

/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_INIT  (compensator path entry point)
 *
 * What it does:
 *   - Records start time for elapsed time calculation.
 *   - Sets b_start_flag so main loop prints the sweep header over UART.
 *   - Points the DDS at the first frequency in the table.
 *   - Resets settle_counter and transitions to SETTLING.
 *
 * Why no amplitude set here?
 *   For compensator sweeps, amplitude is set in SFRA_Init() from the
 *   compile-time macro SINE_INJECTED_AMPLITUDE_ADC. We don't touch it here.
 *   For plant sweep, amplitude is set in SFRA_STATE_SFRA_INIT after the
 *   DC op is verified — we don't pass through this state on the plant path.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_Init(void)
{
    g_sfra.start_time_ms  = HAL_GetTick();
    g_sfra.b_start_flag   = true;
    g_sfra.freq_index     = 0;
    SFRA_UpdateFrequency(g_sfra.freq_table[0]);
    g_sfra.settle_counter = 0;

    /*
     * On the compensator path, go directly to SETTLING.
     * The injection amplitude is already set (from SFRA_Init),
     * and DEJECT_WAIT is not needed at the very first frequency
     * because there's no previous injection to fade out.
     */
    g_sfra.state = SFRA_STATE_SETTLING;
}


#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_PERIPH_INIT  (plant path only)
 *
 * What it does:
 *   - Enables the relay GPIO to connect the load.
 *   - Starts the HRTIM waveform output with 0% duty (PWM enabled but
 *     no pulse yet — this energises the gate driver bootstrap).
 *   - Waits one FSM tick, then moves to CHECK_SIGNALS.
 *
 * Why separate from SFRA_STATE_INIT?
 *   On the compensator path, the HRTIM is already running when SFRA starts.
 *   On the plant path, we need to sequence: relay ON → wait → HRTIM ON →
 *   wait → start ramping. Keeping peripheral init in its own state makes
 *   this sequencing explicit and auditable.
 *
 * NOTE: HAL_HRTIM_WaveformCountStart_IT and WaveformOutputStart are called
 * from main.c before entering the while(1) loop for the compensator path.
 * For the plant path, we trigger them here, from the FSM.
 * This function assumes the HRTIM is configured but NOT yet started.
 *
 * IMPORTANT: Adjust the GPIO/HRTIM calls to match your actual pin and
 * HRTIM timer assignments.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_PeriphInit(void)
{
    /* Enable relay — connects the 120Ω load to the output */
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_GPIO_Pin, GPIO_PIN_SET);

    /* Start HRTIM at 0 duty. The HRTIM compare is set to 0 by default
     * (from the zeroed struct). The ISR will write u32_pwm_duty_count
     * to the compare register each cycle — starting at 0. */
    HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_TIMER_A);
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1);

    /* Record start time now (sweep timer starts from periph init) */
    g_sfra.start_time_ms = HAL_GetTick();
    g_sfra.b_start_flag  = true;

    g_sfra.state = SFRA_STATE_CHECK_SIGNALS;
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_CHECK_SIGNALS  (plant path only)
 *
 * What it does:
 *   Reads the current VOUT ADC and ISENSE ADC values.
 *   Verifies they are above the noise floor — confirming that:
 *     1. The voltage sensing divider is connected and reading something real.
 *     2. The current sensor is connected (even at 0A it should read a mid-rail
 *        bias if your ISENSE conditioning circuit has a DC offset).
 *
 * Why this state exists:
 *   Before we start ramping duty, we want to confirm the ADC channels are
 *   alive. A disconnected VOUT sense resistor would read 0, and we'd ramp
 *   duty to 90% chasing a target that can never be reached — destructive.
 *
 * What is VOUT_NOISE_FLOOR_ADC?
 *   VOUT_TO_ADC_COUNT(5.0f) — corresponding to 5V at the output.
 *   With a 24V input, even before the converter is switching, the input
 *   voltage can charge the output through the diode to near Vin. So seeing
 *   some voltage here is expected. We're just checking it's not zero (open
 *   circuit / disconnected divider).
 *
 *   ISENSE check: just verify u32_isense_ave_adc > 0 (non-zero ADC reading).
 *   At zero current, the Hall sensor output = Vcc/2 = 1.65V, which gives
 *   an ADC count of about 2048. If it reads 0, the ADC channel is broken.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_CheckSignals(void)
{
    bool vout_ok   = (g_sfra.u32_vout_adc   > VOUT_NOISE_FLOOR_ADC);
    bool isense_ok = (g_sfra.u32_isense_ave_adc > 0U);

    if(vout_ok && isense_ok)
    {
        /* Zero the duty counter — we start ramping from 0 */
        g_sfra.u32_duty_dc_op_count = 0U;
        g_sfra.u32_pwm_duty_count   = 0U;
        g_sfra.state = SFRA_STATE_RAMP_UP;
    }
    else
    {
        /* Fault: ADC signals missing. Do not proceed. */
        SFRA_Plant_TriggerFault(SFRA_FAULT_NO_SIGNAL);
    }
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_RAMP_UP  (plant path only)
 *
 * What it does:
 *   Increments u32_duty_dc_op_count by PWM_DUTY_INC_TICKS each ISR call.
 *   The ISR reads u32_duty_dc_op_count and writes it to the HRTIM compare
 *   register (since amplitude is still 0, u32_pwm_duty_count = dc_op_count).
 *   We monitor VOUT ADC each tick and stop when it reaches VOUT_TARGET_ADC.
 *
 * Safety guard:
 *   If duty reaches PWM_DUTY_MAX_TICKS without VOUT reaching target,
 *   something is wrong (open-circuit output, wrong Vin, component fault).
 *   We trigger a fault rather than holding 90% duty indefinitely.
 *
 * Why increment in the FSM (not the ISR)?
 *   The FSM runs inside the ISR (called from CounterResetCallback).
 *   So incrementing here IS happening at the ISR rate — every 10µs at 100kHz.
 *   The separation is logical: the ISR computes the PWM compare value from
 *   dc_op_count; the FSM increments dc_op_count. Each has its own job.
 *
 * Rate of ramp with PWM_DUTY_INC_TICKS = 3 at 100kHz:
 *   Duty change per second = 3 ticks * 100,000 ISR/sec = 300,000 ticks/sec
 *   HRTIM_PERIOD = 54400 ticks → duty change per second = 300000/54400 ≈ 5.5 duty/sec
 *   So we ramp from 0% to 90% (48960 ticks) in about 48960/300000 ≈ 0.16 sec.
 *   This is fast. For first bring-up, reduce to PWM_DUTY_INC_TICKS = 1 (0.5s ramp).
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_RampUp(void)
{
    /* Increment duty */
    g_sfra.u32_duty_dc_op_count += PWM_DUTY_INC_TICKS;

    /* Guard: never exceed max duty */
    if(g_sfra.u32_duty_dc_op_count >= PWM_DUTY_MAX_TICKS)
    {
        g_sfra.u32_duty_dc_op_count = PWM_DUTY_MAX_TICKS;
        /* We've hit max duty and presumably haven't reached VOUT target */
        SFRA_Plant_TriggerFault(SFRA_FAULT_MAX_DUTY_REACHED);
        return;
    }

    /* Check if we've reached the target output voltage */
    if(g_sfra.u32_vout_adc >= VOUT_TARGET_ADC)
    {
        /* Stop incrementing — move to verification */
        g_sfra.vout_verify_acc    = 0ULL;
        g_sfra.u32_verify_counter = 0U;
        g_sfra.state = SFRA_STATE_VERIFY_DCOP;
    }

    /*
     * The ISR will use the updated u32_duty_dc_op_count on the very next
     * PWM cycle (u32_pwm_duty_count = dc_op_count + 0 since amplitude = 0).
     */
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_VERIFY_DCOP  (plant path only)
 *
 * What it does:
 *   Holds duty constant (no increment) and accumulates N_VERIFY_SAMPLES
 *   readings of VOUT ADC. After N_VERIFY_SAMPLES, computes the average
 *   and checks if it's within ±VOUT_TOL_ADC of VOUT_TARGET_ADC.
 *
 * Why this state is necessary:
 *   During RAMP_UP, VOUT_ADC can momentarily reach VOUT_TARGET_ADC due to
 *   a transient or output voltage overshoot (boost converter with no closed
 *   loop will have some LC ringing as duty increases). If we latched dc_op
 *   at that instant, our actual steady-state operating point might be wrong.
 *
 *   By averaging N_VERIFY_SAMPLES (5000 samples = 50ms at 100kHz), we ensure
 *   the switching ripple (100kHz) averages out perfectly (it's a zero-mean
 *   AC signal), and any LC resonance transient (typically ~1-5kHz for a boost
 *   design) also averages toward zero. What remains is the true DC output.
 *
 * Why uint64_t for the accumulator?
 *   Maximum ADC value = 4095. N_VERIFY_SAMPLES = 5000.
 *   Maximum accumulator value = 4095 * 5000 = 20,475,000.
 *   This fits in uint32_t (max ~4.29 billion) but we use uint64_t for safety
 *   in case someone increases N_VERIFY_SAMPLES significantly.
 *
 * If verification fails:
 *   VOUT average is outside ±VOUT_TOL_ADC. This means the converter hasn't
 *   settled, or the operating point has drifted. We go back to RAMP_UP and
 *   try again. The duty stays at its current value — no reset to 0. The ramp
 *   will increment from where we left off, letting the converter settle more.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_VerifyDcOp(void)
{
    g_sfra.vout_verify_acc += (uint64_t)g_sfra.u32_vout_adc;
    g_sfra.u32_verify_counter++;

    if(g_sfra.u32_verify_counter >= N_VERIFY_SAMPLES)
    {
        uint32_t vout_avg = (uint32_t)(g_sfra.vout_verify_acc / (uint64_t)N_VERIFY_SAMPLES);

        /* Check if average is within tolerance of target */
        int32_t error = (int32_t)vout_avg - (int32_t)VOUT_TARGET_ADC;
        if(error < 0) error = -error;

        if((uint32_t)error <= VOUT_TOL_ADC)
        {
            /* Stable! Latch the duty count as the DC operating point */
            g_sfra.u32_duty_dc_op_latch = g_sfra.u32_duty_dc_op_count;
            g_sfra.state = SFRA_STATE_SFRA_INIT;
        }
        else
        {
            /* Not yet stable — go back to ramp, let it adjust */
            g_sfra.vout_verify_acc    = 0ULL;
            g_sfra.u32_verify_counter = 0U;
            g_sfra.state = SFRA_STATE_RAMP_UP;
        }
    }
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_SFRA_INIT  (plant path only)
 *
 * What it does:
 *   This is the bridge between the bring-up phase and the injection phase.
 *   At this point:
 *     - The converter is at steady-state with dc_op_latch duty
 *     - VOUT is verified at VOUT_TARGET
 *     - Amplitude is still 0 (no injection yet)
 *
 *   We now:
 *     1. Generate the frequency table (logarithmic sweep)
 *     2. Compute the injection amplitude = PERCENT * dc_op_latch
 *        (expressed in HRTIM duty ticks, as a fraction of the DC operating point)
 *     3. Set amplitude_target = amplitude (both start at the computed value)
 *        The ISR uses amplitude to compute sine_out = amplitude * sine_lut[index]
 *        When amplitude is added to dc_op_latch, the perturbation is exactly
 *        FLOAT_SINE_INJECTED_AMPLITUDE_PERCENT of the DC operating duty.
 *     4. Point DDS at freq_table[0] and set start time.
 *     5. Transition to DEJECT_WAIT for the first frequency.
 *        Even on the very first frequency, we use DEJECT_WAIT — it will
 *        ramp amplitude UP to the target (amplitude starts at 0 after memset).
 *        Wait... actually we want to RAMP UP the amplitude on the first freq.
 *        We handle this by setting amplitude = 0, amplitude_target = computed,
 *        and entering DEJECT_WAIT which will ramp it up over fade_samples.
 *        This gives a smooth amplitude onset on the first frequency too.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_SfraInit(void)
{
    /* Generate logarithmic frequency table */
    SFRA_GenerateFrequencyTable();

    /*
     * Injection amplitude in HRTIM ticks.
     * sine_out = amplitude * sine_lut[index]  (sine_lut values are ±1.0)
     * u32_pwm_duty_count = dc_op_latch + (int32_t)sine_out
     *
     * So peak duty deviation = amplitude ticks.
     * At 3% and dc_op_latch = 33700:  amplitude = 0.03 * 33700 = 1011 ticks.
     * Peak duty swing = ±1011/54400 = ±1.86% duty (small signal ✓).
     */
    float computed_amplitude = FLOAT_SINE_INJECTED_AMPLITUDE_PERCENT *
                               (float)g_sfra.u32_duty_dc_op_latch;

    /*
     * Start with amplitude = 0, ramp up to computed_amplitude in DEJECT_WAIT.
     * This avoids a step injection on the first frequency.
     */
    g_sfra.amplitude        = 0.0f;
    g_sfra.amplitude_target = computed_amplitude;

    /* Point DDS at first frequency, compute settle/measure/fade sample counts */
    g_sfra.freq_index     = 0;
    g_sfra.settle_counter = 0;
    g_sfra.fade_counter   = 0;
    SFRA_UpdateFrequency(g_sfra.freq_table[0]);

    /* Compute the per-ISR amplitude step for the fade-in.
     * Over fade_samples ISR calls, amplitude moves from 0 to amplitude_target. */
    if(g_sfra.fade_samples > 0U)
    {
        g_sfra.amplitude_step = computed_amplitude / (float)g_sfra.fade_samples;
    }

    g_sfra.start_time_ms = HAL_GetTick();

    /* Enter DEJECT_WAIT, which will ramp amplitude up to amplitude_target */
    g_sfra.state = SFRA_STATE_DEJECT_WAIT;
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_DEJECT_WAIT  (plant path only)
 *
 * This is the most critical new state. Let me explain the full picture.
 *
 * WHY this state exists:
 *   When we finish measuring at frequency f1 and want to switch to f2,
 *   we cannot simply change phase_inc (the DDS frequency) in one step.
 *   Here's why:
 *
 *   At frequency f1, the DDS is generating:
 *     sine_out(t) = A * sin(2π*f1*t)
 *   At the moment of switch, the phase is at some arbitrary point θ₁.
 *   If we change phase_inc to f2 and continue, the signal immediately jumps to:
 *     sine_out(t) = A * sin(2π*f2*t + θ₁)
 *   This is a DISCONTINUITY in the duty cycle signal. The converter sees it
 *   as a step change in duty, which causes an output voltage transient
 *   proportional to A * d(sine)/dt at that instant.
 *
 *   At A = 3% * 33700 ≈ 1011 HRTIM ticks and worst case θ₁ near zero crossing
 *   (maximum slope), the rate of change can be large. The converter's output
 *   LC filter doesn't attenuate this step — it's below the control bandwidth.
 *
 * WHAT this state does:
 *   TWO sub-phases, handled by the same state based on whether amplitude has
 *   reached its target:
 *
 *   Sub-phase A — FADE OUT (or FADE IN on first frequency):
 *     amplitude steps toward amplitude_target each ISR.
 *     The ISR uses g_sfra.amplitude (which is changing) directly.
 *     When amplitude reaches target:
 *       - If fading TO zero (between frequencies): proceed to sub-phase B.
 *       - If fading TO a value (first frequency or... we use sub-phase B differently).
 *
 *   Actually let me simplify the logic:
 *   The state has ONE job: smoothly move amplitude toward amplitude_target.
 *   When it arrives, check what to do based on whether target is 0 or non-zero:
 *     - If target is 0: we just finished fading out an old frequency.
 *       Now advance freq_index, update DDS to new frequency, compute new
 *       amplitude_target = A (non-zero), set new amplitude_step, reset
 *       fade_counter, stay in DEJECT_WAIT to fade the new frequency back IN.
 *       Wait — this would be two passes through DEJECT_WAIT. Is this right?
 *
 *   REVISED DESIGN (simpler and more correct):
 *   DEJECT_WAIT always does:
 *     1. Fade amplitude FROM current value TO zero (FADE OUT only).
 *     2. Once at zero: reset DDS accumulators (phase_acc=0, index=0),
 *        update phase_inc to new frequency, zero accumulators, go to SETTLING.
 *   The amplitude RAMP UP happens naturally during SETTLING — over settle_samples
 *   ISR calls, we step amplitude from 0 back to the computed value.
 *   This is cleaner: DEJECT_WAIT = "amplitude ramp to zero + housekeeping",
 *   SETTLING = "amplitude ramp to target + transient waiting period".
 *
 *   amplitude_step during DEJECT_WAIT = -current_amplitude / fade_samples
 *   amplitude_step during SETTLING    = +target_amplitude / settle_samples
 *
 * DEJECT_WAIT exit:
 *   When fade_counter >= fade_samples, amplitude should be at or near zero.
 *   Hard-clamp to 0.0f (floating point may leave a tiny residual).
 *   Reset DDS phase, update to new frequency, go to SETTLING.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_DejectWait(void)
{
    g_sfra.fade_counter++;

    /* Step amplitude toward zero */
    g_sfra.amplitude -= g_sfra.amplitude_step;
    if(g_sfra.amplitude < 0.0f) g_sfra.amplitude = 0.0f;

    if(g_sfra.fade_counter >= g_sfra.fade_samples)
    {
        /* Hard clamp — ensure exactly zero before DDS reset */
        g_sfra.amplitude     = 0.0f;
        g_sfra.fade_counter  = 0U;

        /* Reset DDS phase accumulator.
         * We reset HERE (after amplitude is zero) so there's no discontinuity.
         * The next sine_out = 0 * lut[0] = 0. Clean start. */
        g_sfra.phase_acc = 0U;
        g_sfra.index     = 0U;

        /* Reset all IQ accumulators for the new frequency */
        g_sfra.input_I_acc  = 0.0f;
        g_sfra.input_Q_acc  = 0.0f;
        g_sfra.output_I_acc = 0.0f;
        g_sfra.output_Q_acc = 0.0f;
        g_sfra.settle_counter = 0U;

        /*
         * Compute amplitude ramp-up step for SETTLING phase.
         * Over settle_samples ISR calls, we ramp amplitude from 0 to amplitude_target.
         * This means the first settle_samples ISR calls are simultaneously:
         *   a) letting the converter reach steady-state at the new frequency, AND
         *   b) slowly introducing the injection signal.
         * This is safe because at the start of settling the amplitude is tiny
         * (fraction of target) — the converter sees a very small perturbation
         * that grows gradually. By the end of settling, amplitude = full target.
         */
        float computed_amplitude = FLOAT_SINE_INJECTED_AMPLITUDE_PERCENT *
                                   (float)g_sfra.u32_duty_dc_op_latch;
        g_sfra.amplitude_target = computed_amplitude;

        if(g_sfra.settle_samples > 0U)
        {
            g_sfra.amplitude_step = computed_amplitude / (float)g_sfra.settle_samples;
        }
        else
        {
            g_sfra.amplitude      = computed_amplitude;  /* Instantaneous if settle=0 */
            g_sfra.amplitude_step = 0.0f;
        }

        g_sfra.state = SFRA_STATE_SETTLING;
    }
}

#endif /* TOGGLE_SWEEP_IPLANT_FS_100KHZ */


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_SETTLING  (both paths)
 *
 * Compensator path:
 *   Injection is already at full amplitude (set in SFRA_Init).
 *   We simply wait settle_samples ISR cycles for the compensator's
 *   transient response to die out at the new frequency.
 *   Then reset accumulators and go to MEASURING.
 *
 * Plant path:
 *   Amplitude is ramping up from 0 to amplitude_target (step added each ISR).
 *   This ramp is handled in the ISR — the ISR always uses g_sfra.amplitude
 *   directly, which is being incremented here in the FSM.
 *   We also wait for the converter to reach sinusoidal steady-state.
 *
 *   NOTE: the amplitude ramp completes within settle_samples. So by the time
 *   we transition to MEASURING, amplitude has been at its full value for some
 *   fraction of the settling period. The settling period is long enough
 *   (50 cycles) that even the last few cycles at full amplitude are sufficient.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_Settling(void)
{
    g_sfra.settle_counter++;

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    /* Ramp amplitude up during settling */
    if(g_sfra.amplitude < g_sfra.amplitude_target)
    {
        g_sfra.amplitude += g_sfra.amplitude_step;
        if(g_sfra.amplitude > g_sfra.amplitude_target)
        {
            g_sfra.amplitude = g_sfra.amplitude_target;
        }
    }
#endif

    if(g_sfra.settle_counter >= g_sfra.settle_samples)
    {
        /*
         * Settling complete. Reset phase_acc so measurement starts
         * at a known phase reference (θ=0). This ensures the IQ
         * accumulation begins from a consistent starting point,
         * which matters because we want:
         *   Input signal: x[n] = A*sin(ω*n*Ts + 0)
         * starting cleanly from the beginning of the measurement window.
         *
         * Also zero all IQ accumulators (belt-and-suspenders — they were
         * already zeroed when entering SETTLING from DEJECT_WAIT, but the
         * ISR may have accumulated a few values during the tail of settling).
         */
        g_sfra.phase_acc      = 0U;
        g_sfra.index          = 0U;
        g_sfra.measure_counter = 0U;
        g_sfra.input_I_acc    = 0.0f;
        g_sfra.input_Q_acc    = 0.0f;
        g_sfra.output_I_acc   = 0.0f;
        g_sfra.output_Q_acc   = 0.0f;

        g_sfra.state = SFRA_STATE_MEASURING;
    }
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_MEASURING  (both paths)
 *
 * Simply counts ISR ticks. The ISR does the actual IQ accumulation work
 * (accumulating input_I_acc, input_Q_acc, output_I_acc, output_Q_acc)
 * when it sees state == SFRA_STATE_MEASURING.
 *
 * We don't touch the accumulators here — they're being filled by the ISR.
 * When measure_counter reaches measure_samples, we're done and CALCULATE.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_Measuring(void)
{
    g_sfra.measure_counter++;

    if(g_sfra.measure_counter >= g_sfra.measure_samples)
    {
        g_sfra.state = SFRA_STATE_CALCULATE;
    }
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_CALCULATE  (both paths)
 *
 * One-shot state: call SFRA_Calculate() then immediately move to NEXT_FREQ.
 * SFRA_Calculate() sets b_result_ready_flag = true, which the main loop
 * catches to print the CSV result over UART.
 *
 * Why is CALCULATE a separate state instead of being done inside MEASURING?
 *   sqrtf(), log10f(), atan2f() are relatively heavy operations (~20-100 cycles
 *   each on Cortex-M4 FPU). Putting them in a dedicated state ensures they
 *   happen exactly once, and the ISR only sees this state for a single tick —
 *   brief enough that the DDS keeps running without disruption.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_Calculate(void)
{
    SFRA_Calculate();
    g_sfra.state = SFRA_STATE_NEXT_FREQ;
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_NEXT_FREQ  (both paths)
 *
 * Advances the frequency index.
 *
 * Compensator path:
 *   Goes to SETTLING (via SFRA_UpdateFrequency).
 *   No fade-out needed because there's no live voltage risk.
 *
 * Plant path:
 *   Goes to DEJECT_WAIT before SETTLING.
 *   Sets up the amplitude fade-out step:
 *     amplitude_step = amplitude / fade_samples  (positive, used as subtraction)
 *
 * Terminal condition:
 *   Both paths: if freq_index >= num_freqs, go to DONE (compensator)
 *   or RAMP_DOWN (plant).
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_NextFreq(void)
{
    /* Clear result flag before advancing — ensures main loop doesn't
     * process a stale result from the previous frequency */
    g_sfra.b_result_ready_flag = false;

    g_sfra.freq_index++;

    if(g_sfra.freq_index >= g_sfra.num_freqs)
    {
        /* Sweep complete */
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
        /*
         * Plant path: fade amplitude to zero, then ramp duty down.
         * Set amplitude_step for fade-out using the CURRENT frequency's
         * fade_samples (already computed).
         */
        g_sfra.amplitude_target = 0.0f;
        if(g_sfra.fade_samples > 0U)
        {
            g_sfra.amplitude_step = g_sfra.amplitude / (float)g_sfra.fade_samples;
        }
        g_sfra.fade_counter = 0U;
        g_sfra.state = SFRA_STATE_RAMP_DOWN;
#else
        /* Compensator path: go directly to DONE */
        g_sfra.state = SFRA_STATE_DONE;
#endif
    }
    else
    {
        /* Advance to next frequency */
        SFRA_UpdateFrequency(g_sfra.freq_table[g_sfra.freq_index]);
        g_sfra.settle_counter = 0U;

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
        /*
         * Plant path: set up amplitude fade-out for DEJECT_WAIT.
         * amplitude_step is subtracted each ISR during DEJECT_WAIT.
         * After fade_samples ISRs, amplitude reaches 0.
         * new fade_samples was computed by SFRA_UpdateFrequency above.
         */
        g_sfra.amplitude_target = 0.0f;
        if(g_sfra.fade_samples > 0U)
        {
            g_sfra.amplitude_step = g_sfra.amplitude / (float)g_sfra.fade_samples;
        }
        g_sfra.fade_counter = 0U;
        g_sfra.state = SFRA_STATE_DEJECT_WAIT;
#else
        /* Compensator path: go straight to SETTLING */
        g_sfra.state = SFRA_STATE_SETTLING;
#endif
    }
}


#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_RAMP_DOWN  (plant path only)
 *
 * Symmetric to RAMP_UP and DEJECT_WAIT combined.
 * Two sub-phases:
 *   Phase 1 — Amplitude fade to zero:
 *     We reuse the fade mechanism (amplitude_step, fade_counter).
 *     This takes fade_samples ISR calls.
 *     During this phase, duty = dc_op_latch + sine_out (with sine_out → 0).
 *   Phase 2 — Duty ramp to zero:
 *     Once amplitude = 0, we decrement u32_duty_dc_op_latch by
 *     PWM_DUTY_INC_TICKS each ISR until it reaches 0.
 *     The ISR computes u32_pwm_duty_count = dc_op_latch + 0 = dc_op_latch,
 *     which decrements each ISR call.
 *     When dc_op_latch = 0, HRTIM output is at 0% duty.
 *
 * After duty reaches 0:
 *   Disable HRTIM waveform output.
 *   Disable relay GPIO.
 *   Go to DONE.
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_RampDown(void)
{
    if(g_sfra.amplitude > 0.0f)
    {
        /* Sub-phase 1: fade amplitude to zero */
        g_sfra.fade_counter++;
        g_sfra.amplitude -= g_sfra.amplitude_step;
        if(g_sfra.amplitude < 0.0f) g_sfra.amplitude = 0.0f;

        /* When done fading, set up duty ramp-down step */
        if(g_sfra.fade_counter >= g_sfra.fade_samples || g_sfra.amplitude <= 0.0f)
        {
            g_sfra.amplitude     = 0.0f;
            g_sfra.fade_counter  = 0U;
            /* amplitude_step is no longer needed — zero it */
            g_sfra.amplitude_step = 0.0f;
        }
    }
    else
    {
        /* Sub-phase 2: ramp duty to zero */
        if(g_sfra.u32_duty_dc_op_latch > PWM_DUTY_INC_TICKS)
        {
            g_sfra.u32_duty_dc_op_latch -= PWM_DUTY_INC_TICKS;
        }
        else
        {
            /* Duty has reached zero */
            g_sfra.u32_duty_dc_op_latch = 0U;
            g_sfra.u32_pwm_duty_count   = 0U;

            /* Disable HRTIM output */
            HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1);
            HAL_HRTIM_WaveformCountStop_IT(&hhrtim1, HRTIM_TIMERID_TIMER_A);

            /* Disable relay (disconnect load) */
            HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_GPIO_Pin, GPIO_PIN_RESET);

            g_sfra.state = SFRA_STATE_DONE;
        }
    }
}

#endif /* TOGGLE_SWEEP_IPLANT_FS_100KHZ */


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_DONE
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_Done(void)
{
    g_sfra.end_time_ms     = HAL_GetTick();
    g_sfra.elapsed_time_ms = g_sfra.end_time_ms - g_sfra.start_time_ms;
    g_sfra.b_end_flag      = true;
    g_sfra.state           = SFRA_STATE_STOP;
}


/* --------------------------------------------------------------------------
 * STATE: SFRA_STATE_FAULT  (plant path only)
 * -------------------------------------------------------------------------- */
static void SFRA_Handle_Fault(void)
{
    /*
     * The fault handler in SFRA_Plant_TriggerFault already zeroed amplitude
     * and dc_op_latch. The ISR will apply 0 duty to HRTIM on its next tick.
     * We just idle here until the user resets.
     * Main loop reads b_fault_flag and prints fault reason via UART.
     */
    (void)0;
}


/* =============================================================================
 * SFRA_Run — FSM Dispatch (called each ISR tick)
 *
 * This is a pure dispatch table. Each state has a dedicated handler.
 * No logic lives here — logic lives in the handlers.
 *
 * Called from HRTIM CounterResetCallback (100kHz), so this executes
 * 100,000 times per second. The switch dispatch itself is ~3 cycles.
 * Each handler is O(1) — no loops, no heavy math (math lives in CALCULATE).
 * ============================================================================= */
void SFRA_Run(void)
{
    switch(g_sfra.state)
    {
        case SFRA_STATE_INIT:           SFRA_Handle_Init();         break;

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
        case SFRA_STATE_PERIPH_INIT:    SFRA_Handle_PeriphInit();   break;
        case SFRA_STATE_CHECK_SIGNALS:  SFRA_Handle_CheckSignals(); break;
        case SFRA_STATE_RAMP_UP:        SFRA_Handle_RampUp();       break;
        case SFRA_STATE_VERIFY_DCOP:    SFRA_Handle_VerifyDcOp();   break;
        case SFRA_STATE_SFRA_INIT:      SFRA_Handle_SfraInit();     break;
        case SFRA_STATE_DEJECT_WAIT:    SFRA_Handle_DejectWait();   break;
#endif

        case SFRA_STATE_SETTLING:       SFRA_Handle_Settling();     break;
        case SFRA_STATE_MEASURING:      SFRA_Handle_Measuring();    break;
        case SFRA_STATE_CALCULATE:      SFRA_Handle_Calculate();    break;
        case SFRA_STATE_NEXT_FREQ:      SFRA_Handle_NextFreq();     break;

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
        case SFRA_STATE_RAMP_DOWN:      SFRA_Handle_RampDown();     break;
        case SFRA_STATE_FAULT:          SFRA_Handle_Fault();        break;
#endif

        case SFRA_STATE_DONE:           SFRA_Handle_Done();         break;
        case SFRA_STATE_STOP:           /* Idle */                  break;
        default:                                                     break;
    }
}


/* =============================================================================
 * SFRA_Init
 * ============================================================================= */
void SFRA_Init(void)
{
    memset(&g_sfra, 0, sizeof(g_sfra));

    /*
     * For the compensator path: generate frequency table here and set
     * amplitude from the compile-time macro.
     * For the plant path: frequency table and amplitude are generated
     * in SFRA_STATE_SFRA_INIT (after DC op is verified), so we skip them here.
     */
#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
    /*
     * Plant path starts at PERIPH_INIT, not INIT.
     * The frequency table will be generated in SFRA_STATE_SFRA_INIT.
     * Amplitude will be set there too.
     */
    g_sfra.state = SFRA_STATE_PERIPH_INIT;
#else
    /* Compensator path */
    SFRA_GenerateFrequencyTable();
    g_sfra.amplitude = SINE_INJECTED_AMPLITUDE_ADC;
    g_sfra.state = SFRA_STATE_INIT;
#endif

    g_sfra.b_result_ready_flag = false;
    g_sfra.b_fault_flag        = false;
}


/* =============================================================================
 * SFRA_GenerateFrequencyTable
 * ============================================================================= */
void SFRA_GenerateFrequencyTable(void)
{
    float decades = log10f((float)FREQ_STOP_HZ / (float)FREQ_START_HZ);
    float ratio   = powf(10.0f, 1.0f / (float)FREQ_POINTS_PER_DECADE);

    g_sfra.num_freqs = (uint16_t)(decades * (float)FREQ_POINTS_PER_DECADE) + 1U;

    g_sfra.freq_table[0] = (float)FREQ_START_HZ;
    for(uint16_t i = 1U; i < g_sfra.num_freqs; i++)
    {
        g_sfra.freq_table[i] = g_sfra.freq_table[i - 1U] * ratio;
    }
    g_sfra.freq_table[g_sfra.num_freqs - 1U] = (float)FREQ_STOP_HZ;
}
