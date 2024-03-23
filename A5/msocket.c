#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>
#include <errno.h>
#include <sys/sem.h>	
#include <time.h>
 
int m_socket(int domain, int type, int protocol) {
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    SOCK_INFO *sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);

    int semid1, semid2, mtx[N+1];
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);
    key_t key3=ftok("msocket.h", 103);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
    semid2 = semget(key2, 1, 0777|IPC_CREAT);

    for(int i=0;i<=N;i++){
        mtx[i] = semget(key3+i, 1, 0777|IPC_CREAT);
    }
    
    int freeidx = -1;
    for (int i = 0; i < N; i++)
    {   
        P(mtx[i]);
        if (SM[i].free == 1)
        {
            freeidx = i;
            V(mtx[i]);
            break;
        }
        V(mtx[i]);
    }

    if (freeidx == -1)
    {
        errno = ENOBUFS;
        return -1;
    }
    
    P(mtx[freeidx]);
    SM[freeidx].free = 0;
    SM[freeidx].pid = getpid();
    V(mtx[freeidx]);

    V(semid1);
    P(semid2);

    if(sockinfo->sockid == -1){
        P(mtx[N]);
        errno = sockinfo->errnum;
        sockinfo->sockid=0;
        sockinfo->errnum=0;
        sockinfo->port=0;
        strcpy(sockinfo->IP, "");
        V(mtx[N]);

        return -1;
    }

    P(mtx[N]);
    P(mtx[freeidx]);

    SM[freeidx].udpsockfd = sockinfo->sockid;
    sockinfo->sockid=0;
    sockinfo->errnum=0;
    sockinfo->port=0;
    strcpy(sockinfo->IP, "");

    V(mtx[N]);

    SM[freeidx].swnd.curr_seq_no=1;
    SM[freeidx].swnd.timestamp = time(NULL);
    SM[freeidx].swnd.wndsize = SEND_BUFFER_SIZE;
    SM[freeidx].swnd.recv_wndsize=RECV_BUFFER_SIZE;
    SM[freeidx].swnd.window_start = 0;
    SM[freeidx].swnd.window_end = -1;
    for(int i=0;i<SEND_BUFFER_SIZE;i++){
        SM[freeidx].swnd.unack_msgs[i].seq_no = -1;
    }

    SM[freeidx].rwnd.wndsize = RECV_BUFFER_SIZE;
    SM[freeidx].rwnd.window_start = 0;
    SM[freeidx].rwnd.window_end = RECV_BUFFER_SIZE-1;
    for(int i=0;i<RECV_BUFFER_SIZE;i++){
        SM[freeidx].rwnd.exp_msgs[i].seq_no = i+1;
    }
    SM[freeidx].rwnd.curr_seq_no = 6;
    SM[freeidx].nospace = 0;
    SM[freeidx].msg_count = 0;

    V(mtx[freeidx]);

    return freeidx;
}

int m_bind(int sockfd, char *src_ip, int src_port, char *dest_ip, int dest_port) {
    int semid1, semid2, mtx[N+1];
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);
    key_t key3=ftok("msocket.h", 103);

    semid1 = semget(key1, 1, 0777|IPC_CREAT);
    semid2 = semget(key2, 1, 0777|IPC_CREAT);

    for(int i=0;i<=N;i++){
        mtx[i] = semget(key3+i, 1, 0777|IPC_CREAT);
    }


    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    SOCK_INFO *sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    
    P(mtx[sockfd]);
    P(mtx[N]);
    sockinfo->sockid = SM[sockfd].udpsockfd;
    sockinfo->port = src_port;
    strcpy(sockinfo->IP, src_ip);
    V(mtx[N]);
    V(mtx[sockfd]);

    V(semid1);
    P(semid2);

    if(sockinfo->sockid == -1){
        errno = sockinfo->errnum;
        P(mtx[N]);
        strcpy(sockinfo->IP, "");
        sockinfo->sockid=0;
        sockinfo->port=0;
        sockinfo->errnum = 0;
        V(mtx[N]);
        return -1;
    }
    
    P(mtx[N]);
    strcpy(sockinfo->IP, "");
    // sockinfo->IP="";
    sockinfo->sockid=0;
    sockinfo->port=0;
    sockinfo->errnum = 0;
    V(mtx[N]);

    P(mtx[sockfd]);
    SM[sockfd].port= dest_port;
    strcpy(SM[sockfd].ip, dest_ip);
    V(mtx[sockfd]);

    return 0;
}

