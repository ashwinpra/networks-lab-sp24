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
    char filename[100];

    printf("Enter filename (with extension): ");
    scanf("%s", filename);

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

    // send filename to server
    sendto(sockfd, (const char*)filename, strlen(filename), 0, (const struct sockaddr*) &servaddr, sizeof(servaddr));

    // receive HELLO from server
    char buffer[MAXLINE];
    len = sizeof(servaddr);
    n = recvfrom(sockfd, (char*)buffer, MAXLINE, 0, (struct sockaddr*)&servaddr, &len);
    buffer[n] = '\0';

    // it is possible that the server sends NOTFOUND <filename> instead of HELLO
    char notfound[MAXLINE];
    strcpy(notfound, "NOTFOUND ");
    strcat(notfound, filename);

    if (strcmp(buffer, notfound) == 0) {
        printf("File not found\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // create local file
    FILE* fp = fopen("client.txt", "w");

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

        // if received word is END, then break
        if (strcmp(buffer, "END") == 0) {
            break;
        }

        // else, write word to local file
        fprintf(fp, "%s\n", buffer);
    }

    fclose(fp);
    close(sockfd);

    return 0;
}