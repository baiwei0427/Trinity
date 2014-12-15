#ifndef DUAL_TBF_H
#define DUAL_TBF_H

#include "network.h" 
#include "params.h"

/* Dual token bucket rate limiter */
struct dual_tbf_rl
{
	//Array(Queue) of packets for small flows
	struct Packet* small_packets;
	//Array(Queue) of packets for large flows
	struct Packet* large_packets;
	//Head offset  of packets of small flows
	unsigned int small_head;		
	//Head offset  of packets of large flows
	unsigned int large_head;		
	//Current queue length of small flows
	unsigned int small_len;		
	//Current queue length of large flows
	unsigned int large_len;		
	//Maximum queue length	of small flows
	unsigned int small_max_len;	
	//Maximum queue length	of large flows
	unsigned int large_max_len;
	//Bandwidth guarantee rate in Mbps
	unsigned int bg_rate;
	//Work conserving rate in Mbps
	unsigned int wc_rate;
	//tokens in bytes for bandwidth guarantee traffic 
	unsigned int bg_tokens;
	//tokens in bytes for work conserving traffic 
	unsigned int wc_tokens;
	//bucket size in bytes for bandwidth guarantee traffic
	unsigned int bg_bucket;
	//bucket size in bytes for work conserving traffic 
	unsigned int wc_bucket;
	//Last update timer of timer
	ktime_t last_update_time;
	//Lock for the small flow queue
	spinlock_t small_lock;
	//Lock for the large flow queue 
	spinlock_t large_lock;
};

/* Initialize dual token bucket rate limiter
 *  dual_tbfPtr: pointer of dual token bucker rate limiter
 *  bg_rate: bandwidth guarantee rate you want to enforce
 *  wc_rate: work conserving guarantee rate you want to enforce
 *  bg_bucket: bandwidth guarantee bucket size 
 *  wc_bucket: work conserving bucket size 
 *  small_max_len: maximum number of packets of small flows to store in the queue  
 *	 large_max_len: maximum number of packets of large flows to store in the queue 
 *  flags: GFP_ATOMIC, GFP_KERNEL, etc. This variable is very imporant! 
 *  if initialization succeeds, the function returns 1. Otherwise, it returns 0.  
 */
static unsigned int Init_dual_tbf(
	struct dual_tbf_rl* dual_tbfPtr, 
	unsigned int bg_rate,
	unsigned int wc_rate,
	unsigned int bg_bucket,
	unsigned int wc_bucket,
	unsigned int small_max_len,
	unsigned int large_max_len ,
	int flags)
{
	if(unlikely(dual_tbfPtr==NULL))
		return 0;
	
	struct Packet* small_tmp=kmalloc(small_max_len*sizeof(struct Packet),flags);
	struct Packet* large_tmp=kmalloc(large_max_len*sizeof(struct Packet),flags);
	if(unlikely(small_tmp==NULL||large_tmp==NULL))
		return 0;
	
	dual_tbfPtr->small_packets=small_tmp;
	dual_tbfPtr->large_packets=large_tmp;
	dual_tbfPtr->small_head=0;
	dual_tbfPtr->large_head=0;
	dual_tbfPtr->small_len=0;
	dual_tbfPtr->large_len=0;
	dual_tbfPtr->small_max_len=small_max_len;
	dual_tbfPtr->large_max_len=large_max_len;
	dual_tbfPtr->bg_rate=bg_rate;
	dual_tbfPtr->wc_rate=wc_rate;
	dual_tbfPtr->bg_bucket=bg_bucket;
	dual_tbfPtr->wc_bucket=wc_bucket;
	dual_tbfPtr->bg_tokens=bg_bucket;
	dual_tbfPtr->wc_tokens=wc_bucket;
	dual_tbfPtr->last_update_time=ktime_get(); //time of now
	spin_lock_init(&(dual_tbfPtr->small_lock));
	spin_lock_init(&(dual_tbfPtr->large_lock));
	return 1;
}

