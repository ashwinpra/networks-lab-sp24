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
#include <errno.h>
#include <sys/sem.h>	
#include <signal.h>
#include <time.h>
#include <string.h>

SOCK_INFO* sockinfo;
struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
int semid1, semid2, mtx[N+1];

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
            P(mtx[i]);
            if (SM[i].free == 0)
            {  
                FD_SET(SM[i].udpsockfd, &fds);
                if (SM[i].udpsockfd > maxfd)
                {
                    maxfd = SM[i].udpsockfd;
                }
            }
            V(mtx[i]);
        }

        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;
        int retval = select(maxfd + 1, &fds, NULL, NULL, &tv);

        if (retval == -1)
        {
            // could be caused by socket being closed abruptly during select call
            continue; // try again
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
                    bzero(buf, 1024);
                    int n = recvfrom(SM[i].udpsockfd, buf, 1024, 0, (struct sockaddr *)&cliaddr, &len);
                    buf[n]='\0';
                    if (n == -1)
                    {
                        perror("recvfrom failed");
                        pthread_exit(NULL);
                    }

                    if(n==0){
                        printf("Connection closed by client\n");
                        continue;
                    }

                    if(dropMessage(p)){
                        printf("Dropped message: %s\n",buf);
                        continue;
                    }

                    printf("Received: %s\n", buf);
                    
                    // if its a normal message, it's of the form "seq::msg"
                    // if its an ACK, it's of the form "<last_inorder_seq>::<rwnd_size>::ACK"

                    // handle separately if its ack
                    if (strcmp(buf + n - 3, "ACK") == 0)
                    {   
                        int last_inorder_seq = atoi(strtok(buf, "::"));
                        int rwnd_size = atoi(strtok(NULL, "::"));
                        P(mtx[i]);
                        SM[i].swnd.recv_wndsize = rwnd_size;
                        V(mtx[i]);

                        // case 1: ack = start, then fine, move window by 1 
                        // case 2: ack is somewhere between start to end - move window completely 
                        // case 3: ack is not in the range - duplicate ack, ignore

                        int index=SM[i].swnd.window_start;
                        int count=0;int flag=0;

                        P(mtx[i]);
                        while(1){
                            count++;
                            if(SM[i].swnd.unack_msgs[index].seq_no == last_inorder_seq){
                                flag=1;
                                break;
                            }
                            if(index==SM[i].swnd.window_end ) break;
                            index=(index+1)%SEND_BUFFER_SIZE;
                        }
                        V(mtx[i]);

                        P(mtx[i]);
                        index=SM[i].swnd.window_start;
                        if(flag){
                            while(count--){
                                SM[i].swnd.unack_msgs[index].seq_no=-1;
                                bzero(SM[i].swnd.unack_msgs[index].message, 1024);
                                index=(index+1)%SEND_BUFFER_SIZE;
                                SM[i].swnd.window_start=index;
                                SM[i].swnd.wndsize++;
                            }
                        }
                        V(mtx[i]);
                        break;
                    }

                    else {
                        // remove header; message will be of the form "seq:msg"
                       
                        int seq_num = atoi(strtok(buf, "::"));
                        char* msg = strtok(NULL, "::");

                        // case 1: in order (start index) - accept it (move window would be done by recvfrom function)
                        // case 2: out of order - store it 
                        // case 3: duplicate - ignore it

                        int index;
                        P(mtx[i]);
                        if(SM[i].rwnd.wndsize==RECV_BUFFER_SIZE){
                            index=SM[i].rwnd.window_start;
                            int next_seq=SM[i].rwnd.exp_msgs[index].seq_no;

                            while(1){
                                if(SM[i].rwnd.exp_msgs[index].seq_no == seq_num){
                                    strcpy(SM[i].rwnd.exp_msgs[index].message, msg);
                                    
                                    if(next_seq==seq_num){
                                        SM[i].rwnd.window_end=index;
                                        SM[i].rwnd.wndsize--;
                                        index=(index+1)%RECV_BUFFER_SIZE;
                                        next_seq=((next_seq)%15)+1;
                                        while(index!=SM[i].rwnd.window_start && SM[i].rwnd.exp_msgs[index].seq_no==next_seq){
                                            if(SM[i].rwnd.exp_msgs[index].message[0]=='\0') break;
                                            SM[i].rwnd.window_end=index;
                                            SM[i].rwnd.wndsize--;
                                            next_seq=((next_seq)%15)+1;
                                            index=(index+1)%RECV_BUFFER_SIZE;
                                        }
                                    }
                                    V(mtx[i]);
                                    break;
                                }
                                
                                index=(index+1)%RECV_BUFFER_SIZE;

                                if(index==SM[i].rwnd.window_start) {
                                    V(mtx[i]);
                                    break;
                                }
                            }

                        }
                        else {
                            index=(SM[i].rwnd.window_end+1)%RECV_BUFFER_SIZE;
                            int next_seq=SM[i].rwnd.exp_msgs[index].seq_no;

                            while(index!=SM[i].rwnd.window_start){
                                if(SM[i].rwnd.exp_msgs[index].seq_no == seq_num){
                                    strcpy(SM[i].rwnd.exp_msgs[index].message, msg);
                                    
                                    if(next_seq==seq_num){
                                        SM[i].rwnd.window_end=index;
                                        SM[i].rwnd.wndsize--;
                                        index=(index+1)%RECV_BUFFER_SIZE;
                                        next_seq=((next_seq)%15)+1;
                                        while(index!=SM[i].rwnd.window_start && SM[i].rwnd.exp_msgs[index].seq_no==next_seq){
                                                if(SM[i].rwnd.exp_msgs[index].message[0]=='\0') break;
                                            SM[i].rwnd.window_end=index;
                                            SM[i].rwnd.wndsize--;
                                            next_seq=((next_seq)%15)+1;
                                            index=(index+1)%RECV_BUFFER_SIZE;
                                        }
                                    }

                                    V(mtx[i]);
                                    break;
                                }
                                
                                index=(index+1)%RECV_BUFFER_SIZE;
                            } 

                        }

                        // send ACK in proper format: "<last_inorder_seq>::<rwnd_size>::ACK"
                        int last_seq_no;
                        P(mtx[i]);
                        if(SM[i].rwnd.wndsize==RECV_BUFFER_SIZE){
                            last_seq_no=(SM[i].rwnd.curr_seq_no-RECV_BUFFER_SIZE-1+15)%15;
                            if(last_seq_no==0) last_seq_no=15;
                        }else last_seq_no=SM[i].rwnd.exp_msgs[SM[i].rwnd.window_end].seq_no;
                            
                        index=SM[i].rwnd.window_end;
                        char ack[1024];
                        bzero(ack, 1024);
                        sprintf(ack, "%d::%d::ACK", last_seq_no, SM[i].rwnd.wndsize);
                        if(SM[i].rwnd.wndsize==0) SM[i].nospace=1;
                        else SM[i].nospace=0;
                        sendto(SM[i].udpsockfd, ack, strlen(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                        V(mtx[i]);
                        break;
                    }   
                        
                }
            }
        }

        else
        {  
            printf("No data within %d seconds.\n", T);
            // see if any of the 'nospace's can be updated
            for(int i=0; i<N; i++)
            {   
                P(mtx[i]);
                if(SM[i].free == 0 && SM[i].nospace == 1)
                {
                    if(SM[i].rwnd.wndsize > 0)
                    {
                        int last_seq_no;
                        if(SM[i].rwnd.wndsize==RECV_BUFFER_SIZE){
                            last_seq_no=(SM[i].rwnd.curr_seq_no-RECV_BUFFER_SIZE-1+15)%15;
                            if(last_seq_no==0) last_seq_no=15;
                        }else last_seq_no=SM[i].rwnd.exp_msgs[SM[i].rwnd.window_end].seq_no;

                        int index=SM[i].rwnd.window_end;
                        char ack[1024];
                        bzero(ack, 1024);
                        sprintf(ack, "%d::%d::ACK", last_seq_no, SM[i].rwnd.wndsize);

                        struct sockaddr_in cliaddr;
                        cliaddr.sin_family = AF_INET;
                        cliaddr.sin_port = htons(SM[i].port);
                        inet_aton(SM[i].ip, &cliaddr.sin_addr);
                        int len = sizeof(cliaddr);

                        sendto(SM[i].udpsockfd, ack, strlen(ack), 0, (struct sockaddr *)&cliaddr, len);
                    }
                }
                V(mtx[i]);
            }
        }
    }
}


