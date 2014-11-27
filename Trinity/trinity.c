#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/init.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/inet.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/string.h>
#include <linux/time.h>  
#include <linux/fs.h>
#include <linux/random.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/list.h>

#include "rx.h"
#include "network.h"
#include "params.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BAI Wei wbaiab@cse.ust.hk");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Kernel module of  Trinity");

static char *param_dev=NULL;
MODULE_PARM_DESC(param_dev, "Interface to operate Trinity");
module_param(param_dev, charp, 0);

//Hook for outgoing packets at LOCAL_OUT 
static struct nf_hook_ops nfho_outgoing;
//Hook for outgoing packets at LOCAL_IN
static struct nf_hook_ops nfho_incoming;
//RX context pointer
static struct rx_context* rxPtr;
//Lock for rx information 
static spinlock_t rxLock;

//POSTROUTING for outgoing packets
static unsigned int hook_func_out(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	if(!out)
		return NF_ACCEPT;
        
	if(strcmp(out->name,param_dev)!=0)
		return NF_ACCEPT;
	
	return NF_ACCEPT;
}

//PREROUTING for incoming packets
static unsigned int hook_func_in(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct pair_rx_context* pairPtr=NULL;
	ktime_t now;
	struct iphdr *ip_header;		//IP  header structure
	unsigned int local_ip;
	unsigned int remote_ip;
	unsigned long flags;					//variable for save current states of irq
	unsigned int bit;						//feedback information
	
	if(!in)
		return NF_ACCEPT;
    
	if(strcmp(in->name,param_dev)!=0)
		return NF_ACCEPT;
	
	ip_header=(struct iphdr *)skb_network_header(skb);

	//The packet is not ip packet (e.g. ARP or others)
	if (likely(!ip_header))
		return NF_ACCEPT;
	
	local_ip=ip_header->daddr;
	remote_ip=ip_header->saddr;
	pairPtr=Search_pair(rxPtr,local_ip,remote_ip);
	if(likely(pairPtr!=NULL))
	{
		spin_lock_irqsave(&rxLock,flags);
		now=ktime_get();
		pairPtr->last_update_time=now;
		//If the interval is larger than control interval
		if(ktime_us_delta(now,pairPtr->start_update_time)>=CONTROL_INTERVAL_US)    
		{
			//Calculate the fraction of ECN marking in this control interval
			bit=pairPtr->stats.rx_ecn_bytes*1000/pairPtr->stats.rx_bytes;			
			if(pairPtr->stats.rx_bytes>0)
			{
				print_pair_rx_context(pairPtr);
			}
			pairPtr->stats.rx_bytes=0;
			pairPtr->stats.rx_ecn_bytes=0;
			pairPtr->start_update_time=now;
		}
		pairPtr->stats.rx_bytes+=skb->len;
		//If the packet is marked with ECN (CE bits==11)
		if((ip_header->tos<<6)==192)
		{
			pairPtr->stats.rx_ecn_bytes+=skb->len;
		}
		spin_unlock_irqrestore(&rxLock,flags);
	}
	//generate_feedback(bit,skb);
	return NF_ACCEPT;	
}

int init_module()
{
	struct endpoint_rx_context* endpointPtr=NULL;
	struct pair_rx_context* pairPtr=NULL;  
	unsigned int local_ip,remote_ip;
	int i,j;
	
	//Get interface
    if(param_dev==NULL) 
    {
        printk(KERN_INFO "Trinity: not specify network interface.\n");
        param_dev = "eth1\0";
	}
	// trim 
	for(i = 0; i < 32 && param_dev[i] != '\0'; i++) 
	{
		if(param_dev[i] == '\n') 
		{
			param_dev[i] = '\0';
			break;
		}
	}
	
	//Initialize RX context information
	rxPtr=kmalloc(sizeof(struct rx_context), GFP_KERNEL);
	Init_rx_context(rxPtr);
	//Initialize rxLock
	spin_lock_init(&rxLock);
	
	//NF_LOCAL_IN Hook
	nfho_incoming.hook = hook_func_in;							//function to call when conditions below met
	nfho_incoming.hooknum =  NF_INET_LOCAL_IN;		//called in NF_IP_LOCAL_IN
	nfho_incoming.pf = PF_INET;											//IPv4 packets
	nfho_incoming.priority = NF_IP_PRI_FIRST;				//set to highest priority over all other hook functions
	nf_register_hook(&nfho_incoming);								//register hook*/
	
	//NF_LOCAL_OUT Hook
	nfho_outgoing.hook = hook_func_out;						//function to call when conditions below met
	nfho_outgoing.hooknum =  NF_INET_LOCAL_OUT;	//called in NF_IP_LOCAL_OUT
	nfho_outgoing.pf = PF_INET;											//IPv4 packets
	nfho_outgoing.priority = NF_IP_PRI_FIRST;				//set to highest priority over all other hook functions
	nf_register_hook(&nfho_outgoing);								//register hook	
	
	//Testcode
	for(i=4;i>=0;i--)
	{
		//local_ip: 192.168.101.1 and 192.168.101.101-192.168.101.104 (5 IP addresses in total)
		endpointPtr=kmalloc(sizeof(struct endpoint_rx_context), GFP_KERNEL);
		if(i==0)
		{
			local_ip=256*256*256+101*256*256+168*256+192;
		}
		else
		{
			local_ip=(100+i)*256*256*256+101*256*256+168*256+192;
		}
		Init_endpoint_rx_context(endpointPtr,local_ip,500);	
		Insert_endpoint(endpointPtr,rxPtr);
		
		for(j=4;j>=0;j--)
		{
			//remote_ip: 192.168.101.2,192.168.101.3 and 192.168.101.106-192.168.101.108 (5 IP addresses in total)
			pairPtr=kmalloc(sizeof(struct pair_rx_context), GFP_KERNEL);	
			if(j==0)
			{
				remote_ip=2*256*256*256+101*256*256+168*256+192;
			}
			else if(j==1)
			{
				remote_ip=3*256*256*256+101*256*256+168*256+192;
			}
			else
			{
				remote_ip=(104+j)*256*256*256+101*256*256+168*256+192;
			}
			Init_pair_rx_context(pairPtr,local_ip,remote_ip,100);
			Insert_pair(pairPtr,rxPtr);	
		}
	}
	
	printk(KERN_INFO "Start Trinity kernel module\n");
	return 0;
}

