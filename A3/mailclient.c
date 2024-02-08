/*
    Networks Lab Assignment 3
    SMTP + POP3 Mail Client 
    Sarika Bishnoi 21CS10058
    Ashwin Prasanth 21CS30009
*/

// todo: 
// ! check if get_mail* functions are working properly 
// ! check if final . is getting handled properly 
// ! server-client sync

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

void strip(char* s);
void append_CRLF(char *line);
int get_choice();
int get_mail_from_user(char* lines[MAX_LINES+3]);
void get_mail_from_server(int sockfd, char mail[(MAX_LINES+1)*(MAX_LINE_LEN+1)+1]);
void get_maillist_from_server(int sockfd, int num_mails, char* mails[MAX_LINES+4]);
void send_message(int sockfd, char *msg);
void receive_message(int sockfd, char *msg);
int receive_smtp_status(int sockfd, int expected);
int receive_pop3_status(int sockfd, char* expected);


int main(int argc, char const *argv[])
{
    if (argc != 4){
        printf("Usage: %s <server_ip> <smtp_port> <pop3_port>\n", argv[0]);
        exit(0);
    }

    const char *server_ip = argv[1];
    int smtp_port = atoi(argv[2]);
    int pop3_port = atoi(argv[3]);

    struct sockaddr_in server_addr;
    char buf[MAX_LINE_LEN+2];

    char username[100], password[100];
    printf("Enter username: ");
    fgets(username, 100, stdin);
    printf("Enter password: ");
    fgets(password, 100, stdin);

    int ret; // for checking status of commands

    while (1) {
        int choice = get_choice();

        if (choice==1) {
            // manage mail - POP3 client

            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0){
                perror("Cannot create socket\n");
                exit(0);
            }

            server_addr.sin_family = AF_INET;
            inet_aton(server_ip, &server_addr.sin_addr);
            server_addr.sin_port = htons(pop3_port);

            if((connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
                perror("Unable to connect to server\n");
                exit(0);
            }

            // expected: +OK POP3 server ready
            if(!receive_pop3_status(sockfd, "+OK")) continue;

            // now in AUTHORIZATION state
            
            // send USER 
            strcpy(buf, "USER ");
            strcat(buf, username);
            send_message(sockfd, buf);

            // expected: +OK
            if(!receive_pop3_status(sockfd, "+OK")) continue;

            // send PASS
            strcpy(buf, "PASS ");
            strcat(buf, password);
            send_message(sockfd, buf);

            // expected: +OK
            if(!receive_pop3_status(sockfd, "+OK")) continue;

            // now in TRANSACTION state

            // send STAT to get number of mails
            send_message(sockfd, "STAT");
            char stat_resp[1000];
            receive_message(sockfd, stat_resp);

            // expected: +OK <num_mails> <total_size>
            if(strncmp(stat_resp, "+OK", 3) != 0) {
                printf("Error in managing mail: %s\n", stat_resp);
                send_message(sockfd, "QUIT\r\n");
                continue;
            }

            char *token = strtok(stat_resp, " ");
            token = strtok(NULL, " ");
            int num_mails = atoi(token);

            // query for each of those mails 
            // make an array for storing all mails
            char* mails[MAX_LINES+3];
            get_mails_from_server(sockfd, num_mails, mails);

            int deleted[num_mails];
            for(int i=0; i<num_mails; i++) {
                deleted[i] = 0;
            }

            while (1) {
                // print all mails
                for (int i=0; i<num_mails; i++) {
                    if(mails[i] != NULL && !deleted[i]) {
                        printf("%s\n", mails[i]);
                    }
                }

                int mail_choice; 
                while(1) {
                    printf("Enter mail number to view (-1 to go back): ");
                    scanf("%d", &mail_choice);
                    if(mail_choice == -1) {
                        break;
                    }
                    if(mail_choice < 1 || mail_choice > num_mails) {
                        printf("Mail no. out of range, give again\n");
                        continue;
                    }
                }

                // send RETR
                sprintf(buf, "RETR %d", mail_choice);
                send_message(sockfd, buf);

                // expected: +OK
                if(!receive_pop3_status(sockfd, "+OK")) continue;

                char mail[(MAX_LINES+1)*(MAX_LINE_LEN+1)+1];
                get_mail_from_server(sockfd, mail);

                // print mail
                printf("%s\n", mail);

                char del_choice = getchar();
                if(del_choice == 'd') {
                    // send DELE
                    sprintf(buf, "DELE %d", mail_choice);
                    send_message(sockfd, buf);

                    // expected: +OK
                    if(!receive_pop3_status(sockfd, "+OK")) continue;

                    deleted[mail_choice-1] = 1;
                }

            }

        }
        else if (choice==2) {
            // send mail - SMTP client

            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0){
                perror("Cannot create socket\n");
                exit(0);
            }

            server_addr.sin_family = AF_INET;
            inet_aton(server_ip, &server_addr.sin_addr);
            server_addr.sin_port = htons(smtp_port);

            if((connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
                perror("Unable to connect to server\n");
                exit(0);
            }
            
            // expected: 220
            if(!receive_smtp_status(sockfd, 220)) continue;

            // send HELO 
            strcpy(buf, "HELO ");
            strcat(buf, server_ip);
            send_message(sockfd, buf);

            // expected: 250 
            if(!receive_smtp_status(sockfd, 250)) continue;

            char *lines[MAX_LINES+3]; // 3 extra for From, To, Subject
            int ret = get_mail_from_user(lines);
            if(ret == 0) {
                send_message(sockfd, "QUIT");
                receive_smtp_status(sockfd, 221);
                close(sockfd);
                continue;
            }

            // send MAIL FROM
            strcpy(buf, "MAIL FROM:<");
            strcat(buf, lines[0]);
            strcat(buf, ">");
            send_message(sockfd, buf);

            // expected: 250 
            if(!receive_smtp_status(sockfd, 250)) continue;


            // send RCPT TO
            strcpy(buf, "RCPT TO:<");
            strcat(buf, lines[1]);
            strcat(buf, ">");
            send_message(sockfd, buf);

            // expected: 250 
            if(!receive_smtp_status(sockfd, 250)) continue;

            // send DATA
            send_message(sockfd, "DATA");

            // expected: 354
            if(!receive_smtp_status(sockfd, 354)) continue;

            // send mail
            int i=0; 
            while(lines[i] != NULL) {
                if(i==0)
                    strcpy(buf, "From: ");
                else if(i==1)
                    strcpy(buf, "To: ");
                else if(i==2)
                    strcpy(buf, "Subject: ");
                else 
                    strcpy(buf, "");
                send_message(sockfd, strcat(buf, lines[i]));
                i++;
            }

            // send .CRLF
            send_message(sockfd, ".");

            // expected: 250
            if(!receive_smtp_status(sockfd, 250)) continue;

            // send QUIT
            send_message(sockfd, "QUIT");

            // expected: 221
            if(!receive_smtp_status(sockfd, 221)) continue; 

            close(sockfd);
        }

        else if (choice==3) {
            // quit
            break;
        }

        else {
            printf("Invalid choice\n");
        }
    }

    return 0;
}

