#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10
#define MAX_DATA_SIZE 1024 
void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}
void *get_in_addr(struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int parseMessage(char* filename, char*buf){
	char* temp;
	int i = 0;

	temp = strstr(buf, "GET ");

	if(temp != NULL || strstr(buf, "HTTP/1.") !=NULL || strstr(buf, "\r\n\r\n") != NULL){
		temp += 6;
		while(1){
			if(temp[i] == ' '){
				filename[i] = '\0';
				break;
			}
			filename[i] = temp[i];
			i++;
		}
	}
	else{
		return 0;
	}
	return 1;
}

int main(int argc, char const *argv[])
{
	struct addrinfo hints, *servinfo, *p;
	int sockfd, new_fd;
	int rv;
	int yes =1;
	struct sigaction sa;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];
	char buf[MAX_DATA_SIZE];
	int numbytes;
	char filename[MAX_DATA_SIZE];
	FILE* fp;
	int count;
	char returnMessage[20];

	if(argc != 2){
		fprintf(stderr, "usage: server portnumber\n");
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for(p =  servinfo; p!= NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, 
				p->ai_protocol)) == -1){
			perror("server: socket");
			continue;
		}

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT|SO_REUSEADDR, &yes, 
				sizeof(int)) == -1){
			perror("setsockopt");
			continue;
		}
			
		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("server:bind");
			continue;
		}

		break;
	}

	if(p == NULL){
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	if(listen(sockfd, BACKLOG) == -1){
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1){
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1){
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if(new_fd == -1){
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if(!fork()){
			close(sockfd);
			if((numbytes = recv(new_fd, buf, MAX_DATA_SIZE-1, 0)) == -1){
				perror("recv");
				exit(1);
			}
	    	if(parseMessage(filename, buf)){
	    		if((fp =fopen(filename, "r")) != NULL)
	    			strcpy(returnMessage, "HTTP/1.0 200 OK\r\n\r\n");
	    		else
	    			strcpy(returnMessage, "HTTP/1.0 404 Not Found\r\n\r\n");
	    	}
	    	else{
	    		strcpy(returnMessage, "HTTP/1.0 400 Bad Request\r\n\r\n");
	    	}
			//send returnMessages
			if(send(new_fd, returnMessage, strlen(returnMessage),0) == -1){
				perror("send");
			}
			while((count = fread(buf, 1, MAX_DATA_SIZE, fp)) > 0){
				printf("%d\n", count);
				if(send(new_fd, buf, count,0) == -1){
					perror("send");
				}
			}
			fclose(fp);
			close(new_fd);
			exit(0);
		}
		close(new_fd);

	}

	return 0;
}
