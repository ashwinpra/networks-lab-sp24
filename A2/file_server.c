#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    int sockfd, newsockfd; 
    int clilen; 
    struct sockaddr_in cli_addr, serv_addr; 
    char buf[100];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0) {
        perror("Cannot create socket\n");
        exit(0);
    }

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);

    if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to bind local address\n");
        exit(0);
    }

    listen (sockfd, 5);

    while(1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);

        if(newsockfd < 0) {
            perror("Error in accept\n");
            exit(0);
        }

        strcpy(buf, "Hello client!");
        send(newsockfd, buf, strlen(buf)+1, 0); // assumes that it is received together (change it)

        recv(newsockfd, buf, 100, 0);

        printf("Received \"%s\" in server\n", buf);

        close(newsockfd);
    }

    return 0;
}