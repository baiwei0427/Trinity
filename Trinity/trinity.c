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

#include "tx.h"				
#include "rx.h"
#include "rl.h"
#include "rc.h"
#include "network.h"
#include "params.h"
#include "control.h"

//Trinity needs to maintain per-flow state.
#ifdef TRINITY
#include "flow.h"
#include "hash.h"
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BAI Wei wbaiab@cse.ust.hk");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Kernel module of Trinity/ElasticSwitch");
MODULE_SUPPORTED_DEVICE(DEVICE_NAME)

static char *param_dev=NULL;
MODULE_PARM_DESC(param_dev, "Network interface to operate");
module_param(param_dev, charp, 0);

//Open virtual characer device "/dev/trinity"
static int device_open(struct inode *, struct file *);
//Release virtual characer device "/dev/trinity"
static int device_release(struct inode *, struct file *);
//user space-kernel space communication (for Linux kernel 2.6.38.3)
static int device_ioctl(struct file *, unsigned int, unsigned long) ; 
//Hook for outgoing packets 
static struct nf_hook_ops nfho_outgoing;
//Hook for outgoing packets 
static struct nf_hook_ops nfho_incoming;

//RX context pointer
static struct rx_context* rxPtr;
//Lock for rx information 
//static spinlock_t rxLock;
//TX context pointer
static struct tx_context* txPtr;
//Lock for TX information
//static spinlock_t txLock;
#ifdef TRINITY
//FlowTable
static struct FlowTable ft;
#endif

static int device_open(struct inode *inode, struct file *file) 
{
	//printk(KERN_INFO "Device %s is opened\n",DEVICE_NAME);
	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) 
{
	//printk(KERN_INFO "Device %s is closed\n",DEVICE_NAME);
	module_put(THIS_MODULE);
	return SUCCESS;
}