void strip(char* s) {
    char* start = s;
    char* end = s + strlen(s);

    while (isspace((unsigned char)*start)) start++;

    if (end > start) {
        while (isspace((unsigned char)*(end - 1))) end--;
    }

    memmove(s, start, end - start);

    // Null-terminate the string
    s[end - start] = '\0';
}

void append_CRLF(char *line) {
    for(int i=0; i<strlen(line); i++) {
        if(line[i] == '\n') {
            line[i] = '\r';
            line[i+1] = '\n';
            line[i+2] = '\0';
            break;
        }
    }
}

int get_choice() {
    char choice_str[10];
    printf("\nMail Client\n");
    printf("-----------\n");
    printf("1. Manage Mail\n");
    printf("2. Send Mail\n");
    printf("3. Quit\n");
    printf("Enter your choice: ");
    fgets(choice_str, 10, stdin);
    return atoi(choice_str);
}

// gets mail and also check if format is correct
int get_mail_from_user(char* lines[MAX_LINES+3]) {

    for (int i=0; i<MAX_LINES+3; i++) {
        lines[i] = NULL;
    }
    char line[MAX_LINE_LEN+1];
    int n = 0;
    fputs("Enter mail:\n", stdout);

    while(1) {
        fgets(line, MAX_LINE_LEN, stdin);
        // strip extra spaces in between and newline at the end
        strip(line);

        if(n==0 || n==1 || n==2) {
            char *token = strtok(line, " ");
            if ( (n==0 && strcmp(token, "From:") != 0) || 
                 (n==1 && strcmp(token, "To:") != 0) || 
                 (n==2 && strcmp(token, "Subject:") != 0) ) {
                printf("Incorrect format\n");
                return 0;
            }
            char* tok = strtok(NULL, "\n");

            if(n==0 || n==1) {
                // check if email is valid (should have exactly one @)
                int at_count = 0;
                for(int i=0; i<strlen(tok); i++) {
                    if(tok[i] == '@') {
                        at_count++;
                    }
                }
                if(at_count != 1) {
                    printf("Incorrect format\n");
                    return 0;
                }
            }

            lines[n] = malloc(strlen(tok)+1);
            strcpy(lines[n], tok);
            n++;
        }

        else {
            if(strcmp(line, ".") == 0) {
                break;
            }
            lines[n] = malloc(strlen(line)+1);
            strcpy(lines[n], line);
            n++;
        }
    }
    return 1;
}

