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
/* Variables -----------------------------------------------------------------*/
extern iq_t g_iq;
/* Function Prototypes --------------------------------------------------------*/
void IQ_Init(void);
void IQ_Reset(void);
void IQ_Accumulate(float input,float output,float sin_ref,float cos_ref);
#endif /* ENGINE_INC_IQ_H_ */
