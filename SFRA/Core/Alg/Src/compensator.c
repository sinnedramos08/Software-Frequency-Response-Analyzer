/*
 * compensator.c
 *
 *  Created on: Nov 27, 2024
 *      Author: EAntonio
 */
#include "compensator.h"

discrete_pi_controller_t	discretepi_iloop;
compensator_2p2z_t 			comp2p2z_iloop;
compensator_2p2z_t			comp2p2z_vloop;
filter_LPFData_t			Order1_LPF_filter;

void filter_Order1_LPF_Init(filter_LPFData_t * p_filter,float f_a1, float f_b0, float f_b1)
{
	memset(p_filter, 0, sizeof(*p_filter));
	/*
	 * Difference Equation:
	 * y[n] = -a1*y[n-1]+b0*x[n]+b1*x[n-1]
	 * From tool
	 * y[n] = a1*y[n-1]+b0*x[n]+b1*x[n-1]
	 * Defined value for a1 is negated from original
	 */
    p_filter->f_a1 = f_a1;
    p_filter->f_b0 = f_b0;
    p_filter->f_b1 = f_b1;
}

void compensator_2P2Z_Init(compensator_2p2z_t * p_compensator, float f_ref, float f_a1, float f_a2, float f_b0, float f_b1, float f_b2, float f_k)
{
	for(int i=0;i<3;i++)
	{
	    p_compensator->f_x[i] = 0.0f;
	    p_compensator->f_y[i] = 0.0f;
	}

	p_compensator->f_ref = f_ref;
	p_compensator->f_a1 = f_a1;
	p_compensator->f_a2 = f_a2;
	p_compensator->f_b0 = f_b0 * f_k;
	p_compensator->f_b1 = f_b1 * f_k;
	p_compensator->f_b2 = f_b2 * f_k;

}

void pi_Discrete_Controller_Init(discrete_pi_controller_t * p_pi_controller, float f_kp, float f_ki)
{
	p_pi_controller->f_ref = 0.0f;
	p_pi_controller->f_fdbk = 0.0f;
	p_pi_controller->f_error = 0.0f;

	p_pi_controller->f_kp = f_kp;
	p_pi_controller->f_ki = f_ki;
	p_pi_controller->f_integral = 0.0f;

	p_pi_controller->f_out = 0.0f;
}
