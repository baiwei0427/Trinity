#ifndef QUEUE_H
#define QUEUE_H

#define MAX_QUEUE_LEN 1024

struct Packet
{
	int (*okfn)(struct sk_buff *); //function pointer to reinject packets
	struct sk_buff *skb;                  //socket buffer pointer to packet   
};

struct PacketQueue
{
    struct Packet *packets; //Array of packets
    unsigned int head;         //Head offset
	unsigned int len;            //Current queue length
    spinlock_t queue_lock;//Lock for the PacketQueue
};

//Token bucket rate limiter
struct TBF
{
	struct PacketQueue* queuePtr;
	unsigned int rate; //Mbps
	unsigned int token;
	unsigned int bucket;
	ktime_t last_update_time;
};

//flags should be GFP_ATOMIC, GFP_KERNEL, etc
static void Init_PacketQueue(struct PacketQueue* q, int flags)
{
	q->packets=kmalloc(MAX_QUEUE_LEN*sizeof(struct Packet), flags);
	q->head=0;
	q->len=0;
	spin_lock_init(&q->queue_lock);
}

static void Free_PacketQueue(struct PacketQueue* q)
{
	kfree(q->packets);
}

//flags should be GFP_ATOMIC, GFP_KERNEL, etc
static void Init_TBF(struct TBF* tbf, unsigned int rate, unsigned int bucket, int flags)
{
	tbf->rate=rate;
	tbf->bucket=bucket;
	tbf->token=bucket;
	tbf->last_update_time=ktime_get();
	tbf->queuePtr=kmalloc(sizeof(struct PacketQueue),flags);
	Init_PacketQueue(tbf->queuePtr,flags);
}

static void Free_TBF(struct TBF* tbf)
{
	Free_PacketQueue(tbf->queuePtr);
	kfree(tbf->queuePtr);
}

//Enqueue packet. If it succeeds, return 1
static int Enqueue_PacketQueue(struct PacketQueue* q,struct sk_buff *skb,int (*okfn)(struct sk_buff *))
{
	//There is capacity to contain new packets
	if(q->len<MAX_QUEUE_LEN) 
    {
		//Index for new insert packet
		int queueIndex=(q->head+q->len)%MAX_QUEUE_LEN;
		q->packets[queueIndex].skb=skb;
		q->packets[queueIndex].okfn=okfn;
		q->len++;
		return 1;
	} 
    else
    {
		return 0;
	}
}

static int Dequeue_PacketQueue(struct PacketQueue* q)
{	
	if(q->len>0) 
    {
		q->len--;
		//Dequeue packet
		(q->packets[q->head].okfn)(q->packets[q->head].skb);
		//Reinject head packet of current queue
		q->head=(q->head+1)%MAX_QUEUE_LEN;
		return 1;
	} 
    else 
    {
		return 0;
	}
}

#endif