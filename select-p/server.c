#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#define MAX_CLIENTS 10
#define max(a, b) ((a > b) ? a : b)

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(0);
    }

    int port = atoi(argv[1]);

    int sockfds[MAX_CLIENTS];

    sockfds[0] = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfds[0] < 0) {
        perror("Cannot create socket\n");
        exit(0);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockfds[0], &fds);
    int nfds = sockfds[0] + 1;

    if (bind(sockfds[0], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Unable to bind local address\n");
        exit(0);
    }

    listen(sockfds[0], 5);

    int num_clients = 1; 

    while (1) {
        select(nfds, &fds, NULL, NULL, NULL);

        if(FD_ISSET(sockfds[0], &fds)) {
            // accept the connection
            struct sockaddr_in cli_addr;
            int clilen = sizeof(cli_addr);
            sockfds[num_clients] = accept(sockfds[0], (struct sockaddr *)&cli_addr, &clilen);
            // get the client IP address
            if (sockfds[num_clients] < 0) {
                perror("Error in accept\n");
                exit(0);
            }
            FD_SET(sockfds[num_clients], &fds);
            nfds = max(nfds, sockfds[num_clients] + 1);
            num_clients++;
            printf("Accepted client\n");
        }

        for (int i = 1; i < num_clients; i++) {
            if (FD_ISSET(sockfds[i], &fds)) {
                char buf[100];
                int n = recv(sockfds[i], buf, 100, 0);
                if (n < 0) {
                    perror("Error in reading\n");
                    exit(0);
                }
                else {
                    buf[n] = '\0';
                    printf("%d: Received: %s\n", i, buf);
                }
            }

        }
    }
    return 0;
}
