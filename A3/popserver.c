// POP3 server
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

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    int pop3_port = atoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Cannot create socket\n");
        exit(0);
    }

    return 0;
}