void *sender(void* arg) {
    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    while(1){
        sleep((4*T)/10);
        for(int i=0; i<N; i++){
            if(SM[i].free == 0 && SM[i].swnd.recv_wndsize > 0 && SM[i].port!=0 && strcmp(SM[i].ip,"")!=0 && SM[i].swnd.window_end!=-1){
                        // check if T is over
                        time_t curr_time;
                        time(&curr_time);
                        if(difftime(curr_time, SM[i].swnd.timestamp) > T){
                            // retransmit
                            int curr_seq_no = SM[i].swnd.unack_msgs[SM[i].swnd.window_start].seq_no;
                            if(curr_seq_no==-1) continue;
                            for(int j=0;j<SEND_BUFFER_SIZE;j++){
                                int index=(SM[i].swnd.window_start+j)%SEND_BUFFER_SIZE;
                                if(j>=SM[i].swnd.recv_wndsize) break;

                                struct sockaddr_in cliaddr;
                                cliaddr.sin_family = AF_INET;
                                cliaddr.sin_port = htons(SM[i].port);
                                inet_aton(SM[i].ip, &cliaddr.sin_addr);
                                int len = sizeof(cliaddr);

                                if(SM[i].swnd.unack_msgs[index].seq_no == curr_seq_no){
                                    int x=sendto(SM[i].udpsockfd, SM[i].swnd.unack_msgs[index].message, strlen(SM[i].swnd.unack_msgs[index].message), 0, (struct sockaddr *)&cliaddr, len);
                                    P(mtx[i]);
                                    if(j==0) SM[i].swnd.timestamp = curr_time;
                                    curr_seq_no=((curr_seq_no)%15)+1;
                                    SM[i].swnd.window_end=index;
                                    SM[i].msg_count++;
                                    V(mtx[i]);
                                }
                                
                                else break;
                            }
                        }
            }
        }

        for(int i=0; i<N; i++){
            if(SM[i].free == 0 && SM[i].swnd.recv_wndsize && SM[i].port!=0 && strcmp(SM[i].ip,"")!=0){
                if(SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message[0]=='\0') continue;

                struct sockaddr_in cliaddr;
                cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(SM[i].port);
                inet_aton(SM[i].ip, &cliaddr.sin_addr);
                int len = sizeof(cliaddr);
             
                int already_sent=0;
                if(SM[i].swnd.window_end==-1 || ((((SM[i].swnd.window_end+1)%SEND_BUFFER_SIZE)==SM[i].swnd.window_start) && (((SM[i].swnd.unack_msgs[SM[i].swnd.window_end].seq_no%15)+1)==SM[i].swnd.unack_msgs[SM[i].swnd.window_start].seq_no || SM[i].swnd.unack_msgs[SM[i].swnd.window_end].seq_no==-1))){
                    SM[i].swnd.window_end=SM[i].swnd.window_start;
                    sendto(SM[i].udpsockfd, SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message, strlen(SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message), 0, (struct sockaddr *)&cliaddr, len);
                    P(mtx[i]);
                    SM[i].swnd.timestamp = time(NULL);
                    SM[i].msg_count++;
                    V(mtx[i]);

                }

                int j=SM[i].swnd.window_end;
                int curr_seq_no = ((SM[i].swnd.unack_msgs[j].seq_no)%15)+1;
                if(curr_seq_no==-1) continue;
                already_sent=((SM[i].swnd.window_end<SM[i].swnd.window_start)?(SM[i].swnd.window_end+SEND_BUFFER_SIZE):SM[i].swnd.window_end)-SM[i].swnd.window_start+1;
               
                j=(j+1)%SEND_BUFFER_SIZE;

                while(j!=SM[i].swnd.window_start){
                    if(SM[i].swnd.unack_msgs[j].seq_no == -1) break;
                    if(already_sent>=SM[i].swnd.recv_wndsize) break;
                    if(SM[i].swnd.unack_msgs[j].seq_no == curr_seq_no){
                        // send message
                        sendto(SM[i].udpsockfd, SM[i].swnd.unack_msgs[j].message, strlen(SM[i].swnd.unack_msgs[j].message), 0, (struct sockaddr *)&cliaddr, len);
                        P(mtx[i]);
                        SM[i].swnd.window_end=j;
                        SM[i].swnd.timestamp = time(NULL);
                        SM[i].msg_count++;      
                        V(mtx[i]);
                        curr_seq_no=((curr_seq_no)%15)+1;
                    }
                    
                    else break;

                    j=(j+1)%SEND_BUFFER_SIZE;
                    already_sent++;
                }
            }
        }
   }
}

