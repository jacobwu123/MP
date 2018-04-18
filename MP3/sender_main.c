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
#include <time.h>
#include "myprotocal.h"

int global_file_offset;
int Send_Sequence_Number;

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
void send_packet(int socket, struct sockaddr_in * send_address, struct sendQ_slot * msg, int length);

int handle_input_file(char* file, unsigned long long int length, char * buf, int offset)
{
  FILE * fd_send;
  int i;
  //printf("file opened %s\n", file);
  fd_send = fopen(file, "r");
  fseek(fd_send, offset, SEEK_SET);
  if(!fd_send) return -1;
  
  for(i=0; i<length; i++)
  {
  	if((buf[i]=fgetc(fd_send))==EOF)
  	{
  		fclose(fd_send);
  		return 1;
  	}
  }
  fclose(fd_send);
  return 0;
}

void send_multiple_packet (int socket, struct sockaddr_in * send_address, SwpState * state,
	int start_frame, int last_frame)
{
	int i;
	for(i = start_frame; i <= last_frame; i++)
	{
		send_packet(socket, send_address, &(state -> sendQ[i%SWS]), MAXDATASIZE);
	}
}

void send_packet(int socket, struct sockaddr_in * send_address, struct sendQ_slot * msg, int length)
{
	printf("sent:%c, SeqNO: %d\n", msg->msg[0], msg->SeqNo);
	char buf[sizeof(struct sendQ_slot)];
	memcpy(buf, msg, sizeof(struct sendQ_slot));
    if(sendto(socket, buf, sizeof(struct sendQ_slot), 0, (struct sockaddr*) send_address, 
    	sizeof(struct sockaddr)) < 0)
      perror("sendto()");
}

bool swpInWindows(long long AckNum, long long left, long long right)
{
	if( AckNum >= left && AckNum <= right)
		return true;

	return false;
}

void fill_sending_window(SwpState * state, long long LAR, long long LFS, char* filename)
{
	int i;
	for(i = LFS + 1; i < LAR + SWS + 1 ; i++)
	{
		state->sendQ[i%SWS].SeqNo = Send_Sequence_Number;
		if(handle_input_file(filename, MAXDATASIZE, state->sendQ[i%SWS].msg, global_file_offset))
			exit(1);

		printf("message:%s\n", state->sendQ[i%SWS].msg);
		Send_Sequence_Number++;
		global_file_offset += MAXDATASIZE;
	}
}

static int deliverSWP(SwpState * state, struct recvQ_slot * recvBuf, 
	int socket, struct sockaddr_in * send_address, char * filename)
{
	long long ACK_Sequence_Number;
	ACK_Sequence_Number = recvBuf->SeqNo;
	if(swpInWindows(ACK_Sequence_Number, state -> LAR + 1, state-> LFS))
	{
		// if(state -> LAR == ACK_Sequence_Number)
		// 	return 0;
		do
		{
			struct sendQ_slot * slot;
			slot = &state -> sendQ[++state -> LAR % SWS];
			//msgDestroy(&slot->msg);
			//semSignal(&state->sendWindowNotFull);
		} while (state->LAR != ACK_Sequence_Number);
		fill_sending_window(state, state-> LAR, state-> LFS, filename);
		send_multiple_packet(socket, send_address, state, state->LAR + 1, state -> LFS + SWS);
		state -> LFS = state -> LFS + SWS;
		printf("LAR:%d, LFS:%d\n", state->LAR,state->LFS);
		return 1;
	}
	return 1;
}


int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	udpPort = (unsigned short int)atoi(argv[2]);
	numBytes = atoll(argv[4]);
	
	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
} 

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	long send_start;
	long last_ack,received_ack;
	int i;
	int recv_socket;
	socklen_t transfer_Len;
	struct timespec ack_timer;
	struct timespec my_timer;
	//struct Msg my_packet[SWS];
	struct sockaddr_in transfer_addr;
	struct sockaddr_in from_addr;
	fd_set rfds;
	struct timeval tv;

	char recvBuf[sizeof(struct recvQ_slot)];


	unsigned long long int numBytes;

	global_file_offset = 0;
	Send_Sequence_Number = 0;

	//create socket

	int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(senderSocket < 0)
		perror("socket()");

	if(bind(senderSocket, (struct sockaddr*)&transfer_addr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(senderSocket);
		exit(1);
	}
		//get receiver IP address
		
	memset(&transfer_addr, 0, sizeof(transfer_addr));
	socklen_t transfer_addr_len = sizeof(transfer_addr);
	transfer_addr.sin_family = AF_INET;
	transfer_addr.sin_port = htons(hostUDPport);
	inet_pton(AF_INET, hostname, &transfer_addr.sin_addr);

	SwpState curr_state;
	curr_state.LAR = -1;
	curr_state.LFS = 0;


	curr_state.sendQ[0].SeqNo = 0;
	handle_input_file(filename, 1, curr_state.sendQ[0].msg, global_file_offset);
	global_file_offset+=MAXDATASIZE;
	// send_packet(senderSocket, &transfer_addr, &buffer, 1);
	//fill_sending_window(&curr_state,curr_state.LAR, curr_state.LFS, filename);
	send_multiple_packet(senderSocket, &transfer_addr, &curr_state, 0, 0);
	Send_Sequence_Number ++;
	curr_state.LFS = curr_state.LFS + SWS - 1;
	while(1){
		 if((numBytes = recvfrom(senderSocket,recvBuf,sizeof(struct recvQ_slot),0,
		 		(struct sockaddr *) &transfer_addr,&transfer_addr_len))==-1){
		 	if (errno != EAGAIN || errno != EWOULDBLOCK){
		 		printf("error:%s\n", strerror(errno));
		 		perror("can not receive ack");
            	exit(2);
		 	}
		 	else
		 	{
		 		//resend because of time out
		 		send_packet(senderSocket, &transfer_addr, &(curr_state.sendQ[(curr_state.LAR + 1)%SWS]), MAXDATASIZE);
		 	}

        }
        struct recvQ_slot recv_pkt;
        memcpy(&recv_pkt, recvBuf, sizeof(struct recvQ_slot));
        printf("ACK:%d\n", recv_pkt.SeqNo);
        deliverSWP(&curr_state, &recv_pkt, senderSocket, &transfer_addr, filename);
	}

}