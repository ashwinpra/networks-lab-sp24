#include<stdio.h> 
#include<stdlib.h> 
#include<string.h> 
#include<msocket.h>
#include<unistd.h>

// this will run on port 8082, and talk to user 2 on port 8083
int main(int argc, char const *argv[])
{   
    char buf[1024];

    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if(sockfd < 0){
        perror("socket creation failed");
        return -1;
    }

    printf("Socket %d created!\n", sockfd);

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8083);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // printf("Binding to user2\n");

    if(m_bind(sockfd, "127.0.0.1", 8082, "127.0.0.1", 8083) < 0){
        perror("bind failed");
        return -1;
    }


    FILE *fp = fopen("test3.txt", "r");
    if(fp == NULL){
        perror("fopen failed");
        return -1;
    }
    int count=1;

    while(fgets(buf, 1015, fp) != NULL){
      
        while(1){
        if(m_sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){
            perror("sendto failed");
            sleep(7);
            continue;
        }
        printf("count = %d\n", count);
        count++;
        break;
        }
        
    }

    strcpy(buf, "EOF");
    while(1){
    if(m_sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){
        perror("sendto failed");
        sleep(7);
        continue;
    }
    printf("Sent: %s\n", buf);
    printf("count = %d\n", count);
    break;
    }

    sleep(1000);
    
    m_close(sockfd);

    printf("Closed!");

    sleep(10);

    return 0;
}