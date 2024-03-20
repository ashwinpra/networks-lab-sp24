#include<stdio.h> 
#include<stdlib.h> 
#include<string.h> 
#include<msocket.h>
#include<unistd.h>

// this will run on port 8081, and talk to user 1 on port 8080
int main(int argc, char const *argv[])
{   
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if(sockfd < 0){
        perror("socket creation failed");
        return -1;
    }

    printf("Socket %d created!\n", sockfd);
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8080);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 

    // printf("Binding to user1\n");

    if(m_bind(sockfd, "127.0.0.1", 8081, "127.0.0.1", 8080) < 0){
        perror("bind failed");
        return -1;
    }

    // printf("Bind done!\n");

    sleep(15);

    char buf[1024];

    // while(m_recvfrom(sockfd, buf, 1024, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){    
    //     printf("1: Not received yet\n");
    // }

    // // printf("Received now!\n");
    // printf("\nReceived: %s\n\n", buf);

    // while(m_recvfrom(sockfd, buf, 1024, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){    
    //     printf("2: Not received yet\n");
    // }

    // printf("\nReceived: %s\n", buf);

    // receive the file from user1
    FILE *fp = fopen("test2.txt", "w");
    if(fp == NULL){
        perror("fopen failed");
        return -1;
    }

    while(1) {
        if(m_recvfrom(sockfd, buf, 1024, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){    
            printf("Not received yet\n");
        }
        else {
            printf("\nReceived: %s\n", buf);
            if(strcmp(buf, "EOF") == 0){
                break;
            }
            fprintf(fp, "%s", buf);
            printf("Written to file\n");
        }
    }



    m_close(sockfd);

    return 0;
}