void cleanup_module()
{
	//Unregister two hooks
	nf_unregister_hook(&nfho_outgoing);  
	nf_unregister_hook(&nfho_incoming);	
	
	Empty_rx_context(rxPtr);	
	kfree(rxPtr);
	printk(KERN_INFO "Stop Trinity kernel module\n");
	
}
/*int init_module()
{
	int i,j;
	struct endpoint_rx_context* endpointPtr=NULL;
	struct pair_rx_context* pairPtr=NULL;  
	
	printk(KERN_INFO "Start Trinity kernel module\n");
	rxPtr=kmalloc(sizeof(struct rx_context), GFP_KERNEL);
	Init_rx_context(rxPtr);
	for(i=1;i<5;i++)
	{
		endpointPtr=kmalloc(sizeof(struct endpoint_rx_context), GFP_KERNEL);	
		Init_endpoint_rx_context(endpointPtr,i*256*256*256+168*256+192,400);
		Insert_endpoint(endpointPtr,rxPtr);
		
		for(j=1;j<5;j++)
		{
			pairPtr=kmalloc(sizeof(struct pair_rx_context), GFP_KERNEL);	
			Init_pair_rx_context(pairPtr,i*256*256*256+168*256+192,j*256*256*256+i*256*256+168*256+192,100);
			Insert_pair(pairPtr,rxPtr);
		}
	}
	
	print_rx_context(rxPtr);
	return 0;
}

void cleanup_module()
{
	Empty_rx_context(rxPtr);	
	kfree(rxPtr);
	printk(KERN_INFO "Remove Trinity kernel module\n");
}*/

/*static struct list_head pair_list;
	
//Called when module loaded using 'insmod'¡¡
int init_module()
{
	int i=0;
	struct pair_rx_context* ptr=NULL; 
	
	INIT_LIST_HEAD(&pair_list);
	//Allocate four pair_rx_context from kernel
	for(i=1;i<5;i++)
	{
		ptr=kmalloc(sizeof(struct pair_rx_context), GFP_KERNEL);
		ptr->local_ip=i*256*256*256+168*256+192;
		ptr->remote_ip=i*256*256*256+256*256+168*256+192;
		ptr->rate=100;
		ptr->stats.rx_bytes=i;
		ptr->stats.rx_ecn_bytes=i;
		INIT_LIST_HEAD(&(ptr->list));
		//Add this structure to the tail of the linked list
		list_add_tail(&(ptr->list),&pair_list);
	}
	printk(KERN_INFO "Display the list:\n");
	list_for_each_entry(ptr,&pair_list,list)
	{
		print_pair_rx_context(ptr);
	}
	printk(KERN_INFO "Display done\n");
	return 0;
}

//Called when module unloaded using 'rmmod'
void cleanup_module()
{
	struct pair_rx_context* ptr=NULL; 
	struct pair_rx_context* next=NULL; 
	printk(KERN_INFO "Remove start\n");	
	list_for_each_entry_safe(ptr, next, &pair_list, list)
    {
		print_pair_rx_context(ptr);
        list_del(&ptr->list);
        kfree(ptr);
    }	
	printk(KERN_INFO "Remove done\n");	
}*/