int m_sendto(int sockfd, char *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    // printf("Sendto called!\n");
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);

    int mtx; 
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    key_t key3=ftok("msocket.h", 103);

    mtx - semget(key3+sockfd, 1, 0777|IPC_CREAT);

    int dest_port = ntohs(((struct sockaddr_in *)dest_addr)->sin_port);
    
    if(msocket[sockfd].port != dest_port || strcmp(msocket[sockfd].ip, inet_ntoa(((struct sockaddr_in *)dest_addr)->sin_addr))!=0 ){
        errno = ENOTCONN;
        return -1;
    }

    if(msocket[sockfd].swnd.wndsize==0){
        errno = ENOBUFS;
        return -1;
    }

    int index=(msocket[sockfd].swnd.window_end+1)%SEND_BUFFER_SIZE;
    while(1){
        if(msocket[sockfd].swnd.unack_msgs[index].seq_no <= -1){
            sprintf(msocket[sockfd].swnd.unack_msgs[index].message, "%d:%s", msocket[sockfd].swnd.curr_seq_no, 
            buf);
            P(mtx);
            msocket[sockfd].swnd.unack_msgs[index].seq_no = msocket[sockfd].swnd.curr_seq_no;
            msocket[sockfd].swnd.curr_seq_no=((msocket[sockfd].swnd.curr_seq_no)%15)+1;
            msocket[sockfd].swnd.wndsize--;
            V(mtx);
            return strlen(buf);
        }
        index=(index+1)%SEND_BUFFER_SIZE;
        if(index==msocket[sockfd].swnd.window_start){
            break;
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
    mtx = semget(key3+sockfd, 1, 0777|IPC_CREAT);
    
    bzero(buf, len);
    int index=(msocket[sockfd].rwnd.window_start)%RECV_BUFFER_SIZE;
    if(msocket[sockfd].rwnd.exp_msgs[index].message[0] != '\0'){
        sprintf(buf, "%s", msocket[sockfd].rwnd.exp_msgs[index].message);
        P(mtx);
        msocket[sockfd].rwnd.exp_msgs[index].seq_no = msocket[sockfd].rwnd.curr_seq_no;
        msocket[sockfd].rwnd.curr_seq_no=((msocket[sockfd].rwnd.curr_seq_no)%15)+1;
        bzero(msocket[sockfd].rwnd.exp_msgs[index].message, 1024);
        msocket[sockfd].rwnd.wndsize++;
        msocket[sockfd].rwnd.window_start = (msocket[sockfd].rwnd.window_start+1)%RECV_BUFFER_SIZE;
        V(mtx);

        struct sockaddr_in *src = (struct sockaddr_in *)src_addr;
        src->sin_family = AF_INET;
        src->sin_port = htons(msocket[sockfd].port);
        src->sin_addr.s_addr = inet_addr(msocket[sockfd].ip);        

        return strlen(buf);
    }
        
    else{
        errno = ENOMSG;
        return -1;
    }

    return 0;
}

int m_close(int sockfd)
{
    int mtx; 
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    key_t key3=ftok("msocket.h", 103);
    mtx = semget(key3+sockfd, 1, 0777|IPC_CREAT);
    
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    P(mtx);
    SM[sockfd].free = 1;
    V(mtx);

    return 0;
}

int getmsgcount(int sockfd){
    key_t key = ftok("msocket.h", 99);
    int shmid = shmget(key, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    int mtx; 
    struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
    key_t key3=ftok("msocket.h", 103);
    mtx = semget(key3+sockfd, 1, 0777|IPC_CREAT);

    P(mtx);
    int ret = SM[sockfd].msg_count;
    V(mtx);

    return ret;
}

int dropMessage(float P){
    float r = (float)rand()/(float)(RAND_MAX);
    if(r<p){
        return 1;
    }
    return 0;
}