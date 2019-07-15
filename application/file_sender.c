#include "netdb.h"
#include "arpa/inet.h"
#include "stdio.h" 
#include "stdlib.h"
#include "string.h" 
#include "sys/socket.h"
#include "unistd.h"

#define CHATTING_BUF_MAX 80 
#define TCP_SERVER_PORT 8080 

static void live_chatting(int sockfd);
  
int main() 
{ 
    int sockfd, connfd; 
    struct sockaddr_in servaddr, cli; 
  
    // socket create and varification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Socket successfully created..\n"); 
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(TCP_SERVER_PORT); 
  
    // connect the client socket to server socket 
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        printf("connection with the server failed...\n"); 
        exit(0); 
    } 
    else
        printf("connected to the server..\n"); 
  
    // function for chat 
    live_chatting(sockfd); 
  
    // close the socket 
    close(sockfd);
	return 0; 
}


void live_chatting(int sockfd) 
{ 
    char buf[CHATTING_BUF_MAX]; 
    uint16_t n;
 
    while (1) { 
        memset(buf, 0, CHATTING_BUF_MAX); 
        printf("Enter the string : "); 
        n = 0; 
        while ((buf[n++] = getchar()) != '\n'); 
        printf("String typed: %s\n", buf); 
        write(sockfd, buf, sizeof(buf)); 
       	memset(buf, 0, CHATTING_BUF_MAX); 
        read(sockfd, buf, sizeof(buf)); 
        printf("From Server : %s", buf); 
        if ((strncmp(buf, "exit", 4)) == 0) { 
            printf("Client Exit...\n"); 
            break; 
        } 
    } 
} 
