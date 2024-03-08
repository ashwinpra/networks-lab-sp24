/*
    Networks Lab Assignment 4
    Peer-to-peer chat application
    Ashwin Prasanth - 21CS30009
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 3
#define MAX_MSG_LEN 250
#define MAX_USER_LEN 10
#define max(a, b) ((a > b) ? a : b)

typedef struct{
    char username[MAX_USER_LEN];
    char ip[20];
    int port;
    int sockfd;
}user_info;

user_info table[MAX_CLIENTS]; // table of user_info

int main(int argc, char const *argv[]){

    if(argc != 2){
        printf("Usage: %s <username>\n", argv[0]);
        exit(0);
    }

    char my_username[10];
    strcpy(my_username, argv[1]);

    // hardcoding the user table
    // adding an extra entry for sockfd so that connection need not be re-established for known users
    strcpy(table[0].username, "user_1");
    strcpy(table[0].ip, "127.0.0.1");
    table[0].port = 50000;
    table[0].sockfd = -1;
    strcpy(table[1].username, "user_2");
    strcpy(table[1].ip, "127.0.0.1");
    table[1].port = 50001;
    table[1].sockfd = -1;
    strcpy(table[2].username, "user_3");
    strcpy(table[2].ip, "127.0.0.1");
    table[2].port = 50002;
    table[2].sockfd = -1;

    int sockfds[MAX_CLIENTS+2];

    sockfds[0] = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfds[0] < 0) {
        perror("Cannot create socket\n");
        exit(0);
    }
    // sockfds[1] will wait for stdin
    sockfds[1] = 0;

    char ip[20];
    int port=0;

    for(int i = 0; i < MAX_CLIENTS; i++){
        if(strcmp(table[i].username, my_username) == 0){
            strcpy(ip, table[i].ip);
            port = table[i].port;
            table[i].sockfd = sockfds[0];
            break;
        }
    }

    if(!port){
        printf("User not found\n");
        exit(0);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ip, &server_addr.sin_addr);

    if (bind(sockfds[0], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Unable to bind local address\n");
        exit(0);
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfds[0], &fds);
    FD_SET(sockfds[1], &fds);
    int nfds = sockfds[0] + 1;

    listen(sockfds[0], 5);

    int num_clients = 2; // index for sockfds array

    while (1) {

        if(num_clients > MAX_CLIENTS+2){
            printf("Max clients reached\n");
            break;
        }

        struct timeval tv;
        tv.tv_sec = 300;
        tv.tv_usec = 0;
        int ret = select(nfds, &fds, NULL, NULL, &tv);
        if(ret == 0){
            printf("Peer Timeout\n");
            break;
        }

        // check for new connection at sockfds[0]
        if(FD_ISSET(sockfds[0], &fds)) {
            // accept the connection, also add to array of fds
            struct sockaddr_in cli_addr;
            socklen_t clilen = sizeof(cli_addr);
            sockfds[num_clients] = accept(sockfds[0], (struct sockaddr *)&cli_addr, &clilen);
            if (sockfds[num_clients] < 0) {
                perror("Error in accept\n");
                exit(0);
            }
            FD_SET(sockfds[num_clients], &fds);
            nfds = max(nfds, sockfds[num_clients] + 1);
            num_clients++;
        }

        // check for user input at sockfds[1]
        if(FD_ISSET(sockfds[1], &fds)) {
            char buf[MAX_MSG_LEN+MAX_USER_LEN+2];
            fgets(buf, MAX_MSG_LEN+MAX_USER_LEN+2, stdin);
            fflush(stdin);

            // check if format: <username>/<message> is followed
            char* find = strchr(buf, '/');
            if (find == NULL) {
                printf("Invalid format\n");
                continue;
            }
            char* username = strtok(buf, "/");
            char* message = strtok(NULL, "\n");

            for(int i = 0; i < MAX_CLIENTS; i++){
                if(strcmp(table[i].username, username) == 0){

                    if(table[i].sockfd == -1){
                        // need to establish connection
                        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
                        if (sockfd < 0) {
                            perror("Cannot create socket\n");
                            exit(0);
                        }
                        struct sockaddr_in serv_addr;
                        serv_addr.sin_family = AF_INET;
                        serv_addr.sin_port = htons(table[i].port);
                        inet_aton(table[i].ip, &serv_addr.sin_addr);
                        // bind with current port
                        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                            perror("Connection failed\n");
                            exit(0);
                        }
                        table[i].sockfd = sockfd;
                        sockfds[num_clients] = sockfd;
                        nfds = max(nfds, sockfd + 1);
                        num_clients++;
                    }

                    // send message now
                    char* full_message = (char*)malloc(strlen(my_username) + strlen(message) + 3);
                    sprintf(full_message, "%s: %s\n", my_username, message);
                    send(table[i].sockfd, full_message, strlen(full_message), 0);
                    break;

                }
            }
        }

        // check for messages from clients
        for (int i = 2; i < num_clients; i++) {
            if (FD_ISSET(sockfds[i], &fds)) {

                char buf[MAX_MSG_LEN+MAX_USER_LEN+2];
                int n = recv(sockfds[i], buf, MAX_MSG_LEN+MAX_USER_LEN+2, 0);

                if(n==0) {
                    // some peer disconnected
                    printf("Connection disconnected by peer\n");
                    // closing the connection, removing from fds, updating in table, and updating sockfds array
                    close(sockfds[i]);
                    FD_CLR(sockfds[i], &fds);
                    for(int j = 0; j < MAX_CLIENTS; j++){
                        if(table[j].sockfd == sockfds[i]){
                            table[j].sockfd = -1;
                            break;
                        }
                    }
                    for(int j = i; j < num_clients; j++){
                        sockfds[j] = sockfds[j+1];
                    }
                    num_clients--;
                    continue;
                }

                else {
                    for (int j = 0; j < strlen(buf); j++) {
                        if (buf[j] == '\n') {
                            buf[j] = '\0';
                            break;
                        }
                    }
                    char* username = strtok(buf, ":");
                    char* message = strtok(NULL, "\n");
                    printf("Message from %s: %s\n", username, message);
                    for(int j = 0; j < MAX_CLIENTS; j++){
                        if(strcmp(table[j].username, username) == 0){
                            table[j].sockfd = sockfds[i];
                            break;
                        }
                    }
                }
            }
        }

        // reset the fds
        FD_ZERO(&fds);
        for(int i = 0; i < num_clients; i++){
            FD_SET(sockfds[i], &fds);
            nfds = max(nfds, sockfds[i] + 1);
        }
    }

    // close all sockets
    close(sockfds[0]);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(table[i].sockfd != -1)
            close(table[i].sockfd);
    }

    return 0;
}