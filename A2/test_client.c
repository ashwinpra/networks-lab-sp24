#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FILE_ACCESS_SIZE 50

int main() {
    int sockfd; 
    struct sockaddr_in serv_addr; 

    char filename[100];

    while(1) {
        printf("File Encryption Client (Press Ctrl-C to quit anytime)\n");
        printf("Enter filename with extension: ");
        scanf("%s", filename);

        int fd = open(filename, O_RDONLY);
        while(fd < 0) {
            printf("File not found. Enter filename (with extension): ");
            scanf("%s", filename);
            fd = open(filename, O_RDONLY);
        }

        int k;
        printf("Enter key: ");
        scanf("%d", &k);

        sockfd = socket(AF_INET, SOCK_STREAM, 0); 

        if(sockfd < 0) {
            perror("Unable to create socket\n");
            exit(0);
        }

        serv_addr.sin_family = AF_INET; 
        inet_aton("127.0.0.1", &serv_addr.sin_addr);
        serv_addr.sin_port = htons(8383);

        if((connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) < 0) {
            perror("Unable to connect to server\n");
            exit(0);
        }

        // send k to server
        char k_str[3];
        if (k >= 10)
            sprintf(k_str, "%d", k);
        else
            sprintf(k_str, "0%d", k);
        send(sockfd, k_str, strlen(k_str)+1, 0);


        // read the contents of the file 50 bytes at a time
        char buf[FILE_ACCESS_SIZE];
        int n;
        while((n = read(fd, buf, FILE_ACCESS_SIZE)) > 0) {
            printf("Sending: %s\n", buf);
            send(sockfd, buf, n, 0);
        }

        // send EOF delimeter
        strcpy(buf, "$");
        send(sockfd, buf, strlen(buf)+1, 0);
        close(fd);

        // open a new file to store the encrypted file
        char enc_filename[100];
        strcpy(enc_filename, filename);
        strcat(enc_filename, ".enc");

        fd = open(enc_filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

        // receive the encrypted file from server
        char buf2[FILE_ACCESS_SIZE];
        while((n = recv(sockfd, buf2, FILE_ACCESS_SIZE, 0)) > 0) {
                // buf2[n] = '\0';
                if(buf2[n-2] == '$') {
                    if(strlen(buf2) == 2) {
                    
                    }
                    else{
                        buf2[n-2] = '\0';
                        n -= 2;
                        write(fd, buf2, n);
                    }
                    break;
                }
                write(fd, buf2, n);
        }

        printf("Encrypted file received from server.\nOriginal file: %s\nEncrypted file: %s\n\n", filename, enc_filename);

        close(fd);
        close(sockfd);
    }

    return 0;
}