#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include  <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LEN 80
#define MAX_LINES 50 
#define CRLF "\r\n"

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

void get_mail_from_user(char* lines[MAX_LINES+3]) {

    char line[MAX_LINE_LEN+1];
    int n = 0;
    fputs("Enter mail:\n", stdout);

    while(1) {
        fgets(line, MAX_LINE_LEN, stdin);
        // strip extra spaces in between and newline at the end
        strip(line);


        if(n==0 || n==1 || n==2) {
            char *token = strtok(line, " ");
            if (n==0) {
                if (strcmp(token, "From:") != 0) {
                    printf("Mail should have a sender\n");
                    break;
                }
            }
            else if (n==1) {
                if (strcmp(token, "To:") != 0) {
                    printf("Mail should have a recipient\n");
                    break;
                }
            }
            else if (n==2) {
                if (strcmp(token, "Subject:") != 0) {
                    printf("Mail should have a subject\n");
                    break;
                }
            }
            char* tok = strtok(NULL, "\n");
            lines[n] = malloc(strlen(tok)+1);
            strcpy(lines[n], tok);
            n++;
        }

        else {
            lines[n] = malloc(strlen(line)+1);
            strcpy(lines[n], line);
            n++;
            if(strcmp(line, ".") == 0) {
                break;
            }
            
        }
    }

    for(int i=0; i<n; i++) {
        printf("Line %d: %s\n", i, lines[i]);
    }
}


int main(int argc, char const *argv[])
{
    if (argc != 4){
        printf("Usage: %s <server_ip> <smtp_port> <pop3_port>\n", argv[0]);
        exit(0);
    }

    char *server_ip = argv[1];
    int smtp_port = atoi(argv[2]);
    int pop3_port = atoi(argv[3]);

    struct sockaddr_in server_addr;
    char buf[MAX_LINE_LEN+2];

    char username[100], password[100];
    printf("Enter username: ");
    fgets(username, 100, stdin);
    printf("Enter password: ");
    fgets(password, 100, stdin);

    server_addr.sin_port = htons(smtp_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    while (1) {
        int choice = get_choice();

        if (choice==1) {
            // manage mail
        }
        else if (choice==2) {
            // send mail
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
            int bytes_read = recv(sockfd, buf, MAX_LINE_LEN, 0);
            printf("%s\n", buf);

            // send HELO 
            strcpy(buf, "HELO ");
            strcat(buf, server_ip);
            strcat(buf, CRLF);

            printf("sending      %s\n", buf);

            send(sockfd, buf, strlen(buf), 0);

            // expected: 250 
            bzero(buf, MAX_LINE_LEN);
            bytes_read = recv(sockfd, buf, MAX_LINE_LEN, 0);
            printf("%s\n", buf);

            char *lines[MAX_LINES+3]; // 3 extra for From, To, Subject
            get_mail_from_user(lines);

            // send MAIL FROM
            strcpy(buf, "MAIL FROM: <");
            strcat(buf, lines[0]);
            strcat(buf, ">");
            strcat(buf, CRLF);
            printf("sending  mail    %s\n", buf);
            printf("lines[0] %s\n", lines[0] );

            send(sockfd, buf, strlen(buf), 0);

            // expected: 250 
            bzero(buf, MAX_LINE_LEN);
            recv(sockfd, buf, MAX_LINE_LEN, 0);
            printf("%s\n", buf);

            // send RCPT TO
            strcpy(buf, "RCPT TO: <");
            strcat(buf, lines[1]);
            strcat(buf, ">");
            strcat(buf, CRLF);
            printf("sending  rcpt    %s\n", buf);
            send(sockfd, buf, strlen(buf), 0);

            // expected: 250 
            bzero(buf, MAX_LINE_LEN);
            recv(sockfd, buf, MAX_LINE_LEN, 0);
            printf("%s\n", buf);

            // send DATA
            strcpy(buf, "DATA");
            strcat(buf, CRLF);

            send(sockfd, buf, strlen(buf), 0);

            // expected: 354
            bzero(buf, MAX_LINE_LEN);
            recv(sockfd, buf, MAX_LINE_LEN, 0);
            printf("%s\n", buf);

            // send mail
            int i=0; 
            while(strcmp(lines[i], ".") != 0) {
                printf("i %d\n", strncmp(lines[i],"\0",1));
                printf("lines[i] %s\n", lines[i]);
                
                if(i==0)
                    strcpy(buf, "From: ");
                else if(i==1)
                    strcpy(buf, "To: ");
                else if(i==2)
                    strcpy(buf, "Subject: ");
                else 
                    strcpy(buf, "");

                strcat(buf, lines[i]);
                strcat(buf, CRLF);
                printf("sending  mail    %s\n", buf);
                send(sockfd, buf, strlen(buf), 0);
                i++;
            }

            // send .CRLF
            strcpy(buf, ".");
            strcat(buf, CRLF);
            printf("sending  mail    %s\n", buf);
            send(sockfd, buf, strlen(buf), 0);

            // expected: 250
            bzero(buf, MAX_LINE_LEN);
            recv(sockfd, buf, MAX_LINE_LEN, 0);

            // send QUIT
            strcpy(buf, "QUIT");
            strcat(buf, CRLF);

            send(sockfd, buf, strlen(buf), 0);

            // expected: 221
            recv(sockfd, buf, MAX_LINE_LEN, 0);

            close(sockfd);

            // todo: split into functions
            // todo: verify that status codes are correct
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
