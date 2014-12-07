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
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/time.h>  
#include <linux/fs.h>
#include <linux/random.h>
#include <linux/errno.h>
#include <linux/timer.h>

#include "tx.h"

MODULE_LICENSE("GPL");

//static struct PacketQueue *queuePtr;
static char *param_dev="eth1\0";
static struct nf_hook_ops nfho_outgoing;
struct tx_context tx;

static void xmit_tasklet(unsigned long data)
{
	struct tx_context *txPtr=(struct tx_context*)data;
	unsigned int skb_len;
	ktime_t now=ktime_get();
	
	txPtr->tbfPtr->tokens+=ktime_us_delta(now,txPtr->tbfPtr->last_update_time)*(txPtr->tbfPtr->rate)/8;
	txPtr->tbfPtr->last_update_time=now;
	while(1)
	{
		if(txPtr->tbfPtr->len>0)
		{
			skb_len=txPtr->tbfPtr->packets[txPtr->tbfPtr->head].skb->len;
			if(skb_len<=txPtr->tbfPtr->tokens)
			{
				txPtr->tbfPtr->tokens-=skb_len;
				spin_lock_bh(&(txPtr->tbfPtr->spinlock));
				Dequeue_tbf(txPtr->tbfPtr);
				spin_unlock_bh(&(txPtr->tbfPtr->spinlock));
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
	
	if(txPtr->tbfPtr->tokens>=txPtr->tbfPtr->bucket&&txPtr->tbfPtr->len==0)
		txPtr->tbfPtr->tokens=txPtr->tbfPtr->bucket;	
		
	//Start time again
	hrtimer_start(&(txPtr->timer), ktime_set( 0, txPtr->delay_in_us*1000), HRTIMER_MODE_REL);
}

/* HARDIRQ timeout */
static enum hrtimer_restart my_hrtimer_callback( struct hrtimer *timer )
{
	/* schedue xmit tasklet to go into softirq context */
	struct tx_context *txPtr = container_of(timer, struct tx_context, timer);
	tasklet_schedule(&(txPtr->xmit_timeout));
	return HRTIMER_NORESTART;
}

//POSTROUTING for outgoing packets
static unsigned int hook_func_out(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
	int result=0;
	//unsigned long flags;
	
	if(!out)
		return NF_ACCEPT;
        
	if(strcmp(out->name,param_dev)!=0)
		return NF_ACCEPT;
	
	if(ip_hdr(skb)!=NULL)
	{
		spin_lock_bh(&(tx.tbfPtr->spinlock));
		//spin_lock_irqsave(&(queuePtr->queue_lock),flags);
		result=Enqueue_tbf(tx.tbfPtr,skb,okfn);
		//spin_unlock_irqrestore(&(queuePtr->queue_lock),flags);
		spin_unlock_bh(&(tx.tbfPtr->spinlock));
		if(result==0)
		{
			return NF_DROP;
		}
		else
		{
			return NF_STOLEN;
		}
	}
	return NF_ACCEPT;
}

int init_module()
{	
	//Initialize TX information
	Init_tx(&tx,100,32000,512,&xmit_tasklet,&my_hrtimer_callback,100,GFP_KERNEL);
	
	//NF_POST_ROUTING Hook
	nfho_outgoing.hook = hook_func_out;						
	nfho_outgoing.hooknum =  NF_INET_POST_ROUTING;	
	nfho_outgoing.pf = PF_INET;											
	nfho_outgoing.priority = NF_IP_PRI_FIRST;			
	nf_register_hook(&nfho_outgoing);		
	
	return 0;
}

void cleanup_module()
{
	nf_unregister_hook(&nfho_outgoing);  
	Free_tx(&tx);
}