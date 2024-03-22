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
// #include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>

SOCK_INFO* sockinfo;
struct sembuf pop = {0, -1, 0}, vop = {0, 1, 0};
int semid1, semid2, mtx, ack_mtx;


void *receiver(void *arg) {
    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    while (1)
    {   
        fd_set fds;
        FD_ZERO(&fds);
        int maxfd = -1;
        P(mtx);
        for (int i = 0; i < N; i++)
        {  
            if (SM[i].free == 0)
            {  
                // printf("%d is free\n", i);
                FD_SET(SM[i].udpsockfd, &fds);
                if (SM[i].udpsockfd > maxfd)
                {
                    maxfd = SM[i].udpsockfd;
                }
            }
        }
        V(mtx);

        // printf("maxfd = %d\n", maxfd);

        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;
        // printf("Waiting on select\n");
        int retval = select(maxfd + 1, &fds, NULL, NULL, &tv);
        // printf("Select done with retval = %d\n", retval);

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
                    bzero(buf, 1024);
                    int n = recvfrom(SM[i].udpsockfd, buf, 1024, 0, (struct sockaddr *)&cliaddr, &len);
                    buf[n]='\0';
                    if (n == -1)
                    {
                        perror("recvfrom()");
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
                    
                    // if its a normal message, it's of the form "seq:msg"
                    // if its an ACK, it's of the form "<last_inorder_seq>:<rwnd_size>:ACK"

                    // handle separately if its ack
                    if (strcmp(buf + n - 3, "ACK") == 0)
                    {   
                        printf("ACK received: [%s]\n", buf);
                        int last_inorder_seq = atoi(strtok(buf, ":"));
                        int rwnd_size = atoi(strtok(NULL, ":"));
                        // printf("ACK last_inorder_seq = %d, rwnd_size = %d\n", last_inorder_seq, rwnd_size);
                        // P(mtx);
                        SM[i].swnd.recv_wndsize = rwnd_size;
                        V(mtx);
                        // printf("Updated %d recv_wndsize to %d\n", i, rwnd_size);
                        // if(SM[i].swnd.recv_wndsize == 0) SM[i].nospace = 1;
                        // V(ack_mtx);

                        //! case 1: ack = start, then fine, move window by 1 
                        //! case 2: ack is somewhere between start to end - move window completely 
                        //! case 3: ack is not in the range - duplicate ack, ignore

                        int index=SM[i].swnd.window_start;
                        int count=0;int flag=0;

                        while(1){
                            count++;
                            // P(mtx);
                            // printf("SM[i].swnd.unack_msgs[%d].seq_no = %d\n", index, SM[i].swnd.unack_msgs[index].seq_no);
                            // printf("last_inorder_seq = %d\n", last_inorder_seq);
                            if(SM[i].swnd.unack_msgs[index].seq_no == last_inorder_seq){
                                // SM[j].swnd.window_start=(index+count)%SEND_BUFFER_SIZE;
                                // SM[j].swnd.wndsize+=count;
                                flag=1;
                                V(mtx);
                                break;
                            }
                            if(index==SM[i].swnd.window_end ) break;
                            index=(index+1)%SEND_BUFFER_SIZE;
                            V(mtx);
                            // printf("Broke\n");
                        }

                        // P(mtx);
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
                        // V(mtx);

                        break;
                    }

                    else {
                        // remove header; message will be of the form "seq:msg"
                       
                        int seq_num = atoi(strtok(buf, ":"));
                        char* msg = strtok(NULL, ":");

                        //! case 1: in order (start index) - accept it (move window would be done by recvfrom function)
                        //! case 2: out of order - store it 
                        //! case 3: duplicate - ignore it
                        //null the string
                        int index;

                        // P(mtx);
                        if(SM[i].rwnd.wndsize==RECV_BUFFER_SIZE){
                            // printf("In case 1\n");
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
                                            // printf("Writing to SM[%d].rwnd.exp_msgs[%d].message = %s\n", i, index, SM[i].rwnd.exp_msgs[index].message);
                                            SM[i].rwnd.window_end=index;
                                            SM[i].rwnd.wndsize--;
                                            next_seq=((next_seq)%15)+1;
                                            index=(index+1)%RECV_BUFFER_SIZE;
                                        }
                                    }
                                    V(mtx);
                                    break;
                                }
                                
                                index=(index+1)%RECV_BUFFER_SIZE;

                                if(index==SM[i].rwnd.window_start) {
                                    V(mtx);
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
                                    // printf("Writing to SM[%d].rwnd.exp_msgs[%d].message = %s\n", i, index, SM[i].rwnd.exp_msgs[index].message);
                                    
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

                                    V(mtx);
                                    break;
                                }
                                
                                index=(index+1)%RECV_BUFFER_SIZE;
                            } 

                        }

                        // send ACK in proper format: "<last_inorder_seq>:<rwnd_size>:ACK"
                        int last_seq_no;
                        // sleep(5);
                        // P(mtx);
                        if(SM[i].rwnd.wndsize==RECV_BUFFER_SIZE){
                            // printf("window start = %d, window end = %d\n", SM[i].rwnd.window_start, SM[i].rwnd.window_end);
                            // printf("curr seq no = %d\n", SM[i].rwnd.exp_msgs[SM[i].rwnd.window_end].seq_no);
                            last_seq_no=(SM[i].rwnd.curr_seq_no-RECV_BUFFER_SIZE-1+15)%15;
                            if(last_seq_no==0) last_seq_no=15;
                        }else last_seq_no=SM[i].rwnd.exp_msgs[SM[i].rwnd.window_end].seq_no;
                            
                        index=SM[i].rwnd.window_end;
                        char ack[1024];
                        bzero(ack, 1024);
                        sprintf(ack, "%d:%d:ACK", last_seq_no, SM[i].rwnd.wndsize);
                        if(SM[i].rwnd.wndsize==0) SM[i].nospace=1;
                        else SM[i].nospace=0;
                        sendto(SM[i].udpsockfd, ack, strlen(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                        V(mtx);
                        //  P(ack_mtx);
                        // printf("ACK done\n");
                        break;
                    }   
                        
                }
            }
        }

        else
        {  
            printf("No data within %d seconds.\n", T);
            // see if any of the 'nospace's can be updated
            P(mtx);
            // printf("Lock acquired\n");
            for(int i=0; i<N; i++)
            {
                if(SM[i].free == 0 && SM[i].nospace == 1)
                {
                    if(SM[i].rwnd.wndsize > 0)
                    {
                        // SM[i].nospace = 0;
                        // send duplicate ACK with updated rwnd size
                        // printf("This dude %d has space now!!\n",i);
                        int last_seq_no;
                        // todo: check this
                        // printf("window start = %d, window end = %d\n", SM[i].rwnd.window_start, SM[i].rwnd.window_end);
                        // for(int k=0;k<RECV_BUFFER_SIZE;k++){
                        //     // printf("SM[%d].rwnd.exp_msgs[%d].seq_no = %d, ", i, k, SM[i].rwnd.exp_msgs[k].seq_no);
                        // }printf("\n");
                        if(SM[i].rwnd.wndsize==RECV_BUFFER_SIZE){
                            // printf("window start = %d, window end = %d\n", SM[i].rwnd.window_start, SM[i].rwnd.window_end);
                            // printf("curr seq no = %d\n", SM[i].rwnd.curr_seq_no);
                            last_seq_no=(SM[i].rwnd.curr_seq_no-RECV_BUFFER_SIZE-1+15)%15;
                            if(last_seq_no==0) last_seq_no=15;
                        }else last_seq_no=SM[i].rwnd.exp_msgs[SM[i].rwnd.window_end].seq_no;

                        int index=SM[i].rwnd.window_end;
                        char ack[1024];
                        bzero(ack, 1024);
                        sprintf(ack, "%d:%d:ACK", last_seq_no, SM[i].rwnd.wndsize);
                        printf("Sending ack: [%s]\n", ack);

                        struct sockaddr_in cliaddr;
                        cliaddr.sin_family = AF_INET;
                        cliaddr.sin_port = htons(SM[i].port);
                        inet_aton(SM[i].ip, &cliaddr.sin_addr);
                        int len = sizeof(cliaddr);

                        sendto(SM[i].udpsockfd, ack, strlen(ack), 0, (struct sockaddr *)&cliaddr, len);
                        // P(ack_mtx);
                        // printf("ACK done\n");
                        
                    }
                }
            }
            V(mtx);
            // printf("Lock released\n");
        }
    }

    printf("Receiver thread done?!?!\n");
}


