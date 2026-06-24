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

// Variables
static compensator_strategy_loop_t g_comp_strategy_loop;	// For which type of loop vloop or iloop

const sfra_strategy_t compensator_strategy =
{
    .ISR = CompensatorStrategy_ISR,
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
	.timer_idx = HRTIM_TIMERINDEX_TIMER_A
#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
	.timer_idx = HRTIM_TIMERINDEX_TIMER_D
#endif
};

// Functions
void CompensatorStrategy_Init(void)
{
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ

	g_comp_strategy_loop.p_comp = &comp2p2z_iloop;

#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ

	g_comp_strategy_loop.p_comp = &comp2p2z_vloop;

#endif
}


void CompensatorStrategy_ISR(void)
{
	SFRA_Run();
	DDS_Update();

	// Run Compensator 2p2z
	g_comp_strategy_loop.p_comp->f_ref = g_dds.f_sine_out;
	g_comp_strategy_loop.p_comp->f_fdbk = 0.0f;
	compensator_2P2Z_Update(g_comp_strategy_loop.p_comp);

	// Accumulator During FSM Measuring
	if(g_sfra.state == SFRA_STATE_MEASURING)
	{
		IQ_Accumulate(g_comp_strategy_loop.p_comp->f_ref, g_comp_strategy_loop.p_comp->f_out, g_dds.f_sine_ref, g_dds.f_cosine_ref);

	}
}
