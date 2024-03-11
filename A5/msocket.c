// get the shared memory segment
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <msocket.h>

// todo: change IPC_PRIVATE to a key accordingly

int m_socket(int domain, int type, int protocol)
{
    // todo: "type" checking

    int shmid = shmget(IPC_PRIVATE, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);
    
    int freeidx = -1;
    for (int i = 0; i < N; i++)
    {
        if (msocket[i].free == 1)
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

    msocket[freeidx].free = 0;
    msocket[freeidx].pid = getpid();
    if((msocket[freeidx].udpsockfd = socket(domain, SOCK_DGRAM, protocol)) == -1)
    {
        errno = EMISC;
        return -1;
    }
    msocket[freeidx].swnd.wndsize = SEND_BUFFER_SIZE;
    msocket[freeidx].rwnd.wndsize = RECV_BUFFER_SIZE;
}

int m_close(int sockfd)
{
    int shmid = shmget(IPC_PRIVATE, N*sizeof(msocket_t), 0666 | IPC_CREAT);
    msocket_t *msocket = (msocket_t *)shmat(shmid, 0, 0);

    for (int i = 0; i < N; i++)
    {
        if (msocket[i].pid == getpid())
        {
            msocket[i].free = 1;
            if(close(msocket[i].udpsockfd) == -1) {
                errno = EMISC;
                return -1;
            }
            return 0;
        }
    }

    errno = EMISC;
    return -1;
}