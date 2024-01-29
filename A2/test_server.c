#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FILE_ACCESS_SIZE 70 // using a different value in server just to show that it does not have to be the same

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

    printf("Server running...\n");

    while(1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);

        if(newsockfd < 0) {
            perror("Error in accept\n");
            exit(0);
        }

        if (fork() == 0) {
            int n, k;

            // receive k from client
            char k_str[3];
            recv(newsockfd, k_str, 3, 0);
            k = atoi(k_str);

            // making a new text file with naming convention as given in assignmemnt
            char filename[100];
            strcpy(filename, inet_ntoa(cli_addr.sin_addr));
            char port[20];
            sprintf(port, ".%d", ntohs(cli_addr.sin_port));
            strcat(filename, port);
            strcat(filename, ".txt");

            int fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

            char buf[FILE_ACCESS_SIZE];
            // reading data from client 70 bytes at a time until EOF delimeter is received
            while((n = recv(newsockfd, buf, FILE_ACCESS_SIZE, 0)) > 0) {
                if(buf[n-2] == '$') {
                    // looking for EOF delimieter sent by client ($)
                    if(strlen(buf) == 2) {
                        break;
                    }
                    else{
                        buf[n-2] = '\0';
                        n -= 2;
                        write(fd, buf, n);
                        break;
                    }
                }
                // writing it to the file
                write(fd, buf, n);
            }
            close(fd);

            // reopening the file as read-only to encrypt it
            fd = open(filename, O_RDONLY);

            char enc_filename[100];
            strcpy(enc_filename, filename);
            strcat(enc_filename, ".enc");

            int enc_fd = open(enc_filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

            // encrypting 70 bytes at a time and writing it to the encrypted file
            char word[FILE_ACCESS_SIZE];
            while((n = read(fd, word, FILE_ACCESS_SIZE)) > 0) {
                write(enc_fd, encrypt_word(word, k), n);
            }

            close(fd);
            close(enc_fd);

            // send encrypted file back to client
            fd = open(enc_filename, O_RDONLY);

            char enc_word[FILE_ACCESS_SIZE];
            while((n = read(fd, enc_word, FILE_ACCESS_SIZE)) > 0) {
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