/* Release resources of token bucket rate limiter */
static void Free_dual_tbf(struct dual_tbf_rl* dual_tbfPtr)
{
	if(dual_tbfPtr!=NULL)
	{
		kfree(dual_tbfPtr->small_packets);
		kfree(dual_tbfPtr->large_packets);
	}
}

/* Enqueue a packet of small/large (is_small) flows to dual token bucket rate limiter. If it succeeds, return 1 */
static unsigned int Enqueue_dual_tbf(struct dual_tbf_rl* dual_tbfPtr, struct sk_buff *skb, int (*okfn)(struct sk_buff *), unsigned int is_small)
{
	//Small flows
	if(is_small==1)
	{
		//If there is still capacity to contain new packets
		if(dual_tbfPtr->small_len<dual_tbfPtr->small_max_len)
		{
			//Index for new insert packet
			unsigned int queueIndex=(dual_tbfPtr->small_head+dual_tbfPtr->small_len)%dual_tbfPtr->small_max_len;
			dual_tbfPtr->small_packets[queueIndex].skb=skb;
			dual_tbfPtr->small_packets[queueIndex].okfn=okfn;
			dual_tbfPtr->small_len++;
			return 1;
		}
		else
		{
			return 0;
		}
	}
	//Large flows
	else
	{
		//If there is still capacity to contain new packets
		if(dual_tbfPtr->large_len<dual_tbfPtr->large_max_len)
		{
			//Index for new insert packet
			unsigned int queueIndex=(dual_tbfPtr->large_head+dual_tbfPtr->large_len)%dual_tbfPtr->large_max_len;
			dual_tbfPtr->large_packets[queueIndex].skb=skb;
			dual_tbfPtr->large_packets[queueIndex].okfn=okfn;
			dual_tbfPtr->large_len++;
			return 1;
		}
		else
		{
			return 0;
		}
	}
}

/* Dequeue a packet of small/large (is_small) flows from dual token bucket rate limiter. If it succeeds, return 1 */
static unsigned int Dequeue_dual_tbf(struct dual_tbf_rl* dual_tbfPtr, unsigned int is_small, unsigned int is_bg)
{
	//Short flows. Short flows can only use tokens of bandwidth guarantee traffic (we don't care about is_bg here)
	if(is_small==1)
	{
		if(dual_tbfPtr->small_len>0)
		{
			dual_tbfPtr->small_len--;
			//Modify packet DSCP and enable ECN
			enable_ecn_dscp(dual_tbfPtr->small_packets[dual_tbfPtr->small_head].skb,BANDWIDTH_GUARANTEE_DSCP);
			//Dequeue packet
			(dual_tbfPtr->small_packets[dual_tbfPtr->small_head].okfn)(dual_tbfPtr->small_packets[dual_tbfPtr->small_head].skb);
			//Reinject head packet of current queue
			dual_tbfPtr->small_head=(dual_tbfPtr->small_head+1)%(dual_tbfPtr->small_max_len);
			return 1;
		}
		else
		{
			return 0;
		}
	}
	//Large flows. Large flows can use tokens of both bandwidth guarantee and work conserving traffic (see is_bg)
	else
	{
		if(dual_tbfPtr->large_len>0)
		{
			dual_tbfPtr->large_len--;
			//If we use tokens of bandwidth guarantee traffic 
			if(is_bg==1)
			{
				enable_ecn_dscp(dual_tbfPtr->large_packets[dual_tbfPtr->large_head].skb,BANDWIDTH_GUARANTEE_DSCP);
			}
			//If we use tokens of work conserving traffic 
			else
			{
				enable_ecn_dscp(dual_tbfPtr->large_packets[dual_tbfPtr->large_head].skb,WORK_CONSERVING_DSCP);
			}			
			//Dequeue packet
			(dual_tbfPtr->large_packets[dual_tbfPtr->large_head].okfn)(dual_tbfPtr->large_packets[dual_tbfPtr->large_head].skb);
			//Reinject head packet of current queue
			dual_tbfPtr->large_head=(dual_tbfPtr->large_head+1)%(dual_tbfPtr->large_max_len);
			return 1;
		}
		else
		{
			return 0;
		}
	}
}
#endif