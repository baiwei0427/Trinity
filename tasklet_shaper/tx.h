#ifndef TX_H
#define TX_H

struct Packet
{
	int (*okfn)(struct sk_buff *); //function pointer to reinject packets
	struct sk_buff *skb;                  //socket buffer pointer to packet   
};

/* Token bucket rate limiter */
struct tbf_rl
{
	struct Packet* packets;//Array of packets
	unsigned int head;		//Head offset  of packets
	unsigned int len;			//Current queue length
	unsigned int max_len;	//Maximum queue length
	unsigned int rate;			//Mbps
	unsigned int tokens;	//Bytes
	unsigned int bucket;	//Bytes
	ktime_t last_update_time;
	spinlock_t spinlock;
};

struct tx_context
{
	struct tbf_rl* tbfPtr;
	struct tasklet_struct xmit_timeout;
	struct hrtimer timer;
	unsigned int delay_in_us;	//Timer granularity (us)
	spinlock_t spinlock;
	/*We can also add more information here. e.g. guarantee bandwidth, local IP and remote IP*/
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
	spin_lock_init(&(tbfPtr->spinlock));
	return 1;
}

/* Intialize TX context structure 
 * txPtr: pointer of TX context 
 * rate: rate you want to enforce 
 * bucket: bucket size (maximum burst size)
 * max_len: maximum number of packets to store in the queue
 * tasklet_func: tasklet function
 * timer_func: hrtimer callback function 
 * delay: timer granularity (us)
 * flags: GFP_ATOMIC, GFP_KERNEL, etc. This variable is very imporant! 
 * if initialization succeeds, the function returns 1. Otherwise, it returns 0.  
 */ 
static unsigned int Init_tx(
	struct tx_context* txPtr, 
	unsigned int rate, 
	unsigned int bucket, 
	unsigned int max_len,
	void (*tasklet_func)(unsigned long),
	enum hrtimer_restart  (*timer_func)(struct hrtimer *),
	unsigned int delay,
	int flags)
{
	if(txPtr==NULL)
		return 0;
	
	struct tbf_rl* tmp=kmalloc(sizeof(struct tbf_rl),flags);
	if(tmp==NULL)
		return 0;
	
	//Initialize token bucket rate limiter
	txPtr->tbfPtr=tmp;
	Init_tbf(txPtr->tbfPtr,rate,bucket,max_len,flags);
	//Initialize tasklet 
	tasklet_init(&(txPtr->xmit_timeout), *tasklet_func, (unsigned long)txPtr);
	//tasklet_init(&(txPtr->xmit_timeout), tasklet_func, (unsigned long)txPtr);
	//Initialize hrtimer
	hrtimer_init(&(txPtr->timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	txPtr->timer.function=timer_func;//&timer_func;
	txPtr->delay_in_us=delay;
	hrtimer_start(&(txPtr->timer), ktime_set( 0, delay*1000 ), HRTIMER_MODE_REL);
	//Initialize spinlock
	spin_lock_init(&(txPtr->spinlock));
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

/* Release resources of TX context */
static void Free_tx(struct tx_context* txPtr)
{
	hrtimer_cancel(&(txPtr->timer));
	tasklet_kill(&(txPtr->xmit_timeout));
	Free_tbf(txPtr->tbfPtr);
	kfree(txPtr->tbfPtr);
} 

/* Enqueue a packet to token bucket rate limiter. If it succeeds, return 1 */
static unsigned int Enqueue_tbf(struct tbf_rl* tbfPtr, struct sk_buff *skb, int (*okfn)(struct sk_buff *))
{
	//If there is still capacity to contain new packets
	if(tbfPtr->len<tbfPtr->max_len)
	{
		//Index for new insert packet
		unsigned int queueIndex=(tbfPtr->head+tbfPtr->len)%tbfPtr->max_len;
		tbfPtr->packets[queueIndex].skb=skb;
		tbfPtr->packets[queueIndex].okfn=okfn;
		tbfPtr->len++;
		return 1;
	}
	else
	{
		return 0;
	}
}

/* Dequeue a packet from token bucket rate limiter. If it succeeds, return 1 */
static unsigned int Dequeue_tbf(struct tbf_rl* tbfPtr)
{	
	if(tbfPtr->len>0) 
    {
		tbfPtr->len--;
		//Dequeue packet
		(tbfPtr->packets[tbfPtr->head].okfn)(tbfPtr->packets[tbfPtr->head].skb);
		//Reinject head packet of current queue
		tbfPtr->head=(tbfPtr->head+1)%(tbfPtr->max_len);
		return 1;
	} 
    else 
    {
		return 0;
	}
}

#endif