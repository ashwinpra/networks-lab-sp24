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
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    SOCK_INFO *sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);

    int semid1, semid2, mtx;
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);
    key_t key3=ftok("msocket.h", 103);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
	semid2 = semget(key2, 1, 0777|IPC_CREAT);
    mtx = semget(key3, 1, 0777|IPC_CREAT);
    
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

    P(mtx);
    SM[freeidx].free = 0;
    SM[freeidx].pid = getpid();
    V(mtx);

    /*signals on Sem1 and then waits on Sem2. On being woken, checks sock_id field of SOCK_INFO. If -1, return 
    error and set errno correctly. If not, put UDP socket id returned in that field in SM table (same as if m_socket called socket()) 
    and return the index in SM table as usual. In both cases, reset all fields of SOCK_INFO to 0.*/

    V(semid1);
    P(semid2);

    if(sockinfo->sockid == -1){
        errno = sockinfo->errno;
        
        P(mtx);
        sockinfo->sockid=0;
        sockinfo->errno=0;
        sockinfo->port=0;
        sockinfo->IP="";
        V(mtx);

        return -1;
    }

    P(mtx);
    SM[freeidx].udpsockfd = sockinfo->sockid;
    
    sockinfo->sockid=0;
    sockinfo->errno=0;
    sockinfo->port=0;
    sockinfo->IP="";

    SM[freeidx].swnd.curr_seq_no=1;
    SM[freeidx].swnd.timestamp = time(NULL);
    SM[freeidx].swnd.wndsize = SEND_BUFFER_SIZE;
    SM[freeidx].swnd.window_start = 0;
    SM[freeidx].swnd.window_end = 0;
    for(int i=0;i<SEND_BUFFER_SIZE;i++){
        SM[freeidx].swnd.unack_msgs[i].seq_no = -1;
    }

    SM[freeidx].rwnd.wndsize = RECV_BUFFER_SIZE;
    SM[freeidx].rwnd.window_start = 0;
    SM[freeidx].rwnd.window_end = RECV_BUFFER_SIZE-1;
    for(int i=0;i<RECV_BUFFER_SIZE;i++){
        SM[freeidx].rwnd.exp_msgs[i].seq_no = i+1;
    }
    SM[freeidx].rwnd.curr_seq_no = 6; // next sequence number to be added to window
    SM[freeidx].nospace = 0;

    V(mtx);

    return freeidx;
}

int m_bind(int sockfd, char *src_ip, int src_port, char *dest_ip, int dest_port) {

    /*Find the corresponding actual UDP socket id from the SM table. 
    Put the UDP socket ID, IP, and port in SOCK_INFO table. Signal Sem1. 
    Then wait on Sem2. On being woken, checks sock_id field of SOCK_INFO. 
    If -1, return error and set errno correctly. If not return success.
    In both cases, reset all fields of SOCK_INFO to 0.*/

    int semid1, semid2, mtx;
	struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);
    key_t key3=ftok("msocket.h", 103);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
	semid2 = semget(key2, 1, 0777|IPC_CREAT);
    mtx = semget(key3, 1, 0777|IPC_CREAT);


    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);


    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    SOCK_INFO *sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    
    struct sockaddr_in src_addr;
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(src_port);
    src_addr.sin_addr.s_addr = inet_addr(src_ip);

    P(mtx);
    sockinfo->sockid = SM[sockfd].udpsockfd;
    sockinfo->port = dest_port;
    sockinfo->IP = dest_ip;

    V(mtx);
    V(semid1);

    P(semid2);

    if(sockinfo->sockid == -1){
        errno = sockinfo->errno;
        P(mtx);
        sockinfo->IP="";
        sockinfo->sockid=0;
        sockinfo->port=0;
        sockinfo->errno = 0;
        V(mtx);
        return -1;
    }
    
    P(mtx);
    sockinfo->IP="";
    sockinfo->sockid=0;
    sockinfo->port=0;
    sockinfo->errno = 0;
    SM[sockfd].port= dest_port;
    SM[sockfd].ip = dest_ip;
    V(mtx);

    return 0;
}

int m_sendto(int sockfd, char *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);

    int mtx; 
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    key_t key3=ftok("msocket.h", 103);
    mtx = semget(key3, 1, 0777|IPC_CREAT);

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
        int index=(msocket[sockfd].swnd.window_start+i)%SEND_BUFFER_SIZE;
        if(msocket[sockfd].swnd.unack_msgs[index].seq_no == -1){
            sprintf(msocket[sockfd].swnd.unack_msgs[index].message, "%d:%s", msocket[sockfd].swnd.curr_seq_no, buf);
            P(mtx);
            msocket[sockfd].swnd.unack_msgs[index].seq_no = msocket[sockfd].swnd.curr_seq_no;
            msocket[sockfd].swnd.curr_seq_no++;
            msocket[sockfd].swnd.wndsize--;
            V(mtx);
            return strlen(buf);
        }
    }
    return 0;
}



int m_recvfrom(int sockfd, char* buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){

    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);

    int mtx; 
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    key_t key3=ftok("msocket.h", 103);
    mtx = semget(key3, 1, 0777|IPC_CREAT);

    if(msocket[sockfd].nospace == 1){
        errno = ENOMSG;
        return -1;
    }

    bzero(buf, len);

    // !update this - send message from "start" index, then move start pointer to next index
    for(int i=0;i<RECV_BUFFER_SIZE;i++){
        if(msocket[sockfd].rwnd.exp_msgs[i].seq_no != -1){
            sprintf(buf, "%s", msocket[sockfd].rwnd.exp_msgs[i].message);
            msocket[sockfd].rwnd.exp_msgs[i].seq_no = -1;
            bzero(msocket[sockfd].rwnd.exp_msgs[i].message, 1024);
            msocket[sockfd].rwnd.wndsize++;

            struct sockaddr_in *src = (struct sockaddr_in *)src_addr;
            src->sin_family = AF_INET;
            src->sin_port = htons(msocket[sockfd].port);
            src->sin_addr.s_addr = inet_addr(msocket[sockfd].ip);
            *addrlen = sizeof(*src);
            return strlen(buf);
        }
    }
    return 0;
}

int m_close(int sockfd)
{
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    int mtx; 
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    key_t key3=ftok("msocket.h", 103);
    mtx = semget(key3, 1, 0777|IPC_CREAT);


    for (int i = 0; i < N; i++)
    {
        if (SM[i].pid == getpid())
        {   
            P(mtx);
            SM[i].free = 1;
            if(close(SM[i].udpsockfd) == -1) {
                V(mtx);
                return -1;
            }
            V(mtx);
            return 0;
        }
    }
    return -1;
}