#ifndef _MSOCKET_H
#define _MSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define N 25 // max number of active sockets 

int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);

#endif