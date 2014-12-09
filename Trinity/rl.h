#ifndef RL_H
#define RL_H

#include "params.h"
#include "tx.h"

static void xmit_tasklet(unsigned long data)
{
	struct pair_tx_context  *pair_txPtr=(struct pair_tx_context*)data;
	unsigned int skb_len;
	ktime_t now=ktime_get();
	
	pair_txPtr->rateLimiter.tokens+=ktime_us_delta(now,pair_txPtr->rateLimiter.last_update_time)*(pair_txPtr->rateLimiter.rate)/8;
	pair_txPtr->rateLimiter.last_update_time=now;
	
	while(1)
	{
		if(pair_txPtr->rateLimiter.len>0)
		{
			skb_len=pair_txPtr->rateLimiter.packets[pair_txPtr->rateLimiter.head].skb->len;
			if(skb_len<=pair_txPtr->rateLimiter.tokens)
			{
				pair_txPtr->rateLimiter.tokens-=skb_len;
				spin_lock_bh(&(pair_txPtr->rateLimiter.rl_lock));
				Dequeue_tbf(&(pair_txPtr->rateLimiter));
				spin_unlock_bh(&(pair_txPtr->rateLimiter.rl_lock));
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	
	if(pair_txPtr->rateLimiter.tokens>=pair_txPtr->rateLimiter.bucket&&pair_txPtr->rateLimiter.len==0)
		pair_txPtr->rateLimiter.tokens=pair_txPtr->rateLimiter.bucket;	
		
	//Start time again
	hrtimer_start(&(pair_txPtr->timer), ktime_set( 0, pair_txPtr-> timer_interval*1000), HRTIMER_MODE_REL);
}

/* HARDIRQ timeout */
static enum hrtimer_restart my_hrtimer_callback( struct hrtimer *timer )
{
	/* schedue xmit tasklet to go into softirq context */
	struct pair_tx_context  *pair_txPtr= container_of(timer, struct pair_tx_context, timer);
	tasklet_schedule(&(pair_txPtr->xmit_timeout));
	return HRTIMER_NORESTART;
}

#endif