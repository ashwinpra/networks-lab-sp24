#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    int sockfd; 
    struct sockaddr_in serv_addr; 
    char buf[100];

    sockfd = socket(AF_INET, SOCK_STREAM, 0); 

    if(sockfd < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    serv_addr.sin_family = AF_INET; 
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(20000);

    if((connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) < 0) {
        perror("Unable to connect to server\n");
        exit(0);
    }

    for(int i=0; i<100; i++) buf[i] = '\0';
    recv(sockfd, buf, 100, 0);
    printf("Received \"%s\" in client\n", buf);

    strcpy(buf, "Hello server!");
    send(sockfd, buf, strlen(buf)+1, 0);

    close(sockfd);

    return 0;
}