void send_message(int sockfd, char *msg) {
    char buf[MAX_LINE_LEN+2];
    bzero(buf, MAX_LINE_LEN+2);
    strcpy(buf, msg);
    strcat(buf, CRLF);
    send(sockfd, buf, strlen(buf), 0);
}

void receive_message(int sockfd, char *msg) {
    char buf[100];
    bzero(buf, 100);
    bzero(msg, 1000);
    
    // receive until CRLF is found
    while(1) {
        recv(sockfd, buf, 100, 0);
        strcat(msg, buf);
        if(buf[strlen(buf)-2] == '\r' && buf[strlen(buf)-1] == '\n') {
            break;
        }
    }
}

void get_mail_from_server(int sockfd, char mail[(MAX_LINES+1)*(MAX_LINE_LEN+1)+1]){
    bzero(mail, (MAX_LINES+1)*(MAX_LINE_LEN+1)+1);
    char buf[101];
    char temp[100];
    bzero(buf, 101);

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
                    
                    if(strcmp(temp,".\r\n")==0){
                        temp_index=0;
                        bzero(temp,100);
                        done=1;
                        break;
                    }

                    strcat(mail, temp);
                    temp_index=0;
                    bzero(temp,100);
                }
            }
            else if(temp_index!=0 && (buf[buf_index]=='\n') && (temp[temp_index-1]=='\r')){
                    temp[temp_index++]=buf[buf_index++];
                    temp[temp_index++]='\0';
                    
                    if(strcmp(temp,".\r\n")==0){
                        temp_index=0;
                        bzero(temp,100);
                        done=1;
                        break;
                    }

                    strcat(mail, temp);
                    temp_index=0;
                    bzero(temp,100);
            }
            else{
                temp[temp_index++]=buf[buf_index++];
            }
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
        sprintf(buf, "RETR %d", i);
        send_message(sockfd, buf);

        // expected: +OK
        if(!receive_pop3_status(sockfd, "+OK")) continue;

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
                            }
                            else if(line==3){
                                // subject comes after "Subject: "
                                strcpy(subject, temp+9);
                            }
                            else if(line==4){
                                // time comes after "Received: "
                                strcpy(time, temp+10);
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
                            }
                            else if(line==3){
                                // subject comes after "Subject: "
                                strcpy(subject, temp+9);
                            }
                            else if(line==4){
                                // time comes after "Received: "
                                strcpy(time, temp+10);
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
        mails[i-1] = malloc(strlen(mail_num)+strlen(sender)+strlen(time)+strlen(subject)+8);
        strcpy(mails[i-1], mail_num);
        strcat(mails[i-1], " <");
        strcat(mails[i-1], sender);
        strcat(mails[i-1], "> ");
        strcat(mails[i-1], time);
        strcat(mails[i-1], " <");
        strcat(mails[i-1], subject);
        strcat(mails[i-1], ">");
        }
    }

int receive_smtp_status(int sockfd, int expected) {
    char full_msg[1000];
    receive_message(sockfd, full_msg);
    char num_str[4];
    for(int i=0; i<3; i++) {
        num_str[i] = full_msg[i];
    }
    num_str[3] = '\0';
    int status = atoi(num_str);
    if(status != expected) {
        if(status==550) printf("Error in sending mail: %s\n", full_msg);
        send_message(sockfd, "QUIT\r\n");
        return 0;
    }
    return 1;
}

int receive_pop3_status(int sockfd, char* expected) {
    char full_msg[1000];
    receive_message(sockfd, full_msg);
    if(strncmp(full_msg, expected, strlen(expected)) != 0) {
        printf("Error in managing mail: %s\n", full_msg);
        send_message(sockfd, "QUIT\r\n");
        return 0;
    }
    return 1;
}