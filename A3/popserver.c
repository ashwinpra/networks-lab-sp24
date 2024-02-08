#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

int authenticate(int client_socket,char * username){

    while (1) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        recv(client_socket, buffer, sizeof(buffer), 0);

        // Check if the client issued the QUIT command
        if (strcmp(buffer, "QUIT\r\n") == 0) {
            // Respond with goodbye message and exit loop
            char quit_message[] = "+OK Goodbye\r\n";
            send(client_socket, quit_message, strlen(quit_message), 0);
            
            return 0;
        } else if (strncmp(buffer, "USER ",5) == 0) {
            //extract username from buffer string: USER username
            bzero(username, 100);
            int i = 5,j=0;
            while(buffer[i] != '\r') {
                if(buffer[i]!=' ' && buffer[i]!='\n') username[j++] = buffer[i];
                i++;
            }

            if(j==0) {
                char err_message[] = "-ERR Username cannot be empty\r\n";
                send(client_socket, err_message, strlen(err_message), 0);
                continue;
            } 

            //check if username exists in user.txt

            FILE *fp = fopen("user.txt", "r");
            if(fp == NULL) {
                char err_message[] = "-ERR User file not found\r\n";
                send(client_socket, err_message, strlen(err_message), 0);
                continue;
            }

            char line[100];
            int user_exists = 0;
            char *token;
            while(fgets(line, 100, fp) != NULL) {
                token = strtok(line, " ");
                if(strcmp(token, username) == 0) {
                    user_exists = 1;
                    break;
                }
            }

            if(!user_exists){
                char err_message[] = "-ERR User does not exist\r\n";
                send(client_socket, err_message, strlen(err_message), 0);
                return 0;
            }

            char success_message[] = "+OK User exists\r\n";
            send(client_socket, success_message, strlen(success_message), 0);

            //GET PASSWORD
            memset(buffer, 0, sizeof(buffer));
            recv(client_socket, buffer, sizeof(buffer), 0);

            if (strncmp(buffer, "PASS ",5) == 0) {
                //extract password from buffer string: PASS password
                char password[100];
                bzero(password, 100);
                i = 5,j=0;
                while(buffer[i] != '\r') {
                    if(buffer[i]!=' ' && buffer[i]!='\n') password[j++] = buffer[i];
                    i++;
                }

                if(j==0) {
                    char err_message[] = "-ERR Password cannot be empty\r\n";
                    send(client_socket, err_message, strlen(err_message), 0);
                    continue;
                } 

                //check if password is correct

                token = strtok(NULL, " ");
                for(j=0;j<strlen(token);j++) {
                    if(token[j] == '\n' || token[j]=='\r') {token[j] = '\0';break;}
                }
                if(strcmp(token, password) == 0) {
                    char success_message[] = "+OK Authentication successful\r\n";
                    send(client_socket, success_message, strlen(success_message), 0);
                    return 1;
                } else {
                    char err_message[] = "-ERR Authentication failed\r\n";
                    send(client_socket, err_message, strlen(err_message), 0);
                    return 0;
                }

            } else {
                // Respond with error message for unrecognized commands during authentication
                char err_message[] = "-ERR Command not recognized during authentication\r\n";
                send(client_socket, err_message, strlen(err_message), 0);
            }
            
            
        } else if (strcmp(buffer, "PASS\r\n") == 0) {
            //send username first
            char err_message[] = "-ERR Username not provided\r\n";
            send(client_socket, err_message, strlen(err_message), 0);
        } else {
            // Respond with error message for unrecognized commands during authentication
            char err_message[] = "-ERR Command not recognized during authentication\r\n";
            send(client_socket, err_message, strlen(err_message), 0);
        }
    }

}