void *garbage_collector(void *arg) {

    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    while(1){
        sleep(2*T);
        // see if any pid has exited
        for(int i=0; i<N; i++){
            P(mtx[i]);
            if(SM[i].free == 0){
                if(kill(SM[i].pid, 0) == -1){
                    printf("Closing socket %d\n", i);
                    close(SM[i].udpsockfd);
                }
            }
            V(mtx[i]);
        }
    }
}

int main()
{   
    srand(time(0));

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);
    key_t key3=ftok("msocket.h", 103);
    
    semid1 = semget(key1, 1, 0777|IPC_CREAT);
    semid2 = semget(key2, 1, 0777|IPC_CREAT);

    for(int i=0;i<=N;i++){
        mtx[i] = semget(key3+i, 1, 0777|IPC_CREAT);
        semctl(mtx[i], 0, SETVAL, 1);
    }
    
    semctl(semid1, 0, SETVAL, 0);
    semctl(semid2, 0, SETVAL, 0);

    //shared memory
    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    sockinfo = (SOCK_INFO*) shmat(shmid_sockinfo, 0, 0);

    P(mtx[N]);
    sockinfo->sockid = 0;
    sockinfo->port = 0;
    strcpy(sockinfo->IP, "");
    V(mtx[N]);

    //shared memory
    int key = ftok("msocket.h", 99);
    int shmid = shmget(key, N * sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);
    memset(SM, 0, N * sizeof(msocket_t));

    for(int i=0;i<N;i++){
        P(mtx[i]);
        SM[i].free = 1;
        V(mtx[i]);
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_t R, S, G;
    pthread_create(&R, &attr, receiver, (void *)shmid);          // to handle receiving messages
    pthread_create(&S, &attr, sender, (void *)shmid);            // to handle sending messages
    pthread_create(&G, &attr, garbage_collector, (void *)shmid); // to handle garbage collection

    while(1){
        P(semid1);
        P(mtx[N]);
        if(sockinfo->sockid==0 && sockinfo->port==0 && strcmp(sockinfo->IP,"")==0){
            //m_socket call
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if(sockfd == -1){
                sockinfo->sockid = -1;
                sockinfo->errnum = errno;
            }
            else{
                sockinfo->sockid = sockfd;
            }
            V(semid2);
            V(mtx[N]);
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
                sockinfo->errnum = errno;
            }
            else{
                sockinfo->sockid = sockfd;
            }
            V(semid2);
            V(mtx[N]);
        }
    }

    pthread_join(R, NULL);
    pthread_join(S, NULL);
    pthread_join(G, NULL);

    return 0;
}