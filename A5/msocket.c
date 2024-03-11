#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>

int m_socket(int domain, int type, int protocol)
{
    // todo: get key
    int key;
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

int m_close(int sockfd)
{
    // todo: get key
    int key;
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