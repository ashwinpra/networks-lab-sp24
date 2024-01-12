#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#define MAXLINE 1024

int main() {
    int sockfd; 
    struct sockaddr_in servaddr, cliaddr; 
    int n; 
    socklen_t len; 
    char buffer[MAXLINE];

    // socket file descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd<0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(8181);

    // binding the socket with server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("\nServer Running....\n");

    len = sizeof(cliaddr);
    n = recvfrom(sockfd, (char*)buffer, MAXLINE, 0, (struct sockaddr*)&cliaddr, &len);
    buffer[n] = '\0';

    // received filename from client, open it 
    FILE* fp = fopen(buffer, "r");

    if (fp == NULL) {
        // send NOTFOUND <filename> to client
        char notfound[MAXLINE];
        strcpy(notfound, "NOTFOUND ");
        strcat(notfound, buffer);

        sendto(sockfd, (const char*)notfound, strlen(notfound), 0, (const struct sockaddr*) &cliaddr, sizeof(cliaddr));

        close(sockfd);
        exit(EXIT_FAILURE);
    }
    char word[MAXLINE];

    // send first line of file (HELLO) to client
    fgets(word, MAXLINE, fp);
    sendto(sockfd, (const char*)word, strlen(word), 0, (const struct sockaddr*) &cliaddr, sizeof(cliaddr));
    
    char word_recv[MAXLINE];
    int word_num = 1;

    while(fscanf(fp, "%s", word) != EOF) {

        // remove trailing newline (if any)
        for (int i=0; i<strlen(word); i++) {
            if (word[i] == '\n') {
                word[i] = '\0';
                break;
            }
        }

        sprintf(word_recv, "WORD%d", word_num); // word_recv = WORDi

        // receive word from client
        n = recvfrom(sockfd, (char*)buffer, MAXLINE, 0, (struct sockaddr*)&cliaddr, &len);
        buffer[n] = '\0';

        // if received word is WORDi, then send the string
        if (strcmp(buffer, word_recv) == 0) {
            // send word to client
            sendto(sockfd, (const char*)word, strlen(word), 0, (const struct sockaddr*) &cliaddr, sizeof(cliaddr));
            word_num++;
        }
    }

    fclose(fp);
    close(sockfd);

    return 0;
}
