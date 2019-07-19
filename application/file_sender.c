#include "netdb.h"
#include "arpa/inet.h"
#include "stdio.h" 
#include "stdlib.h"
#include "string.h" 
#include "pthread.h"
#include "sys/socket.h"
#include "unistd.h"
#include "memory.h"
#include "time.h"
#include "assert.h"
#include "dirent.h" 

#define HEAP_SIZE				1024*1024 // 1 MB

#define CHATTING_BUF_MAX 		80 
#define TCP_SERVER_PORT 		8080
#define TCP_PKT_MTU 			65535
#define TCP_PKT_BUF_SIZE 		1500 
#define TCP_SERVER_IPv4_ADDR	"127.0.0.1"
#define INPUT_FILE_PATH			"/Users/local/Downloads/en_windows_7_professional_n_with_sp1_x64_dvd_u_677207.iso"

#define	MAX_CLIENT_THREAD		10


int tcp_pkt_buf_size = TCP_PKT_BUF_SIZE;
int client_thread_num = MAX_CLIENT_THREAD;
pthread_t client_sock[MAX_CLIENT_THREAD];
uint32_t tcp_port_list[MAX_CLIENT_THREAD] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

extern int errno;

static void live_chatting(int sockfd);
static uint32_t calculate_time_delta(struct timespec start, struct timespec end);
static void *client_socket(void* argv);
static void list_files_recursively(const char *path);
 
int main(int argc, char *argv[]) {
	SB_STATUS result;
	uint8_t i;
	int ret;

	if ( argc != 3 ) {
		printf("CLI format : file_sender <TCP_BLOCK_SIZE> <THREAD_NUM>\n");
		return -1;
	} else {
		tcp_pkt_buf_size = atoi(argv[1]);
		client_thread_num = atoi(argv[2]);
	}

	// init snowblood memory heap 
	result = sb_init_memory(HEAP_SIZE, SB_MEM_CONFIG_MODE_1);
    assert (result == SB_OK);

	// list all files
	list_files_recursively("/Users/local/Personal/Repos/SnowBlood");

	/*
	// Create threads as client socket
	for (i = 0;i < client_thread_num;i++) {
    	ret = pthread_create(client_sock+i, NULL, client_socket, (void *)(tcp_port_list+i));
    	assert(ret == 0);
	}
	*/
	while(1);
	return 0;
}

static void *client_socket(void* argv) {
    int sockfd, connfd; 
    struct sockaddr_in servaddr, cli; 
	FILE *fp;
	uint8_t *tcp_pkt_buf;
	long tx_sent_bytes;
	uint32_t file_read_size;
	long int socket_sent_size;
	struct timespec start_ts, end_ts;
	uint32_t tcp_port;
  
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
	tcp_port = TCP_SERVER_PORT + *((uint32_t *)argv); 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(TCP_SERVER_IPv4_ADDR); 
    servaddr.sin_port = htons(tcp_port); 

    // Read input file
	tcp_pkt_buf = (uint8_t *)sb_malloc(tcp_pkt_buf_size);
	assert (tcp_pkt_buf);
  
    // connect the client socket to server socket 
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        printf("connection with the server failed...\n"); 
        exit(0); 
    } 
    else
        printf("connected to the server with tcp port of <%d> \n", tcp_port); 
 /*
	// Read next un-sent file in the directory
	while (1) { 
		// TODO : Find out next available un-sent file
		fp = fopen(INPUT_FILE_PATH, "rb");
		assert (fp);

		// Read data out of input file by the size of tcp_pkt_buf_size and send it to TCP server
		tx_sent_bytes = 0;
		clock_gettime(CLOCK_REALTIME, &start_ts);
		while (1) {
			memset(tcp_pkt_buf, 0, tcp_pkt_buf_size);	
			file_read_size = fread(tcp_pkt_buf, 1, tcp_pkt_buf_size, fp);
			if (file_read_size < tcp_pkt_buf_size) {
				// Reach EOF
    	        printf("Send loop is ended\n");
				break;
			}
    	    socket_sent_size = write(sockfd, tcp_pkt_buf, file_read_size);
			if ( socket_sent_size <= 0 ) {
    	        printf("Socekt send failed errno = %s \n ", strerror(errno));
    	    	assert (0);
			} else {
    	        //printf("Send %ld bytes to server \n ", socket_sent_size);
    	    }
			tx_sent_bytes += socket_sent_size; 
		} 

		if (file_read_size > 0) {
    	    socket_sent_size = write(sockfd, tcp_pkt_buf, file_read_size);
    	    if ( socket_sent_size <= 0 ) {
    	        printf("Socekt send failed errno = %s \n ", strerror(errno));
    	    	assert (0);
    	    } else {
    	        //printf("Send %ld bytes to server \n ", socket_sent_size);
    	    }
			tx_sent_bytes += socket_sent_size; 
		}
		clock_gettime(CLOCK_REALTIME, &end_ts);
	
		printf("File transfer result : total sent bytes = %ld, time delta = %d s, TCP packet size = %d \n", tx_sent_bytes, calculate_time_delta(start_ts, end_ts)/1000000, tcp_pkt_buf_size);	
		fclose(fp);
	}
*/
    // Free up resouces 
    close(sockfd);
	sb_free(tcp_pkt_buf);
	return NULL;
}

static uint32_t calculate_time_delta(struct timespec start, struct timespec end) {
	return ( (end.tv_sec - start.tv_sec)*1000000 ) + ( (end.tv_nsec - start.tv_nsec)/1000 );	
}

static void list_files_recursively(const char *path) {
    struct dirent *de;
    DIR *dr;
	char tmp_file_path[1024];
	size_t len = strlen(path);
	size_t cur_path_len;

	assert ( len + 2 <= 1024);

	memcpy(tmp_file_path, path, len);
	tmp_file_path[len] = '/';

	
	//printf("[DEBUG] [%s] \n", path);
    // list out all files in current directory
    dr = opendir(path);
	if ( !dr ) {
    	printf("opendir errno = %s \n ", strerror(errno));
   	}
    assert (dr);

    while ( ( de = readdir(dr)) ) {
        if (de->d_type == DT_DIR) {
			if ( !strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") ) {
				continue;
			} else {
				//printf("[DEBUG BEFORE] %s %s [%s]\n", path, de->d_name, tmp_file_path);
				cur_path_len = strlen(de->d_name);
				assert (len + 2 + cur_path_len <= 1024);
				memcpy(tmp_file_path+len+1, de->d_name, cur_path_len);
				tmp_file_path[len+cur_path_len+1] = '\0';
				//printf("[DEBUG AFTER] %s %s [%s]\n", path, de->d_name, tmp_file_path);
				list_files_recursively(tmp_file_path);
			}
        } else {
        	printf("%s/%s\n", path, de->d_name);
		}
    }

    closedir(dr);
}

void live_chatting(int sockfd) 
{ 
    char buf[CHATTING_BUF_MAX]; 
    uint16_t n;
	int error = 0;
 
    while (1) { 
        memset(buf, 0, CHATTING_BUF_MAX); 
        printf(" To server : "); 
        n = 0; 
        while ((buf[n++] = getchar()) != '\n'); 
        write(sockfd, buf, n); 
       	memset(buf, 0, CHATTING_BUF_MAX); 
        error = read(sockfd, buf, sizeof(buf));
		if ( error <= 0 ) { 
        	printf("Read failed error = %d, errno = %s \n ", error, strerror(errno)); 
        }
		printf("From Server : %s", buf); 
        if ((strncmp(buf, "exit", 4)) == 0) { 
            printf("Client Exit...\n"); 
            break; 
        } 
    } 
} 
