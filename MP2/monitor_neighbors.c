#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

extern long int nodesCosts[256];
extern int neighbors[256];
extern char filename[16];
extern int nexthop[256];

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void initializeNeighbors(){
	int i = 0;
	for(i = 0; i < 256; i++){
		neighbors[i] = 0;
		nexthop[i] = -1;
	}
}

int getDest(unsigned char* dest){
	char d[3];
	strncpy(d, (const char*)dest,2);
	d[2] = '\0';
	return atoi(d);	
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];
	
	initializeNeighbors();
	
	int bytesRecvd;
	FILE* fp;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			neighbors[heardFrom]=1;
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp((const char*)recvBuf, "send", 4))
		{
			//TODO send the requested message to the requested destination node
			// ...
			int dest = getDest(&recvBuf[4]);
			printf("dest is: %d\n", dest);
			char logline[64];
			if(neighbors[dest]){
				strncpy((char*)recvBuf,"forw", 4);
				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
					 (struct sockaddr*)&globalNodeAddrs[dest], sizeof(globalNodeAddrs[dest]));
			
				fp = fopen(filename,"a+");
				sprintf(logline, "sending packet dest %d nexthop %d message %s\n",
					dest,nexthop[dest],&recvBuf[6]);
				fwrite(logline,1,strlen(logline),fp);
				fclose(fp);		
			}
			else if(dest == globalMyID){
				fp = fopen(filename, "a+");
				sprintf(logline, "receive packet message %s\n", &recvBuf[6]);
				fwrite(logline, 1, strlen(logline), fp);
				fclose(fp);
			}
			else{
				fp = fopen(filename, "a+");
				sprintf(logline, "unreachable dest %d\n", dest);
				fwrite(logline, 1, strlen(logline), fp);
				fclose(fp);
			}
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp((const char*)recvBuf, "cost", 4))
		{
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
			int dest = getDest(&recvBuf[4]);
			nodesCosts[dest] = atoi((const char*)&recvBuf[6]);
		}
		
		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ... 
		else if(!strncmp((const char*)recvBuf, "forw",4))
		{
			int dest = getDest(&recvBuf[4]);
			char logline[64];
			if(neighbors[dest]){
				sendto(globalSocketUDP, recvBuf, bytesRecvd, 0,
					 (struct sockaddr*)&globalNodeAddrs[dest], sizeof(globalNodeAddrs[dest]));
			
				fp = fopen(filename,"a+");
				sprintf(logline, "forwarding packet dest %d nexthop %d message %s\n",
					dest,nexthop[dest],&recvBuf[6]);
				fwrite(logline,1,strlen(logline),fp);
				fclose(fp);		
			}
			else if(dest == globalMyID){
				fp = fopen(filename, "a+");
				sprintf(logline, "receive packet message %s\n", &recvBuf[6]);
				fwrite(logline, 1, strlen(logline), fp);
				fclose(fp);
			}
			else{
				fp = fopen(filename, "a+");
				sprintf(logline, "unreachable dest %d\n", dest);
				fwrite(logline, 1, strlen(logline), fp);
				fclose(fp);
			}
		}
	}
	//(should never reach here)
	close(globalSocketUDP);
}

void readAndParseInitialCostsFile(char* fileName, long int* costs){
	int i;
	for(i = 0; i < 256; i++)
		nodesCosts[i] = 1;

	int id;
	long int cost;
	FILE*fp = fopen(fileName, "r");
	if(fp != NULL){
		printf("Reading and parsing initial costs...\n");
		while(fscanf(fp, "%d %ld", &id, &cost) > 0){
			nodesCosts[id] = cost;
		}
		printf("Done.\n");
		fclose(fp);
	}
}
