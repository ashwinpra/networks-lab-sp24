#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LEN 80
#define MAX_LINES 50 
#define CRLF "\r\n"

void remove_CRLF(char *line) {
    for(int i=0; i<strlen(line); i++) {
        if(line[i] == '\r' && line[i+1] == '\n') {
            line[i] = '\0';
            break;
        }
    }
}

void get_maillist_from_server(int sockfd, int num_mails, char* mails[MAX_LINES+3]){
    for (int i=0; i<num_mails; i++) {
        mails[i] = NULL;
    }
    char buf[101];
    char temp[100];
    bzero(buf, 101);

    for(int i=1; i<=num_mails; i++) {
        // send RETR
        // sprintf(buf, "RETR %d", i);
        // send_message(sockfd, buf);

        // // expected: +OK
        // if(!receive_pop3_status(sockfd, "+OK")) continue;

        char sender[100], time[100], subject[100];

        // server will send line-by-line until .CRLF
        // from it, extract mail in "From: " line, Time of mail, Subject
        //receive mail from client
            int line=0;
            bzero(temp,100);
            int temp_index=0,buf_index=0,done=0;
            while(!done){
                bzero(buf, 100);
               
                int n = recv(sockfd, buf, 100, 0);
                if(n>=100) buf[n]='\0';
               
                buf_index=0;
                while(buf_index<n){
                    if(buf[buf_index]=='\r'){
                        if(buf_index==n-1){
                            temp[temp_index++]=buf[buf_index++];
                        }else{
                            temp[temp_index++]=buf[buf_index++];
                            temp[temp_index++]=buf[buf_index++];
                            temp[temp_index++]='\0';
                            line++;
                            
                            if(line==1){
                                // sender comes after "From: "
                                strcpy(sender, temp+6);
                                remove_CRLF(sender);
                            }
                            else if(line==3){
                                // subject comes after "Subject: "
                                strcpy(subject, temp+9);
                                remove_CRLF(subject);
                            }
                            else if(line==4){
                                // time comes after "Received: "
                                strcpy(time, temp+10);
                                remove_CRLF(time);
                            }
                            else if(strcmp(temp,".\r\n")==0){
                                temp_index=0;
                                bzero(temp,100);
                                done=1;
                                break;
                            }
                            temp_index=0;
                            bzero(temp,100);
                        }
                    }
                    else if(temp_index!=0 && (buf[buf_index]=='\n') && (temp[temp_index-1]=='\r')){
                            temp[temp_index++]=buf[buf_index++];
                            temp[temp_index++]='\0';
                            line++;
                            
                            if(line==1){
                                // sender comes after "From: "
                                strcpy(sender, temp+6);
                                remove_CRLF(sender);
                            }
                            else if(line==3){
                                // subject comes after "Subject: "
                                strcpy(subject, temp+9);
                                remove_CRLF(subject);
                            }
                            else if(line==4){
                                // time comes after "Received: "
                                strcpy(time, temp+10);
                                remove_CRLF(time);
                            }
                            else if(strcmp(temp,".\r\n")==0){
                                temp_index=0;
                                bzero(temp,100);
                                done=1;
                                break;
                            }
                            temp_index=0;
                            bzero(temp,100);
                    }
                    else{
                        temp[temp_index++]=buf[buf_index++];
                    }
                }
            }

        // add to mails array
        char mail_num[4];
        sprintf(mail_num, "%d", i);
        mails[i-1] = malloc(strlen(mail_num)+strlen(sender)+strlen(time)+strlen(subject)+6);
        strcpy(mails[i-1], mail_num);
        strcat(mails[i-1], " <");
        strcat(mails[i-1], sender);
        strcat(mails[i-1], "> ");
        strcat(mails[i-1], time);
        strcat(mails[i-1], " ");
        strcat(mails[i-1], subject);
        }
    }


int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        perror("Cannot create socket\n");
        exit(0);
    }

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &server_addr.sin_addr);
    server_addr.sin_port = htons(2500);

    if((connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
        perror("Unable to connect to server\n");
        exit(0);
    }

    printf("Connected to server\n");

    int num_mails = 1; 
    char* mails[MAX_LINES+3];
    get_maillist_from_server(sockfd, num_mails, mails);

    for (int i=0; i<num_mails; i++) {
        if (mails[i] != NULL) {
            printf("mail[%d]: %s\n", i, mails[i]);
        } else{
            printf("mail[%d]: NULL\n", i);
        }
    }
}
