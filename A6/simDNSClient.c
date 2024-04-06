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
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <net/if.h>

#define T 5 // timeout in seconds

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

int domainNameIsValid(char *domain) {
    // 1. only alphanumeric characters, except hyphen (but not at the beginning or end), also no consecutive hyphens. dots are allowed
    // 2. minimum length of 3, maximum length of 31 

    if(strlen(domain) < 3 || strlen(domain) > 31) {
        return 0;
    }

    int hyphenFlag = 0;
    for(int i=0; i<strlen(domain); i++) {
        if((domain[i] >= 'a' && domain[i] <= 'z') || (domain[i] >= 'A' && domain[i] <= 'Z') || (domain[i] >= '0' && domain[i] <= '9')) {
            hyphenFlag = 0;
        } else if(domain[i] == '-') {
            if(i == 0 || i == strlen(domain)-1 || hyphenFlag == 1) {
                return 0;
            }
            hyphenFlag = 1;
        } else if(domain[i] == '.') {
            hyphenFlag = 0;
        } else {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char* argv[]) {

    if(argc != 2) {
        printf("Usage: %s <dest-MAC>\n", argv[0]);
        return 1;
    }

    char *destMAC = argv[1];
     
    printf("Welcome to simDNS Client\n");

    int curr_ID = 0; 
    int pendingQueries[100]; 

    // open a raw socket to capture all packets till Ethernet
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    struct sockaddr_ll addr;
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex("enp0s25");
    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }


    // wait for query from user: of the form getIP N <domain-1> <domain-2> ... <domain-N> 
    char query[100];

    while(1){
        
        // read query from user
        fgets(query, 100, stdin);
        query[strlen(query)-1] = '\0';

        printf("Got query: %s\n", query);

        if(strcmp(query, "EXIT") == 0) {
            break;
        }

        char *token = strtok(query, " ");
        if(strcmp(token, "getIP") != 0) {
            printf("Error: Query format: getIP N <domain-1> <domain-2> ... <domain-N> \n");
            continue;
        }

        int N = atoi(strtok(NULL, " "));

        if(N>8) {
            printf("Error: N should be less than or equal to 8\n");
            continue;
        }

        char domains[N][32];
        for(int i=0; i<N; i++) {
            char *domain = strtok(NULL, " ");
            if(!domainNameIsValid(domain)) {
                printf("Error: Invalid domain name <%s>\n", domain);
                continue;
            }
            strcpy(domains[i], domain);
        }

        // construct simDNS packet

        simDNSQuery qryPacket;
        qryPacket.id = curr_ID++;
        qryPacket.type = 0; 
        qryPacket.n_queries = N;
        for(int i=0; i<N; i++) {
            qryPacket.queries[i].len = strlen(domains[i]);
            strcpy(qryPacket.queries[i].domain, domains[i]);
        }

        // make ethernet and IP headers
        char packet[65536];

        struct ethhdr *eth = (struct ethhdr *)packet;
        memset(eth->h_source, 0, 6); 
        sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &eth->h_dest[0], &eth->h_dest[1], &eth->h_dest[2], &eth->h_dest[3], &eth->h_dest[4], &eth->h_dest[5]);
        eth->h_proto = htons(ETH_P_IP);

        struct iphdr *ip = (struct iphdr *)(packet + sizeof(struct ethhdr));
        ip->ihl = 5;
        ip->version = 4;
        ip->tos = 0;
        ip->tot_len = htons(sizeof(struct iphdr) + sizeof(simDNSQuery));
        ip->id = htons(0); 
        ip->frag_off = 0;
        ip->ttl = 64; // arbitrary value
        ip->protocol = 254; // 254 for simDNS
        ip->saddr = inet_addr("127.0.0.1"); 
        ip->daddr = inet_addr("127.0.0.1"); 

        // prepare the packet
        // todo: check here
        memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), &qryPacket, sizeof(simDNSQuery));

        // send the packet over IP
        struct sockaddr_ll addr;
        addr.sll_family = AF_PACKET;
        addr.sll_protocol = htons(ETH_P_ALL);
        addr.sll_halen = 6;
        sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr.sll_addr[0], &addr.sll_addr[1], &addr.sll_addr[2], &addr.sll_addr[3], &addr.sll_addr[4], &addr.sll_addr[5]);

        sendto(sockfd, packet, sizeof(struct ethhdr) + ntohs(ip->tot_len), 0, (struct sockaddr *)&addr, sizeof(addr));

        printf("Sent query!\n");

        // store the query ID in pendingQueries
        pendingQueries[qryPacket.id] = 1; 

        // wait for response
        fd_set fds; 
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds); 

        struct timeval tv;
        tv.tv_sec = T;
        tv.tv_usec = 0;

        int ret = select(sockfd+1, &fds, NULL, NULL, &tv);  

        if(ret < 0) {
            perror("select");
            return 1;
        }

        if(ret == 0) {
            printf("Timeout\n");
            for(int i=0; i<N; i++) {
                // if it was pending, increase the timeout count
                if(pendingQueries[qryPacket.id] > 0) {
                    pendingQueries[qryPacket.id]++;
                }

                if(pendingQueries[qryPacket.id] > 3) {
                    printf("Error: Timeout for query ID %d\n", qryPacket.id);
                    pendingQueries[qryPacket.id] = 0;
                }
            }
            continue;
        }

        if(FD_ISSET(sockfd, &fds)) {
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
            simDNSResponse resPacket;
            memcpy(&resPacket, packet + sizeof(struct ethhdr) + sizeof(struct iphdr), sizeof(simDNSResponse));

            // process the response
            if(pendingQueries[resPacket.id] == 1) {
                printf("Query ID: %d\n", resPacket.id);
                printf("Total query strings: %d\n", resPacket.n_responses);
                for(int i=0; i<resPacket.n_responses; i++) {
                    if(resPacket.responses[i].valid == 1) {
                        printf("Domain %d: IP: %s\n", i+1, resPacket.responses[i].ip);
                    } else {
                        printf("Domain %d: IP: Not found\n", i+1);
                    }
                }
                pendingQueries[resPacket.id] = 0;
            }
        }
        }

}