/*
 * sfra_strategies.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef STRATEGIES_INC_SFRA_STRATEGIES_H_
#define STRATEGIES_INC_SFRA_STRATEGIES_H_

typedef struct
{
    uint32_t timer_idx;
    void (*ISR)(void); 	// Pointer to a function

} sfra_strategy_t;

#endif /* STRATEGIES_INC_SFRA_STRATEGIES_H_ */
