#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>

int m_socket(int domain, int type, int protocol) {
    int key = ftok("msocket.h", 65);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);
    
    int freeidx = -1;
    for (int i = 0; i < N; i++)
    {
        if (SM[i].free == 1)
        {
            freeidx = i;
            break;
        }
    }

    if (freeidx == -1)
    {
        errno = ENOBUFS;
        return -1;
    }

    SM[freeidx].free = 0;
    SM[freeidx].pid = getpid();
    if((SM[freeidx].udpsockfd = socket(domain, SOCK_DGRAM, protocol)) == -1)
    {
        errno = EMISC;
        return -1;
    }

    SM[freeidx].swnd.wndsize = SEND_BUFFER_SIZE;
    SM[freeidx].rwnd.wndsize = RECV_BUFFER_SIZE;
    memset(SM[freeidx].send_buffer, NULL, SEND_BUFFER_SIZE*1024);
    memset(SM[freeidx].recv_buffer, NULL, RECV_BUFFER_SIZE*1024);

    return freeidx;
}

int m_bind(int sockfd, char *src_ip, int src_port, char *dest_ip, int dest_port) {
    int key = ftok("msocket.h", 65);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);
    
    struct sockaddr_in src_addr;
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(src_port);
    src_addr.sin_addr.s_addr = inet_addr(src_ip);
    
    if(bind(msocket[sockfd].udpsockfd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0)
    {
        errno=EMISC;
        perror("bind failed");
        return -1;
    }
    msocket[sockfd].port= dest_port;
    msocket[sockfd].ip = dest_ip;
    return 0;
}

int m_sendto(int sockfd, char *buf, int len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    int key = ftok("msocket.h", 65);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);
    char * dest_port = ntohs(((struct sockaddr_in *)dest_addr)->sin_port);
    char *dest_ip = inet_ntoa(((struct sockaddr_in *)dest_addr)->sin_addr);
    
    if(msocket[sockfd].port != dest_port || msocket[sockfd].ip != dest_ip){
        errno = ENOTBOUND;
        return -1;
    }
    if(msocket[sockfd].swnd.wndsize==0){
        errno = ENOBUFS;
        return -1;
    }
    

    for(int i=0;i<SEND_BUFFER_SIZE;i++){
        if(msocket[sockfd].send_buffer[i][0] == NULL){
            sprintf(msocket[sockfd].send_buffer[i], "%s", buf);
            msocket[sockfd].swnd.wndsize --;
            break;
        }
    }
    return 0;
}



int m_recvfrom(int sockfd, char *buf, int len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    int key = ftok("msocket.h", 65);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);
    if(msocket[sockfd].rwnd.wndsize == RECV_BUFFER_SIZE){
        errno = ENOMSG;
        return -1;
    }
    bzero(buf, len);
    for(int i=0;i<RECV_BUFFER_SIZE;i++){
        if(msocket[sockfd].recv_buffer[i][0] != NULL){
            sprintf(buf, "%s", msocket[sockfd].recv_buffer[i]);
            msocket[sockfd].recv_buffer[i][0] = NULL;
            msocket[sockfd].rwnd.wndsize ++;
            return 0;
        }
    }
    return strlen(buf);
}

int m_close(int sockfd)
{
    int key = ftok("msocket.h", 65);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    for (int i = 0; i < N; i++)
    {
        if (SM[i].pid == getpid())
        {
            SM[i].free = 1;
            if(close(SM[i].udpsockfd) == -1) {
                errno = EMISC;
                return -1;
            }
            return 0;
        }
    }

    errno = EMISC;
    return -1;
}