//This context of this function should be kernel thread rather than interrupt. Is this correct?
static int device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) 
{
	//All possible pointers we will use in this function
	struct pair_context_user* user_pairPtr=NULL;
	struct endpoint_context_user* user_endpointPtr=NULL;
	struct pair_rx_context* rx_pairPtr=NULL;
	struct pair_tx_context* tx_pairPtr=NULL;
	struct endpoint_rx_context* rx_endpointPtr=NULL;
	struct endpoint_tx_context* tx_endpointPtr=NULL;
	
	switch (ioctl_num) 
	{
		//Case 1: Insert a new pair RX context
		case IOCTL_INSERT_RX_PAIR:
			user_pairPtr=(struct pair_context_user*)ioctl_param;
			rx_pairPtr=kmalloc(sizeof(struct pair_rx_context), GFP_KERNEL);	
			if(rx_pairPtr!=NULL)
			{
				Init_pair_rx_context(rx_pairPtr,user_pairPtr->local_ip,user_pairPtr->remote_ip,user_pairPtr->rate);
				Insert_rx_pair(rx_pairPtr,rxPtr);
			}
			else
			{
				printk(KERN_INFO "Kmalloc error when inserting a new pair RX context\n");
			}
			break;
		//Case 2: Insert a new pair TX context
		case IOCTL_INSERT_TX_PAIR:
			user_pairPtr=(struct pair_context_user*)ioctl_param;
			tx_pairPtr=kmalloc(sizeof(struct pair_tx_context), GFP_KERNEL);	
			if(tx_pairPtr!=NULL)
			{
				Init_pair_tx_context(tx_pairPtr,user_pairPtr->local_ip,user_pairPtr->remote_ip,user_pairPtr->rate,
				BUCKET_SIZE_BYTES, MAX_QUEUE_LEN,&xmit_tasklet, &my_hrtimer_callback, TIMER_INTERVAL_US,GFP_KERNEL);
				Insert_tx_pair(tx_pairPtr,txPtr);
			}
			else
			{
				printk(KERN_INFO "Kmalloc error when inserting a new pair TX context\n");
			}
			break;
		//Case 3: Delete a pair RX context 
		case IOCTL_DELETE_RX_PAIR:
			user_pairPtr=(struct pair_context_user*)ioctl_param;
			Delete_rx_pair(user_pairPtr->local_ip,user_pairPtr->remote_ip,rxPtr);
			break;
		//Case 4: Delete a pair TX context
		case IOCTL_DELETE_TX_PAIR:
			user_pairPtr=(struct pair_context_user*)ioctl_param;
			Delete_tx_pair(user_pairPtr->local_ip,user_pairPtr->remote_ip,txPtr);
			break;
		//Case 5: Insert a new endpoint RX context
		case IOCTL_INSERT_RX_ENDPOINT:
			user_endpointPtr=(struct endpoint_context_user*)ioctl_param;
			rx_endpointPtr=kmalloc(sizeof(struct endpoint_rx_context), GFP_KERNEL);	
			if(rx_endpointPtr!=NULL)
			{
				Init_endpoint_rx_context(rx_endpointPtr,user_endpointPtr->local_ip,user_endpointPtr->rate);
				Insert_rx_endpoint(rx_endpointPtr,rxPtr);
			}
			else
			{
				printk(KERN_INFO "Kmalloc error when inserting a new endpoint RX context\n");	
			}
			break;
		//Case 6: Insert a new endpoint TX context
		case IOCTL_INSERT_TX_ENDPOINT:
			user_endpointPtr=(struct endpoint_context_user*)ioctl_param;
			tx_endpointPtr=kmalloc(sizeof(struct endpoint_tx_context), GFP_KERNEL);	
			if(tx_endpointPtr!=NULL)
			{
				Init_endpoint_tx_context(tx_endpointPtr,user_endpointPtr->local_ip,user_endpointPtr->rate);
				Insert_tx_endpoint(tx_endpointPtr,txPtr);
			}
			else
			{
				printk(KERN_INFO "Kmalloc error when inserting a new endpoint TX context\n");		
			}
			break;
		//Case 7: Delete an endpoint RX context
		case IOCTL_DELETE_RX_ENDPOINT:
			user_endpointPtr=(struct endpoint_context_user*)ioctl_param;
			Delete_rx_endpoint(user_endpointPtr->local_ip,rxPtr);
			break;
		//Case 8: Delete an endpoint TX context
		case IOCTL_DELETE_TX_ENDPOINT:
			user_endpointPtr=(struct endpoint_context_user*)ioctl_param;
			Delete_tx_endpoint(user_endpointPtr->local_ip,txPtr);
			break;
		//Case 9: Display RX
		case IOCTL_DISPLAY_RX:
			print_rx_context(rxPtr);
			break;
		//Case 10: Display TX
		case IOCTL_DISPLAY_TX:
			print_tx_context(txPtr);
			break;
			
	}
	return SUCCESS;	
}

struct file_operations ops = {
    .read = NULL,
    .write = NULL,
   // .ioctl = device_ioctl, //For 2.6.32 kernel
    .unlocked_ioctl = device_ioctl, //For 2.6.38 kernel
    .open = device_open,
    .release = device_release,
};

