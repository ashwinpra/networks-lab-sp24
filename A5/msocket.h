#ifndef _MSOCKET_H
#define _MSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>

#define N 25 // max number of active sockets 
#define T 5 // timeout in seconds
#define p 0.05 // probability of packet drop (vary it)
#define SEND_BUFFER_SIZE 10
#define RECV_BUFFER_SIZE 5

#define SOCK_MTP 120 // random number for SOCK_MTP type

// error numbers
#define ENOBUFS 501 
#define ENOTBOUND 502
#define ENOMSG 503
#define EMISC 504

typedef struct {
    int unack_msgs[SEND_BUFFER_SIZE]; // sequence number of messages sent but not yet acknowledged
    int wndsize;    // window size indicating max number of messages that can be sent without receiving ACK 
} swnd_t; 

typedef struct {
    int exp_msgs[RECV_BUFFER_SIZE]; // sequence number of messages expected to be received
    int wndsize;    // window size indicating max number of messages that can be received based on buffer availability
} rwnd_t;

typedef struct {
    int free;  // 1 if its free, 0 otherwise
    int pid; // pid of the process that created the socket
    int udpsockfd; // socket descriptor of the underlying UDP socket
    char *ip; // ip address of the other end of the MTP socket
    int port; // port number of the other end of the MTP socket
    char send_buffer[SEND_BUFFER_SIZE][1024]; 
    char recv_buffer[RECV_BUFFER_SIZE][1024];
    swnd_t swnd; 
    rwnd_t rwnd; 
    int nospace; // 1 if no space in recv_buffer, 0 otherwise
} msocket_t;

int errno; // global variable to store error number

int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);

#endif