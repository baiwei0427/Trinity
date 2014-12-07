#ifndef PARAMS_H
#define PARAMS_H

//Rate control interval 
unsigned int CONTROL_INTERVAL_US=1000;

//The following four parameters are used to generate feedback packet
//unsigned int FEEDBACK_PACKET_SIZE = 64;
//The IP header length should be 24 bytes (4 bytes for option)
const u16 FEEDBACK_HEADER_SIZE=24;
const u8 FEEDBACK_PACKET_TTL=64;
const u8 FEEDBACK_PACKET_TOS=0x6;
int FEEDBACK_PACKET_IPPROTO=143; // should be some unused protocol

//The following parameters are used for rate limters
const unsigned int MAX_QUEUE_LEN=512;
const unsigned int TIMER_INTERVAL_US=100;
const unsigned int BUCKET_SIZE_BYTES=32*1024;

#endif