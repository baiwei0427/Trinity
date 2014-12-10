#ifndef	RX_H
#define	RX_H

#include <linux/list.h>
#include <linux/time.h>  

//Define the structure of RX statistics
struct rx_stats
{
	unsigned long rx_bytes;
	unsigned long rx_ecn_bytes;
};

//Define the structure of per VM-to-VM pair RX context
struct pair_rx_context 
{
	unsigned int local_ip;			
	unsigned int remote_ip;			
	//Statistic information
	struct rx_stats stats;
	//Bandwidth guarantee rate (Mbps)
	unsigned int rate;
	ktime_t start_update_time;
	ktime_t last_update_time;
	//Structure of link list
	struct list_head list;
	//spinlock for this structure. Use this lock when we want to modify any field in this structure
	spinlock_t pair_lock;
};

//Define the structure of per endpoint (VM) RX context
struct endpoint_rx_context
{
	unsigned int local_ip;
	//The total guarantee bandwidth for the endpoint
	unsigned int guarantee_bw;
	//The head point of link list for VM-to-VM pair RX context 
	struct list_head pair_list;
	//Structure of link list
	struct list_head list;
	//The  number of VM-to-VM pairs for this endpoint
	unsigned int pair_num; 
	//spinlock for this structure. Use this lock when we want to modify any field in this structure
	spinlock_t endpoint_lock;
};

//Define the structure for (per NIC/physical server) RX context
struct rx_context
{
	//The head point of link list for endpoint RX context
	struct list_head endpoint_list;
	//The number of endpoints for this physical server
	unsigned int endpoint_num;
	//spinlock for this structure. Use this lock when we want to modify any field in this structure
	spinlock_t rx_lock;
};

static void print_pair_rx_context(struct pair_rx_context* ptr)
{
	char local_ip[16]={0};           
	char remote_ip[16]={0};
	unsigned int throughput=0;	//unit: Mbps
	unsigned int fraction=0; //ECN marking fraction
	unsigned long interval=(unsigned long)ktime_us_delta(ptr->last_update_time,ptr->start_update_time);
	
	if(ptr->stats.rx_bytes>0)
	{
		fraction=ptr->stats.rx_ecn_bytes*100/ptr->stats.rx_bytes;
	}
	if(interval>0)
	{
		throughput=ptr->stats.rx_bytes*8/interval;
	}
	snprintf(local_ip, 16, "%pI4", &(ptr->local_ip));
	snprintf(remote_ip, 16, "%pI4", &(ptr->remote_ip));
	printk(KERN_INFO "RX: %s to %s, ECN marking fraction %u%%, bandwidth guarantee is %u Mbps, actual incoming throughput is %u Mbps\n",remote_ip,local_ip,fraction,ptr->rate,throughput);
}

static void print_endpoint_rx_context(struct endpoint_rx_context* ptr)
{
	char local_ip[16]={0};      
	snprintf(local_ip, 16, "%pI4", &(ptr->local_ip));
	printk(KERN_INFO "RX: %s, endpoint bandwidth guarantee is %u Mbps\n",local_ip,ptr->guarantee_bw);
}

static void print_rx_context(struct rx_context* ptr)
{
	struct endpoint_rx_context* endpoint_ptr=NULL; 
	struct pair_rx_context* pair_ptr=NULL;
	unsigned int pair_num=0;
	
	list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
	{
		print_endpoint_rx_context(endpoint_ptr);
		list_for_each_entry(pair_ptr,&(endpoint_ptr->pair_list),list)
		{
			pair_num++;
			print_pair_rx_context(pair_ptr);
		}
	}	
	printk(KERN_INFO "RX: There are %u endpoint RX entries and %u pair RX entries in total\n",ptr->endpoint_num,pair_num);
}

/* Initialize RX context and return 1 if it succeeds */
static unsigned int Init_rx_context(struct rx_context* ptr)
{
	if(likely(ptr!=NULL))
	{
		ptr->endpoint_num=0;
		spin_lock_init(&(ptr->rx_lock));
		INIT_LIST_HEAD(&(ptr->endpoint_list));
		return 1;
	}
	else
	{
		return 0;
	}
}

