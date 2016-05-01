#include <sys/types.h>

enum packet_type {
        NORMAL_PKT = 1,
        EOF_PKT    = 2,
};

//#define   MSS      1430

typedef struct packet_t_struct{
//        int            packet_type;
		uint32_t packet_type;
//		int            buf_bytes;
        uint32_t buf_bytes;
//        int            sequencenumber;
		uint32_t sequencenumber;
//		off_t          f_offset;
		uint32_t          f_offset;
        unsigned char  data[0];
} packet_t;

#define    MSS        (1458 - sizeof(packet_t))

typedef struct eof_packet_t_struct {
//        unsigned short packet_type;
	uint32_t packet_type;
//        int  eof;
	uint32_t eof;
//        long file_sz;
//	uint64_t file_sz;
	uint64_t file_sz;
} eof_packet_t;

typedef struct ack_struct{
//        int            sequencenumber;
		uint32_t           sequencenumber;
//        int            buf_len;
		uint32_t		buf_len;
//        long           tot_bytes;
//	uint64_t tot_bytes;
	uint32_t tot_bytes;

//		off_t          f_offset;
		uint32_t          f_offset;

} ack_t;
