#include "netdb.h"
#include "netinet/in.h"
#include "sys/socket.h"
#include "sys/types.h"
#include "sys/ioctl.h"
#include "sys/stat.h"
#include "unistd.h"
#include "assert.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "pthread.h"
#include "unistd.h"
#include "memory.h"
#include "file_common.h"

#define HEAP_SIZE               1024*1024 // 1 MB

#define TCP_SERVER_PORT 		8080
#define CHATTING_BUF_MAX 		100
#define TCP_PKT_MTU             65535
#define OUTPUT_FILE_PATH        "/tmp/test_out/"

#define MAX_FILE_PATH_LEN		1024
#define MAX_SERVER_THREAD       10

int server_thread_num = MAX_SERVER_THREAD;
pthread_t server_sock[MAX_SERVER_THREAD];
uint32_t tcp_port_list[MAX_SERVER_THREAD] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

extern int errno;

static void live_chatting(int sockfd);
static void *server_socket(void* argv);
static FILE *open_file(char *file_path);
static void recursive_create_dir(char *dir_path);
static FILE_TRANSFER_HEADER network_to_host_byte_order(FILE_TRANSFER_HEADER header_n);

int main(int argc, char *argv[]) {
    SB_STATUS result;
    uint8_t i;
    int ret;

    // init snowblood memory heap 
    result = sb_init_memory(HEAP_SIZE, SB_MEM_CONFIG_MODE_1);
    assert (result == SB_OK);

    // Create threads as server socket
    for (i = 0;i < server_thread_num;i++) {
        ret = pthread_create(server_sock+i, NULL, server_socket, (void *)(tcp_port_list+i));
        assert(ret == 0);
    }
	while(1);	
	return 0;
}