/* Initialize endpoint RX context and return 1 if it succeeds */
static unsigned int Init_endpoint_rx_context(struct endpoint_rx_context* ptr, unsigned int ip, unsigned int bw)
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

/* Initialize pair RX context and return 1 if it succeeds */
static unsigned int Init_pair_rx_context(struct pair_rx_context* ptr, unsigned int local_ip, unsigned int remote_ip, unsigned int bw)
{
	if(likely(ptr!=NULL))
	{
		ktime_t now=ktime_get();
		ptr->local_ip=local_ip;
		ptr->remote_ip=remote_ip;
		ptr->rate=bw;
		ptr->stats.rx_bytes=0;
		ptr->stats.rx_ecn_bytes=0;
		//The last update time is set to current time
		ptr->last_update_time=now;
		ptr->start_update_time=now;
		spin_lock_init(&(ptr->pair_lock));
		INIT_LIST_HEAD(&(ptr->list));
		return 1;
	}
	else
	{
		return 0;
	}
}

//Insert a new pair RX context to an endpoint RX context
static void Insert_rx_pair_endpoint(struct pair_rx_context* pair_ptr, struct endpoint_rx_context* endpoint_ptr)
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

//Insert a new pair RX context to a RX context
static void Insert_rx_pair(struct pair_rx_context* pair_ptr, struct rx_context* ptr)
{
	if(likely(pair_ptr!=NULL&&ptr!=NULL))
	{
		struct endpoint_rx_context* endpoint_ptr=NULL; 
		list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
		{
			//If the local_ip is matched
			if(endpoint_ptr->local_ip==pair_ptr->local_ip)
			{
				//Insert the pair RX context to corresponding endpoint RX context
				Insert_rx_pair_endpoint(pair_ptr, endpoint_ptr);
				return;
			}
		}
		printk(KERN_INFO "Can not find matching endpoint RX information entry\n");
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
} 

//Insert a new endpoint RX context to a RX context
static void Insert_rx_endpoint(struct endpoint_rx_context* endpoint_ptr, struct rx_context* ptr)
{
	if(likely(endpoint_ptr!=NULL&&ptr!=NULL))
	{
		unsigned long flags;		
		spin_lock_irqsave(&ptr->rx_lock,flags);
		list_add_tail(&(endpoint_ptr->list),&(ptr->endpoint_list));
		ptr->endpoint_num++;	
		spin_unlock_irqrestore(&ptr->rx_lock,flags);
	}
	else
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
}

static struct pair_rx_context* Search_rx_pair(struct rx_context* ptr, unsigned int local_ip, unsigned int remote_ip)
{
	if(likely(ptr!=NULL))
	{
		struct pair_rx_context* pair_ptr=NULL;
		struct endpoint_rx_context* endpoint_ptr=NULL; 
	
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
  
//Clear all endpoint or pair RX information entries 
static void Empty_rx_context(struct rx_context* ptr)
{
	if(likely(ptr!=NULL))
	{
		unsigned long flags;		
		struct endpoint_rx_context* endpoint_ptr=NULL; 
		struct endpoint_rx_context* endpoint_next=NULL; 
		struct pair_rx_context* pair_ptr=NULL; 
		struct pair_rx_context* pair_next=NULL; 
	
		spin_lock_irqsave(&ptr->rx_lock,flags);
		list_for_each_entry_safe(endpoint_ptr, endpoint_next, &(ptr->endpoint_list), list)
		{
			print_endpoint_rx_context(endpoint_ptr);
			list_for_each_entry_safe(pair_ptr, pair_next, &(endpoint_ptr->pair_list), list)
			{
				print_pair_rx_context(pair_ptr);	
				list_del(&(pair_ptr->list));
				kfree(pair_ptr);
			}
			list_del(&(endpoint_ptr->list));
			kfree(endpoint_ptr);
		}	
		spin_unlock_irqrestore(&ptr->rx_lock,flags);
	}
	{
		printk(KERN_INFO "Error: NULL pointer\n");
	}
}

//Delete a pair RX context (local_ip, remote_ip) from an endpoint RX context
//Return 1 if delete succeeds
static unsigned int Delete_rx_pair_endpoint(unsigned int local_ip, unsigned int remote_ip, struct endpoint_rx_context* endpoint_ptr)
{
	unsigned long flags;		
	struct pair_rx_context* pair_ptr=NULL; 
	struct pair_rx_context* pair_next=NULL; 
	
	if(unlikely(endpoint_ptr==NULL))
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return 0;
	}
	
	//No RX pair context in this endpoint
	if(unlikely(endpoint_ptr->pair_num==0))
		return 0;
		
	list_for_each_entry_safe(pair_ptr, pair_next, &(endpoint_ptr->pair_list), list)
	{
		//print_pair_rx_context(pair_ptr);	
		//If we find corresponding pair RX entry
		if(pair_ptr->local_ip==local_ip&&pair_ptr->remote_ip==remote_ip)
		{
			spin_lock_irqsave(&endpoint_ptr->endpoint_lock,flags);
			list_del(&(pair_ptr->list));
			kfree(pair_ptr);
			endpoint_ptr->pair_num--;
			spin_unlock_irqrestore(&endpoint_ptr->endpoint_lock,flags);
			return 1;
		}
	}
	printk(KERN_INFO "Can not delete corresponding pair RX information entry\n");
	return 0;
}

//Delete a pair RX context (local_ip, remote_ip) from a RX context
//Return 1 if delete succeeds
static unsigned int Delete_rx_pair(unsigned int local_ip, unsigned int remote_ip, struct rx_context* ptr)
{
	struct endpoint_rx_context* endpoint_ptr=NULL; 
		
	if(unlikely(ptr==NULL))
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return 0;
	}
	
	//No RX endpoint context 
	if(unlikely(ptr->endpoint_num==0))
		return 0;
	
	list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
	{	
		//If the local_ip is matched
		if(endpoint_ptr->local_ip==local_ip)
		{
			//Delete the pair RX context from the corresponding endpoint RX context
			return Delete_rx_pair_endpoint(local_ip, remote_ip, endpoint_ptr);
		}
	}
	return 0;
}

//Delete a endpoint RX context (local_ip) from a RX context
//Return 1 if delete succeeds
//Note that all related pair RX entries will also be deleted!
static unsigned int Delete_rx_endpoint(unsigned int local_ip, struct rx_context* ptr)
{
	unsigned long flags;		
	struct endpoint_rx_context* endpoint_ptr=NULL; 
	struct endpoint_rx_context* endpoint_next=NULL; 
	struct pair_rx_context* pair_ptr=NULL; 
	struct pair_rx_context* pair_next=NULL; 

	if(unlikely(ptr==NULL))
	{
		printk(KERN_INFO "Error: NULL pointer\n");
		return 0;
	}
	
	//No RX endpoint context 
	if(unlikely(ptr->endpoint_num==0))
		return 0;
	
	list_for_each_entry_safe(endpoint_ptr, endpoint_next, &(ptr->endpoint_list), list)
    {
		//If we find corresponding endpoint RX entry
		if(endpoint_ptr->local_ip==local_ip)
		{
			spin_lock_irqsave(&ptr->rx_lock,flags);
			//Delete all pair RX entries related to this endpoint RX entry
			list_for_each_entry_safe(pair_ptr, pair_next, &(endpoint_ptr->pair_list), list)
			{
				print_pair_rx_context(pair_ptr);	
				list_del(&(pair_ptr->list));
				kfree(pair_ptr);
			}
			list_del(&(endpoint_ptr->list));
			kfree(endpoint_ptr);
			ptr->endpoint_num--;
			spin_unlock_irqrestore(&ptr->rx_lock,flags);
			return 1;
		}
	}		
	return 0;
}

#endif