// Copyright 2014 Hong Kong University of Science and Technology
// Primary contact: Wei Bai(baiwei0427@gmail.com)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* Actually, Shaper is a Hierarchical Token Bucket Filter (HTB).
   There are multiple (queue_num) queues (PacketQueue) in the Shaper.
   For each queue, it has 3 parameters :
   a. Token Generation rate
   b. Token bucket depth
   c. Max. Queue Length (a finite length would allow this to be used as  policer as packets are dropped after queue gets full)
*/

#include "connector.h" 
#include "packet.h"
#include "queue.h"
#include "shaper.h"
#include <tcp.h>

#define MTU 1500 //MTU=1500 bytes by default
#define max_queue_num 8 //Max number of tenants (queues) 

SHAPER::SHAPER() :shaper_timer_(this), init_(1)
{
	//Initialize queues 
	for(int i=0;i<max_queue_num;i++)
	{
		q_array[i]=new PacketQueue();
	}
	
	//Initialize queue (tenant) number
	bind("queue_num_",&queue_num_);
	
	//Initialize rates for each queue
	bind_bw("rate_0_",&rate_array[0]);
	bind_bw("rate_1_",&rate_array[1]);
	bind_bw("rate_2_",&rate_array[2]);
	bind_bw("rate_3_",&rate_array[3]);
	bind_bw("rate_4_",&rate_array[4]);
	bind_bw("rate_5_",&rate_array[5]);
	bind_bw("rate_6_",&rate_array[6]);
	bind_bw("rate_7_",&rate_array[7]);
	
	//Initialize buckets for each queue
	bind("bucket_0_",&bucket_array[0]);
	bind("bucket_1_",&bucket_array[1]);
	bind("bucket_2_",&bucket_array[2]);
	bind("bucket_3_",&bucket_array[3]);
	bind("bucket_4_",&bucket_array[4]);
	bind("bucket_5_",&bucket_array[5]);
	bind("bucket_6_",&bucket_array[6]);
	bind("bucket_7_",&bucket_array[7]);
	
	//Initialize maximum queue length for each queue
	bind("qlen_0_",&qlen_array[0]);
	bind("qlen_1_",&qlen_array[1]);
	bind("qlen_2_",&qlen_array[2]);
	bind("qlen_3_",&qlen_array[3]);
	bind("qlen_4_",&qlen_array[4]);
	bind("qlen_5_",&qlen_array[5]);
	bind("qlen_6_",&qlen_array[6]);
	bind("qlen_7_",&qlen_array[7]);
}

SHAPER::~SHAPER()
{
	//Clear the pending timer
	shaper_timer_.cancel();
	
	//Free up the packet queues 
	for(int i=0;i<max_queue_num;i++)
	{
		if (q_array[i]->length()!=0)
		{
			for (Packet *p=q_array[i]->head();p!=0;p=p->next_) 
				Packet::free(p);
		}
		delete q_array[i];
	}
}

//Callback function when receive an input packet
void SHAPER::recv(Packet *p, Handler *)
{
	//Initialize packet queues
	if(init_)
	{
		//Deal with incorrect queue number
		if(queue_num_>max_queue_num)
			queue_num_=max_queue_num;
		else if(queue_num_<=0)
			queue_num_=1;
			
		//Start with a full bucket for each queue
		for(int i=0;i<queue_num_;i++)
		{
			tokens_array[i]=bucket_array[i];
		}
		init_=0;
		
		//Get current time
		lastupdatetime_ = Scheduler::instance().clock();
		//Start timer
		shaper_timer_.resched(MTU*8/aggregate_rate());
	}
	
	//Get prio in IP header
	hdr_ip *iph=hdr_ip::access(p);
	int prio=iph->prio();
	
	//Deal with wrong prio in IP header
	if(prio>=queue_num_)
		prio=queue_num_-1;
	
	//Enqueue packets based on prio in IP header
	if(q_array[prio]->length()<qlen_array[prio])
	{
		q_array[prio]->enque(p);
	}
	else
	{
		drop(p);
	}
}

//Return aggregate rate of all queues
double SHAPER::aggregate_rate()
{
	double aggr_rate=0;
	for(int i=0;i<queue_num_;i++)
	{
		aggr_rate+=rate_array[i];
	}
	return aggr_rate;
}

void SHAPER_Timer::expire(Event* /*e*/)
{
	shaper_->timeout(0);
}

void SHAPER::timeout(int)
{
	//Get current time
	double now=Scheduler::instance().clock();
	
	//For each queue
	for(int i=0;i<queue_num_;i++)
	{
		//update tokens
		tokens_array[i]+=(now-lastupdatetime_)*rate_array[i];
		
		//Release packets as long as there are enough tokens
		while(q_array[i]->length()>0)
		{
			Packet *p=q_array[i]->head();
			hdr_cmn *ch=hdr_cmn::access(p);
			int pktsize = ch->size()<<3;
			
			if(tokens_array[i]>=pktsize)
			{
				p=q_array[i]->deque();
				target_->recv(p);
				tokens_array[i]-=pktsize;
			}
			else 
			{
				break;
			}
		}
		
		//If no packet in the queue now, ensure tokens should not be larger than bucket size
		if(q_array[i]->length()==0)
		{
			if(tokens_array[i]>bucket_array[i])
				tokens_array[i]=bucket_array[i];
		}
	}
	
	//Reset lastupdatetime_ to now
	lastupdatetime_ = now;
	//Start timer
	shaper_timer_.resched(MTU*8/aggregate_rate());
}


static class SHAPERClass : public TclClass {
public:
	SHAPERClass() : TclClass ("SHAPER") {}
	TclObject* create(int,const char*const*) {
		return (new SHAPER());
	}
}class_shaper;

