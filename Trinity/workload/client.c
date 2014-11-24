#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h> 
#include <sys/time.h>
#include <pthread.h>

//Print usage information
void usage();

int main(int argc, char **argv)
{	
	if(argc!=6)
	{
		usage();
		return 0;
	}
    
    char client[16];                                   //Source IP address (e.g. 192.168.101.101) 
	char server[16];							        //Destination IP address 
    int client_port;                                   //TCP source port number 
	int server_port;                                  //TCP destination port number
	int data_size;							            //Request data size (KB)
	struct timeval tv_start;			        //Start time (after three way handshake)
	struct timeval tv_end;				        //End time
	int sockfd;								            //Socket
    struct sockaddr_in client_addr;   //Client address (IP:Port)
	struct sockaddr_in server_addr;  //Server address (IP:Port)
	int len;											        //Read length
	char buf[BUFSIZ];						        //Receive buffer
	unsigned long fct;						        //Flow Completion Time
	
    //Initialize 'client'
    memset(client,'\0',16);
	//Initialize ¡®server¡¯
	memset(server,'\0',16);
    
    //Get source IP address
	strncpy(client,argv[1],strlen(argv[1]));
    //Get TCP source port: char* to int
	client_port=atoi(argv[2]);        
	//Get destination IP address
	strncpy(server,argv[3],strlen(argv[3]));
	//Get TCP destination port: char* to int
	server_port=atoi(argv[4]);        
	//Get data_size: char* to int
	data_size=atoi(argv[5]);
	
    //Initialize client socket address
	memset(&client_addr,0,sizeof(client_addr));
	client_addr.sin_family=AF_INET;
	//source IP address
	client_addr.sin_addr.s_addr=inet_addr(client);
	//source port number
	client_addr.sin_port=htons(client_port);
    
	//Initialize server socket address
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	//destination IP address
	server_addr.sin_addr.s_addr=inet_addr(server);
	//destination port number
	server_addr.sin_port=htons(server_port);

	//Init socket
	if((sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
	{
		perror("socket error\n");  
		return 0;  
	}
    
    //Bind socket on source IP: source port
    if(bind(sockfd,(struct sockaddr *)&client_addr,sizeof(struct sockaddr))<0)  
	{  
		perror("bind error");  
		return 0;  
	}
    
	//Establish connection
	if(connect(sockfd,(struct sockaddr *)&server_addr,sizeof(struct sockaddr))<0)
	{
		printf("Can not connect to %s\n",server);
		return 0;
	}
	
	//Get start time after connection establishment
	gettimeofday(&tv_start,NULL);
	
	//Send request
	len=send(sockfd,argv[3],strlen(argv[3]),0);
	//Receive data
	while(1)
	{
		len=recv(sockfd,buf,BUFSIZ,0);
		if(len<=0)
			break;
	}
	//Get end time after receiving all of the data 
	gettimeofday(&tv_end,NULL);
	//Close connection
	close(sockfd);
	//Calculate time interval (unit: microsecond)
	fct=(tv_end.tv_sec-tv_start.tv_sec)*1000000+(tv_end.tv_usec-tv_start.tv_usec);
	printf("From %s: %d KB %lu us\n", server,data_size,fct);
	return 0;
}


void usage()
{
	printf("./client.o [source IP address] [source port] [destination IP address] [destination port] [request data size(KB)]\n");
}