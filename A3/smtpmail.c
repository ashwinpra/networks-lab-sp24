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


int main(int argc, char *argv[])
{
	int			sockfd, newsockfd ; /* Socket descriptors */
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	char buf[101];		/* We will use this buffer for communication */

    if(argc!=2){
        printf("Usage: ./smtpmail <port>\n");
        exit(0);
    }

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Cannot create socket\n");
		exit(0);
	}

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(atoi(argv[1]));

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); 
			
	while (1) {


		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen) ;

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
			strcpy(buf, "220 <iitkgp.edu> Service ready\n");
			send(newsockfd, buf, strlen(buf), 0);

			//recieve HELO from client
			recv(newsockfd, buf, 100, 0);

            if(strncmp(buf,"HELO",4)==0){
                printf("HELO recieved\n");
                printf("%s\n",buf);
                
                char temp[20];
                bzero(temp,20);
                int i=5;
                for(i;i<strlen(buf);i++){
                    if(buf[i]!=' ')
                    temp[i-5]=buf[i];
                }

                bzero(buf, 100);
                strcpy(buf, "250 OK Hello ");
                strcat(buf, temp);
                printf("temp : %s\n",temp);
                printf("helo : test %s\n",buf);
                send(newsockfd, buf, strlen(buf), 0);
            }else{
                printf("Error: HELO not recieved\n");
                exit(0);
            }


            //recieve MAIL FROM from client
            recv(newsockfd, buf, 100, 0);

            if(strncmp(buf,"MAIL FROM",9)==0){
                printf("MAIL FROM recieved\n");
                printf("%s\n",buf);
                
                char temp[20];
                bzero(temp,20);
                int i=10,fl=0;
                //get domain from < > 
                for(i;i<strlen(buf);i++){
                    if(buf[i]=='<'){
                        fl=1;continue;
                    }
                    if(buf[i]=='>'){break;}
                    if(fl==1)
                        temp[i-10]=buf[i];
                }

                bzero(buf, 100);
                strcpy(buf, "250 ");
                strcat(buf, temp);
                strcat(buf, "... Sender ok");
                strcat(buf, CRLF);
                printf("mail from : test %s\n",buf);
                send(newsockfd, buf, strlen(buf), 0);
            }else{
                printf("Error: MAIL FROM not recieved\n");
                exit(0);
            }
			

            //recieve RCPT TO from client
            recv(newsockfd, buf, 100, 0);

            if(strncmp(buf,"RCPT TO",7)==0){
                printf("RCPT TO recieved\n");
                printf("%s\n",buf);
              
                bzero(buf, 100);
                strcpy(buf, "250 root... Recipient ok ");
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }else{
                printf("Error: RCPT TO not recieved\n");
                exit(0);
            }

            //recieve DATA from client
            recv(newsockfd, buf, 100, 0);

            if(strncmp(buf,"DATA",4)==0){
                printf("DATA recieved\n");
                
              
                bzero(buf, 100);
                strcpy(buf, "354 Enter mail, end with \".\" on a line by itself");
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }else{
                printf("Error: DATA not recieved\n");
                exit(0);
            }

            //recieve mail from client
            char username[20];
            char temp[100];

            bzero(username,20);
            int line=0;

            char path[100],line1[100];
                bzero(path,100);
                int fd;
                
            bzero(temp,100);
            int temp_index=0,buf_index=0,done=0;
            while(!done){
                bzero(buf, 100);
               
                int n = recv(newsockfd, buf, 100, 0);
                // printf("%s\n%d\n%d\n",buf,strlen(buf), n);
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
                            // printf("line %d\n",line);
                            // printf("temp:%s",temp);
                            
                            if(line==1){
                                strcpy(line1,temp);
                                temp_index=0;
                                bzero(temp,100);
                            }else if(line==2){
                                //get username from "TO: username@domain"
                                int i=4;
                                for(i;i<strlen(temp);i++){
                                    if(temp[i]=='@'){break;}
                                    if(temp[i]!=' ')
                                    username[i-4]=temp[i];
                                }

                                strcpy(path,"./");
                                strcat(path,username);
                                strcat(path,"/mymailbox");
                                fd=open(path,O_WRONLY|O_APPEND);
                                write(fd,line1,strlen(line1));
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                            }else if(line==3){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                                time_t t = time(NULL);
                                struct tm tm = *localtime(&t);
                                char time[100];
                                bzero(time,100);
                                sprintf(time,"Received: <%d-%d-%d : %d:%d>\r\n",tm.tm_mday,tm.tm_mon + 1,tm.tm_year + 1900,tm.tm_hour,tm.tm_min);
                                write(fd,time,strlen(time));

                            }else if(strcmp(temp,".\r\n")==0){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);


                                printf("Mail recieved\n");
                                bzero(buf, 100); 
                                strcpy(buf, "250 OK Message accepted for delivery");
                                strcat(buf, CRLF);
                                send(newsockfd, buf, strlen(buf), 0);
                                done=1;
                                break;
                            }else{
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);
                            }
                        }
                    }else if(temp_index!=0 && (buf[buf_index]=='\n') && (temp[temp_index-1]=='\r')){
                            temp[temp_index++]=buf[buf_index++];
                            temp[temp_index++]='\0';
                            line++;
                            
                            if(line==1){
                                strcpy(line1,temp);
                                temp_index=0;
                                bzero(temp,100);
                            }else if(line==2){
                                //get username from "TO: username@domain"
                                int i=4;
                                for(i;i<strlen(temp);i++){
                                    if(temp[i]=='@'){break;}
                                    if(temp[i]!=' ')
                                    username[i-4]=temp[i];
                                }

                                strcpy(path,"./");
                                strcat(path,username);
                                strcat(path,"/mymailbox");
                                fd=open(path,O_WRONLY|O_APPEND);
                                write(fd,line1,strlen(line1));
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                            }else if(line==3){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);

                                time_t t = time(NULL);
                                struct tm tm = *localtime(&t);
                                char time[100];
                                bzero(time,100);
                                sprintf(time,"Received: <%d-%d-%d : %d:%d>\r\n",tm.tm_mday,tm.tm_mon + 1,tm.tm_year + 1900,tm.tm_hour,tm.tm_min);
                                write(fd,time,strlen(time));

                            }else if(strcmp(temp,".\r\n")==0){
                                write(fd,temp,strlen(temp));
                                temp_index=0;
                                bzero(temp,100);


                                printf("Mail recieved\n");
                                bzero(buf, 100); 
                                strcpy(buf, "250 OK Message accepted for delivery");
                                strcat(buf, CRLF);
                                send(newsockfd, buf, strlen(buf), 0);
                                done=1;
                                break;
                            }else{
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

            //recieve QUIT from client
            recv(newsockfd, buf, 100, 0);

            if(strncmp(buf,"QUIT",4)==0){
                printf("QUIT recieved\n");
              
                bzero(buf, 100);
                strcpy(buf, "221 <iitkgp.edu> closing connection");
                strcat(buf, CRLF);
                send(newsockfd, buf, strlen(buf), 0);
            }else{
                printf("Error: QUIT not recieved\n");
                exit(0);
            }

			close(newsockfd);
			exit(0);
		}

		close(newsockfd);
	}
	return 0;
}
