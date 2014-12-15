#ifndef PARAMS_H
#define PARAMS_H

//Rate control interval 
unsigned int CONTROL_INTERVAL_US=5*1000;

/* The following four parameters are used to generate feedback packet */
const u16 FEEDBACK_HEADER_SIZE=20;
const u8 FEEDBACK_PACKET_TTL=64;
const u8 FEEDBACK_PACKET_TOS=0xa; //(DSCP=2 TOS=4*2+2=10=0Xa)
int FEEDBACK_PACKET_IPPROTO=143; // should be some unused protocol

/* The following parameters are used for rate limters */
const unsigned int MAX_QUEUE_LEN=256;
const unsigned int TIMER_INTERVAL_US=100;
const unsigned int BUCKET_SIZE_BYTES=32*1024;
const u8 BANDWIDTH_GUARANTEE_DSCP=0x1;
const u8 WORK_CONSERVING_DSCP=0x0;

/* The following parameters are used for rate control */
const unsigned int LINK_CAPACITY=960; //Mbps
const unsigned int ELASTICSWITCH_ALPHA=100;	//ALPHA/1000 is the actual factor
const unsigned int TRINITY_ALPHA=500;
const unsigned int MINIMUM_RATE_INCREASE=10;
//const unsigned int MINIMUM_RATE=10;
 

#endif