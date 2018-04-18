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

#define MAXDATASIZE 1
#define SWS 1
#define RWS 10
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
		long long SeqNo;
		char msg[MAXDATASIZE];
	} sendQ[SWS];

	/* receiver side state: */
	long long NFE; /* seqno of next frame
	expected */

	struct recvQ_slot {
		long long SeqNo;
		char msg[MAXDATASIZE];
	} recvQ[RWS];


} SwpState;

#endif