static void *server_socket(void* argv) {
	int sockfd, connfd, res;
	unsigned int len;
	struct sockaddr_in server_addr, client_addr;
    FILE *fp;
    uint8_t *tcp_pkt_buf;
    long rx_recv_bytes;
    uint32_t file_write_size;
    long int socket_recv_size;
	uint32_t tcp_port;
	uint16_t socket_server_id = *((uint32_t *)argv);
	char tmp_file_path[MAX_FILE_PATH_LEN];
	char output_file_path[MAX_FILE_PATH_LEN];
	FILE_TRANSFER_HEADER header_h, header_n;

	// Socket creation
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(sockfd != -1);

	memset(&server_addr, 0, sizeof(server_addr)); 
	memset(&client_addr, 0, sizeof(client_addr)); 

	// Assign IP, PORT and binding socket
	tcp_port = TCP_SERVER_PORT + socket_server_id;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(tcp_port);

	res = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	assert(res == 0);

	// Put TCP Server into listen mode
	res = listen(sockfd, 5);
	assert(res == 0);

	len = sizeof(client_addr);
		
	// Accept the data packet from client
	while (1) {
		printf("TCP Server is setup for listenning with port <%d>, waiting for client connection request...\n", tcp_port);
		connfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
		assert(connfd >= 0);

		printf("TCP client connection is accepted with port <%d>\n", tcp_port);

		tcp_pkt_buf = (uint8_t *)sb_malloc(TCP_PKT_MTU);
		assert (tcp_pkt_buf);

		while (1) {
		// 1.1 Header for FILE_TRANSFER_CMD_START
        socket_recv_size = read(connfd, (uint8_t *)(&header_n), FT_MSG_HD_LEN);
		if ( socket_recv_size == 0 ) {
			// socket is ended
			printf("Socket is ended, errno = %s \n ", strerror(errno));
 			break;
		} else if ( socket_recv_size != FT_MSG_HD_LEN ) {
            //printf("Read loop is ended, errno = %s \n ", strerror(errno));
			assert (0);
		} else {
           	printf("[DBBUG] Server socket <%d> : FILE_TRANSFER_CMD_START header read %ld bytes from client\n ", socket_server_id, socket_recv_size);
        }
		header_h = network_to_host_byte_order(header_n);
		
		assert (header_h.msg_type == FILE_TRANSFER_CMD_START);

		// 1.2 Payload for FILE_TRANSFER_CMD_START
		memset(tmp_file_path, 0, MAX_FILE_PATH_LEN);
		socket_recv_size = read(connfd, tmp_file_path, header_h.pld_len);
		if ( socket_recv_size != header_h.pld_len ) {
            //printf("Read loop is ended, errno = %s \n ", strerror(errno));
            assert (0);
        } else {
            printf("[DBBUG] FILE_TRANSFER_CMD_START paylod read %ld bytes from client\n ", socket_recv_size);
        }
		memcpy(output_file_path, OUTPUT_FILE_PATH, strlen(OUTPUT_FILE_PATH));
		memcpy(&output_file_path[strlen(OUTPUT_FILE_PATH)], tmp_file_path, header_h.pld_len+1);
        printf("Server socket <%d> : file path len = %d , file path = %s %s\n", socket_server_id, header_h.pld_len, tmp_file_path, output_file_path);
		
		// 2 FILE_TRANSFER_CMD_DATA
		rx_recv_bytes = 0;
		fp = open_file(output_file_path);
    	if ( !fp ) {
            printf("Server socket <%d> : file open failed, file path = %s errno = %s \n ", socket_server_id, output_file_path, strerror(errno));
        	assert(0);
		}
		
		while (1) {
			// 2.1 Header for next command
			socket_recv_size = read(connfd, (uint8_t *)(&header_n), FT_MSG_HD_LEN);
        	if ( socket_recv_size != FT_MSG_HD_LEN ) {
        	    assert (0);
        	} else {
        	    printf("[DBBUG] FILE_TRANSFER_CMD_DATA or FILE_TRANSFER_CMD_END header read %ld bytes from client\n ", socket_recv_size);
        	}
        	header_h = network_to_host_byte_order(header_n);
			
			if ( header_h.msg_type == FILE_TRANSFER_CMD_END ) {
				printf("Server socket <%d> : one file transmission is ended\n", socket_server_id);
				break; 
			}

        	assert (header_h.msg_type == FILE_TRANSFER_CMD_DATA);

			// 2.2 Payload for FILE_TRANSFER_CMD_DATA
			memset(tcp_pkt_buf, 0, header_h.pld_len);
			socket_recv_size = read(connfd, tcp_pkt_buf, header_h.pld_len);
    	    if ( socket_recv_size != header_h.pld_len ) {
				assert (0);
			} else {
                printf("[DBBUG] Read %ld bytes from client, read seq %u\n ", socket_recv_size, header_h.seq_num);
			}
			file_write_size = fwrite(tcp_pkt_buf, 1, socket_recv_size, fp);
			assert (file_write_size == socket_recv_size);
			rx_recv_bytes += file_write_size;
		}
		printf("Server socket [%d ] file transfer result : output file %s, total received bytes = %ld \n", socket_server_id, output_file_path, rx_recv_bytes);
		fclose(fp);

		}

		// Close client socket
		close(connfd);
		sb_free(tcp_pkt_buf);
	}

	return NULL;
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

static void recursive_create_dir(char *dir_path) {
	struct stat status = { 0 };
	char tmp_folder_path[MAX_FILE_PATH_LEN];

    char *last_occurrence_of_slash = strrchr(dir_path, '/');
    memcpy(tmp_folder_path, dir_path, last_occurrence_of_slash - dir_path);
    tmp_folder_path[last_occurrence_of_slash - dir_path] = '\0';	

	if( stat( dir_path, &status) != -1 ) {
		return;
	} else {
		recursive_create_dir( tmp_folder_path );	
		mkdir(dir_path, 0700);
	}
}

static FILE *open_file(char *file_path) {
	FILE *fp;
	char tmp_folder_path[MAX_FILE_PATH_LEN];

	// First check whether the folder exists or not
	char *last_occurrence_of_slash = strrchr(file_path, '/');
	memcpy(tmp_folder_path, file_path, last_occurrence_of_slash - file_path);
	tmp_folder_path[last_occurrence_of_slash - file_path ] = '\0';

	recursive_create_dir(tmp_folder_path);

    fp = fopen(file_path, "wb");
	return fp;
}

static FILE_TRANSFER_HEADER network_to_host_byte_order(FILE_TRANSFER_HEADER header_n) {
    FILE_TRANSFER_HEADER header_h;

    header_h.msg_type = ntohs(header_n.msg_type);
    header_h.seq_num = ntohs(header_n.seq_num);
    header_h.pld_len = ntohs(header_n.pld_len);
    return header_h;
}