void *sender(void* arg) {

    printf("Sender thread started\n");

    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    /*
    The thread S behaves in the following manner. It sleeps for some time ( < T/2 ), and wakes up periodically.
    On waking up, it first checks whether the message timeout period (T) is over
    (by computing the time difference between the current time and the time when the messages
    within the window were sent last) for the messages sent over any of the active MTP sockets.
    If yes, it retransmits all the messages within the current swnd for that MTP socket. 
    
    It then
    checks the current swnd for each of the MTP sockets and determines whether there is a
    pending message from the sender-side message buffer that can be sent. If so, it sends that
    message through the UDP sendto() call for the corresponding UDP socket and updates the
    send timestamp.

    ratan was here
    */
   while(1){
        sleep((4*T)/10);
        printf("Sender thread woke up\n");
        /*On waking up, it first checks whether the message timeout period (T) is over
    (by computing the time difference between the current time and the time when the messages
    within the window were sent last) for the messages sent over any of the active MTP sockets.*/
        for(int i=0; i<N; i++){
            // printf("Checking first loop for i =%d\n", i);
            // printf("SM[i].free = %d, SM[i].swnd.recv_wndsize = %d, SM[i].port = %d, SM[i].ip = %s\n", SM[i].free, SM[i].swnd.recv_wndsize, SM[i].port, SM[i].ip);
            if(SM[i].free == 0 && SM[i].swnd.recv_wndsize > 0 && SM[i].port!=0 && strcmp(SM[i].ip,"")!=0 && SM[i].swnd.window_end!=-1){
                        // printf("Hereee in sender %d\n",i);
                        // check if T is over
                        time_t curr_time;
                        time(&curr_time);
                        if(difftime(curr_time, SM[i].swnd.timestamp) > T){
                            printf("retransmit?!\n");
                            // retransmit
                            int curr_seq_no = SM[i].swnd.unack_msgs[SM[i].swnd.window_start].seq_no;
                            if(curr_seq_no==-1) continue;
                            for(int j=0;j<SEND_BUFFER_SIZE;j++){
                                int index=(SM[i].swnd.window_start+j)%SEND_BUFFER_SIZE;
                                // printf("j=%d, recvwindowsize=%d\n", j, SM[i].swnd.recv_wndsize);
                                if(j>=SM[i].swnd.recv_wndsize) break;

                                struct sockaddr_in cliaddr;
                                cliaddr.sin_family = AF_INET;
                                cliaddr.sin_port = htons(SM[i].port);
                                inet_aton(SM[i].ip, &cliaddr.sin_addr);
                                int len = sizeof(cliaddr);

                                if(SM[i].swnd.unack_msgs[index].seq_no == curr_seq_no){
                                    printf("Sending message! %s\n", SM[i].swnd.unack_msgs[index].message);
                                    int x=sendto(SM[i].udpsockfd, SM[i].swnd.unack_msgs[index].message, strlen(SM[i].swnd.unack_msgs[index].message), 0, (struct sockaddr *)&cliaddr, len);
                                    P(mtx);
                                    
                                    if(j==0) SM[i].swnd.timestamp = curr_time;
                                    curr_seq_no=((curr_seq_no)%15)+1;
                                    SM[i].swnd.window_end=index;
                                    V(mtx);
                                    // printf("here2\n");
                                }
                                
                                else break;
                            }
                        }
            }
        }


        for(int i=0; i<N; i++){
            // printf("[%d], free = %d, recv_wndsize = %d, port = %d, ip = %s\n", i, SM[i].free, SM[i].swnd.recv_wndsize, SM[i].port, SM[i].ip);
            if(SM[i].free == 0 && SM[i].swnd.recv_wndsize && SM[i].port!=0 && strcmp(SM[i].ip,"")!=0){
                // printf("Here4, i=%d, start = %d, end=%d, wndsize=%d\n", i,SM[i].swnd.window_start, SM[i].swnd.window_end, SM[i].swnd.wndsize);
                    if(SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message[0]=='\0') break;

                    // printf("here right here\n");
                struct sockaddr_in cliaddr;
                cliaddr.sin_family = AF_INET;
                cliaddr.sin_port = htons(SM[i].port);
                inet_aton(SM[i].ip, &cliaddr.sin_addr);
                int len = sizeof(cliaddr);

             
                int already_sent=0;
                // printf("start=%d, end=%d\n", SM[i].swnd.window_start, SM[i].swnd.window_end);
                // printf("start seq no= %d\n", SM[i].swnd.unack_msgs[SM[i].swnd.window_start].seq_no);
               if(SM[i].swnd.window_end!=-1) printf("end seq no= %d\n", SM[i].swnd.unack_msgs[SM[i].swnd.window_end].seq_no);
                if(SM[i].swnd.window_end==-1 || ((((SM[i].swnd.window_end+1)%SEND_BUFFER_SIZE)==SM[i].swnd.window_start) && (((SM[i].swnd.unack_msgs[SM[i].swnd.window_end].seq_no%15)+1)==SM[i].swnd.unack_msgs[SM[i].swnd.window_start].seq_no || SM[i].swnd.unack_msgs[SM[i].swnd.window_end].seq_no==-1))){
                    SM[i].swnd.window_end=SM[i].swnd.window_start;
                    // printf("Sending message!!! %s\n", SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message);
                    sendto(SM[i].udpsockfd, SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message, strlen(SM[i].swnd.unack_msgs[SM[i].swnd.window_start].message), 0, (struct sockaddr *)&cliaddr, len);
                    printf("Sent to %s:%d, i=%d\n", SM[i].ip, SM[i].port, i);
                    SM[i].swnd.timestamp = time(NULL);

                }
                int j=SM[i].swnd.window_end;
                int curr_seq_no = ((SM[i].swnd.unack_msgs[j].seq_no)%15)+1;
                if(curr_seq_no==-1) continue;
                // printf("start=%d, end=%d, curr_seq_no=%d\n", SM[i].swnd.window_start, SM[i].swnd.window_end, curr_seq_no);
               already_sent=((SM[i].swnd.window_end<SM[i].swnd.window_start)?(SM[i].swnd.window_end+SEND_BUFFER_SIZE):SM[i].swnd.window_end)-SM[i].swnd.window_start+1;
               
                // printf("already_sent=%d\n", already_sent);
                j=(j+1)%SEND_BUFFER_SIZE;

                while(j!=SM[i].swnd.window_start){
                    if(SM[i].swnd.unack_msgs[j].seq_no == -1) break;
                    // printf("already_sent=%d, recv_wndsize=%d\n", already_sent, SM[i].swnd.recv_wndsize);
                    if(already_sent>=SM[i].swnd.recv_wndsize) break;
                    if(SM[i].swnd.unack_msgs[j].seq_no == curr_seq_no){
                        // send message
                        printf("Sending message!!! %s\n", SM[i].swnd.unack_msgs[j].message);
                        sendto(SM[i].udpsockfd, SM[i].swnd.unack_msgs[j].message, strlen(SM[i].swnd.unack_msgs[j].message), 0, (struct sockaddr *)&cliaddr, len);
                        P(mtx);
                        SM[i].swnd.window_end=j;
                        SM[i].swnd.timestamp = time(NULL);
                        V(mtx);
                        curr_seq_no=((curr_seq_no)%15)+1;
                    }
                    
                    else break;

                    j=(j+1)%SEND_BUFFER_SIZE;
                    already_sent++;
                }
            }
        }
        // printf("OVER!\n");
   }
}

