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

void messageCat(char* message, char* hostname, char* filename, char* portnumber){
	strcpy(message, "GET ");
	strcat(message, filename);
	strcat(message," HTTP/1.0\r\nHost: ");
	strcat(message, hostname);
	strcat(message,":");
	strcat(message,portnumber);
	strcat(message,"\r\n\r\n");
}
int getPortnumber(const char* str, char* portnumber){
	char* temp;
	int i = 0;

	temp = strstr(str, ":"); 
	temp ++;
	while(1){
		if(temp[i] == '/'){
			portnumber[i] = '\0';
			break;
		}
		portnumber[i] = temp[i];
		i++;
	}
	return strlen(portnumber);
}

void getHost(const char* url, char* hostname, int offset, char* portnumber, 
				char*filename)
{
	int i,j;

	for(i = offset;;i++){
		hostname[i-offset] = url[i];
		if(url[i+1] == '/' || url[i+1] == ':'){
			hostname[i-6] = '\0';
			break;
		}
	}

	if(url[i+1] == ':')
		i += getPortnumber(&(url[7]), portnumber);
	else
		strcpy(portnumber, PORT);

	for(j = i+1; ;j++){
		filename[j-i-1] = url[j];
		if(url[j+1] == '\0'){
			filename[j-i] = '\0';
			break;
		}
	}
}

int getServer(struct addrinfo* servinfo, struct addrinfo* *p){
	int sockfd;

	for(*p = servinfo; *p!=NULL;*p= (*p)->ai_next){
		if((sockfd = socket((*p)->ai_family, (*p)->ai_socktype, 
			(*p)->ai_protocol)) == -1){
			perror("client:socket");
			continue;
		}
		if(connect(sockfd, (*p)->ai_addr, (*p)->ai_addrlen)==-1){
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	return sockfd;
}

int readData(int sockfd, char* url){
	FILE *fp;
	char buf[MAXDATASIZE];
	int isRedirection = 0;
	char* contents;
	int count;
	int i = 0;
	char* temp; 

	fp = fopen("output","ab");
	//read the incoming data and find"\r\n\r\n"
	if((count = recv(sockfd, buf, MAXDATASIZE,0)) >0){
		if(strstr(buf, "301 Moved Permanently\r\n") != NULL){
			isRedirection = 1;
			contents = buf;
			temp = strstr(buf, "Location: ");
			temp += strlen("Location: ");
			while(1){
				url[i] = temp[i];
				if(temp[i+1] == '\r'){
					url[i+1] = '\0';
					break;
				} 
				i++;
			}
			printf("%s\n", url);
		}
		else{
			contents = strstr(buf,"\r\n\r\n");
			contents += 4;
		}
		if(fp!= NULL)
			fwrite(contents,1, count-(contents-buf),fp);
	}

	while((count = recv(sockfd,buf, MAXDATASIZE,0)) >0){
		if(fp!=NULL){
			fwrite(buf,1,count,fp);
		}
	}
	fclose(fp);

	if(isRedirection)
		remove("output");
	return isRedirection;
}

int main(int argc, char const *argv[])
{
	char hostname[MAXDATASIZE];
	char filename[MAXDATASIZE];
	char message[MAXDATASIZE];

	struct addrinfo hints, *servinfo,*p;
	int rv;
	int sockfd;
	char portnumber[10];
	char url[MAXDATASIZE];

	if(argc != 2){
		fprintf(stderr, "usage: client hostname\n");
	}

	getHost(&argv[1][0], hostname, 7, portnumber, filename);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(hostname, portnumber, &hints, &servinfo))!=0){
		fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(rv));
		return 1;
	}

	sockfd = getServer(servinfo, &p);

	if(p == NULL){
		fprintf(stderr, "client: fail to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo);
	messageCat(message, hostname, filename, portnumber);
	
	if(send(sockfd, message, strlen(message),0)== -1) perror("send");
	
	while(readData(sockfd, &(url[0])) == 1){
		printf("reach here\n");
		printf("%s\n", url);
		memset(&hostname[0], 0, sizeof(hostname));
		memset(&portnumber[0], 0, sizeof(portnumber));
		memset(&filename[0], 0, sizeof(filename));

		getHost(url, hostname, 7, portnumber, filename);
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		if((rv = getaddrinfo(hostname, portnumber, &hints, &servinfo))!=0){
			fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(rv));
			return 1;
		}
		sockfd = getServer(servinfo, &p);
		if(p == NULL){
			fprintf(stderr, "client: fail to connect\n");
			return 2;
		}
		freeaddrinfo(servinfo);
		memset(&message[0], 0, sizeof(message));
		messageCat(message, hostname, filename, portnumber);
		if(send(sockfd, message, strlen(message),0)== -1) perror("send");
	}

	return 0;
}
