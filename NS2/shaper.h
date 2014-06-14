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

#ifndef ns_shaper_h
#define ns_shaper_h

#include "connector.h"
#include "timer-handler.h"

#define max_queue_num 8 //Max number of tenants (queues) 

class SHAPER;

class SHAPER_Timer : public TimerHandler {
public:
	SHAPER_Timer(SHAPER *t) : TimerHandler() { shaper_ = t;}
	
protected:
	virtual void expire(Event *e);
	SHAPER *shaper_;
};

class SHAPER : public Connector {
public:
	SHAPER();
	~SHAPER();
	void timeout(int);
	double aggregate_rate();
	
protected:
	void recv(Packet *, Handler *); //receive an input packet
	double getupdatedtokens(); //update tokens for multi-tenants
	int command(int argc, const char*const* argv); //command function; reset values periodically
	
	PacketQueue *q_array[max_queue_num]; //PacketQueue array for multi-tenants 
	double tokens_array[max_queue_num]; //accumulated tokens array for multi-tenants
	double rate_array[max_queue_num]; //token bucket rate array for multi-tenants
	int bucket_array[max_queue_num]; //bucket depth array for multi-tenants
	int qlen_array[max_queue_num]; //queue length array for multi-tenants
	int queue_num_;	//number of queues. Each tenant has one dedicated PacketQueue for one NIC
	
	double lastupdatetime_;
	SHAPER_Timer shaper_timer_;	//timer for rate limiting
	int init_;
};

#endif




