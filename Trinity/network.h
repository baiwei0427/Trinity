#ifndef NETWORK_H
#define NETWORK_H

#include <linux/skbuff.h>
#include "params.h"

//Create a feebdack packet. Note that bit is the congestion information and pkt is an incoming packet  
//I modity send_reset function in Linux kernel.
//Return 1 if successful. Else, return 0. 
static unsigned int generate_feedback(unsigned int bit, struct sk_buff *pkt)
{
	struct sk_buff *skb;
	struct ethhdr *eth_from;
	struct iphdr *iph_to, *iph_from;
	unsigned int *ip_opt=NULL;		  	
	unsigned int addr_type = RTN_LOCAL;
			
	eth_from = eth_hdr(pkt);
	if(unlikely(eth_from->h_proto != __constant_htons(ETH_P_IP)))
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
	iph_to->id=0;//htons(bit); If I set id=htons(bit) here, kernel will crash. I don't know why.
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
	ip_opt=(unsigned int*)iph_to+sizeof(struct iphdr)/4;
	*ip_opt=htonl(bit);
	
	/* "Never happens" */
	if (skb->len > dst_mtu(skb_dst(skb)))
		goto free_skb;
	
	ip_local_out(skb);
	printk(KERN_INFO "Generate feedback packet\n");
	return 1;

 free_skb:
	kfree_skb(skb);
	return 0;
}
	/*
	skb = netdev_alloc_skb(pkt->dev, FEEDBACK_PACKET_SIZE);
	if(likely(skb)) 
	{
		skb_set_queue_mapping(skb, 0);
		skb->len = FEEDBACK_PACKET_SIZE;
		skb->protocol = __constant_htons(ETH_P_IP);
		skb->pkt_type = PACKET_OUTGOING;
		
		//Fill in the information of Ethernet header
		skb_reset_mac_header(skb);
		skb_set_tail_pointer(skb, FEEDBACK_PACKET_SIZE);
		eth_to = eth_hdr(skb);

		memcpy(eth_to->h_source, eth_from->h_dest, ETH_ALEN);
		memcpy(eth_to->h_dest, eth_from->h_source, ETH_ALEN);
		eth_to->h_proto = eth_from->h_proto;
	
		//Fill in the information of IP header
		skb_pull(skb, ETH_HLEN);
		skb_reset_network_header(skb);
		iph_to = ip_hdr(skb);
		iph_from = ip_hdr(pkt);
		
		iph_to->ihl = 5;
		iph_to->version = 4;
		iph_to->tos = 0x2; //ECT
		iph_to->tot_len = __constant_htons(FEEDBACK_HEADER_SIZE);
		iph_to->id = bit; 
		iph_to->frag_off = 0;
		iph_to->ttl = FEEDBACK_PACKET_TTL;
		iph_to->protocol = (u8)FEEDBACK_PACKET_IPPROTO;
		iph_to->saddr = iph_from->daddr;
		iph_to->daddr = iph_from->saddr;

		ip_send_check(iph_to);
		skb_push(skb, ETH_HLEN);
		dev_queue_xmit(skb);
		return 1;
	}
	return 0;*/


#endif