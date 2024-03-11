#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>

void *receiver(void *arg)
{
    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    while (1)
    {

        fd_set fds;
        FD_ZERO(&fds);
        int maxfd = -1;
        for (int i = 0; i < N; i++)
        {
            if (SM[i].free == 0)
            {
                FD_SET(SM[i].udpsockfd, &fds);
                if (SM[i].udpsockfd > maxfd)
                {
                    maxfd = SM[i].udpsockfd;
                }
            }
        }

        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;
        int retval = select(maxfd + 1, &fds, NULL, NULL, &tv);

        if (retval == -1)
        {
            perror("select()");
            exit(1);
        }

        else if (retval)
        {
            for (int i = 0; i < N; i++)
            {
                if (SM[i].free == 0 && FD_ISSET(SM[i].udpsockfd, &fds))
                {

                    // receive message
                    struct sockaddr_in cliaddr;
                    int len = sizeof(cliaddr);
                    char buf[1024];
                    int n = recvfrom(SM[i].udpsockfd, buf, 1024, 0, (struct sockaddr *)&cliaddr, &len);
                    if (n == -1)
                    {
                        perror("recvfrom()");
                        exit(1);
                    }
                    printf("Received message: %s\n", buf);
                    // add it to recv_buffer of the receiver socket (find by matching ip and port)
                    for (int j = 0; j < N; j++)
                    {
                        if (SM[j].free == 0 && SM[j].port == ntohs(cliaddr.sin_port) && strcmp(SM[j].ip, inet_ntoa(cliaddr.sin_addr)) == 0)
                        {
                            // todo: receive message
                            // this includes dropping headers, checking sequence number, adding to recv_buffer, sending ACK (?)
                            break;
                        }
                    }
                }
            }
        }

        else
        {
            printf("No data within %d seconds.\n", T);
        }
    }
}

void *sender()
{
    /*
    The thread S behaves in the following manner. It sleeps for some time ( < T/2 ), and wakes
up periodically. On waking up, it first checks whether the message timeout period (T) is over
(by computing the time difference between the current time and the time when the messages
within the window were sent last) for the messages sent over any of the active MTP sockets.
If yes, it retransmits all the messages within the current swnd for that MTP socket. It then
checks the current swnd for each of the MTP sockets and determines whether there is a
pending message from the sender-side message buffer that can be sent. If so, it sends that
message through the UDP sendto() call for the corresponding UDP socket and updates the
send timestamp.
    */
    while (1)
    {
        // sleep for some time
        sleep(T / 2);

        // check for timeout for all sockets

    }
}

void *garbage_collector()
{
    
}

int main()
{
    int key;
    int shmid = shmget(key, N * sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);
    memset(SM, 0, N * sizeof(msocket_t));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_t R, S, G;
    pthread_create(&R, &attr, receiver, (void *)shmid);          // to handle receiving messages
    pthread_create(&S, &attr, sender, (void *)shmid);            // to handle sending messages
    pthread_create(&G, &attr, garbage_collector, (void *)shmid); // to handle garbage collection

    pthread_join(R, NULL);
    pthread_join(S, NULL);
    pthread_join(G, NULL);

    return 0;
}