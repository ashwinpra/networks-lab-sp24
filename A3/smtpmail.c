#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include<sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>

#define CRLF "\r\n"

int receive_command(int sockfd, char* expected_command, char* buf) {
    bzero(buf, 100);
    recv(sockfd, buf, 100, 0);
    if (strncmp(buf, expected_command, strlen(expected_command)) == 0) {
        return 1;
    }
    else if(strncmp(buf,"QUIT",4)==0){
        printf("QUIT received\n\n");
        bzero(buf, 100);
        strcpy(buf, "221 <iitkgp.edu> closing connection");
        strcat(buf, CRLF);
        send(sockfd, buf, strlen(buf), 0);
        exit(0);
    }
    return 0;
}
int main(int argc, char *argv[])
{
	int sockfd, newsockfd ; 
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;

	char buf[101];		/* We will use this buffer for communication */

    if(argc!=2){
        printf("Usage: ./smtpmail <port>\n");
        exit(0);
    }

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Cannot create socket\n");
		exit(0);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); 
			
	while (1) {

		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen) ;

		if (newsockfd < 0) {
			printf("Accept error\n");
			exit(0);
		}

		if (fork() == 0) {

			close(sockfd);	

			int client_port;
			char client_ip[INET_ADDRSTRLEN];

			if (getpeername(newsockfd, (struct sockaddr*)&cli_addr, &clilen) == 0) {
			
				inet_ntop(AF_INET, &(cli_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
				client_port = ntohs(cli_addr.sin_port);
				printf("Client IP: %s\n", client_ip);
				printf("Client Port: %d\n", client_port);

			} else {
				perror("getpeername");
			}

			//send 220 <iitkgp.edu> Service ready to the client
            bzero(buf, 100);
			strcpy(buf, "220 <iitkgp.edu> Service ready\r\n");
			send(newsockfd, buf, strlen(buf), 0);

            //receive HELO from client
            if(receive_command(newsockfd, "HELO", buf)) {
                printf("%s\n",buf);
                char temp[20];
                bzero(temp,20);
                for(int i=5; i<strlen(buf); i++){
                    if(buf[i]!=' ')
                    temp[i-5]=buf[i];
                }

                bzero(buf, 100);
                strcpy(buf, "250 OK Hello ");
                strcat(buf, temp);
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }
            else{
                printf("Error: HELO not received\n");
                exit(0);
            }

            //receive MAIL FROM from client
            if(receive_command(newsockfd, "MAIL FROM", buf)){
                printf("%s\n",buf);
                
                char temp[20];
                bzero(temp,20);
                int fl=0,j=0;
                //get domain from < > 
                for(int i=10; i<strlen(buf); i++){
                    if(buf[i]=='<'){
                        fl=1;continue;
                    }
                    if(buf[i]=='>'){break;}
                    if(fl==1)
                        temp[j++]=buf[i];
                }

                bzero(buf, 100);
                strcpy(buf, "250 ");
                strcat(buf, temp);
                strcat(buf, "... Sender ok");
                strcat(buf, CRLF);
                printf("%s\n",buf);
                send(newsockfd, buf, strlen(buf), 0);
            }
            else{
                printf("Error: MAIL FROM not received\n");
                exit(0);
            }
			
            char username[20];
            char temp[100];
            int fd;
            char path[100],line1[100];
            bzero(path,100);
            bzero(username,20);

            //receive RCPT TO from client
            if(receive_command(newsockfd, "RCPT TO", buf)){
                printf("%s\n",buf);

                int fl=0,j=0;
                //get domain from < > 
                for(int i=7; i<strlen(buf); i++){
                    if(buf[i]=='<'){
                        fl=1;continue;
                    }
                    if(buf[i]=='@'){break;}
                    if(fl==1)
                        username[j++]=buf[i];
                }

                strcpy(path,"./");
                strcat(path,username);
                strcat(path,"/mymailbox");
                fd=open(path,O_WRONLY|O_APPEND);

                bzero(buf, 100);
                if(fd==-1){
                    printf("User not found\n");
                    strcpy(buf, "550 No such user");
                    strcat(buf, CRLF);
                    send(newsockfd, buf, strlen(buf), 0);
                    if(receive_command(newsockfd, "QUIT", buf)){
                        printf("QUIT received\n");
                        bzero(buf, 100);
                        strcpy(buf, "221 <iitkgp.edu> closing connection");
                        strcat(buf, CRLF);
                        send(newsockfd, buf, strlen(buf), 0);
                    }
                    close(newsockfd);
                    exit(0);
                }
              
                strcpy(buf, "250 root... Recipient ok ");
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }
            else{
                printf("Error: RCPT TO not received\n");
                exit(0);
            }

            //receive DATA from client
            if(receive_command(newsockfd, "DATA", buf)){
                printf("DATA received\n\n");
                bzero(buf, 100);
                strcpy(buf, "354 Enter mail, end with \".\" on a line by itself");
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }
            else{
                printf("Error: DATA not received\n");
                exit(0);
            }

            //receive mail from client
            int line=0;
            bzero(temp,100);
            int temp_index=0,buf_index=0,done=0;
            while(!done){
                bzero(buf, 100);
               
                int n = recv(newsockfd, buf, 100, 0);
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
                                strcpy(line1,temp);
                                temp_index=0;
                                bzero(temp,100);
                            }
                            else if(line==2){
                                for(int i=4; i<strlen(temp); i++){
                                    if(temp[i]=='@'){break;}
                                    if(temp[i]!=' ')
                                    username[i-4]=temp[i];
                                }
                                write(fd,line1,strlen(line1));
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                            }
                            else if(line==3){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                                time_t t = time(NULL);
                                struct tm tm = *localtime(&t);
                                char time[100];
                                bzero(time,100);
                                sprintf(time,"Received: <%d-%d-%d : %d:%d>\r\n",tm.tm_mday,tm.tm_mon + 1,tm.tm_year + 1900,tm.tm_hour,tm.tm_min);
                                write(fd,time,strlen(time));

                            
                            }
                            else if(strcmp(temp,".\r\n")==0){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);
                                printf("Mail received\n\n");
                                bzero(buf, 100); 
                                strcpy(buf, "250 OK Message accepted for delivery");
                                strcat(buf, CRLF);
                                send(newsockfd, buf, strlen(buf), 0);
                                done=1;
                                break;
                            }
                            else{
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);
                            }
                        }
                    }
                    else if(temp_index!=0 && (buf[buf_index]=='\n') && (temp[temp_index-1]=='\r')){
                            temp[temp_index++]=buf[buf_index++];
                            temp[temp_index++]='\0';
                            line++;
                            
                            if(line==1){
                                strcpy(line1,temp);
                                temp_index=0;
                                bzero(temp,100);
                            }
                            else if(line==2){
                                //get username from "TO: username@domain"
                                for(int i=4; i<strlen(temp); i++){
                                    if(temp[i]=='@'){break;}
                                    if(temp[i]!=' ')
                                    username[i-4]=temp[i];
                                }

                                write(fd,line1,strlen(line1));
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                            }
                            else if(line==3){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                                time_t t = time(NULL);
                                struct tm tm = *localtime(&t);
                                char time[100];
                                bzero(time,100);
                                sprintf(time,"Received: <%d-%d-%d : %d:%d>\r\n",tm.tm_mday,tm.tm_mon + 1,tm.tm_year + 1900,tm.tm_hour,tm.tm_min);
                                write(fd,time,strlen(time));

                            }
                            else if(strcmp(temp,".\r\n")==0){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);


                                printf("Mail received\n");
                                bzero(buf, 100); 
                                strcpy(buf, "250 OK Message accepted for delivery");
                                strcat(buf, CRLF);
                                send(newsockfd, buf, strlen(buf), 0);
                                done=1;
                                break;
                            }
                            else{
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);
                            }
                    }
                    else{
                        temp[temp_index++]=buf[buf_index++];
                    }
                }
            }

            //receive QUIT from client
            if(receive_command(newsockfd, "QUIT", buf)){
                printf("QUIT received\n\n");
                bzero(buf, 100);
                strcpy(buf, "221 <iitkgp.edu> closing connection");
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }
            else{
                printf("Error: QUIT not received\n");
                exit(0);
            }

            close(fd);
			close(newsockfd);
			exit(0);
		}

		close(newsockfd);
	}
	return 0;
}
