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
#include <ctype.h>
#include <sys/time.h>

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

typedef struct _pendingQuery {
    int timeoutCount;
    query queries[8];
    struct timeval tv; 
} pendingQuery;

// to strip extra spaces from the input string
void strip(char* s) {
    char* start = s;
    char* end = s + strlen(s);

    while (isspace((unsigned char)*start)) start++;

    if (end > start) {
        while (isspace((unsigned char)*(end - 1))) end--;
    }

    memmove(s, start, end - start);

    s[end - start] = '\0';
}

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

void constructPacket(char packet[1024], char *destMAC, simDNSQuery qryPacket) {

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

    memcpy(packet + sizeof(struct ethhdr) + sizeof(struct iphdr), &qryPacket, sizeof(simDNSQuery));
}

int main(int argc, char* argv[]) {

    if(argc != 3) {
        printf("Usage: %s <dest-MAC> <interface>\n", argv[0]);
        return 1;
    }

    char *destMAC = argv[1];
    char* interface = argv[2];
     
    int curr_ID = 0; 
    pendingQuery pendingQueries[1000];
    memset(pendingQueries, 0, sizeof(pendingQueries));

    // open a raw socket to capture all packets till Ethernet
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_ll addr;
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex(interface);
    sscanf(destMAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &addr.sll_addr[0], &addr.sll_addr[1], &addr.sll_addr[2], &addr.sll_addr[3], &addr.sll_addr[4], &addr.sll_addr[5]);


    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }


    // wait for query from user: of the form getIP N <domain-1> <domain-2> ... <domain-N> 
    char query[100];

    while(1){
        printf("Enter query: ");
        // read query from user
        fgets(query, 100, stdin);
        query[strlen(query)-1] = '\0';

        strip(query);

        if(strcmp(query, "EXIT") == 0) {
            break;
        }

        char query_copy[100];
        strcpy(query_copy, query);

        char *token = strtok(query_copy, " ");
        if(strcmp(token, "getIP") != 0) {
            printf("Error: Query format: getIP N <domain-1> <domain-2> ... <domain-N> \n\n");
            continue;
        }

        int N = atoi(strtok(NULL, " "));

        if(N == 0) {
            printf("Error: N should be a valid number\n\n");
            continue;
        }

        if(N>8) {
            printf("Error: N should be less than or equal to 8\n\n");
            continue;
        }

        // check if number of query strings matches N
        int count = 0;
        for(int i=0; i<strlen(query); i++) {
            if(query[i] == ' ') count++;
        }

        if(count != N+1) {
            printf("Error: Number of query strings should match N\n\n");
            continue;
        }
        
        // check if all domain names are valids
        int invalid_flag = 0;
        char domains[N][32];
        for(int i=0; i<N; i++) {
            char *domain = strtok(NULL, " ");
            if(!domainNameIsValid(domain)) {
                printf("Error: Invalid domain name: %s\n", domain);
                invalid_flag = 1;
                continue;
            }
            strcpy(domains[i], domain);
        }
        if(invalid_flag) {continue;} 


        // construct simDNS packet
        simDNSQuery qryPacket;
        qryPacket.id = curr_ID++;
        qryPacket.type = 0; 
        qryPacket.n_queries = N;
        for(int i=0; i<N; i++) {
            qryPacket.queries[i].len = strlen(domains[i]);
            strcpy(qryPacket.queries[i].domain, domains[i]);
        }

        char packet[1024];
        constructPacket(packet, destMAC, qryPacket);

        if(sendto(sockfd, packet, sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(simDNSQuery), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("sendto");
            return 1;
        }

        // store the query ID in pendingQueries
        pendingQueries[qryPacket.id].timeoutCount = 1;
        for(int i=0; i<N; i++) {
            pendingQueries[qryPacket.id].queries[i].len = qryPacket.queries[i].len;
            strcpy(pendingQueries[qryPacket.id].queries[i].domain, qryPacket.queries[i].domain);
        }

        // set timeval to check for timeouts for all pending queries
        for(int i=0; i<1000; i++) {
            if(pendingQueries[i].timeoutCount >= 1) {
                gettimeofday(&pendingQueries[i].tv, NULL);
            }
        }

        // wait for response
        fd_set fds; 
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds); 

        int ret = select(sockfd+1, &fds, NULL, NULL, NULL);  

        if(ret < 0) {
            perror("select");
            return 1;
        }

        if(FD_ISSET(sockfd, &fds)) {
            int queryIsPending = 1;
            while(queryIsPending){
                int timeout_flag = 0;

                // check if timeout has occured for any pending query
                for(int i=0; i<1000; i++) {
                    if(pendingQueries[i].timeoutCount >= 1) {
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        if(tv.tv_sec - pendingQueries[i].tv.tv_sec >= T) {
                            pendingQueries[i].timeoutCount++;
                            timeout_flag = 1;

                            // resend the query
                            simDNSQuery qryPacket;
                            qryPacket.id = i;
                            qryPacket.type = 0;
                            qryPacket.n_queries = 0;
                            for(int j=0; j<8; j++) {
                                if(pendingQueries[i].queries[j].len > 0) {
                                    qryPacket.n_queries++;
                                    qryPacket.queries[j].len = pendingQueries[i].queries[j].len;
                                    strcpy(qryPacket.queries[j].domain, pendingQueries[i].queries[j].domain);
                                }
                            }

                            char packet[1024];
                            constructPacket(packet, destMAC, qryPacket);

                            if(sendto(sockfd, packet, sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(simDNSQuery), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                                perror("sendto");
                                return 1;
                            }

                        }
                        if(pendingQueries[i].timeoutCount > 3) {
                            printf("\n----------------------------------------\n");
                            printf("Error: Timeout for query ID: %d\n", i);
                            printf("----------------------------------------\n\n");
                            pendingQueries[i].timeoutCount = 0;
                    }
                    }
                }

                if(timeout_flag) {break;}

                char packet[1024];
                int len = recvfrom(sockfd, packet, 1024, 0, NULL, NULL);
                if(len < 0) {
                    perror("recvfrom");
                    return 1;
                }

                // check ethernet and IP headers
                struct ethhdr *eth = (struct ethhdr *)packet;
                if(ntohs(eth->h_proto) != ETH_P_IP) {continue;}

                struct iphdr *ip = (struct iphdr *)(packet + sizeof(struct ethhdr));
                if(ip->protocol != 254) {continue;}

                // extract the data and process response
                simDNSResponse resPacket;
                memcpy(&resPacket, packet + sizeof(struct ethhdr) + sizeof(struct iphdr), sizeof(simDNSResponse));

                if(pendingQueries[resPacket.id].timeoutCount >= 1) {
                    printf("\n----------------------------------------\n");
                    printf("Query ID: %d\n", resPacket.id);
                    printf("Total query strings: %d\n", resPacket.n_responses);
                    for(int i=0; i<resPacket.n_responses; i++) {
                        if(resPacket.responses[i].valid == 1) {
                            printf("%s: IP: %s\n", pendingQueries[resPacket.id].queries[i].domain, resPacket.responses[i].ip);
                        } else {
                            printf("%s: IP not found\n", pendingQueries[resPacket.id].queries[i].domain);
                        }
                    }
                    printf("----------------------------------------\n\n");
                    pendingQueries[resPacket.id].timeoutCount = 0;
                }

                for(int i=0; i<1000; i++) {
                    if(pendingQueries[i].timeoutCount >= 1) {
                        queryIsPending = 1;
                        break;
                    } else {
                        queryIsPending = 0;
                    }
                }
            }
        }
        }

}