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
#include "sys/errno.h"
#include "sb_common.h"
#include "file_common.h"

#define HEAP_SIZE				1024*1024 // 1 MB

#define CHATTING_BUF_MAX 		80 
#define TCP_SERVER_PORT 		8080
#define TCP_PKT_MTU 			65535
#define TCP_PKT_BUF_SIZE 		1500 
#define TCP_SERVER_IPv4_ADDR	"127.0.0.1"
#define INPUT_FILE_PATH			"/Users/local/Personal/Repos/SnowBlood"

#define	MAX_CLIENT_THREAD		10

#define RB_SIZE		10
#define MAX_FILE_PATH_LEN	1024 
#define RING_BUFFER_FULL_TIMEOUT 60000 // 60 s


typedef struct {
	char** file_path_list;
	uint16_t cur_size;
	uint16_t front;
	uint16_t rare;
	uint16_t size;	
} ring_buffer;

int tcp_pkt_buf_size = TCP_PKT_BUF_SIZE;
int client_thread_num = MAX_CLIENT_THREAD;
pthread_t consumer[MAX_CLIENT_THREAD];
uint32_t tcp_port_list[MAX_CLIENT_THREAD] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
pthread_t producer;

pthread_cond_t pipeline_cond_full;
pthread_cond_t pipeline_cond_empty;
pthread_mutex_t rb_mutex;
ring_buffer *rb;
struct timespec ring_buffer_full_timeout;
size_t root_input_file_path_len;

extern int errno;

static void live_chatting(int sockfd);
static uint32_t calculate_time_delta(struct timespec start, struct timespec end);
static void *client_socket(void* argv);
static void *producer_worker(void* argv);
static void list_files_recursively(const char *path);

ring_buffer *ring_buffer_init(uint16_t size);
SB_STATUS ring_buffer_deinit(ring_buffer *rb);
SB_STATUS ring_buffer_add(ring_buffer *rb, char *file_path);
SB_STATUS ring_buffer_remove(ring_buffer *rb, char *file_path);
bool ring_buffer_is_full(ring_buffer *rb);
bool ring_buffer_is_empty(ring_buffer *rb);

static void updateNextDispatchedTimeoutInMs(uint32_t timeInMillisecond);
static FILE_TRANSFER_HEADER host_to_network_byte_order(FILE_TRANSFER_HEADER header_h);

SB_STATUS file_transfer_start(uint16_t client_id, int sockfd, char *file_path, uint8_t *tcp_pkt_buf);
SB_STATUS file_transfer_data(uint16_t client_id, int sockfd, char *file_path, uint8_t *tcp_pkt_buf);
SB_STATUS file_transfer_end(uint16_t client_id, int sockfd);

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
	
	rb = ring_buffer_init(RB_SIZE);
	assert ( rb );

	root_input_file_path_len = strlen(INPUT_FILE_PATH)+1;

	updateNextDispatchedTimeoutInMs(RING_BUFFER_FULL_TIMEOUT);

    ret = pthread_cond_init(&pipeline_cond_full, NULL);
   	assert(ret == 0);

    ret = pthread_cond_init(&pipeline_cond_empty, NULL);
   	assert(ret == 0);
    
	ret = pthread_mutex_init(&rb_mutex, NULL);
   	assert(ret == 0);

	// Create threads as client socket
	for (i = 0;i < client_thread_num;i++) {
    	ret = pthread_create(consumer+i, NULL, client_socket, (void *)(tcp_port_list+i));
    	assert(ret == 0);
	}
	
   	ret = pthread_create(&producer, NULL, producer_worker, NULL);
   	assert(ret == 0);
	while(1);
	return 0;
}

