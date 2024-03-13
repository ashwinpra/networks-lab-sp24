#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>
#include <sys/select.h>
#include <sys/sem.h>	

SOCK_INFO *sockinfo;

int semid1, semid2;
struct sembuf pop, vop;


void *receiver(void *arg) {
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
                        pthread_exit(NULL);
                    }

                    printf("Received message: %s\n", buf);
                    
                    // add it to recv_buffer of the receiver socket (find by matching ip and port)
                    for (int j = 0; j < N; j++)
                    {
                        if (SM[j].free == 0 && SM[j].port == ntohs(cliaddr.sin_port) && strcmp(SM[j].ip, inet_ntoa(cliaddr.sin_addr)) == 0)
                        {
                            // if its a normal message, it's of the form "seq:msg"
                            // if its an ACK, it's of the form "<last_inorder_seq>:<rwnd_size>:ACK"

                            // handle separately if its ack
                            if (strcmp(buf + n - 3, "ACK") == 0)
                            {
                                int last_inorder_seq = atoi(strtok(buf, ":"));
                                int rwnd_size = atoi(strtok(NULL, ":"));
                                SM[j].swnd.wndsize = rwnd_size;
                                if(rwnd_size == 0){
                                    SM[j].nospace = 1;
                                }

                                for (int k = 0; k < SEND_BUFFER_SIZE; k++)
                                {
                                    if (SM[j].swnd.unack_msgs[k] <= last_inorder_seq)
                                    {
                                        SM[j].swnd.unack_msgs[k] = -1;
                                    }
                                }
                            }

                            else {
                                // remove header; message will be of the form "seq:msg"
                                int seq_num = atoi(strtok(buf, ":"));
                                char* msg = strtok(NULL, ":");

                                strcpy(SM[j].recv_buffer[RECV_BUFFER_SIZE-SM[j].rwnd.wndsize], msg); //todo: check
                                SM[j].rwnd.wndsize--;

                                // send ack
                                sendto(SM[j].udpsockfd, "ACK", 3, 0, (struct sockaddr *)&cliaddr, len);

                                break;
                            }   
                        }
                    }
                }
            }
        }

        else
        {
            printf("No data within %d seconds.\n", T);
            // todo: see if any of the 'nospace's are updated
        }
    }
}


void *sender(void* arg) {

    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);
    /*
    The thread S behaves in the following manner. It sleeps for some time ( < T/2 ), and wakes up periodically.
    On waking up, it first checks whether the message timeout period (T) is over
    (by computing the time difference between the current time and the time when the messages
    within the window were sent last) for the messages sent over any of the active MTP sockets.
    If yes, it retransmits all the messages within the current swnd for that MTP socket. It then
    checks the current swnd for each of the MTP sockets and determines whether there is a
    pending message from the sender-side message buffer that can be sent. If so, it sends that
    message through the UDP sendto() call for the corresponding UDP socket and updates the
    send timestamp. Note: add the header to the message before sending it.
    Header is just the sequence number of the message, separated from the message by a colon.
    */
    while (1)
    {
        // sleep for some time
        sleep(T/2);

        /*On waking up, it first checks whether the message timeout period (T) is over
    (by computing the time difference between the current time and the time when the messages
    within the window were sent last) for the messages sent over any of the active MTP sockets.*/
        // for(int i=0; i<N; i++)
        // {
        //     if(SM[i].free == 0)
        //     {
        //         for(int j=0; j<SEND_BUFFER_SIZE; j++)
        //         {
        //             if(SM[i].send_buffer[j].free == 0)
        //             {
        //                 time_t now;
        //                 time(&now);
        //                 if(difftime(now, SM[i].send_buffer[j].timestamp) > T)
        //                 {
        //                     // retransmit
        //                     sendto(SM[i].udpsockfd, SM[i].send_buffer[j].message, strlen(SM[i].send_buffer[j].message), 0, (struct sockaddr *)&SM[i].dest_addr, sizeof(SM[i].dest_addr));
        //                     SM[i].send_buffer[j].timestamp = now;
        //                 }
        //             }
        //         }
        //     }
        // }

    }
}

void *garbage_collector(void *arg) {

    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

}

int main()
{   
    //shared memory
    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    memset(sockinfo, 0, sizeof(SOCK_INFO));

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
	semid2 = semget(key2, 1, 0777|IPC_CREAT);

    semctl(semid1, 0, SETVAL, 0);
	semctl(semid2, 0, SETVAL, 0);

    pop.sem_num = vop.sem_num = 0;
	pop.sem_flg = vop.sem_flg = 0;
	pop.sem_op = -1 ; vop.sem_op = 1 ;



    //shared memory
    int key = ftok("msocket.h", 99);
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

    while(1){

        //wait on Sem1
        P(semid1);
        /*b) On being signaled, look at SOCK_INFO.
        (c) If all fields are 0, it is a m_socket call. Create a UDP socket. Put the socket id returned in the sock_id field of SOCK_INFO.  If error, put -1 in sock_id field and errno in errno field. Signal on Sem2.
        (d) if sock_id, IP, and port are non-zero, it is a m_bind call. Make a bind() call on the sock_id value, with the IP and port given. If error, reset sock_id to -1 in the structure and put errno in errno field. Signal on Sem2.
        (e) Go back to wait on Sem1*/

        //look at SOCK_INFO
        if(sockinfo->sockid==0 && sockinfo->port==0 && strcmp(sockinfo->IP,"")==0){
            //m_socket call
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if(sockfd == -1){
                sockinfo->sockid = -1;
                sockinfo->errno = errno;
            }
            else{
                sockinfo->sockid = sockfd;
            }
            V(semid2);
        }
        else if(sockinfo->sockid!=0 && sockinfo->port!=0 && strcmp(sockinfo->IP,"")!=0){
            //m_bind call
            int sockfd = sockinfo->sockid;
            struct sockaddr_in src_addr;
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(sockinfo->port);
            inet_aton(sockinfo->IP, &src_addr.sin_addr);
        
            int res = bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr));
            if(res == -1){
                sockinfo->sockid = -1;
                sockinfo->errno = errno;
            }
            else{
                sockinfo->sockid = sockfd;
            }
            V(semid2);
        }
    }

    pthread_join(R, NULL);
    pthread_join(S, NULL);
    pthread_join(G, NULL);

    return 0;
}