//POSTROUTING for outgoing packets
static unsigned int hook_func_out(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct pair_tx_context* pair_txPtr=NULL;
	struct iphdr *ip_header=NULL;		
	unsigned int local_ip;
	unsigned int remote_ip;
	unsigned int result;
#ifdef TRINITY
	struct Flow f;										
	struct Information* info_pointer=NULL;	
	struct tcphdr *tcp_header=NULL;
	unsigned int delete_result;
	unsigned int payload_len;		
	unsigned int is_small=0;
#endif
	
	if(!out)
		return NF_ACCEPT;
        
	if(strcmp(out->name,param_dev)!=0)
		return NF_ACCEPT;

	ip_header=(struct iphdr *)skb_network_header(skb);
	
	//The packet is not ip packet (e.g. ARP or others)
	if (unlikely(ip_header==NULL))
		return NF_ACCEPT;
	
	local_ip=ip_header->saddr;
	remote_ip=ip_header->daddr;
	pair_txPtr=Search_tx_pair(txPtr,local_ip,remote_ip);
	
	if(likely(pair_txPtr!=NULL))
	{
	#ifdef TRINITY
		//Update per-flow state and determine whether this TCP flow is small or large
		//If it is not a TCP packet, just enquue to large flow queue by default
		if(ip_header->protocol==IPPROTO_TCP)
		{
			tcp_header = (struct tcphdr *)((__u32 *)ip_header+ ip_header->ihl);
			Init_Flow(&f);
			f.local_ip=ip_header->saddr;
			f.remote_ip=ip_header->daddr;
			f.local_port=ntohs(tcp_header->source);
			f.remote_port=ntohs(tcp_header->dest);
			if(tcp_header->syn)
			{
				spin_lock_bh(&(ft.table_lock));
				//We should set flags=GFP_ATOMIC in soft irq context
				if(Insert_Table(&ft,&f,GFP_ATOMIC)==0)
				{
					printk(KERN_INFO "Insert fail\n");
				}
				spin_unlock_bh(&(ft.table_lock));
				//The first packet must be enqueued to short flow queue
				is_small=1;
			}
			else if(tcp_header->fin||tcp_header->rst)
			{
				spin_lock_bh(&(ft.table_lock));
				delete_result=Delete_Table(&ft,&f);
				if(delete_result==0)
				{
					printk(KERN_INFO "Delete fail\n");
				}
				spin_unlock_bh(&(ft.table_lock));
				if(delete_result<FLOW_THRESH)
				{
					is_small=1;
				}
			}
			else
			{
				spin_lock_bh(&(ft.table_lock));
				info_pointer=Search_Table(&ft,&f);
				spin_unlock_bh(&(ft.table_lock));
				if(info_pointer!=NULL)
				{
					//TCP payload length=Total length - IP header length-TCP header length
					payload_len= (unsigned int)ntohs(ip_header->tot_len)-(ip_header->ihl<<2)-(tcp_header->doff<<2);    
					//payload length>0 and info_pointer->send_data will not exceed the maximum value of unsigned int  (4,294,967,295)
					if(payload_len>0 && payload_len+info_pointer->send_data<MAX_BYTES_SENT)
					{
						info_pointer->last_update_time=ktime_get();
						info_pointer->send_data+=payload_len;
						if(info_pointer->send_data<FLOW_THRESH)
							is_small=1;
					}
				}
			}
		}
		//Enqueue to small/large flow queue
		spin_lock_bh(&(pair_txPtr->rateLimiter.large_lock));
		result=Enqueue_dual_tbf(&(pair_txPtr->rateLimiter),skb,okfn,is_small);
		spin_unlock_bh(&(pair_txPtr->rateLimiter.large_lock));
	#else
		spin_lock_bh(&(pair_txPtr->rateLimiter.rl_lock));
		result=Enqueue_tbf(&(pair_txPtr->rateLimiter),skb,okfn);
		spin_unlock_bh(&(pair_txPtr->rateLimiter.rl_lock));
	#endif
		//If enqueue succeeds
		if(result==1)
			return NF_STOLEN;
		else
			return NF_DROP;
	}
	return NF_ACCEPT;
}

