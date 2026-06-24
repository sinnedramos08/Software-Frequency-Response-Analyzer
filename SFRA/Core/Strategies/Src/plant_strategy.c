/*
 * plant_strategy.c
 *
 *  Created on: Jun 24, 2026
 *      Author: DRamos
 */

#include "sfra_engine.h"
#include "plant_strategy.h"
#include "sfra_strategies.h"
#include "main.h"
#include "dds.h"
#include "iq.h"

plant_strategy_variables_t g_plant_variables;

const sfra_strategy_t plant_strategy =
{
    .ISR = PlantStrategy_ISR,
	.timer_idx = HRTIM_TIMERINDEX_TIMER_A
};



void PlantStrategy_ISR(void)
{

}
