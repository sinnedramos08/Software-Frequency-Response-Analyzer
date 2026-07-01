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

// Coefficients:
#define USE_120VAC_380VDC_PLD	(1U)

#if USE_120VAC_380VDC_PLD
#define B0_I (+9.819966413717827)
#define B1_I (+0.7141609681875191)
#define B2_I (-9.105805445530306)
#define A1_I (+0.9905480027809929)
#define A2_I (-0.004831222144743453)

#define B0_V (+0.2476368622951489)
#define B1_V (+0.0006598251225833)
#define B2_V (-0.2469770371725656)
#define A1_V (+1.9800607277045987)
#define A2_V (-0.9800607277045986)

#define STRING_OPERATION	"120VAC 380VDC 500W"
#define STRING_ILOOP_FX		"ILOOP FX: 5KHZ"
#define STRING_ILOOP_PM		"ILOOP PM: 47DEG"
#define STRING_ILOOP_GM		"ILOOP GM: 8DB_40Static"

#define STRING_VLOOP_FX		"VLOOP FX: 7HZ"
#define STRING_VLOOP_PM		"VLOOP PM: 50DEG"
#define STRING_VLOOP_GM		"VLOOP PM: >77DB"
#endif


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

extern compensator_2p2z_t 		comp2p2z_iloop;
extern compensator_2p2z_t		comp2p2z_vloop;

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