//PREROUTING for incoming packets
static unsigned int hook_func_in(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	struct pair_rx_context* pair_rxPtr=NULL;
	struct pair_tx_context* pair_txPtr=NULL;
	ktime_t now;
	struct iphdr *ip_header=NULL;	//IP  header structure
	unsigned int local_ip;
	unsigned int remote_ip;
	//unsigned long flags;	//variable for save current states of irq
	unsigned int bit=0;	//feedback information
	unsigned short int feedback=0;
	unsigned short int ECN_fraction=0;
	
	if(!in)
		return NF_ACCEPT;
    
	if(strcmp(in->name,param_dev)!=0)
		return NF_ACCEPT;
	
	ip_header=(struct iphdr *)skb_network_header(skb);

	//The packet is not ip packet (e.g. ARP or others)
	if (unlikely(ip_header==NULL))
		return NF_ACCEPT;
	
	local_ip=ip_header->daddr;
	remote_ip=ip_header->saddr;
	//If it's control packet, we need to do some special operations here
	if((u8)(ip_header->protocol)==FEEDBACK_PACKET_IPPROTO)
	{
		//Retrieve ECN fraction information
		ECN_fraction=ntohs(ip_header->id);
		printk(KERN_INFO "Receive control message packet, ECN fraction is %u%%\n",(unsigned int)ECN_fraction);
		pair_txPtr=Search_tx_pair(txPtr,local_ip,remote_ip);
		if(pair_txPtr!=NULL)
		{
			if(ECN_fraction!=0)
			{
			#ifdef  TRINITY
				//DCTCP-like rate control for work conserving rate
				pair_txPtr->rateLimiter.wc_rate=pair_txPtr->rateLimiter.wc_rate*(200-ECN_fraction)/200;//max(pair_txPtr->rateLimiter.wc_rate*(200-ECN_fraction)/200,MINIMUM_RATE);
			#else
				//Adjust rate to guarantee bandwidth
				pair_txPtr->rateLimiter.rate=pair_txPtr->guarantee_bw;
			#endif
			}	
			else 
			{
			#ifdef TRINITY
				pair_txPtr->rateLimiter.wc_rate=cubic_rc(pair_txPtr->rateLimiter.bg_rate+pair_txPtr->rateLimiter.wc_rate, LINK_CAPACITY,TRINITY_ALPHA)-pair_txPtr->guarantee_bw;
			#else
				pair_txPtr->rateLimiter.rate=max(cubic_rc(pair_txPtr->rateLimiter.rate, LINK_CAPACITY,ELASTICSWITCH_ALPHA),MINIMUM_RATE);
			#endif
			}
		}
		//We should not let any VM receive this packet
		return NF_DROP;
	}
	
	pair_rxPtr=Search_rx_pair(rxPtr,local_ip,remote_ip);
	if(likely(pair_rxPtr!=NULL))
	{
		spin_lock_bh(&(pair_rxPtr->pair_lock));
		//spin_lock_irqsave(&rxLock,flags);
		now=ktime_get();
		pair_rxPtr->last_update_time=now;
		//If the interval is larger than control interval
		if(ktime_us_delta(now,pair_rxPtr->start_update_time)>=CONTROL_INTERVAL_US)    
		{
		#ifdef TRINITY
			if(pair_rxPtr->stats.rx_wc_bytes+pair_rxPtr->stats.rx_bg_bytes>0)
			{
				print_pair_rx_context(pair_rxPtr);
				//We need to generate feedback packet now
				feedback=1;
				//Calculate the ECN fraction of work conserving traffic in this control interval
				if(pair_rxPtr->stats.rx_wc_bytes>0)
				{
					bit=pair_rxPtr->stats.rx_wc_ecn_bytes*100/pair_rxPtr->stats.rx_wc_bytes;
				}
			}
			pair_rxPtr->stats.rx_bg_bytes=0;
			pair_rxPtr->stats.rx_bg_ecn_bytes=0;
			pair_rxPtr->stats.rx_wc_bytes=0;
			pair_rxPtr->stats.rx_wc_ecn_bytes=0;
			pair_rxPtr->start_update_time=now;
		}
		//Bandwidth guarantee traffic
		if((ip_header->tos>>2)==BANDWIDTH_GUARANTEE_DSCP)
		{
			pair_rxPtr->stats.rx_bg_bytes+=skb->len;
			//ECN
			if((ip_header->tos<<6)==0xc0)
				pair_rxPtr->stats.rx_bg_ecn_bytes+=skb->len;
		}
		//Work conserving traffic
		else
		{
			pair_rxPtr->stats.rx_wc_bytes+=skb->len;
			//ECN
			if((ip_header->tos<<6)==0xc0)
				pair_rxPtr->stats.rx_wc_ecn_bytes+=skb->len;
		}
		#else
			if(pair_rxPtr->stats.rx_bytes>0)
			{
				print_pair_rx_context(pair_rxPtr);
				//We need to generate feedback packet now
				feedback=1;
				//Calculate the ECN fraction in this control interval
				bit=pair_rxPtr->stats.rx_ecn_bytes*100/pair_rxPtr->stats.rx_bytes;			
			}
			pair_rxPtr->stats.rx_bytes=0;
			pair_rxPtr->stats.rx_ecn_bytes=0;
			pair_rxPtr->start_update_time=now;
		}
		pair_rxPtr->stats.rx_bytes+=skb->len;
		//If the packet is marked with ECN (CE bits==11)
		if((ip_header->tos<<6)==0xc0)
		{
			pair_rxPtr->stats.rx_ecn_bytes+=skb->len;
		}
		#endif
		spin_unlock_bh(&(pair_rxPtr->pair_lock));
		//spin_unlock_irqrestore(&rxLock,flags);
		clear_ecn(skb);
		//Generate feedback packet now. This function can only be called in LOCAL_IN. I don't know why.
		if(feedback==1)
			generate_feedback(bit,skb);
	}
	return NF_ACCEPT;	
}

