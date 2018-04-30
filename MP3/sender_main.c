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

long long int global_file_offset;
long long int Send_Sequence_Number;

long long int total_packets;

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
int send_packet(int socket, struct sockaddr_in * send_address, struct sendQ_slot * msg, int length);

int handle_input_file(char* file, unsigned long long int length, char * buf, int offset, unsigned long long int bytesToTransfer)
{
  FILE * fd_send;
  int bytesRead, bytesToRead;

  fd_send = fopen(file, "r");
  fseek(fd_send, offset, SEEK_SET);
  if(!fd_send) return -1;

  memset(buf,0,MAXDATASIZE);//clear buffer

  bytesToRead = MAXDATASIZE;

  if(offset + MAXDATASIZE > bytesToTransfer)
  {
  	bytesToRead = bytesToTransfer - offset;
  }

  bytesRead = fread(buf, sizeof(char), bytesToRead, fd_send);

    printf("offset = %d, bytestoread = %d, bytesread = %d\n", offset, bytesToRead, bytesRead );

   global_file_offset += bytesRead;

  fclose(fd_send);
  return bytesRead;
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

int send_packet(int socket, struct sockaddr_in * send_address, struct sendQ_slot * msg, int length)
{
	printf("sent size: %d, SeqNO: %lld\n", msg->packetSize, msg->SeqNo);
	char buf[sizeof(struct sendQ_slot)];
	memcpy(buf, msg, sizeof(struct sendQ_slot));
    return (sendto(socket, buf, sizeof(struct sendQ_slot), 0, (struct sockaddr*) send_address, 
    	sizeof(struct sockaddr)));
}

bool swpInWindows(long long AckNum, long long left, long long right)
{
	if( AckNum >= left && AckNum <= right)
		return true;

	return false;
}

int fill_sending_window(SwpState * state, long long LAR, long long LFS, char* filename, unsigned long long int bytesToTransfer)
{
	//printf("start %lld, end %lld\n", LFS + 1, LAR + SWS  );
	int readLength;
	int i;
	for(i = LFS + 1; i < LAR + SWS + 1 ; i++)
	{
		state->sendQ[i%SWS].SeqNo = Send_Sequence_Number;
		if((readLength = handle_input_file(filename, MAXDATASIZE, state->sendQ[i%SWS].msg, global_file_offset, bytesToTransfer)) < MAXDATASIZE )
			// && state->sendQ[i%SWS].msg[0] == '\0'
		{
			printf("EOF.\n");
			//return 1;
		}
		printf("read length = %d\n", readLength );
		state->sendQ[i%SWS].packetSize = readLength;
		//printf("message:%s\n", state->sendQ[i%SWS].msg);
		Send_Sequence_Number++;
		
	}
	return 0;
}

static int deliverSWP(SwpState * state, struct recvQ_slot * recvBuf, 
	int socket, struct sockaddr_in * send_address, char * filename, unsigned long long int bytesToTransfer)
{
	long long ACK_Sequence_Number;
	ACK_Sequence_Number = recvBuf->SeqNo;
	if(swpInWindows(ACK_Sequence_Number, state -> LAR + 1, state-> LFS))
	{
		do
		{
			struct sendQ_slot * slot;
			slot = &state -> sendQ[++state -> LAR % SWS];
		} while (state->LAR != ACK_Sequence_Number);

		fill_sending_window(state, state-> LAR, state-> LFS, filename, bytesToTransfer);
		send_multiple_packet(socket, send_address, state, state-> LFS + 1, state -> LAR + SWS);
		state -> LFS = state -> LAR + SWS;
		printf("LAR:%lld, LFS:%lld\n", state->LAR,state->LFS);
		return 1;
	}
	return 1;
}


int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	udpPort = (unsigned short int)atoi(argv[2]);
	
	reliablyTransfer(argv[1], udpPort, argv[3], atoll(argv[4]));
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

	unsigned long long int numBytes;

	struct timeval global_timer_start;
	struct timeval global_timer_now;
	struct timeval global_timer_fin_start;
	gettimeofday(&global_timer_start, 0);

	char recvBuf[sizeof(struct recvQ_slot)];

	global_file_offset = 0;
	Send_Sequence_Number = 0;

	//get total number of packets
	total_packets = bytesToTransfer/MAXDATASIZE + (bytesToTransfer % MAXDATASIZE != 0);

	//create socket

	int senderSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(senderSocket < 0)
		perror("socket()");

		//get receiver IP address

	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	if (setsockopt(senderSocket, SOL_SOCKET, SO_RCVTIMEO, &tv,
				sizeof(tv)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		
	memset(&transfer_addr, 0, sizeof(transfer_addr));
	socklen_t transfer_addr_len = sizeof(transfer_addr);
	transfer_addr.sin_family = AF_INET;
	transfer_addr.sin_port = htons(hostUDPport);
	inet_pton(AF_INET, hostname, &transfer_addr.sin_addr);

	SwpState curr_state;
	curr_state.LAR = -1;
	curr_state.LFS = 0;


	curr_state.sendQ[0].SeqNo = 0;
	curr_state.sendQ[0].packetSize = handle_input_file(filename, MAXDATASIZE, curr_state.sendQ[0].msg, global_file_offset, bytesToTransfer);
	//global_file_offset+=MAXDATASIZE;

	//fill_sending_window(&curr_state, curr_state.LAR, curr_state.LFS, filename, bytesToTransfer);

	send_multiple_packet(senderSocket, &transfer_addr, &curr_state, 0, 0);
	Send_Sequence_Number ++;
	curr_state.LFS = 0;

	printf("before while loop: LAR = %lld, total_packets = %lld\n, file_offset = %lld", curr_state.LAR, total_packets, global_file_offset);
	while(curr_state.LAR < total_packets){
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
		 		printf("resend packet, seq_num = %lld, LAR = %lld\n, total_packets = %lld",
		 		 (curr_state.sendQ[(curr_state.LAR + 1)%SWS].SeqNo), curr_state.LAR, total_packets);

		 		send_packet(senderSocket, &transfer_addr, &(curr_state.sendQ[(curr_state.LAR + 1)%SWS]), MAXDATASIZE);
		 		continue;
		 	}

        }
       

        printf("received bytes = %lld\n", numBytes);
        struct recvQ_slot recv_pkt;
        memcpy(&recv_pkt, recvBuf, sizeof(struct recvQ_slot));
        printf("ACK:%lld\n", recv_pkt.SeqNo);
        deliverSWP(&curr_state, &recv_pkt, senderSocket, &transfer_addr, filename, bytesToTransfer);

        //  for (i = 0; i < SWS; ++i)
        // {
        // 	/* code */
        // 	printf(" send window %d, seq_num %lld\n", i, curr_state.sendQ[i].SeqNo);
        // }
	}

	//now start sending FIN signals
	gettimeofday(&global_timer_fin_start, 0);
	while(1)
	{
		struct sendQ_slot FINpacket;
		FINpacket.packetType = 1;
		if (send_packet(senderSocket, &transfer_addr, &FINpacket, MAXDATASIZE) < 0)
		{
			perror("error sending FIN:");
			exit(1);
		}

      	if((numBytes = recvfrom(senderSocket,recvBuf,sizeof(struct recvQ_slot),0,
		 		(struct sockaddr *) &transfer_addr,&transfer_addr_len))==-1)
      	{
      		perror("can't get FIN error:");
      		gettimeofday(&global_timer_now, 0);
        	printf("Completion time = %d\n",(int) (global_timer_now.tv_sec - global_timer_start.tv_sec));
        	if(global_timer_now.tv_sec - global_timer_fin_start.tv_sec >1)
        		exit(1);
      		//exit(1);
      	}

      	struct recvQ_slot fin_recv_pkt;
        memcpy(&fin_recv_pkt, recvBuf, sizeof(struct recvQ_slot));

        if(fin_recv_pkt.packetType == 1)
        {
        	gettimeofday(&global_timer_now, 0);
        	printf("Completion time = %d\n",(int) (global_timer_now.tv_sec - global_timer_start.tv_sec));
        	exit(1);
        }

	}

}