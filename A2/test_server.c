#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
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
    serv_addr.sin_port = htons(8383);

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

        // fork from here

        if (fork() == 0) {
            // keep receiving words from client
            int n, k;

            // receive k from client
            char k_str[100];
            n = recv(newsockfd, k_str, 100, 0);
            k_str[n] = '\0';
            k = atoi(k_str);

            // make a new text file
            char filename[100];
            strcpy(filename, inet_ntoa(cli_addr.sin_addr));
            char port[20];
            sprintf(port, ".%d", ntohs(cli_addr.sin_port));
            strcat(filename, port);
            strcat(filename, ".txt");

            int fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

            while((n = recv(newsockfd, buf, 100, 0)) > 0) {
                // buf[n] = '\0';
                if(buf[n-2] == '$') {
                    if(strlen(buf) == 2) {
                        printf("EOF received.\n");
                    }
                    else{
                        buf[n-2] = '\0';
                        n -= 2;
                        write(fd, buf, n);
                    }
                    break;
                }
                write(fd, buf, n);
            }
            close(fd);

            fd = open(filename, O_RDONLY);

            char enc_filename[100];
            strcpy(enc_filename, filename);
            strcat(enc_filename, ".enc");

            int enc_fd = open(enc_filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

            char word[100];
            while((n = read(fd, word, 100)) > 0) {
                write(enc_fd, encrypt_word(word, k), n);
            }

            close(fd);
            close(enc_fd);

            // send encrypted file back to client
            fd = open(enc_filename, O_RDONLY);

            char enc_word[100];
            while((n = read(fd, enc_word, 100)) > 0) {
                send(newsockfd, enc_word, n, 0);
            }
            // send EOF delimeter
            strcpy(buf, "$");
            send(newsockfd, buf, strlen(buf)+1, 0);
            close(fd);

            printf("Encrypted file sent to client.\n");

            close(newsockfd);
            exit(0);
        }
    }

    return 0;
}