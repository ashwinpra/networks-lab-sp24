#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include  <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

int get_choice() {
    int choice;
    printf("1. Manage Mail\n");
    printf("2. Send Mail\n");
    printf("3. Quit\n");
    printf("Enter your choice: ");
    scanf("%d", &choice);
    return choice;
}

int main(int argc, char const *argv[])
{
    if (argc != 4){
        printf("Usage: %s <server_ip> <smtp_port> <pop3_port>\n", argv[0]);
        exit(0);
    }
    char *server_ip = argv[1];
    int smtp_port = atoi(argv[2]);
    int pop3_port = atoi(argv[3]);

    struct sockaddr_in server_addr;

    char username[100], password[100];
    printf("Enter username: ");
    scanf("%s", username);
    printf("Enter password: ");
    scanf("%s", password);

    server_addr.sin_port = htons(smtp_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    int choice = get_choice();

    if (choice==1) {
        // manage mail
    }
    else if (choice==2) {
        // send mail
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0){
            perror("Cannot create socket\n");
            exit(0);
        }

        server_addr.sin_family = AF_INET;
        inet_aton(server_ip, &server_addr.sin_addr);
        server_addr.sin_port = htons(pop3_port);

        if((connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
            perror("Unable to connect to server\n");
            exit(0);
        }

        // handle the rest after reading RFC 5321

    }
    else if (choice==3) {
        exit(0);
    }
    
    else {
        printf("Invalid choice\n");
    }

    return 0;
}
