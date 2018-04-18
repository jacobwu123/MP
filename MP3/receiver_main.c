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
#include "myprotocal.h"

FILE * fd_log;
int receive_window[RWS];

int handle_output_file(char* file)
{
  int i;
  //printf("file opened %s\n", file);
  fd_log = fopen(file, "w");
  if(!fd_log) return -1;
  
  return 0;
}

int print_to_file(unsigned long long int length, char* buf)
{
  int i;
  fputc(buf[0],fd_log);
  fflush(fd_log);
  return 0;
}

void send_packet(int socket, struct sockaddr_in * send_address, struct sendQ_slot * msg, int length)
{
    if(sendto(socket, msg->msg, length, 0, (struct sockaddr*) send_address, sizeof(struct sockaddr)) < 0)
      perror("sendto()");
}

void receiveSwp( char * buf, int length, SwpState * state,int socket, struct sockaddr_in * send_address)
{
	char * writeBuf;
	struct recvQ_slot recv_pkt;
	memcpy(&recv_pkt, buf, sizeof(struct recvQ_slot));
	long long seq_num = recv_pkt.SeqNo;
	printf("seqNO: %d\n", seq_num);
	// When the frame received is the next freame is expected for this swp
	if(seq_num == state -> NFE)
	{
		print_to_file(MAXDATASIZE, recv_pkt.msg);

		state -> NFE ++;
		while(receive_window[state->NFE % RWS])
		{
			receive_window[state->NFE % RWS] = 0;
			print_to_file(MAXDATASIZE, state->recvQ[state->NFE % RWS].msg);
			state -> NFE ++;
		}
	}

	// When the frame is within windows side, buffer it
	else if(seq_num > state -> NFE && seq_num <= state -> NFE + RWS)
	{
		if(!receive_window[seq_num])
		{
			receive_window[seq_num] = 1;
			memcpy(state-> recvQ[seq_num % RWS].msg, recv_pkt.msg, MAXDATASIZE);
		}
	}

	// now send ack
	struct recvQ_slot ack_pkt;
	char sendBuf[sizeof(struct recvQ_slot)];
	ack_pkt.SeqNo = state -> NFE-1;
	memcpy(sendBuf, &ack_pkt, sizeof(struct recvQ_slot));
	printf("sent ack seqNO:%d\n", ack_pkt.SeqNo);
	if(sendto(socket, sendBuf, sizeof(struct recvQ_slot), 0, (struct sockaddr*) send_address,sizeof(struct sockaddr))<0)
		perror("send ACK error.");

}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[sizeof(struct recvQ_slot)];
	int i;

	SwpState curr_state;
	curr_state.NFE=0;


	int receiverSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(receiverSocket < 0)
	{
		perror("socket()");
		exit(1);
	}

	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "127.0.0.1");
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(myUDPport);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(receiverSocket, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(receiverSocket);
		exit(1);
	}

	if(handle_output_file(destinationFile)<0)
		return;

	int bytesRecvd;
	theirAddrLen = sizeof(theirAddr);

	while(1)
	{
		if ((bytesRecvd = recvfrom(receiverSocket, recvBuf, sizeof(struct recvQ_slot), 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		struct recvQ_slot recv_msg;
		
		receiveSwp((char*)recvBuf, bytesRecvd, &curr_state, receiverSocket, &theirAddr);
		memcpy(&recv_msg, recvBuf, sizeof(struct recvQ_slot));
		printf("recv:%s\n", recv_msg.msg);
	}
	//(should never reach here)
	close(receiverSocket);
}

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = (unsigned short int)atoi(argv[1]);
	
	reliablyReceive(udpPort, argv[2]);
}
