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
    int sockfd, err; 
    struct sockaddr_in servaddr; 
    int n; 
    socklen_t len; 
    char* filename = "test.txt";

    // socket file descriptor 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
    if (sockfd<0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // server information
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(8181);
    err = inet_aton("127.0.0.1", &servaddr.sin_addr);
    if (!err) {
        printf("Error in ip-conversion\n");
        exit(EXIT_FAILURE);
    }

    // client sends file name
    // server looks for file, does proper error handling, then reads the file and first sends HELLO 
    // client receives HELLO, creates a new local file and sends message WORD1 to server
    // server receives WORD1, sends next word after hello to client
    // client receives this word, writes it to the local file, and sends WORD2 to server
    // this continues until client receives END, after which it closes local file


    sendto(sockfd, (const char*)filename, strlen(filename), 0, (const struct sockaddr*) &servaddr, sizeof(servaddr));

    printf("Filename sent from client\n");

    // receive HELLO from server
    char buffer[MAXLINE];
    len = sizeof(servaddr);
    n = recvfrom(sockfd, (char*)buffer, MAXLINE, 0, (struct sockaddr*)&servaddr, &len);
    buffer[n] = '\0';

    // it is possible that the server sends NOTFOUND <filename> instead of HELLO
    char notfound[MAXLINE];
    strcpy(notfound, "NOTFOUND ");
    strcat(notfound, filename);

    if (strcmp(buffer, "NOTFOUND") == 0) {
        printf("File not found\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // create local file
    FILE* fp = fopen("client.txt", "w");

    if (fp == NULL) {
        printf("Error creating local file\n");
        exit(EXIT_FAILURE);
    }

    char word_send[MAXLINE];
    int word_num = 1; 

    while(1){
        sprintf(word_send, "WORD%d", word_num);
        word_num++;

        // send WORDi to server
        sendto(sockfd, (const char*)word_send, strlen(word_send), 0, (const struct sockaddr*) &servaddr, sizeof(servaddr));

        // receive corresponding word in text file 
        len = sizeof(servaddr);

        n = recvfrom(sockfd, (char*)buffer, MAXLINE, 0, (struct sockaddr*)&servaddr, &len);
        buffer[n] = '\0';

        if (strcmp(buffer, "END") == 0) {
            break;
        }

        // write word to local file
        fprintf(fp, "%s\n", buffer);
    }

    fclose(fp);
    close(sockfd);

    return 0;
}