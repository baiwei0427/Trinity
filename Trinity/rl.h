#ifndef RL_H
#define RL_H

#include "params.h"
#include "tx.h"

static void xmit_tasklet(unsigned long data)
{
	struct pair_tx_context  *pair_txPtr=(struct pair_tx_context*)data;
	unsigned int skb_len;
	ktime_t now=ktime_get();

#ifdef TRINITY
	pair_txPtr->rateLimiter.bg_tokens+=ktime_us_delta(now,pair_txPtr->rateLimiter.last_update_time)*(pair_txPtr->rateLimiter.bg_rate)/8;
	pair_txPtr->rateLimiter.wc_tokens+=ktime_us_delta(now,pair_txPtr->rateLimiter.last_update_time)*(pair_txPtr->rateLimiter.wc_rate)/8;
#else
	pair_txPtr->rateLimiter.tokens+=ktime_us_delta(now,pair_txPtr->rateLimiter.last_update_time)*(pair_txPtr->rateLimiter.rate)/8;
#endif	
	pair_txPtr->rateLimiter.last_update_time=now;

#ifdef TRINITY
	//We first dequeue packet of short flows only using tokens of bandwidth guarantee traffic 
	while(1)
	{
		if(pair_txPtr->rateLimiter.small_len>0)
		{
			skb_len=pair_txPtr->rateLimiter.small_packets[pair_txPtr->rateLimiter.small_head].skb->len;
			if(skb_len<=pair_txPtr->rateLimiter.bg_tokens)
			{
				pair_txPtr->rateLimiter.bg_tokens-=skb_len;
				spin_lock_bh(&(pair_txPtr->rateLimiter.small_lock));
				Dequeue_dual_tbf(&(pair_txPtr->rateLimiter),1,1);
				spin_unlock_bh(&(pair_txPtr->rateLimiter.small_lock));
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
	//Then we dequeue packets of large flows using both bandwidth and work conserving traffic
	while(1)
	{
		if(pair_txPtr->rateLimiter.large_len>0)
		{
			skb_len=pair_txPtr->rateLimiter.large_packets[pair_txPtr->rateLimiter.large_head].skb->len;
			if(skb_len<=pair_txPtr->rateLimiter.bg_tokens)
			{
				pair_txPtr->rateLimiter.bg_tokens-=skb_len;
				spin_lock_bh(&(pair_txPtr->rateLimiter.large_lock));
				Dequeue_dual_tbf(&(pair_txPtr->rateLimiter),0,1);
				spin_unlock_bh(&(pair_txPtr->rateLimiter.large_lock));
			}
			else if(skb_len<=pair_txPtr->rateLimiter.wc_tokens)
			{
				pair_txPtr->rateLimiter.wc_tokens-=skb_len;
				spin_lock_bh(&(pair_txPtr->rateLimiter.large_lock));
				Dequeue_dual_tbf(&(pair_txPtr->rateLimiter),0,0);
				spin_unlock_bh(&(pair_txPtr->rateLimiter.large_lock));
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
	if(pair_txPtr->rateLimiter.bg_tokens>pair_txPtr->rateLimiter.bg_bucket&&pair_txPtr->rateLimiter.small_len==0&&pair_txPtr->rateLimiter.large_len==0)
		pair_txPtr->rateLimiter.bg_tokens=pair_txPtr->rateLimiter.bg_bucket;	
	if(pair_txPtr->rateLimiter.wc_tokens>pair_txPtr->rateLimiter.wc_bucket&&pair_txPtr->rateLimiter.large_len==0)
		pair_txPtr->rateLimiter.wc_tokens=pair_txPtr->rateLimiter.wc_bucket;			
#else
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
	if(pair_txPtr->rateLimiter.tokens>pair_txPtr->rateLimiter.bucket&&pair_txPtr->rateLimiter.len==0)
		pair_txPtr->rateLimiter.tokens=pair_txPtr->rateLimiter.bucket;	
#endif

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