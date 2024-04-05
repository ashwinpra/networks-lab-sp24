#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>	
#include <time.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netpacket/packet.h>


typedef struct _query {
    int len;
    char domain[32];
} query;

typedef struct _simDNSQuery {
    int id;
    int type; 
    int n_queries;
    query queries[8];
} simDNSQuery;

typedef struct _response {
    int valid;
    char ip[32];
} response;

typedef struct _simDNSResponse {
    int id;
    int n_responses;
    response responses[8];
} simDNSResponse;

int main() {
    // open a raw socket to capture all packets till Ethernet
    int sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(50000); 
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

    printf("Socket %d created and bound\n", sockfd);

    // read all packets received on the socket
    while(1) {

        // read the packet including headers 
        char packet[65536];
        int len = recvfrom(sockfd, packet, 65536, 0, NULL, NULL);
        if(len < 0) {
            perror("recvfrom");
            return 1;
        }

        // extract the Ethernet header
        struct ethhdr *eth = (struct ethhdr *)packet;
        if(ntohs(eth->h_proto) != ETH_P_IP) {
            continue;
        }

        // extract the IP header
        struct iphdr *ip = (struct iphdr *)(packet + sizeof(struct ethhdr));
        if(ip->protocol != 254) {
            continue;
        }

        // extract the data
        simDNSQuery qryPacket;
        memcpy(&qryPacket, packet + sizeof(struct ethhdr) + sizeof(struct iphdr), sizeof(simDNSQuery));

        simDNSResponse resPacket;
        resPacket.id = qryPacket.id;
        resPacket.n_responses = qryPacket.n_queries;

        // process the query
        printf("Received simDNS query packet with ID %d\n", qryPacket.id);
        printf("Number of queries: %d\n", qryPacket.n_queries);
        for(int i=0; i<qryPacket.n_queries; i++) {
            printf("Query %d: %s\n", i, qryPacket.queries[i].domain);
            struct hostent *host;
            host = gethostbyname(qryPacket.queries[i].domain);
            if(host == NULL) {
                resPacket.responses[i].valid = 0;
            } else {
                resPacket.responses[i].valid = 1;
                strcpy(resPacket.responses[i].ip, inet_ntoa(*((struct in_addr *)host->h_addr)));
            }
        }

        // send the response
        printf("Sending simDNS response packet with ID %d\n", resPacket.id);
        printf("Number of responses: %d\n", resPacket.n_responses);
        for(int i=0; i<resPacket.n_responses; i++) {
            printf("Response %d: %s\n", i, resPacket.responses[i].ip);
        }

        struct sockaddr_ll dest;
        memset(&dest, 0, sizeof(dest));
        dest.sll_family = AF_PACKET;
        dest.sll_protocol = htons(ETH_P_ALL);

        sendto(sockfd, &resPacket, sizeof(simDNSResponse), 0, (struct sockaddr *)&dest, sizeof(dest));

    }

}