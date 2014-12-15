#ifndef	TX_H
#define	TX_H

#include "tbf.h"
#include "dual_tbf.h"

//Define the structure of per VM-to-VM pair TX context
struct pair_tx_context 
{
	unsigned int local_ip;			
	unsigned int remote_ip;			
	//Bandwidth guarantee bandwidth (Mbps)
	unsigned int guarantee_bw;
	//Structure of link list
	struct list_head list;
#ifdef TRINITY
	//Dual token bucket rate limiter
	struct dual_tbf_rl rateLimiter;
#else
	//Token bucket rate limiter
	struct tbf_rl rateLimiter;
#endif
	//We use tasklet(softirq) rather than hardirq context of hrtimer to implement rate limiting
	struct tasklet_struct xmit_timeout;
	//Timer for rate limiting
	struct hrtimer timer;
	//Timer granularity (us)
	unsigned int timer_interval;	
	//spinlock for this structure. Use this lock when we want to modify any field in this structure
	spinlock_t pair_lock;
};

//Define the structure of per endpoint (VM) TX context
struct endpoint_tx_context
{
	unsigned local_ip;
	//The total guarantee bandwidth for the endpoint
	unsigned int guarantee_bw;
	//The head point of link list for VM-to-VM pair TX context 
	struct list_head pair_list;
	//Structure of link list
	struct list_head list;
	//The  number of VM-to-VM pairs for this endpoint
	unsigned int pair_num; 
	//spinlock for this structure. Use this lock when we want to modify any field in this structure
	spinlock_t endpoint_lock;
};

//Define the structure for (per NIC/physical server) TX context
struct tx_context
{
	//The head point of link list for endpoint TX context
	struct list_head endpoint_list;
	//The number of endpoints for this physical server
	unsigned int endpoint_num;
	//spinlock for this structure. Use this lock when we want to modify any field in this structure
	spinlock_t tx_lock;
};

static void print_pair_tx_context(struct pair_tx_context* ptr)
{		
	char local_ip[16]={0};           
	char remote_ip[16]={0};
	snprintf(local_ip, 16, "%pI4", &(ptr->local_ip));
	snprintf(remote_ip, 16, "%pI4", &(ptr->remote_ip));
#ifdef TRINITY
	printk(KERN_INFO "Trinity TX: %s to %s, bandwidth guarantee rate is %u Mbps, work conserving rate is %u Mbps\n",local_ip,remote_ip,ptr->rateLimiter.bg_rate,ptr->rateLimiter.wc_rate);
#else
	printk(KERN_INFO "ElasticSwitch TX: %s to %s, bandwidth guarantee rate is %u Mbps, actual rate is %u Mbps\n",local_ip,remote_ip,ptr->guarantee_bw,ptr->rateLimiter.rate);
#endif
}

static void print_endpoint_tx_context(struct endpoint_tx_context* ptr)
{		
	char local_ip[16]={0};      
	snprintf(local_ip, 16, "%pI4", &(ptr->local_ip));
	printk(KERN_INFO "TX: %s, endpoint bandwidth guarantee is %u Mbps\n",local_ip,ptr->guarantee_bw);
}

static void print_tx_context(struct tx_context* ptr)
{		
	struct endpoint_tx_context* endpoint_ptr=NULL; 
	struct pair_tx_context* pair_ptr=NULL;
	unsigned int pair_num=0;
	
	list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
	{
		print_endpoint_tx_context(endpoint_ptr);
		list_for_each_entry(pair_ptr,&(endpoint_ptr->pair_list),list)
		{
			pair_num++;
			print_pair_tx_context(pair_ptr);
		}
	}	
	printk(KERN_INFO "TX: There are %u endpoint TX entries and %u pair TX entries in total\n",ptr->endpoint_num,pair_num);
}

/* Initialize TX context and return 1 if it succeeds. */
static unsigned int Init_tx_context(struct tx_context* ptr)
{
	if(likely(ptr!=NULL))
	{
		ptr->endpoint_num=0;
		spin_lock_init(&(ptr->tx_lock));
		INIT_LIST_HEAD(&(ptr->endpoint_list));
		return 1;
	}
	else
	{
		return 0;
	}
}

/* Initialize endpoint TX context and return 1 if it succeeds. */
static unsigned int Init_endpoint_tx_context(struct endpoint_tx_context* ptr, unsigned int ip, unsigned int bw)
{
	if(likely(ptr!=NULL))
	{
		ptr->pair_num=0;
		ptr->local_ip=ip;
		ptr->guarantee_bw=bw;
		spin_lock_init(&(ptr->endpoint_lock));
		INIT_LIST_HEAD(&(ptr->pair_list));
		INIT_LIST_HEAD(&(ptr->list));
		return 1;
	}
	else
	{
		return 0;
	}
}

