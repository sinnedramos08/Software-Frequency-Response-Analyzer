/*
 * compensator_strategy.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef STRATEGIES_INC_COMPENSATOR_STRATEGY_H_
#define STRATEGIES_INC_COMPENSATOR_STRATEGY_H_

#include "sfra_engine.h"
#include "compensator.h"
#include "sfra_strategies.h"

extern const sfra_strategy_t compensator_strategy;


// Function Prototypes
void CompensatorStrategy_ISR(void);

#endif /* STRATEGIES_INC_COMPENSATOR_STRATEGY_H_ */
