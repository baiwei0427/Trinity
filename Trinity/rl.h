#ifdef RL_H
#define RL_H

/* Structure of sk_buff and the function pointer to reinject packets */
struct Packet
{
	int (*okfn)(struct sk_buff *); 
	struct sk_buff *skb;                  
};

/* Token bucket rate limiter */
struct tbf_rl
{
	//Array of packets
	struct Packet* packets;
	//Head offset  of packets
	unsigned int head;		
	//Current queue length
	unsigned int len;		
	//Maximum queue length	
	unsigned int max_len;	
	//rate in Mbps
	unsigned int rate;			
	//tokens in bytes
	unsigned int tokens;
	//bucket size in bytes	
	unsigned int bucket;	
	//Last update timer of timer
	ktime_t last_update_time;
	//Lock for this structure
	spinlock_t rl_lock;
};

/* Initialize token bucket rate limiter
 *  tbfPtr: pointer of token bucker rate limiter
 *  rate: rate you want to enforce
 *  bucket: bucket size (maximum burst size)
 *	 max_len: maximum number of packets to store in the queue 
 *  flags: GFP_ATOMIC, GFP_KERNEL, etc. This variable is very imporant! 
 *  if initialization succeeds, the function returns 1. Otherwise, it returns 0.  
 */
static unsigned int Init_tbf(struct tbf_rl* tbfPtr, unsigned int rate, unsigned int bucket, unsigned int max_len, int flags)
{
	if(tbfPtr==NULL)
		return 0;
	
	struct Packet* tmp=kmalloc(max_len*sizeof(struct Packet),flags);
	if(tmp==NULL)
		return 0;
	
	tbfPtr->packets=tmp;
	tbfPtr->head=0;
	tbfPtr->len=0;
	tbfPtr->max_len=max_len;
	tbfPtr->rate=rate;
	tbfPtr->bucket=bucket;
	tbfPtr->tokens=bucket;
	tbfPtr->last_update_time=ktime_get(); //time of now
	spin_lock_init(&(tbfPtr->rl_lock));
	return 1;
}

/* Release resources of token bucket rate limiter */
static void Free_tbf(struct tbf_rl* tbfPtr)
{
	if(tbfPtr!=NULL)
	{
		kfree(tbfPtr->packets);
	}
}

#endif