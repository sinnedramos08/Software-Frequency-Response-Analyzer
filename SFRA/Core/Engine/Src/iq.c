/*
 * iq.c
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */


/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include "iq.h"

/* Private Macros ------------------------------------------------------------*/

/* Variables -----------------------------------------------------------------*/
iq_t g_iq;

/* Private Typedefs ----------------------------------------------------------*/

/* Public Functions ----------------------------------------------------------*/
void IQ_Init(void)
{
	IQ_Reset();
}

void IQ_Reset(void)
{
	g_iq.f_input_I_acc  = 0.0f;
	g_iq.f_input_Q_acc  = 0.0f;

	g_iq.f_output_I_acc = 0.0f;
	g_iq.f_output_Q_acc = 0.0f;

}

void IQ_Accumulate(float input,float output,float sin_ref,float cos_ref)
{
    g_iq.f_input_I_acc += input * sin_ref;
    g_iq.f_input_Q_acc += input * cos_ref;
    g_iq.f_output_I_acc += output * sin_ref;
    g_iq.f_output_Q_acc += output * cos_ref;

}