void *garbage_collector(void *arg) {

    int shmid = (int)arg;
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);

    while(1){
        // see if any pid has exited
        for(int i=0; i<N; i++){
            if(SM[i].free == 0){
                if(kill(SM[i].pid, 0) == -1){
                    // pid has exited
                    P(mtx);
                    SM[i].free = 1;
                    if(close(SM[i].udpsockfd) == -1) {
                        V(mtx);
                        perror("close()");
                        pthread_exit(NULL);
                    }
                    V(mtx);
                }
            }
        }
    }
}

int main()
{   
    srand(time(0));

    key_t key1=ftok("msocket.h", 101);
    key_t key2=ftok("msocket.h", 102);
    key_t key3=ftok("msocket.h", 103);
    key_t key4=ftok("msocket.h", 104);
    
    semid1 = semget(key1, 1, 0777|IPC_CREAT);
    semid2 = semget(key2, 1, 0777|IPC_CREAT);
    mtx = semget(key3, 1, 0777|IPC_CREAT);
    ack_mtx = semget(key4, 1, 0777|IPC_CREAT);
    
    semctl(semid1, 0, SETVAL, 0);
    semctl(semid2, 0, SETVAL, 0);
    semctl(mtx, 0, SETVAL, 1);
    semctl(ack_mtx, 0, SETVAL, 0);

    //shared memory
    key_t key_sockinfo=ftok("msocket.h", 100);
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    sockinfo = (SOCK_INFO*) shmat(shmid_sockinfo, 0, 0);


    P(mtx);
    sockinfo->sockid = 0;
    sockinfo->port = 0;
    strcpy(sockinfo->IP, "");
    V(mtx);

    // printf("in main thread, sockinfo->IP=%s", sockinfo->IP);

    //shared memory
    int key = ftok("msocket.h", 99);
    int shmid = shmget(key, N * sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *SM = (msocket_t *)shmat(shmid, 0, 0);
    memset(SM, 0, N * sizeof(msocket_t));

    P(mtx);
    for(int i=0;i<N;i++){
        SM[i].free = 1;
    }
    V(mtx);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_t R, S, G;
    pthread_create(&R, &attr, receiver, (void *)shmid);          // to handle receiving messages
    pthread_create(&S, &attr, sender, (void *)shmid);            // to handle sending messages
    pthread_create(&G, &attr, garbage_collector, (void *)shmid); // to handle garbage collection

    while(1){
        //wait on Sem1
        // printf("Waiting on sem1\n");
        P(semid1);
        // printf("sem1 Signal received\n");

        //look at SOCK_INFO
        P(mtx);
        // printf("mtx unlocked\n");
        if(sockinfo->sockid==0 && sockinfo->port==0 && strcmp(sockinfo->IP,"")==0){
            //m_socket call
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            // printf("Creating socket\n");
            if(sockfd == -1){
                sockinfo->sockid = -1;
                sockinfo->errno = errno;
            }
            else{
                sockinfo->sockid = sockfd;
            }
            V(semid2);
            V(mtx);
            // printf("Socket created with \n");
        }

        else if(sockinfo->sockid!=0 && sockinfo->port!=0 && strcmp(sockinfo->IP,"")!=0){
            // printf("Bind call!\n");
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
            V(mtx);
            // printf("Bind done!\n");
        }
    }

    pthread_join(R, NULL);
    pthread_join(S, NULL);
    // pthread_join(G, NULL);

    return 0;
}