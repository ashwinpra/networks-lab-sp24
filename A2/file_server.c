#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

char* encrypt_word(char* word, int k){
    for (int i = 0; i < strlen(word); i++) {
        if (word[i] >= 'a' && word[i] <= 'z') 
            word[i] = (word[i] - 'a' + k) % 26 + 'a';
        else if (word[i] >= 'A' && word[i] <= 'Z') 
            word[i] = (word[i] - 'A' + k) % 26 + 'A';
    }
    return word;
}

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
    serv_addr.sin_port = htons(8282);

    if(bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
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

        // receive k from client
        // for(int i=0; i<100; i++) buf[i] = '\0';
        // recv(newsockfd, buf, 100, 0);
        // int k = atoi(buf);

        // printf("Received \"%d\" in server\n", k);

        // // receive large text from client
        // for(int i=0; i<100; i++) buf[i] = '\0';
        // recv(newsockfd, buf, 100, 0);
        // printf("Received \"%s\" in server\n", buf); //! check here

        strcpy(buf,"Message from server");
		send(newsockfd, buf, strlen(buf) + 1, 0);

        recv(newsockfd, buf, 100, 0);
		printf("%s\n", buf);

        close(newsockfd);
    }

    return 0;
}