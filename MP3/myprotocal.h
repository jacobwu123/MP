#ifndef MYPROTOCAL_H
#define MYPROTOCAL_H

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAXDATASIZE 1450
#define SWS 100
#define RWS 400
#define HLEN 3

// typedef struct header_struct
// {
// 	/* data */
// 	long long Send_Sequence_Number;
// 	long long ACK_Sequence_Number;
// } header;


typedef struct {
/* sender side state: */

	long long LAR; /* seqno of last ACK received */
	long long LFS; /* last frame sent */
	// struct header_struct SwpHeader; /* pre-initialized header */

	struct sendQ_slot {
		int packetSize;
		uint8_t packetType; //1 for data and 0 for FIN
		long long SeqNo;
		char msg[MAXDATASIZE];
	} sendQ[SWS];

	/* receiver side state: */
	long long NFE; /* seqno of next frame
	expected */

	struct recvQ_slot {
		int packetSize;
		uint8_t packetType; //0 for data ack and 1 for FINACK
		long long SeqNo;
		char msg[MAXDATASIZE];
	} recvQ[RWS];


} SwpState;

#endif