static void *client_socket(void* argv) {
    int sockfd; 
    struct sockaddr_in servaddr, cli; 
	uint8_t *tcp_pkt_buf;
	uint32_t tcp_port;
	char tmp_file_path[MAX_FILE_PATH_LEN];
	uint16_t socket_client_id = *((uint32_t *)argv);
	SB_STATUS result;

    // TCP socket buffer init 
    tcp_pkt_buf = (uint8_t *)sb_malloc(tcp_pkt_buf_size);
    assert (tcp_pkt_buf);

    // socket create and varification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        exit(0); 
    } 
    else
        printf("Client consumer socket successfully created..\n"); 
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT
    tcp_port = TCP_SERVER_PORT + socket_client_id; 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(TCP_SERVER_IPv4_ADDR); 
    servaddr.sin_port = htons(tcp_port); 
 
    // connect the client socket to server socket 
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else {
        printf("connected to the server with tcp port of <%d> \n", tcp_port);
    }

	while (1) {
		// Fetch one file path from ring buffer
		pthread_mutex_lock( &rb_mutex );
		if ( ring_buffer_is_empty(rb) ) {
			// signal producer to fill more file paths into ring buffer
			pthread_cond_signal( &pipeline_cond_full );
			// block current consumer thread until it's not empty
			pthread_cond_wait( &pipeline_cond_empty, &rb_mutex);
			// Scheduling delay for broadcase signaling
			if ( ring_buffer_is_empty(rb) ) {
				pthread_mutex_unlock( &rb_mutex );
				continue;
			}
		}
		memset(tmp_file_path, 0, MAX_FILE_PATH_LEN);
		result = ring_buffer_remove(rb, tmp_file_path);
		assert (result == SB_OK);
		printf("Consumer socket [%d] : result = %d tmp_file_path %s, rb cur size = %d\n", socket_client_id, result, tmp_file_path, rb->cur_size);
		pthread_mutex_unlock( &rb_mutex );
		
		// FILE_TRANSFER_CMD_START 
		result = file_transfer_start(socket_client_id, sockfd, tmp_file_path, tcp_pkt_buf);
		assert (result == SB_OK);
		
		// FILE_TRANSFER_CMD_DATA
		result = file_transfer_data(socket_client_id, sockfd, tmp_file_path, tcp_pkt_buf);
		assert (result == SB_OK);
		
		// FILE_TRANSFER_CMD_END
		result = file_transfer_end(socket_client_id, sockfd);
		assert (result == SB_OK);
	}

    // Free up resouces 
    close(sockfd);
	sb_free(tcp_pkt_buf);

	return NULL;
}

static void *producer_worker(void* argv) {
	printf("Producer worker is created!\n");
    // list all files
    list_files_recursively(INPUT_FILE_PATH);	
	pthread_mutex_lock ( &rb_mutex );
	if ( !ring_buffer_is_empty ( rb ) ) {
		// brodcast signal client_socket to consume the remaining items in ring buffer
        pthread_cond_broadcast( &pipeline_cond_empty );
	}
	pthread_mutex_unlock ( &rb_mutex);
	return NULL;
}

static uint32_t calculate_time_delta(struct timespec start, struct timespec end) {
	return ( (end.tv_sec - start.tv_sec)*1000000 ) + ( (end.tv_nsec - start.tv_nsec)/1000 );	
}

