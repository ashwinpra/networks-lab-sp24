#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>
#include <sys/sem.h>	
 

int m_socket(int domain, int type, int protocol) {
    int key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    SOCK_INFO *sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);

    int semid1, semid2 ;
	struct sembuf pop, vop ;

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
	semid2 = semget(key2, 1, 0777|IPC_CREAT);
    
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

    /*signals on Sem1 and then waits on Sem2. On being woken, checks sock_id field of SOCK_INFO. If -1, return 
    error and set errno correctly. If not, put UDP socket id returned in that field in SM table (same as if m_socket called socket()) 
    and return the index in SM table as usual. In both cases, reset all fields of SOCK_INFO to 0.*/

    V(semid1);
    P(semid2);

    if(sockinfo->sockid == -1){
        errno = sockinfo->errno;

        sockinfo->sockid=0;
        sockinfo->errno=0;
        sockinfo->port=0;
        sockinfo->IP="";
        return -1;
    }

    SM[freeidx].udpsockfd = sockinfo->sockid;
    
    sockinfo->sockid=0;
    sockinfo->errno=0;
    sockinfo->port=0;
    sockinfo->IP="";

    SM[freeidx].swnd.curr_seq_no=1;
    SM[freeidx].swnd.wndsize = SEND_BUFFER_SIZE;
    SM[freeidx].rwnd.wndsize = RECV_BUFFER_SIZE;
    SM[freeidx].swnd.window_start = 0;
    SM[freeidx].swnd.window_end = 0;
    for(int i=0;i<SEND_BUFFER_SIZE;i++){
        SM[freeidx].swnd.unack_msgs[i].seq_no = -1;
    }
    memset(SM[freeidx].recv_buffer, NULL, RECV_BUFFER_SIZE*1024);

    return freeidx;
}

int m_bind(int sockfd, char *src_ip, int src_port, char *dest_ip, int dest_port) {

    /*Find the corresponding actual UDP socket id from the SM table. 
    Put the UDP socket ID, IP, and port in SOCK_INFO table. Signal Sem1. 
    Then wait on Sem2. On being woken, checks sock_id field of SOCK_INFO. 
    If -1, return error and set errno correctly. If not return success.
    In both cases, reset all fields of SOCK_INFO to 0.*/

    int semid1, semid2 ;
	struct sembuf pop, vop ;

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
	semid2 = semget(key2, 1, 0777|IPC_CREAT);


    int key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);


    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    SOCK_INFO *sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    
    struct sockaddr_in src_addr;
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(src_port);
    src_addr.sin_addr.s_addr = inet_addr(src_ip);

    sockinfo->sockid = SM[sockfd].udpsockfd;
    sockinfo->port = dest_port;
    sockinfo->IP = dest_ip;

    V(semid1);
    P(semid2);


    if(sockinfo->sockid == -1){
        errno = sockinfo->errno;
        sockinfo->IP="";
        sockinfo->sockid=0;
        sockinfo->port=0;
        sockinfo->errno = 0;
        return -1;
    }
    
    sockinfo->IP="";
    sockinfo->sockid=0;
    sockinfo->port=0;
    sockinfo->errno = 0;
    SM[sockfd].port= dest_port;
    SM[sockfd].ip = dest_ip;
    return 0;
}

int m_sendto(int sockfd, char *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    int key = ftok("msocket.h", 99);
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
        if(msocket[sockfd].swnd.unack_msgs[i].seq_no == -1){
            sprintf(msocket[sockfd].swnd.unack_msgs[i].message, "%s", buf);
            msocket[sockfd].swnd.unack_msgs[i].seq_no = msocket[sockfd].swnd.curr_seq_no;
            msocket[sockfd].swnd.curr_seq_no++;
            msocket[sockfd].swnd.wndsize --;
            return strlen(buf);
        }
    }
    return 0;
}



int m_recvfrom(int sockfd, char* buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    int key = ftok("msocket.h", 99);
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
            return strlen(buf);
        }
    }
    return 0;
}

int m_close(int sockfd)
{
    int key = ftok("msocket.h", 99);
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