#ifndef CONTROL_H
#define CONTROL_H

#include <linux/ioctl.h>
#include <linux/param.h>

//major number of device
#define MAJOR_NUM 100   
#define DEVICE_NAME "trinity"
#define DEVICE_FILE_NAME "/dev/trinity"
#define SUCCESS 0

/* The following two structure are used for userspace application rather than kernel module 
  * pair_context_user can represent both struct pair_tx_context  (see tx.h) and pair_rx_context (see rx.h)
  * endpoint_context_user can represent both struct endpoint_tx_context (see tx.h) and endpoint_rx_context (see rx.h)
  */
struct pair_context_user
{
	unsigned int local_ip;			
	unsigned int remote_ip;			
	unsigned int rate;
};

struct endpoint_context_user
{
	unsigned int local_ip;
	unsigned int rate;
};

/*
 * _IOR means that we're creating an ioctl command
 * number for passing information from a user process
 * to the kernel module.
 *
 * The first arguments, MAJOR_NUM, is the major device 
 * number we're using.
 *
 * The second argument is the number of the command 
 * (there could be several with different meanings).
 *
 * The third argument is the type we want to get from 
 * the process to the kernel.
 */

//Insert a new pair RX context
#define IOCTL_INSERT_RX_PAIR _IOR(MAJOR_NUM, 0, struct pair_context_user*) 
//Delete a pair RX context
#define IOCTL_DELETE_RX_PAIR _IOR(MAJOR_NUM, 1, struct pair_context_user*) 
//Insert a new endpoint RX context
#define IOCTL_INSERT_RX_ENDPOINT _IOR(MAJOR_NUM, 2, struct endpoint_context_user*) 
//Delete a endpoint RX context
#define IOCTL_DELETE_RX_ENDPOINT _IOR(MAJOR_NUM, 3, struct endpoint_context_user*) 
//Display current RX information. The third parameter should be set to NULL in userspace application.
#define IOCTL_DISPLAY_RX _IOR(MAJOR_NUM, 4, struct endpoint_context_user*)

//Insert a new pair TX context
#define IOCTL_INSERT_TX_PAIR _IOR(MAJOR_NUM, 5, struct pair_context_user*) 
//Delete a pair TX context
#define IOCTL_DELETE_TX_PAIR _IOR(MAJOR_NUM, 6, struct pair_context_user*) 
//Insert a new endpoint TX context
#define IOCTL_INSERT_TX_ENDPOINT _IOR(MAJOR_NUM, 7, struct endpoint_context_user*) 
//Delete a endpoint TX context
#define IOCTL_DELETE_TX_ENDPOINT _IOR(MAJOR_NUM, 8, struct endpoint_context_user*) 
//Display current TX information. The third parameter should be set to NULL in userspace application.
#define IOCTL_DISPLAY_TX _IOR(MAJOR_NUM, 9, struct endpoint_context_user*)

#endif