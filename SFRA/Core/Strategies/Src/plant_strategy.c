/*
 * plant_strategy.c
 *
 *  Created on: Jun 24, 2026
 *      Author: DRamos
 */
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#include "hrtim.h"
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


static void Plant_StatePeriphInit_Handler(void);
static void Plant_StateCheckSignals_Handler(void);
static void Plant_StateRampUp_Handler(void);
static void Plant_StateVerifyDcop_Handler(void);
static void Plant_StateSfraInit_Handler(void);
static void Plant_StateSweep_Handler(void);
static void Plant_StateRampDown_Handler(void);
static void Plant_StateDone_Handler(void);
static void Plant_StateFault(void);


void PlantStrategy_ISR(void)
{
	Plant_Run();
}

void Plant_Run(void)
{
    switch(g_plant_variables.state)
    {
        case PLANT_STATE_PERIPH_INIT:
            Plant_StatePeriphInit_Handler();
            break;

        case PLANT_STATE_CHECK_SIGNALS:
            Plant_StateCheckSignals_Handler();
            break;

        case PLANT_STATE_RAMP_UP:
            Plant_StateRampUp_Handler();
            break;

        case PLANT_STATE_VERIFY_DCOP:
            Plant_StateVerifyDcop_Handler();
            break;

        case PLANT_STATE_SFRA_INIT:
            Plant_StateSfraInit_Handler();
            break;

        case PLANT_STATE_SWEEP:
            Plant_StateSweep_Handler();
            break;

        case PLANT_STATE_RAMP_DOWN:
            Plant_StateRampDown_Handler();
            break;

        case PLANT_STATE_DONE:
            Plant_StateDone_Handler();
            break;

        case PLANT_STATE_FAULT:
            Plant_StateFault();
            break;
    }
}

static void Plant_StatePeriphInit_Handler(void)
{
	HAL_GPIO_WritePin(RELAY_GPIO_GPIO_Port, RELAY_GPIO_Pin, GPIO_PIN_SET);
	//Ensure no PWM
	g_plant_variables.u32_pwm_duty = 27200;
	__HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_1, g_plant_variables.u32_pwm_duty);
	__HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_2, g_plant_variables.u32_pwm_duty>>1);


	g_plant_variables.u32_verify_counter = 0;
	g_plant_variables.u64_vout_verify_acc = 0;
	g_plant_variables.b_fault_flag = false;

	g_plant_variables.state = PLANT_STATE_CHECK_SIGNALS;
}

static void Plant_StateCheckSignals_Handler(void)
{


	g_plant_variables.b_vout_valid = (g_plant_variables.u32_vout_adc > VOUT_VOLTS_TO_ADC(VOUT_MIN_VOLTS));

	g_plant_variables.b_isense_valid = (abs((int32_t)g_plant_variables.u32_isense_adc - ISENSE_OFFSET_ADC)< ISENSE_AMPS_TO_ADC(ISENSE_MIN_AMPS));

	if(g_plant_variables.b_vout_valid&&g_plant_variables.b_isense_valid)//&& isense_valid)
	{
		g_plant_variables.u32_check_counter++;

		if(g_plant_variables.u32_check_counter > 1000)
		{
			g_plant_variables.state = PLANT_STATE_RAMP_UP;
		}
	}
	else
	{
		g_plant_variables.u32_check_counter = 0;
	}
}


static void Plant_StateRampUp_Handler(void)
{

}
static void Plant_StateVerifyDcop_Handler(void)
{

}
static void Plant_StateSfraInit_Handler(void)
{

}
static void Plant_StateSweep_Handler(void)
{

}
static void Plant_StateRampDown_Handler(void)
{

}
static void Plant_StateDone_Handler(void)
{

}
static void Plant_StateFault(void)
{

}
