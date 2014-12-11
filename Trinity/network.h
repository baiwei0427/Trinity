#ifndef NETWORK_H
#define NETWORK_H

#include "params.h"

//Create a feebdack packet. Note that bit is the congestion information and pkt is an incoming packet  
//Return 1 if successful. Else, return 0. 
//My implementation is based on send_reset function of Linux kernel
static unsigned int generate_feedback(unsigned int bit, struct sk_buff *pkt)
{
	struct sk_buff *skb;
	struct ethhdr *eth_from;
	struct iphdr *iph_to, *iph_from;
	//unsigned int *ip_opt=NULL;		  	
	unsigned int addr_type = RTN_LOCAL;
			
	eth_from = eth_hdr(pkt);
	if(unlikely(eth_from->h_proto!= __constant_htons(ETH_P_IP)))
		return 0;
	
	iph_from=(struct iphdr *)skb_network_header(pkt);
	
	skb=alloc_skb(FEEDBACK_HEADER_SIZE+LL_MAX_HEADER, GFP_ATOMIC);
	if(unlikely(!skb))
		return 0;
	
	skb_reserve(skb, LL_MAX_HEADER);
	skb_reset_network_header(skb);
	iph_to=(struct iphdr *)skb_put(skb, FEEDBACK_HEADER_SIZE);
	iph_to->version=4;
	iph_to->ihl=FEEDBACK_HEADER_SIZE/4;
	iph_to->tos=FEEDBACK_PACKET_TOS; 
	iph_to->tot_len =htons(FEEDBACK_HEADER_SIZE);
	iph_to->id=htons((unsigned short int)bit);
	iph_to->frag_off=0;
	iph_to->protocol=(u8)FEEDBACK_PACKET_IPPROTO;
	iph_to->check=0;
	iph_to->saddr=iph_from->daddr;
	iph_to->daddr=iph_from->saddr;
	
	/* ip_route_me_harder expects skb->dst to be set */
	skb_dst_set_noref(skb, skb_dst(pkt));
	
	skb->protocol=htons(ETH_P_IP);	
	if (ip_route_me_harder(skb, addr_type))
		goto free_skb;
	
	iph_to->ttl=ip4_dst_hoplimit(skb_dst(skb));	

	//Set IP option
	//ip_opt=(unsigned int*)iph_to+sizeof(struct iphdr)/4;
	//*ip_opt=htonl(bit);
	
	/* "Never happens" */
	if (skb->len > dst_mtu(skb_dst(skb)))
		goto free_skb;
	
	ip_local_out(skb);
	printk(KERN_INFO "Generate feedback packet with bit=%u\n",bit);
	return 1;

 free_skb:
	kfree_skb(skb);
	return 0;
}

static void enable_ecn(struct sk_buff *skb)
{
	struct iphdr *iph =ip_hdr(skb);
	if(iph!=NULL)
	{
		ipv4_change_dsfield(iph, 0xff, iph->tos | INET_ECN_ECT_0);
	}
}

static void clear_ecn(struct sk_buff *skb)
{
	struct iphdr *iph=ip_hdr(skb);
	if(iph!=NULL)
	{
		ipv4_change_dsfield(iph, 0xff, iph->tos & ~0x3);
	}
}

#endif