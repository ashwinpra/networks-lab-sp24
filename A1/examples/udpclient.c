// A Simple Client Implementation
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
  
int main() { 
    int sockfd, err; 
    struct sockaddr_in servaddr; 
    int n;
    socklen_t len; 
    char *hello = "CLIENT:HELLO"; 
  
    // Creating socket file descriptor 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sockfd < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&servaddr, 0, sizeof(servaddr)); 
      
    // Server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(8181); 
    err = inet_aton("127.0.0.1", &servaddr.sin_addr);
    if (err == 0) {
	   printf("Error in ip-conversion\n");
	   exit(EXIT_FAILURE);
    }

    // connect to server
    // connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    // send(sockfd, (const char *)hello, strlen(hello), 0);

    sendto(sockfd, (const char *)hello, strlen(hello), 0, 
			(const struct sockaddr *) &servaddr, sizeof(servaddr)); 
            
    printf("Hello message sent from client\n"); 
    getchar(); 
    
          
    close(sockfd); 
    return 0; 
} 