/*Initialize pair TX context  
 * ptr: pointer of pair TX context
 * local_ip: VM IP address on the local server 
 * remote_ip: IP address on the remote server
 * bw: guarantee bandwidth of this VM pair 
 * bucket: bucket size (maximum burst size) in bytes
 * max_len: maximum number of packets (sk_buff) to store in the queue
 * tasklet_func: tasklet function pointer
 * timer_func: hrtimer callback function pointer 
 * delay: timer granularity in us
 * flags: GFP_ATOMIC, GFP_KERNEL, etc. This variable is very imporant! 
 * if initialization succeeds, the function returns 1. Otherwise, it returns 0.  
 */ 
static unsigned int Init_pair_tx_context(
	struct pair_tx_context* ptr, 
	unsigned int local_ip, 
	unsigned int remote_ip, 
	unsigned int bw, 
	unsigned int bucket, 
	unsigned int max_len,
	void (*tasklet_func)(unsigned long),
	enum hrtimer_restart  (*timer_func)(struct hrtimer *),
	unsigned int delay,
	int flags)
{
	if(unlikely(ptr==NULL))
		return 0;
	
	ptr->local_ip=local_ip;
	ptr->remote_ip=remote_ip;	
	ptr->guarantee_bw=bw;
	INIT_LIST_HEAD(&(ptr->list));

#ifdef TRINITY
	if(unlikely(Init_dual_tbf(&(ptr->rateLimiter),bw,MINIMUM_RATE,bucket,bucket,max_len,max_len,flags)==0))
	{
		return 0;
	}
#else
	//Initialize rate limiter of the pair TX context. We set rate to guarantee_bw initially. 
	if(unlikely(Init_tbf(&(ptr->rateLimiter),bw,bucket,max_len,flags)==0))
	{
		return 0;
	}
#endif
	//Initialize tasklet 
	tasklet_init(&(ptr->xmit_timeout), *tasklet_func, (unsigned long)ptr);
	//Initialize hrtimer
	hrtimer_init(&(ptr->timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ptr->timer.function=timer_func;
	ptr->timer_interval=delay;
	hrtimer_start(&(ptr->timer), ktime_set( 0, delay*1000 ), HRTIMER_MODE_REL);
	//Initialize spinlock
	spin_lock_init(&(ptr->pair_lock));
	return 1;
}

/* Release resources of pair TX context */
static void Free_pair_tx_context(struct pair_tx_context* ptr)
{
	if(likely(ptr!=NULL))
	{
		hrtimer_cancel(&(ptr->timer));
		tasklet_kill(&(ptr->xmit_timeout));
	#ifdef TRINITY
		Free_dual_tbf(&(ptr->rateLimiter));
	#else
		Free_tbf(&(ptr->rateLimiter));
	#endif
	}
} 

//Insert a new pair TX context to an endpoint TX context
static void Insert_tx_pair_endpoint(struct pair_tx_context* pair_ptr, struct endpoint_tx_context* endpoint_ptr)
{
	if(likely(pair_ptr!=NULL&&endpoint_ptr!=NULL))
	{
		unsigned long flags;		
		spin_lock_irqsave(&endpoint_ptr->endpoint_lock,flags);
		list_add_tail(&(pair_ptr->list),&(endpoint_ptr->pair_list));
		endpoint_ptr->pair_num++;
		spin_unlock_irqrestore(&endpoint_ptr->endpoint_lock,flags);
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
}

//Insert a new pair TX context to a TX context
static void Insert_tx_pair(struct pair_tx_context* pair_ptr, struct tx_context* ptr)
{
	if(likely(pair_ptr!=NULL&&ptr!=NULL))
	{
		struct endpoint_tx_context* endpoint_ptr=NULL; 
		list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
		{
			//If the local_ip is matched
			if(endpoint_ptr->local_ip==pair_ptr->local_ip)
			{
				//Insert the pair TX context to corresponding endpoint TX context
				Insert_tx_pair_endpoint(pair_ptr, endpoint_ptr);
				return;
			}
		}
		printk(KERN_INFO "Can not find matching endpoint TX information entry\n");
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
} 

//Insert a new endpoint TX context to a TX context
static void Insert_tx_endpoint(struct endpoint_tx_context* endpoint_ptr, struct tx_context* ptr)
{
	if(likely(endpoint_ptr!=NULL&&ptr!=NULL))
	{
		unsigned long flags;		
		spin_lock_irqsave(&ptr->tx_lock,flags);
		list_add_tail(&(endpoint_ptr->list),&(ptr->endpoint_list));
		ptr->endpoint_num++;	
		spin_unlock_irqrestore(&ptr->tx_lock,flags);
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
}

static struct pair_tx_context* Search_tx_pair(struct tx_context* ptr, unsigned int local_ip, unsigned int remote_ip)
{
	if(likely(ptr!=NULL))
	{
		struct pair_tx_context* pair_ptr=NULL;
		struct endpoint_tx_context* endpoint_ptr=NULL; 
	
		list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
		{
			if(endpoint_ptr->local_ip==local_ip)
			{
				list_for_each_entry(pair_ptr,&(endpoint_ptr->pair_list),list)
				{
					if(pair_ptr->remote_ip==remote_ip)
					{
						return pair_ptr;
					}
				}
			}
		}
		//By default, we cannot find the corresponding entry
		return NULL;
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return NULL;
	}
}

//Clear all endpoint or pair TX information entries 
static void Empty_tx_context(struct tx_context* ptr)
{
	if(likely(ptr!=NULL))
	{
		unsigned long flags;		
		struct endpoint_tx_context* endpoint_ptr=NULL; 
		struct endpoint_tx_context* endpoint_next=NULL; 
		struct pair_tx_context* pair_ptr=NULL; 
		struct pair_tx_context* pair_next=NULL; 
	
		spin_lock_irqsave(&ptr->tx_lock,flags);
		list_for_each_entry_safe(endpoint_ptr, endpoint_next, &(ptr->endpoint_list), list)
		{
			print_endpoint_tx_context(endpoint_ptr);
			list_for_each_entry_safe(pair_ptr, pair_next, &(endpoint_ptr->pair_list), list)
			{
				print_pair_tx_context(pair_ptr);	
				list_del(&(pair_ptr->list));
				Free_pair_tx_context(pair_ptr);
				kfree(pair_ptr);
			}
			list_del(&(endpoint_ptr->list));
			kfree(endpoint_ptr);
		}	
		spin_unlock_irqrestore(&ptr->tx_lock,flags);
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
}

//Delete a pair TX context (local_ip, remote_ip) from an endpoint TX context
//Return 1 if delete succeeds
static unsigned int Delete_tx_pair_endpoint(unsigned int local_ip, unsigned int remote_ip, struct endpoint_tx_context* endpoint_ptr)
{
	unsigned long flags;		
	struct pair_tx_context* pair_ptr=NULL; 
	struct pair_tx_context* pair_next=NULL; 
	
	if(unlikely(endpoint_ptr==NULL))
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return 0;
	}
	
	//No TX pair context in this endpoint
	if(unlikely(endpoint_ptr->pair_num==0))
		return 0;
		
	list_for_each_entry_safe(pair_ptr, pair_next, &(endpoint_ptr->pair_list), list)
	{
		//print_pair_tx_context(pair_ptr);	
		//If we find corresponding pair TX entry
		if(pair_ptr->local_ip==local_ip&&pair_ptr->remote_ip==remote_ip)
		{
			spin_lock_irqsave(&endpoint_ptr->endpoint_lock,flags);
			list_del(&(pair_ptr->list));
			Free_pair_tx_context(pair_ptr);
			kfree(pair_ptr);
			endpoint_ptr->pair_num--;
			spin_unlock_irqrestore(&endpoint_ptr->endpoint_lock,flags);
			return 1;
		}
	}
	printk(KERN_INFO "Can not delete corresponding pair TX information entry\n");
	return 0;
}

//Delete a pair TX context (local_ip, remote_ip) from a TX context
//Return 1 if delete succeeds
static unsigned int Delete_tx_pair(unsigned int local_ip, unsigned int remote_ip, struct tx_context* ptr)
{
	struct endpoint_tx_context* endpoint_ptr=NULL; 
	
	if(unlikely(ptr==NULL))
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return 0;
	}
	
	//No TX endpoint context 
	if(unlikely(ptr->endpoint_num==0))
		return 0;
	
	list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
	{	
		//If the local_ip is matched
		if(endpoint_ptr->local_ip==local_ip)
		{
			//Delete the pair TX context from the corresponding endpoint TX context
			return Delete_tx_pair_endpoint(local_ip, remote_ip, endpoint_ptr);
		}
	}
	return 0;
}

//Delete a endpoint TX context (local_ip) from a TX context
//Return 1 if delete succeeds
//Note that all related pair TX entries will also be deleted!
static unsigned int Delete_tx_endpoint(unsigned int local_ip, struct tx_context* ptr)
{
	unsigned long flags;		
	struct endpoint_tx_context* endpoint_ptr=NULL; 
	struct endpoint_tx_context* endpoint_next=NULL; 
	struct pair_tx_context* pair_ptr=NULL; 
	struct pair_tx_context* pair_next=NULL; 
	
	if(unlikely(ptr==NULL))
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return 0;
	}
	
	//No TX endpoint context 
	if(unlikely(ptr->endpoint_num==0))
		return 0;
	
	list_for_each_entry_safe(endpoint_ptr, endpoint_next, &(ptr->endpoint_list), list)
    {
		//If we find corresponding endpoint TX entry
		if(endpoint_ptr->local_ip==local_ip)
		{
			spin_lock_irqsave(&ptr->tx_lock,flags);
			//Delete all pair TX entries related to this endpoint TX entry
			list_for_each_entry_safe(pair_ptr, pair_next, &(endpoint_ptr->pair_list), list)
			{
				print_pair_tx_context(pair_ptr);	
				list_del(&(pair_ptr->list));
				Free_pair_tx_context(pair_ptr);
				kfree(pair_ptr);
			}
			list_del(&(endpoint_ptr->list));
			kfree(endpoint_ptr);
			ptr->endpoint_num--;
			spin_unlock_irqrestore(&ptr->tx_lock,flags);
			return 1;
		}
	}		
	return 0;
}
#endif