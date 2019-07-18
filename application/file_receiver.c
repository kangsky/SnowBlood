#include "netdb.h"
#include "netinet/in.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "sys/ioctl.h"
#include "assert.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "memory.h"

#define HEAP_SIZE               1024*1024 // 1 MB

#define TCP_SERVER_PORT 		8080
#define CHATTING_BUF_MAX 		100
#define TCP_PKT_MTU             65535
#define OUTPUT_FILE_PATH         "/Users/local/Personal/Repos/SnowBlood/dst/test_out.bin"

extern int errno;

static void live_chatting(int sockfd);

int main(int argc, char *argv[]) {
	int sockfd, connfd, res;
	unsigned int len;
	struct sockaddr_in server_addr, client_addr;
    FILE *fp;
    SB_STATUS result;
    uint8_t *tcp_pkt_buf;
    long rx_recv_bytes;
    uint32_t file_write_size;
    long int socket_recv_size;

    // init snowblood memory heap 
    result = sb_init_memory(HEAP_SIZE, SB_MEM_CONFIG_MODE_1);
    assert (result == SB_OK);

	// Socket creation
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(sockfd != -1);

	memset(&server_addr, 0, sizeof(server_addr)); 
	memset(&client_addr, 0, sizeof(client_addr)); 

	// Assign IP, PORT and binding socket 
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(TCP_SERVER_PORT);

	res = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	assert(res == 0);

	// Put TCP Server into listen mode
	res = listen(sockfd, 5);
	assert(res == 0);

	printf("TCP Server is setup for listenning, waiting for client connection request...\n");
		
	// Accept the data packet from client
	len = sizeof(client_addr);
	connfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
	assert(connfd >= 0);

	printf("TCP client connection is accepted\n");
	
	// Recv file from client
    tcp_pkt_buf = (uint8_t *)sb_malloc(TCP_PKT_MTU);
    assert (tcp_pkt_buf);
	
    fp = fopen(OUTPUT_FILE_PATH, "wb");
    assert (fp);	
	rx_recv_bytes = 0;
	
	while (1) {
		memset(tcp_pkt_buf, 0, TCP_PKT_MTU);
		socket_recv_size = read(connfd, tcp_pkt_buf, TCP_PKT_MTU);
        if ( socket_recv_size <= 0 ) {
            printf("Read loop is ended, errno = %s \n ", strerror(errno));
        	break;
		} else {
            //printf("Read %ld bytes from client \n ", socket_recv_size);
		}
		file_write_size = fwrite(tcp_pkt_buf, 1, socket_recv_size, fp);
		assert (file_write_size == socket_recv_size);
		rx_recv_bytes += file_write_size;
	}
	printf("File transfer result : total received bytes = %ld \n", rx_recv_bytes);

	// Close socket
	close(sockfd);
	fclose(fp);
	sb_free(tcp_pkt_buf);
	return 0;
}


static void live_chatting(int sockfd) {
	char buf[CHATTING_BUF_MAX];	
	uint16_t n;
	int error;

	printf("Live chatting started!\n");

	while(1) {
		memset(buf, 0, CHATTING_BUF_MAX);
		
		// Receive message from client
		error = read(sockfd, buf, CHATTING_BUF_MAX);
 		if ( error <= 0 ) {
 			printf("Read failed error = %d, errno = %s \n ", error, strerror(errno));                    
		}
		printf("From client : %s To Client : ", buf);

		memset(buf, 0, CHATTING_BUF_MAX);
		n = 0;
		while ( (buf[n++] = getchar()) != '\n');		

		// Send message to client
		write(sockfd, buf, n);
	
		// If input string is "exit", end the chatting
		if (!strncmp("exit", buf, 4)) {
			printf("Server exit...\n");
			break;
		}
	}

}

