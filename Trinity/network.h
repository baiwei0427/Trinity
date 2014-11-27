#ifndef NETWORK_H
#define NETWORK_H

#include <linux/skbuff.h>
#include "params.h"

//Create a feebdack packet. bit is the congestion information and pkt is an incoming packet  
//Return 1 if successful. 
//Thanks for jvimal's EyeQ implementation. 
static unsigned int generate_feedback(unsigned int bit, struct sk_buff *pkt)
{
	struct sk_buff *skb;
	struct ethhdr *eth_to, *eth_from;
	struct iphdr *iph_to, *iph_from;
	
	eth_from = eth_hdr(pkt);
	if(unlikely(eth_from->h_proto != __constant_htons(ETH_P_IP)))
		return 0;
	
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

		/* NB: this function doesn't "send" the packet */
		ip_send_check(iph_to);
		skb_push(skb, ETH_HLEN);
		/* Driver owns the buffer now; we don't need to free it */
		dev_queue_xmit(skb);
		return 1;
	}
	return 0;
} 

#endif