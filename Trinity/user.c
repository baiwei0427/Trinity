#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "control.h"

static void usage(char* program)
{
	printf("Usage of this program\n");
	printf("%s [display] [tx|rx]\n",program);    
	printf("%s [insert|delete] [tx|rx] [local ip] [bandwidth]\n",program);
	printf("%s [insert|delete] [tx|rx] [local ip] [remote ip] [bandwidth]\n",program);
}

int main(int argc, char *argv[])
{
	int file_desc, ret;
	
	//Try to open device file "/dev/trinity"
	file_desc = open(DEVICE_FILE_NAME, 0);
	//If open fails
	if (file_desc < 0) 
	{
		printf ("Can't open device file: %s\n", DEVICE_FILE_NAME);
		return 0;
	}

	//[program] [display] [tx|rx]
	if(argc==3)
	{
		if((strlen(argv[1])==7)&&(strncmp(argv[1],"display",7)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"rx",2)==0))
		{
			ret=ioctl(file_desc,IOCTL_DISPLAY_RX,NULL);
		}
		else if((strlen(argv[1])==7)&&(strncmp(argv[1],"display",7)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"tx",2)==0))
		{
			ret=ioctl(file_desc,IOCTL_DISPLAY_TX,NULL);
		}
		else
		{
			goto error;
		}
	}
	//[program] [insert|delete] [tx|rx] [local ip] [bandwidth]
	else if(argc==5)
	{
		struct endpoint_context_user endpoint;
		//[program] [insert] [rx] [local ip] [bandwidth]
		if((strlen(argv[1])==6)&&(strncmp(argv[1],"insert",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"rx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(endpoint.local_ip));
			endpoint.rate=(unsigned int)strtol(argv[4], NULL, 10);
			ret=ioctl(file_desc,IOCTL_INSERT_RX_ENDPOINT,&endpoint);
		}
		//[program] [delete] [rx] [local ip] [bandwidth]
		else if((strlen(argv[1])==6)&&(strncmp(argv[1],"delete",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"rx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(endpoint.local_ip));
			//guarantee bandwidth has not use when we delete a RX endpoint entry 
			endpoint.rate=0;
			ret=ioctl(file_desc,IOCTL_DELETE_RX_ENDPOINT,&endpoint);
		}
		//[program] [insert] [tx] [local ip] [bandwidth]
		else if((strlen(argv[1])==6)&&(strncmp(argv[1],"insert",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"tx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(endpoint.local_ip));
			endpoint.rate=(unsigned int)strtol(argv[4], NULL, 10);
			ret=ioctl(file_desc,IOCTL_INSERT_TX_ENDPOINT,&endpoint);
		}
		//[program] [delete] [tx] [local ip] [bandwidth]
		else if((strlen(argv[1])==6)&&(strncmp(argv[1],"delete",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"tx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(endpoint.local_ip));
			//guarantee bandwidth has not use when we delete a TX endpoint entry 
			endpoint.rate=0;
			ret=ioctl(file_desc,IOCTL_DELETE_TX_ENDPOINT,&endpoint);
		}
		else
		{
			goto error;
		}
	}
	//[program] [insert|delete] [tx|rx] [local ip] [remote ip] [bandwidth]
	else if(argc==6)
	{
		struct pair_context_user pair;
		if((strlen(argv[1])==6)&&(strncmp(argv[1],"insert",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"rx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(pair.local_ip));
			inet_pton(AF_INET,argv[4],&(pair.remote_ip));
			pair.rate=(unsigned int)strtol(argv[5], NULL, 10);	
			ret=ioctl(file_desc,IOCTL_INSERT_RX_PAIR,&pair);
		}
		else if((strlen(argv[1])==6)&&(strncmp(argv[1],"delete",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"rx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(pair.local_ip));
			inet_pton(AF_INET,argv[4],&(pair.remote_ip));
			//rate has not use when we delete a RX pair entry 
			pair.rate=0;	
			ret=ioctl(file_desc,IOCTL_DELETE_RX_PAIR,&pair);	
		}
		else if((strlen(argv[1])==6)&&(strncmp(argv[1],"insert",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"tx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(pair.local_ip));
			inet_pton(AF_INET,argv[4],&(pair.remote_ip));
			pair.rate=(unsigned int)strtol(argv[5], NULL, 10);	
			ret=ioctl(file_desc,IOCTL_INSERT_TX_PAIR,&pair);
		}
		else if((strlen(argv[1])==6)&&(strncmp(argv[1],"delete",6)==0)&&(strlen(argv[2])==2)&&(strncmp(argv[2],"tx",2)==0))
		{
			inet_pton(AF_INET,argv[3],&(pair.local_ip));
			inet_pton(AF_INET,argv[4],&(pair.remote_ip));
			//rate has not use when we delete a TX pair entry 
			pair.rate=0;	
			ret=ioctl(file_desc,IOCTL_DELETE_TX_PAIR,&pair);	
		}
		else
		{
			goto error;
		}
	}
	else
	{
		goto error;
	}
	close(file_desc);
	return 0;	
	
error:
	close(file_desc);
	usage(argv[0]);	
	return 0;
}
