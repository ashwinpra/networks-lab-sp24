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
    struct sockaddr_in servaddr; 
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(8080);
    inet_aton("127.0.0.1", servaddr.sin_addr.s_addr);

    char hello[] = "Hello world!";

    sendto(sockfd, (const char*) hello, strlen(hello), 0, (struct sockaddr*) &servaddr, sizeof
    (servaddr));

    close(sockfd);


    return 0;
}
