/*
 * compensator.h
 *
 *  Created on: Nov 27, 2024
 *      Author: EAntonio
 */

#ifndef _COMPENSATOR_H_
#define _COMPENSATOR_H_

#include <stdint.h>
#include <string.h>
#include <float.h>


//Macros
#define MIN(a,b)   ((a)<(b) ? (b) : (a))
#define MAX(a,b)   ((a)>(b) ? (b) : (a))

//Custom structures

typedef struct
{

	float	f_a1;
	float	f_a2;
	float	f_b0;
	float	f_b1;
	float 	f_b2;
	float	f_y[3];
	float	f_x[3];
	float	f_min;
	float	f_max;
	float 	f_ref;
	float	f_fdbk;
	float	f_out;

	uint16_t	u16_adc_buffer[2];
}compensator_2p2z_t;

//Function prototypes
void compensator_2P2Z_Init(compensator_2p2z_t * p_compensator, float f_ref, float f_a1, float f_a2, float f_b0, float f_b1, float f_b2, float f_k);
inline static void compensator_2P2Z_Update(compensator_2p2z_t * p_compensator);

//__attribute__( ( section ( ".ccmram" ) ) )
inline static void compensator_2P2Z_Update(compensator_2p2z_t * p_compensator)
{
	float acc;

	//Calculate compensator terms
	acc = p_compensator->f_a1 * p_compensator->f_y[1];
	acc += p_compensator->f_a2 * p_compensator->f_y[2];
	acc += p_compensator->f_b0 * p_compensator->f_x[0];
	acc += p_compensator->f_b1 * p_compensator->f_x[1];
	acc += p_compensator->f_b2 * p_compensator->f_x[2];

	p_compensator->f_y[0] = acc;

	//Save and shift inputs
	p_compensator->f_x[2] = p_compensator->f_x[1];
	p_compensator->f_x[1] = p_compensator->f_x[0];
	p_compensator->f_x[0] = p_compensator->f_ref - p_compensator->f_fdbk;

	//Save and shift outputs
	p_compensator->f_y[2] = p_compensator->f_y[1];
	p_compensator->f_y[1] = p_compensator->f_y[0];

	p_compensator->f_out = acc;

}

#endif /* _COMPENSATOR_H_ */
