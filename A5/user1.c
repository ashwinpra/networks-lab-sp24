#include<stdio.h> 
#include<stdlib.h> 
#include<string.h> 
#include<msocket.h>
#include<unistd.h>

// this will run on port 8080, and talk to user 2 on port 8081
int main(int argc, char const *argv[])
{   
    printf("My pid: %d\n", getpid());
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if(sockfd < 0){
        perror("socket creation failed");
        return -1;
    }

    printf("Socket %d created!\n", sockfd);


    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8081);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Binding to user2\n");

    if(m_bind(sockfd, "127.0.0.1", 8080, "127.0.0.1", 8081) < 0){
        perror("bind failed");
        return -1;
    }

    printf("Bind done\n");
    sleep(7);

    char buf[] = "Hello from user1";

    if(m_sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){
        perror("sendto failed");
        return -1;
    }

    printf("Sent!\n");
    sleep(30);

    if(m_close(sockfd) < 0){
        perror("close failed");
        return -1;
    }

    sleep(10);

    return 0;
}
