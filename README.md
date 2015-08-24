##1 What is Trinity ?
A network virtualization solution for multi-tenant public clouds. 

##2 Analysis of source codes 

###2.1 Makefile
<code>EXTRA_CFLAGS +=-DTRINITY</code> indicates that Trinity is enabled. If you delete this line, the kernel module just works as [ElasticSwitch](http://conferences.sigcomm.org/sigcomm/2013/papers/sigcomm/p351.pdf) (SIGCOMM 2013).

###2.2 user.c and control.h
These two files delivers the implementation of the configureation program in user space. It communicates with kernel module using <strong>ioctl</strong>.

###2.3 tx.h
This file implements the TX (transmit) context. There are 3 important structures in this file: <code>pair_tx_context </code>, <code>endpoint_tx_context</code> and <code>tx_context</code>. The most imporant structure is <code>pair_tx_context</code> which describes the structure of a VM-to-VM pair. If Trinity is enabled, <code>pair_tx_context</code> leverages a special rate limiter: <code>dual_tbf_rl</code>, a dual priority queue based token bucket rate limiter. Otherwise, <code>pair_tx_context</code> just employs normal token bucket rate limiter. 

###2.4 tbf.h
This file describes the implementation of token bucket rate limiter.

###2.5 dual_tbf.h
This file describes the implementation of dual priority queue based token bucket rate limiter. If you just want to use ElasticSwitch, no need to read this file.

###2.6 rl.h
The header file for rate limiting (rl). It describes the tasklet and hrtimer callback functions of above two rate limiters.

###2.7 rc.h
This file describes a cubic-like rate control (rc) algorithm to increase rate.

###2.8 network.h
This file gives the implementation of several network operation functions. <code>generate_feedback</code> shows how to generate feedback packets in kernel to deliver congestion information.

###2.9 rx.h
This file implements the RX (receive) context. There are 3 important structures in this file: <code>pair_rx_context </code>, <code>endpoint_rx_context</code> and <code>rx_context</code>.

###2.10 flow.h and hash.h
These two files give the implementation of a flow table. If you just want to use ElasticSwitch, no need to read these files.

###2.11 params.h
Definitions of imporant global variables.

###2.12 trinity.c
main program




