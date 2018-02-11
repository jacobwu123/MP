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

#define PORT "80"
#define MAXDATASIZE 1024
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char const *argv[])
{
	char hostname[MAXDATASIZE];
	char filename[MAXDATASIZE];
	int i,j;
	struct addrinfo hints, *servinfo,*p;
	int rv;
	int sockfd;
	FILE *fp;
	char buf[MAXDATASIZE];
	if(argc != 2){
		fprintf(stderr, "usage: client hostname\n");
	}

	for(i = 7;;i++){
		hostname[i-7] = argv[1][i];
		if(argv[1][i+1] == '/')
			break;
	}

	for(j = i+1; ;j++){
		filename[j-i-1] = argv[1][j];
		if(argv[1][j] == '\0')
			break;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(hostname, PORT, &hints, &servinfo))!=0){
		fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(rv));
		return 1;
	}

	for(p = servinfo; p!=NULL;p= p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, 
			p->ai_protocol)) == -1){
			perror("client:socket");
			continue;
		}

		if(connect(sockfd, p->ai_addr, p->ai_addrlen)==-1){
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if(p == NULL){
		fprintf(stderr, "client: fail to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo);
	if(send(sockfd, "GET /index.html HTTP/1.1\r\nHost: illinois.edu:80\r\n\r\n", 
			strlen("GET /index.html HTTP/1.1\r\nHost: illinois.edu:80\r\n\r\n"),0) == -1){
		perror("send");
	}
	fp = fopen("output","ab");
	while(recv(sockfd,buf, MAXDATASIZE-1,0) >0){
		if(fp!=NULL){
			fputs(buf,fp);
		}
	}
	fclose(fp);
	return 0;
}