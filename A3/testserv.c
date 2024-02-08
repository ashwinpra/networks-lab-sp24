#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LEN 80
#define MAX_LINES 50 
#define CRLF "\r\n"

int main(){
    int sockfd, newsockfd ; 
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;

	char buf[101];		/* We will use this buffer for communication */

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Cannot create socket\n");
		exit(0);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(2500);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); 

    printf("Server running...\n");

    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0) {
        printf("Error in accept\n");
        exit(0);
    }

    // send mail to client from ashwin/mymailbox

    FILE *fp = fopen("ashwin/mymailbox", "r");
    if (fp == NULL) {
        printf("Error opening file\n");
        exit(0);
    }

    while(fgets(buf, 100, fp) != NULL) {
        send(newsockfd, buf, strlen(buf), 0);
    }

}