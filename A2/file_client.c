#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
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
    serv_addr.sin_port = htons(8282);

    if((connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) < 0) {
        perror("Unable to connect to server\n");
        exit(0);
    }

    char filename[100];
    printf("Enter filename (with extension): ");
    scanf("%s", filename);

    int fd = open(filename, O_RDONLY);
    while(fd < 0) {
        printf("File not found. Enter filename (with extension): ");
        scanf("%s", filename);
        fd = open(filename, O_RDONLY);
    }

    // int k;
    // printf("Enter key: ");
    // scanf("%d", &k);

    // // send k to server
    // char k_str[100];
    // sprintf(k_str, "%d", k);
    // send(sockfd, k_str, strlen(k_str)+1, 0);

    int i;
    for(i=0; i < 100; i++) buf[i] = '\0';
	recv(sockfd, buf, 100, 0);
	printf("%s\n", buf);

	
	strcpy(buf,"Very long Message from client, this message is so darn long ohmaaagoooood it really is, wonderful!");
	send(sockfd, buf, strlen(buf) + 1, 0);

    close(sockfd);

    return 0;
}