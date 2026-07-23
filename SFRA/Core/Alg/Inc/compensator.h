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
#define B0_I (+14.78605935461151)
#define B1_I (+0.7756673234382446)
#define B2_I (-14.010392031173268)
#define A1_I (+0.7670202702217362)
#define A2_I (0.225204638577282)

#define B0_V (+0.2476368622951489)
#define B1_V (+0.0006598251225833)
#define B2_V (-0.2469770371725656)
#define A1_V (+1.9800607277045987)
#define A2_V (-0.9800607277045986)

#define FILTER_B0 (0.1257621926f)
#define FILTER_B1 (0.1257621926f)
#define FILTER_A1 (0.7484756149f)

#define KP	(78.53f)
#define KI	(1.0447202192208f)

#define STRING_OPERATION	"120VAC 380VDC 500W"
#define STRING_ILOOP_FX		"ILOOP FX: 6KHZ"
#define STRING_ILOOP_PM		"ILOOP PM: 47DEG"
#define STRING_ILOOP_GM		"ILOOP GM: 46DBStatic"

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


typedef struct
{
	// Inputs
	float 		f_ref;
	float		f_fdbk;
	float		f_error;
	// PI Signals
	float		f_kp;
	float		f_ki;
	float		f_integral;
	// Limits
	float		f_min;
	float		f_max;
	// Anti Wind Up
	float		f_integral_limit;
	// Output
	float		f_out;
}discrete_pi_controller_t;

typedef struct
{
    float f_ref;
    float f_a1;
    float f_b0;
    float f_b1;
    float f_x[2];
    float f_y[2];
    float f_out;
} filter_LPFData_t;

extern discrete_pi_controller_t	discretepi_iloop;
extern compensator_2p2z_t 		comp2p2z_iloop;
extern compensator_2p2z_t		comp2p2z_vloop;
extern filter_LPFData_t			Order1_LPF_filter;

//Function prototypes
void compensator_2P2Z_Init(compensator_2p2z_t * p_compensator, float f_ref, float f_a1, float f_a2, float f_b0, float f_b1, float f_b2, float f_k);
inline static void compensator_2P2Z_Update(compensator_2p2z_t * p_compensator);

void pi_Discrete_Controller_Init(discrete_pi_controller_t * p_pi_controller, float f_kp, float f_ki);
static inline void pi_Discrete_Controller_Update(discrete_pi_controller_t * p_pi_controller);

void filter_Order1_LPF_Init(filter_LPFData_t * p_filter,float f_a1, float f_b0, float f_b1);
inline static void filter_Order1_LPF_Update(filter_LPFData_t * p_filter);

//__attribute__( ( section ( ".ccmram" ) ) )
inline static void filter_Order1_LPF_Update(filter_LPFData_t * p_filter)
{
	/*
	 * x0 = x[n] -> Current Input
	 * x1 = x[n-1] -> Previous Input
	 * y1 = y[n-1] -> Previous Output
	 */

	// Save Inputs:
    const float x0 = p_filter->f_ref;	// Current Input
    const float x1 = p_filter->f_x[0];	// Previous Input
    const float y1 = p_filter->f_y[0];	// Previous Output

    /*
     * Difference Equation:
	 * y[n] = -a1*y[n-1]+b0*x[n]+b1*x[n-1]
	 * From tool
	 * y[n] = a1*y[n-1]+b0*x[n]+b1*x[n-1]
	 * Defined value for a1 is negated from original
	 */
    const float y0 = (p_filter->f_a1 * y1) + (p_filter->f_b0 * x0) + (p_filter->f_b1 * x1);	// Compute Current Output

    // Save and Shift Inputs and Outputs
    p_filter->f_x[1] = x1;	// Second Old Value, ignored
    p_filter->f_x[0] = x0;	// Put Current Input to x[0] to save as previous
    p_filter->f_y[1] = y1;	// Second Old Value, ignored
    p_filter->f_y[0] = y0;	// Put current output to y[0] to save as previous

    // Output y0
    p_filter->f_out= y0;	// Current Output

}

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


static inline void pi_Discrete_Controller_Update(discrete_pi_controller_t * p_pi_controller)
{
	// Get the error
	p_pi_controller->f_error= p_pi_controller->f_ref-p_pi_controller->f_fdbk;

	// Compute Integral Output
	p_pi_controller->f_integral += p_pi_controller->f_ki * p_pi_controller->f_error;

    // Sum Proportional and Integral Terms
    p_pi_controller->f_out = (p_pi_controller->f_kp * p_pi_controller->f_error) + p_pi_controller->f_integral;

}


#endif /* _COMPENSATOR_H_ */
