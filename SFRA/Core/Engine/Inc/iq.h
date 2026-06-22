/*
 * iq.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef ENGINE_INC_IQ_H_
#define ENGINE_INC_IQ_H_
/* Includes ------------------------------------------------------------------*/

/* Macros --------------------------------------------------------------------*/

/* Typedefs ------------------------------------------------------------------*/
typedef struct{
    /* Measurement */
    float f_input_I_acc;
    float f_input_Q_acc;

    float f_output_I_acc;
    float f_output_Q_acc;

}iq_t;

typedef struct{
	float f_input_mag;
	float f_input_amp;
	float f_input_phase;

	float f_output_mag;
	float f_output_amp;
	float f_output_phase;
}iq_result_t;
/* Variables -----------------------------------------------------------------*/
extern iq_t g_iq;
/* Function Prototypes --------------------------------------------------------*/
void IQ_Init(void);
void IQ_Reset(void);
void IQ_Accumulate(float input,float output,float sin_ref,float cos_ref);
void IQ_Calculate(uint32_t measure_samples, iq_result_t *result);
#endif /* ENGINE_INC_IQ_H_ */
