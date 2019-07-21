#include "stdint.h"

#define FT_MSG_HD_LEN 6

typedef enum {
	FILE_TRANSFER_CMD_UNKNOWN 	= 0x00,
	FILE_TRANSFER_CMD_START 	= 0x01,
	FILE_TRANSFER_CMD_DATA 		= 0x02,
	FILE_TRANSFER_CMD_END 		= 0x03,
} FILE_TRANSFER_CMD_TYPE;

typedef struct {
	uint16_t	msg_type;
	uint16_t	seq_num;
	uint16_t	pld_len;
} FILE_TRANSFER_HEADER;