// Function to handle client requests
void handle_client(int client_socket) {
    char buffer[1024];

    // Send welcome message
    char welcome_message[] = "+OK POP3 server ready\r\n";
    send(client_socket, welcome_message, strlen(welcome_message), 0);


    char username[100];
    authenticate(client_socket, username);


    char path[100];
    strcpy(path,"./");
    strcat(path,username);
    strcat(path,"/mymailbox");
    FILE *fp = fopen(path, "r");

    if(fp == NULL) {
        char err_message[] = "-ERR Mailbox not found\r\n";
        send(client_socket, err_message, strlen(err_message), 0);
        close(client_socket);
        return;
    }

    //make a list of messages present in the mailbox
    int n = 0,total_size = 0;
    char ** messages = NULL;
        
    char line[100],msg[5000];
    while(fgets(line,sizeof(line),fp)){
        if(strcmp(line,".\r\n") == 0) {
            messages=realloc(messages,(n+1)*sizeof(char *));
            messages[n] = (char *)malloc(strlen(msg)+1);
            strcpy(messages[n],msg);
            total_size+=strlen(msg);
            n++;
            bzero(msg,5000);
        } else {
            strcat(msg,line);
        }
    }

    int number_of_deleted=0,deleted_size=0,deleted[n];
    memset(deleted,0,sizeof(deleted));
    int quit_executed=0;

    // TRANSACTION loop

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        recv(client_socket, buffer, sizeof(buffer), 0);

        // Check if the client issued the QUIT command
        if (strcmp(buffer, "QUIT\r\n") == 0) {
            printf("QUIT command received\n");
            // Respond with goodbye message and exit loop
            char quit_message[] = "+OK Goodbye\r\n";
            send(client_socket, quit_message, strlen(quit_message), 0);
            quit_executed=1;

            if(number_of_deleted){
                FILE *fp = fopen(path, "w");
                for(int i=0;i<n;i++) {
                    if(!deleted[i]) {
                        fprintf(fp,"%s",messages[i]);
                        fprintf(fp,".\r\n");
                    }
                }
                fclose(fp);
            }

            for(int i=0;i<n;i++) {
                free(messages[i]);
            }
            free(messages);
            break;
        } else if (strcmp(buffer, "STAT\r\n") == 0) {
            char stat_response[100];
            sprintf(stat_response,"+OK %d %d\r\n",n-number_of_deleted,total_size-deleted_size);
            send(client_socket, stat_response, strlen(stat_response), 0);
        } else if (strncmp(buffer,"LIST ",5)==0) {

            int index=5;
            while(index<strlen(buffer) && buffer[index]==' ') index++;

            int msg_number = atoi(buffer+index);
            if(msg_number==0){
                if(buffer[index]!='0'){
                    char list_response[100];
                    sprintf(list_response,"+OK %d messages (%d octets)\r\n",n-number_of_deleted,total_size-deleted_size);
                    send(client_socket, list_response, strlen(list_response), 0);
                    for(int i=0;i<n;i++) {
                        if(!deleted[i]) {
                            char list_response[100];
                            sprintf(list_response,"%d %d\r\n",i+1,strlen(messages[i]));
                            send(client_socket, list_response, strlen(list_response), 0);
                        }
                    }
                    send(client_socket, ".\r\n", 3, 0);

                }else{
                    char err_message[] = "-ERR No such message\r\n";
                    send(client_socket, err_message, strlen(err_message), 0);

                }


            }else if(msg_number>n){
                char err_message[] = "-ERR No such message\r\n";
                send(client_socket, err_message, strlen(err_message), 0);

            }else{
                if(deleted[msg_number-1]){
                    char err_message[40]; 
                    sprintf(err_message,"-ERR Message %d already deleted\r\n",msg_number);
                    send(client_socket, err_message, strlen(err_message), 0);
                }else{
                    char list_response[100];
                    sprintf(list_response,"+OK %d %d\r\n",msg_number,strlen(messages[msg_number-1]));
                    send(client_socket, list_response, strlen(list_response), 0);
                }
            }
        } else if (strncmp(buffer, "RETR ", 5) == 0) {
            int msg_number = atoi(buffer+5);
            msg_number--;
            if(msg_number>=n || msg_number<0) {
                char err_message[] = "-ERR No such message\r\n";
                send(client_socket, err_message, strlen(err_message), 0);
            }
            else if(deleted[msg_number]) {
                char err_message[40]; 
                sprintf(err_message,"-ERR Message %d already deleted\r\n",msg_number+1);
                send(client_socket, err_message, strlen(err_message), 0);
            }else {
                char retr_response[40]; 
                sprintf(retr_response,"+OK %d octets\r\n",strlen(messages[msg_number]));
                send(client_socket, retr_response, strlen(retr_response), 0);
                send(client_socket, messages[msg_number], strlen(messages[msg_number]), 0);
                send(client_socket, ".\r\n", 3, 0);
            }
        } else if (strncmp(buffer, "DELE ", 5) == 0) {
            
            int msg_number = atoi(buffer+5);
            msg_number--;
            if(msg_number>=n || msg_number<0) {
                char err_message[] = "-ERR No such message\r\n";
                send(client_socket, err_message, strlen(err_message), 0);
            }
            else if(deleted[msg_number]) {
                char err_message[40]; 
                sprintf(err_message,"-ERR Message %d already deleted\r\n",msg_number+1);
                send(client_socket, err_message, strlen(err_message), 0);
            }else {
                deleted[msg_number]=1;
                number_of_deleted++;
                deleted_size+=strlen(messages[msg_number]);
                char dele_response[40]; 
                sprintf(dele_response,"+OK Message %d deleted\r\n",msg_number+1);
                send(client_socket, dele_response, strlen(dele_response), 0);
            } 
        } else if(strcmp(buffer,"RSET\r\n")){
            for(int i=0;i<n;i++) {
                deleted[i]=0;
            }
            number_of_deleted=0;
            deleted_size=0;
            char rset_response[] = "+OK Reset done\r\n";
            send(client_socket, rset_response, strlen(rset_response), 0);
        } else {
            // Respond with error message for unrecognized commands
            char err_message[] = "-ERR Command not recognized\r\n";
            send(client_socket, err_message, strlen(err_message), 0);
        } 
    }

    // Close the connection
    close(client_socket);
}

int main(int argc, char *argv[]) {


    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(0);
    }
    int pop3_port = atoi(argv[1]);


    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(pop3_port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    printf("POP3 server listening on port %d\n", pop3_port);

    while (1) {
        // Accept incoming connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket < 0) {
            perror("Acceptance failed");
            exit(EXIT_FAILURE);
        }

        // Handle client request
        handle_client(client_socket);
    }


    return 0;
}
