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
};

//Define the structure for (per NIC/physical server) RX context
struct rx_context
{
	//The head point of link list for endpoint RX context
	struct list_head endpoint_list;
	//The number of endpoints for this physical server
	unsigned int endpoint_num;
};

static void print_pair_rx_context(struct pair_rx_context* ptr)
{
	char local_ip[16]={0};           
	char remote_ip[16]={0};
	unsigned int throughput=0;	//unit: Mbps
	unsigned long interval=(unsigned long)ktime_us_delta(ptr->last_update_time,ptr->start_update_time);
	
	if(interval>0)
	{
		throughput=ptr->stats.rx_bytes*8/interval;
	}
	snprintf(local_ip, 16, "%pI4", &(ptr->local_ip));
	snprintf(remote_ip, 16, "%pI4", &(ptr->remote_ip));
	printk(KERN_INFO "%s to %s, %lu/%lu, bandwidth guarantee is %u Mbps, actual incoming throughput is %u Mbps\n",remote_ip,local_ip,ptr->stats.rx_ecn_bytes,ptr->stats.rx_bytes,ptr->rate,throughput);
}

static void print_endpoint_rx_context(struct endpoint_rx_context* ptr)
{
	char local_ip[16]={0};      
	snprintf(local_ip, 16, "%pI4", &(ptr->local_ip));
	printk(KERN_INFO "%s, endpoint bandwidth guarantee is %u Mbps\n",local_ip,ptr->guarantee_bw);
}

static void print_rx_context(struct rx_context* ptr)
{
	struct endpoint_rx_context* endpoint_ptr=NULL; 
	struct pair_rx_context* pair_ptr=NULL;
	
	list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
	{
		print_endpoint_rx_context(endpoint_ptr);
		list_for_each_entry(pair_ptr,&(endpoint_ptr->pair_list),list)
		{
			print_pair_rx_context(pair_ptr);
		}
	}	
}

//Initialize RX context
static void Init_rx_context(struct rx_context* ptr)
{
	ptr->endpoint_num=0;
	INIT_LIST_HEAD(&(ptr->endpoint_list));
}

//Initialize endpoint RX context
static void Init_endpoint_rx_context(struct endpoint_rx_context* ptr, unsigned int ip, unsigned int bw)
{
	ptr->pair_num=0;
	ptr->local_ip=ip;
	ptr->guarantee_bw=bw;
	INIT_LIST_HEAD(&(ptr->pair_list));
	INIT_LIST_HEAD(&(ptr->list));
}

//Initialize pair RX context
static void Init_pair_rx_context(struct pair_rx_context* ptr, unsigned int local_ip, unsigned int remote_ip, unsigned int bw)
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
	INIT_LIST_HEAD(&(ptr->list));
}

//Insert a new pair RX context to an endpoint RX context
static void Insert_pair_endpoint(struct pair_rx_context* pair_ptr, struct endpoint_rx_context* endpoint_ptr)
{
	list_add_tail(&(pair_ptr->list),&(endpoint_ptr->pair_list));
	endpoint_ptr->pair_num++;
}

//Insert a new pair RX context to a RX context
static void Insert_pair(struct pair_rx_context* pair_ptr, struct rx_context* ptr)
{
	struct endpoint_rx_context* endpoint_ptr=NULL; 
	list_for_each_entry(endpoint_ptr,&(ptr->endpoint_list),list)
	{
		//If the local_ip is matched
		if(endpoint_ptr->local_ip==pair_ptr->local_ip)
		{
			//Insert the pair RX context to corresponding endpoint RX context
			Insert_pair_endpoint(pair_ptr, endpoint_ptr);
			return;
		}
	}
	printk(KERN_INFO "Can not find matching endpoint RX information entry\n");
} 

//Insert a new endpoint TX context to a RX context
static void Insert_endpoint(struct endpoint_rx_context* endpoint_ptr, struct rx_context* ptr)
{
	list_add_tail(&(endpoint_ptr->list),&(ptr->endpoint_list));
	ptr->endpoint_num++;	
}

static struct pair_rx_context* Search_pair(struct rx_context* ptr, unsigned int local_ip, unsigned int remote_ip)
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
  
//Clear all endpoint or pair RX information entries 
static void Empty_rx_context(struct rx_context* ptr)
{
	struct endpoint_rx_context* endpoint_ptr=NULL; 
	struct endpoint_rx_context* endpoint_next=NULL; 
	struct pair_rx_context* pair_ptr=NULL; 
	struct pair_rx_context* pair_next=NULL; 
	
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
}

#endif