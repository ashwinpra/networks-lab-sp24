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
#include <net/if.h>

#define p 0.1

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

int dropMessage(float P){
    float r = (float)rand()/(float)(RAND_MAX);
    if(r<P){
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    srand(time(0));

    if(argc != 2) {
        printf("Usage: %s <interface>\n", argv[0]);
        return 1;
    }

    // open a raw socket to capture all packets till Ethernet
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sockfd < 0) {
        perror("socket");
        return 1;
    }

    char* interface = argv[1];

    struct sockaddr_ll addr;
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex(interface);

    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // read all packets received on the socket
    while(1) {

        // read the packet including headers 
        char packet[1024];
        int len = recvfrom(sockfd, packet, 1024, 0, NULL, NULL);
        if(len < 0) {
            perror("recvfrom");
            return 1;
        }

        // extract the Ethernet header
        struct ethhdr *eth = (struct ethhdr *)packet;
        if(ntohs(eth->h_proto) != ETH_P_IP) {continue;}

        // get source MAC to send response later
        char destMAC[18];
        sprintf(destMAC, "%02x:%02x:%02x:%02x:%02x:%02x", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]);

        // extract the IP header
        struct iphdr *ip = (struct iphdr *)(packet + sizeof(struct ethhdr));
        if(ip->protocol != 254) {continue;}

        if(dropMessage(p)){continue;}


        // extract the data
        simDNSQuery qryPacket;
        memcpy(&qryPacket, packet + sizeof(struct ethhdr) + sizeof(struct iphdr), sizeof(simDNSQuery));

        simDNSResponse resPacket;
        resPacket.id = qryPacket.id;
        resPacket.n_responses = qryPacket.n_queries;

        // process the query
        for(int i=0; i<qryPacket.n_queries; i++) {
            struct hostent *host;
            host = gethostbyname(qryPacket.queries[i].domain);
            if(host == NULL) {
                resPacket.responses[i].valid = 0;
            } else {
                resPacket.responses[i].valid = 1;
                strcpy(resPacket.responses[i].ip, inet_ntoa(*((struct in_addr *)host->h_addr)));
            }
        }

        struct ethhdr *eth1 = (struct ethhdr *)packet;
        memset(eth1->h_source, 0, 6); 
        sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &eth1->h_dest[0], &eth1->h_dest[1], &eth1->h_dest[2], &eth1->h_dest[3], &eth1->h_dest[4], &eth1->h_dest[5]);

        struct iphdr *ip1 = (struct iphdr *)(packet + sizeof(struct ethhdr));
        ip1->ihl = 5;
        ip1->version = 4;
        ip1->tos = 0;
        ip1->tot_len = htons(sizeof(struct iphdr) + sizeof(simDNSResponse));
        ip1->id = htons(0); 
        ip1->frag_off = 0;
        ip1->ttl = 64; // arbitrary value
        ip1->protocol = 254; // 254 for simDNS
        ip1->saddr = inet_addr("127.0.0.1"); 
        ip1->daddr = inet_addr("127.0.0.1"); 

        memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), &resPacket, sizeof(simDNSResponse));

        struct sockaddr_ll dest;
        memset(&dest, 0, sizeof(dest));
        dest.sll_family = AF_PACKET;
        dest.sll_protocol = htons(ETH_P_ALL);
        sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &dest.sll_addr[0], &dest.sll_addr[1], &dest.sll_addr[2], &dest.sll_addr[3], &dest.sll_addr[4], &dest.sll_addr[5]);
        dest.sll_ifindex = if_nametoindex("enp0s25"); 

        if(sendto(sockfd, packet, sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(simDNSResponse), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            perror("sendto");
            return 1;
        }

    }

}