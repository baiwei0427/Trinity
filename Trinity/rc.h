#ifndef RC_H
#define RC_H

#include "params.h"

unsigned int elasticswitch_rc(unsigned int current_rate, unsigned int target_rate)
{
	unsigned int increase=0;
	if(unlikely(target_rate<=current_rate))
		return current_rate;
	
	increase=max((target_rate-current_rate)*ALPHA/1000,MINIMUM_RATE_INCREASE);
	return current_rate+increase;
}

#endif