/*
 * sfra_strategies.h
 *
 *  Created on: Jun 22, 2026
 *      Author: denni
 */

#ifndef STRATEGIES_INC_SFRA_STRATEGIES_H_
#define STRATEGIES_INC_SFRA_STRATEGIES_H_

#define TOGGLE_SWEEP_ILOOP_FS_100KHZ		(0U)
#define TOGGLE_SWEEP_VLOOP_FS_6KHZ			(0U)
#define	TOGGLE_SWEEP_PI_ILOOP_FS_100KHZ		(0U)

#define TOGGLE_SWEEP_IPLANT_FS_100KHZ		(0U)

#define TOGGLE_SWEEP_DIGFILTER_FS_100KHZ	(1U)

typedef struct
{

    void (*ISR)(void); 	// Pointer to a function
    uint32_t timer_idx;


} sfra_strategy_t;

#endif /* STRATEGIES_INC_SFRA_STRATEGIES_H_ */