static void list_files_recursively(const char *path) {
    struct dirent *de;
    DIR *dr;
	char tmp_file_path[MAX_FILE_PATH_LEN];
	size_t len = strlen(path);
	size_t cur_path_len;
	int result;

	assert ( len + 2 <= MAX_FILE_PATH_LEN);

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
		cur_path_len = strlen(de->d_name);
        assert (len + 2 + cur_path_len <= MAX_FILE_PATH_LEN);
        memcpy(tmp_file_path+len+1, de->d_name, cur_path_len);
        tmp_file_path[len+cur_path_len+1] = '\0';

        if (de->d_type == DT_DIR) {
			if ( !strcmp(de->d_name, ".") || !strcmp(de->d_name, "..") ) {
				continue;
			} else {
				//printf("[DEBUG BEFORE] %s %s [%s]\n", path, de->d_name, tmp_file_path);
				//printf("[DEBUG AFTER] %s %s [%s]\n", path, de->d_name, tmp_file_path);
				list_files_recursively(tmp_file_path);
			}
        } else {
        	printf("%s/%s\n", path, de->d_name);
			// Insert the file path into ring_buffer
			pthread_mutex_lock( &rb_mutex );
			if ( ring_buffer_is_full(rb) ) {
				// brodcast signal client_socket to consume the ring buffer
				pthread_cond_broadcast( &pipeline_cond_empty );
				// block current producer thread until it's not full
				result = pthread_cond_timedwait( &pipeline_cond_full, &rb_mutex, &ring_buffer_full_timeout );
				assert ( result != ETIMEDOUT );	 
			}
			printf("Producer worker : tmp_file_path %s, rb cur size = %d\n", tmp_file_path, rb->cur_size);
			ring_buffer_add(rb, tmp_file_path);
			
			pthread_mutex_unlock( &rb_mutex );
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
 
ring_buffer *ring_buffer_init(uint16_t size) {
	uint16_t i;
	ring_buffer *rb;
	char** file_path_list;
	
	rb = (ring_buffer *)sb_malloc(sizeof(ring_buffer));
	assert( rb );
	
	rb->cur_size = 0;
	rb->front = 0;
	rb->rare = 0;
	rb->size = size;
	
	file_path_list = (char **)sb_malloc(size*sizeof(char *));
	assert (file_path_list);
	rb->file_path_list = file_path_list;
	
	for( i = 0; i < size; i++ ) {
		char *file_path = (char *)sb_malloc(MAX_FILE_PATH_LEN*sizeof(char));
		assert (file_path);
		memset(file_path, 0, MAX_FILE_PATH_LEN);
		*(file_path_list + i) = file_path; 
	}

	return rb;
}

SB_STATUS ring_buffer_deinit(ring_buffer *rb) {
	uint16_t i;

	for( i = 0; i < rb->size; i++) {
		memset( *(rb->file_path_list+i), 0, MAX_FILE_PATH_LEN);
		sb_free(*(rb->file_path_list+i));
	}
	
	sb_free(rb->file_path_list);
	sb_free(rb);
	return SB_OK;
}

SB_STATUS ring_buffer_add(ring_buffer *rb, char *file_path) {
	if ( !rb || !file_path ) {
		return SB_INVALID_PARAMETER;
	}

	if ( ring_buffer_is_full(rb) || strlen(file_path) > MAX_FILE_PATH_LEN-1 ) {
		return SB_OUT_OF_MEMORY;
	}	
	memcpy(*(rb->file_path_list+rb->rare), file_path, strlen(file_path));
	*((*(rb->file_path_list+rb->rare)) + strlen(file_path)) = '\0';
	rb->cur_size++;
	rb->rare = (rb->rare+1) % rb->size;
	return SB_OK;
}

SB_STATUS ring_buffer_remove(ring_buffer *rb, char *file_path) {
    if ( !rb || !file_path ) {
        return SB_INVALID_PARAMETER;
    }

    if ( ring_buffer_is_empty(rb) ) {
        return SB_INVALID_PARAMETER;
    }
	printf("[DEBUG] ring_buffer_remove : file_path %s front_in_rb %s length %zu rb->cur_size %d \n", file_path, *(rb->file_path_list+rb->front), strlen(*(rb->file_path_list+rb->front)), rb->cur_size);
	memcpy(file_path, *(rb->file_path_list+rb->front), strlen(*(rb->file_path_list+rb->front)));
	assert (strlen(file_path) != 0);
	rb->cur_size--;
	rb->front = (rb->front+1) % rb->size;
	return SB_OK;
}

bool ring_buffer_is_full(ring_buffer *rb) {
	if ( !rb ) {
		return false;
	}
	return (rb->cur_size >= rb->size) ? true : false;
}

bool ring_buffer_is_empty(ring_buffer *rb) {
	if ( !rb ) {
        return false; 
    }
	return !rb->cur_size;
}

static void updateNextDispatchedTimeoutInMs(uint32_t timeInMillisecond) {
    clock_gettime(CLOCK_REALTIME, &ring_buffer_full_timeout);
    ring_buffer_full_timeout.tv_sec += timeInMillisecond / 1000;
    ring_buffer_full_timeout.tv_nsec += (timeInMillisecond%1000) * 1000000;
}

static FILE_TRANSFER_HEADER host_to_network_byte_order(FILE_TRANSFER_HEADER header_h) {
	FILE_TRANSFER_HEADER header_n;

	header_n.msg_type = htons(header_h.msg_type);
	header_n.seq_num = htons(header_h.seq_num);
	header_n.pld_len = htons(header_h.pld_len);
	return header_n;
}

SB_STATUS file_transfer_start(uint16_t client_id, int sockfd, char *file_path, uint8_t *tcp_pkt_buf) {
		FILE_TRANSFER_HEADER header_h, header_n;
		long int socket_sent_size;

        // 1.1 FILE_TRANSFER_CMD_START header
        header_h.msg_type = FILE_TRANSFER_CMD_START;
        header_h.seq_num = 0;
        header_h.pld_len = strlen(file_path) - root_input_file_path_len;

        header_n = host_to_network_byte_order(header_h);

        socket_sent_size = write(sockfd, (uint8_t *)(&header_n), FT_MSG_HD_LEN);
        if ( socket_sent_size != FT_MSG_HD_LEN ) {
            printf("Socekt send failed errno = %s \n ", strerror(errno));
            assert (0);
        } else {
            printf("Consumer socket [%d]: Send FILE_TRANSFER_CMD_START header with %ld bytes\n ", client_id, socket_sent_size);
        }  

        // 1.2 FILE_TRANSFER_CMD_START with file path name
        memcpy(tcp_pkt_buf, file_path + root_input_file_path_len, header_h.pld_len);

        socket_sent_size = write(sockfd, tcp_pkt_buf, header_h.pld_len);
        if ( socket_sent_size != header_h.pld_len ) {
            printf("Socekt send failed errno = %s \n ", strerror(errno));
            assert (0);
        } else {
            printf("Consumer socket [%d]: Send FILE_TRANSFER_CMD_START payload with %ld bytes\n ", client_id, socket_sent_size);
        }
		return SB_OK;
}

SB_STATUS file_transfer_data(uint16_t client_id, int sockfd, char *file_path, uint8_t *tcp_pkt_buf) {
		FILE_TRANSFER_HEADER header_h, header_n;
		FILE *fp;
		uint16_t data_seq_num = 0;
		long int socket_sent_size;
		long file_sent_bytes = 0;
		uint32_t file_read_size;
		struct timespec start_ts, end_ts;

		clock_gettime(CLOCK_REALTIME, &start_ts);
        
        fp = fopen(file_path, "rb");
        if ( !fp ) {
            printf("Consumer socket : file open failed, file path = %s errno = %s \n ", file_path, strerror(errno));
            assert(0);
        }

		// Read data out of input file by the size of tcp_pkt_buf_size and send it to TCP server
        while (1) {
            memset(tcp_pkt_buf, 0, tcp_pkt_buf_size);
            file_read_size = fread(tcp_pkt_buf, 1, tcp_pkt_buf_size, fp);
			if ( file_read_size == 0 ) {
				break;
			}
			// 2.1 FILE_TRANSFER_CMD_DATA header
        	header_h.msg_type = FILE_TRANSFER_CMD_DATA;
        	header_h.seq_num = data_seq_num++;
        	header_h.pld_len = file_read_size;

        	header_n = host_to_network_byte_order(header_h);

        	socket_sent_size = write(sockfd, (uint8_t *)(&header_n), FT_MSG_HD_LEN);
        	if ( socket_sent_size != FT_MSG_HD_LEN ) {
            	printf("Socekt send failed errno = %s \n ", strerror(errno));
            	assert (0);
        	} else {
            	printf("Consumer socket [%d] : Send FILE_TRANSFER_CMD_DATA header with %ld bytes, seq %d\n ",  client_id, socket_sent_size, header_h.seq_num);
        	}
			// 2.2 FILE_TRANSFER_CMD_DATA payload
            socket_sent_size = write(sockfd, tcp_pkt_buf, file_read_size);
            if ( socket_sent_size != file_read_size ) {
                printf("Socekt send failed errno = %s \n ", strerror(errno));
                assert (0);
            } else {
                printf("Consumer socket [%d] : Send data with %ld bytes, seq %u \n ",  client_id, socket_sent_size, header_h.seq_num);
            }
          	file_sent_bytes += socket_sent_size;

            if (file_read_size < tcp_pkt_buf_size) {
                // Reach EOF
                //printf("Send loop is ended\n");
                break;
            }

        }
        clock_gettime(CLOCK_REALTIME, &end_ts);

		printf("Consumer socket file transfer result [%d] : total sent bytes = %ld, time delta = %d s \n", client_id, file_sent_bytes, calculate_time_delta(start_ts, end_ts)/1000000);
        fclose(fp);
		
		return SB_OK;
}

SB_STATUS file_transfer_end(uint16_t client_id, int sockfd) {
        FILE_TRANSFER_HEADER header_h, header_n;
        long int socket_sent_size;

        // 3 FILE_TRANSFER_CMD_END header
        header_h.msg_type = FILE_TRANSFER_CMD_END;
        header_h.seq_num = 0;
        header_h.pld_len = 0;

        header_n = host_to_network_byte_order(header_h);

        socket_sent_size = write(sockfd, (uint8_t *)(&header_n), FT_MSG_HD_LEN);
        if ( socket_sent_size != FT_MSG_HD_LEN ) {
            printf("Socekt send failed errno = %s \n ", strerror(errno));
            assert (0);
        } else {
            printf("Consumer socket [%d] : Send FILE_TRANSFER_CMD_END header with %ld bytes\n ",  client_id, socket_sent_size);
        }
		
		return SB_OK;
}
