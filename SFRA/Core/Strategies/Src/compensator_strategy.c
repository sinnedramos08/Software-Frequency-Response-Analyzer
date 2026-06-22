/*
 * compensator_strategy.c
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#include <sfra_engine.h>
#include <compensator_strategy.h>
#include <compensator.h>


void CompensatorStrategy_ISR(void)
{
	// Create Sine Wave
	// LUT index (top 13 bits)
	g_sfra.index = g_sfra.phase_acc >> DDS_LUT_SHIFT;
	g_sfra.phase_acc += g_sfra.phase_inc;

	// Create the Injected Signal Sine Wave
	g_sfra.sine_out = g_sfra.amplitude * g_sfra.sine_lut[g_sfra.index];						// Generated sine, injected to 2p2z
	g_sfra.cosine_out = g_sfra.amplitude * g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];	// Generated for testing only, not to be processed

	// Create a reference signal sine and cosine
	g_sfra.sine_ref = g_sfra.sine_lut[g_sfra.index];
	g_sfra.cosine_ref = g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];

	// Run Compensator 2p2z
	comp2p2z_iloop.f_ref = g_sfra.sine_out;
	comp2p2z_iloop.f_fdbk = 0.0f;
	compensator_2P2Z_Update(&comp2p2z_iloop);

	// Accumulator During FSM Measuring
	if(g_sfra.state == SFRA_STATE_MEASURING)
	{
	    g_sfra.input_I_acc += g_sfra.sine_out * g_sfra.sine_ref;

	    g_sfra.input_Q_acc +=g_sfra.sine_out * g_sfra.cosine_ref;

	    g_sfra.output_I_acc += comp2p2z_iloop.f_out * g_sfra.sine_ref;

	    g_sfra.output_Q_acc += comp2p2z_iloop.f_out *g_sfra.cosine_ref;
	}
}