int init_module()
{
	int i,ret;
	
	//Get interface
    if(param_dev==NULL) 
    {
	#ifdef TRINITY
        printk(KERN_INFO "Trinity: not specify network interface (choose eth1 by default)\n");
	#else
		printk(KERN_INFO "ElasticSwitch: not specify network interface (choose eth1 by default)\n");
	#endif
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
	if(rxPtr==NULL)
	{
		printk(KERN_INFO "Kmalloc error\n");
		return 0;
	}
	Init_rx_context(rxPtr);
	//Initialize rxLock
	//spin_lock_init(&rxLock);
	
	//Initialize tX context information
	txPtr=kmalloc(sizeof(struct rx_context), GFP_KERNEL);
	if(txPtr==NULL)
	{
		printk(KERN_INFO "Kmalloc error\n");
		return 0;
	}
	Init_tx_context(txPtr);
	//Initialize txLock
	//spin_lock_init(&txLock);
#ifdef TRINITY
	//Initialize FlowTable
	Init_Table(&ft);
#endif
	
	nfho_incoming.hook = hook_func_in;			
	//If we intercept incoming packets in PRE_ROUTING, generate_feedback will crash	
	nfho_incoming.hooknum =  NF_INET_LOCAL_IN;		
	nfho_incoming.pf = PF_INET;											
	nfho_incoming.priority = NF_IP_PRI_FIRST;			
	nf_register_hook(&nfho_incoming);							
	
	nfho_outgoing.hook = hook_func_out;						
	nfho_outgoing.hooknum =  NF_INET_LOCAL_OUT;	
	nfho_outgoing.pf = PF_INET;											
	nfho_outgoing.priority = NF_IP_PRI_FIRST;			
	nf_register_hook(&nfho_outgoing);								
	
	//Register device file
	ret = register_chrdev(MAJOR_NUM, DEVICE_NAME, &ops);
	if (ret < 0) 
	{
		printk(KERN_INFO "Register char device failed with %d\n", MAJOR_NUM);
		return ret;
	}
	printk(KERN_INFO "Register char device successfully with %d\n", MAJOR_NUM);
#ifdef TRINITY
	printk(KERN_INFO "Start Trinity kernel module\n");
#else
	printk(KERN_INFO "Start ElasticSwitch kernel module\n");
#endif
	return SUCCESS;
}

void cleanup_module()
{
	//Unregister device
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
	//Unregister two hooks
	nf_unregister_hook(&nfho_outgoing);  
	nf_unregister_hook(&nfho_incoming);	

#ifdef TRINITY
	//Clear table
	 Empty_Table(&ft);
#endif
	 
	if(rxPtr!=NULL)
	{
		Empty_rx_context(rxPtr);	
		kfree(rxPtr);
	}
	if(txPtr!=NULL)
	{
		Empty_tx_context(txPtr);
		kfree(txPtr);
	}
	
#ifdef TRINITY
	printk(KERN_INFO "Stop Trinity kernel module\n");
#else
	printk(KERN_INFO "Stop ElasticSwitch kernel module\n");
#endif
}

