/*
 * compensator_strategy.c
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#include "sfra_engine.h"
#include "compensator_strategy.h"
#include "sfra_strategies.h"
#include "main.h"
#include "compensator.h"
#include "dds.h"
#include "iq.h"

const sfra_strategy_t compensator_strategy =
{
    .timer_idx = HRTIM_TIMERINDEX_TIMER_A,
    .ISR = CompensatorStrategy_ISR
};


void CompensatorStrategy_ISR(void)
{
	DDS_Update();

	// Run Compensator 2p2z
	comp2p2z_iloop.f_ref = g_dds.f_sine_out;
	comp2p2z_iloop.f_fdbk = 0.0f;
	compensator_2P2Z_Update(&comp2p2z_iloop);

	// Accumulator During FSM Measuring
	if(g_sfra.state == SFRA_STATE_MEASURING)
	{
		IQ_Accumulate(comp2p2z_iloop.f_ref, comp2p2z_iloop.f_out, g_dds.f_sine_ref, g_dds.f_cosine_ref);

	}
}
