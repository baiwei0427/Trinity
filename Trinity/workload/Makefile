all: client server

client: client.c
	gcc client.c -o client.o
    
server: server.c   
	gcc server.c -lpthread -o server.o

clean:
	rm client.o server.o