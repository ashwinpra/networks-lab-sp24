#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>

int main(int argc, char const *argv[])
{
    struct sockaddr_in cliaddr, servaddr; 
    int clilen; 

    int sockfd = connect(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(8080);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

    char buf[100];

    clilen = sizeof(cliaddr);
    recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*) &cliaddr, &clilen);

    return 0;
}
