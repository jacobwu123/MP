#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define MAXDATASIZE 1024 // max number of bytes we can get at once 
#define MSGNUMB 10
void *get_in_addr(struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char*argv[])
{
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int sockfd;
	char s[INET6_ADDRSTRLEN];
	int numbytes;
	char buf[MAXDATASIZE];
	int i;

	if(argc != 4){
		fprintf(stderr, "usage:client inputs\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, 
				p->ai_protocol)) == -1){
			perror("client: socket");
			continue;
		}

		if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if(p == NULL){
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo);
	if (send(sockfd, "HELO\n", strlen("HELO\n"), 0) == -1)
		perror("send");
	

	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';

	if (send(sockfd, "USERNAME cwu57\n", strlen("USERNAME cwu57\n"), 0) == -1)
		perror("send");

	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1){
		perror("recv");
		exit(1);
	}

	for(i = 0; i < MSGNUMB; i++){
		if (send(sockfd, "RECV\n", strlen("RECV\n"), 0) == -1)
			perror("send");
		if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1){
			perror("recv");
			exit(1);
		}
		buf[numbytes] = '\0';

		printf("Received: %s", &buf[12]);
	}

	if (send(sockfd, "BYE\n", strlen("BYE\n"), 0) == -1)
		perror("send");
	

	if((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1){
		perror("recv");
		exit(1);
	}
	buf[numbytes] = '\0';

	close(sockfd);
	return 0;

}
