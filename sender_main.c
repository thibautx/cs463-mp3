#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#define MAXBUFLEN 1400
#define MY_PORT 48000

typedef struct {    // 16 bytes
	uint16_t src_port;  /* source port */
	uint16_t dest_port;  /* destination port */
	uint32_t seq_no;   /* sequence number */
	uint32_t ack_no;   /* acknowledgement number */
	uint8_t SYN;	/*for syncronization*/
	uint8_t ACK;	/*for acknowledgement*/
	uint8_t FIN;	/*for closing the connection*/
	uint8_t DATA;	/*Data bit*/
}TCP_hearder;

enum tcp_state {
	CLOSED      = 0,
	LISTEN      = 1,
	SYN_SENT    = 2,
	SYN_RCVD    = 3,
	ESTABLISHED = 4,
	FIN_WAIT_1  = 5,
	FIN_WAIT_2  = 6,
	CLOSE_WAIT  = 7,
	CLOSING     = 8,
	LAST_ACK    = 9,
	TIME_WAIT   = 10
};

void print_header(TCP_hearder * header){
	printf( "src_port = %d \n", ntohs(header->src_port));
	printf( "dest_port = %d \n", ntohs(header->dest_port));
	printf( "seq_no = %d \n", ntohl(header->seq_no));
	printf( "ack_no = %d \n", ntohl(header->ack_no));
	printf( "SYN = %d \n", header->SYN);
	printf( "ACK = %d \n", header->ACK);
	printf( "FIN = %d \n", header->FIN);
	printf( "DATA = %d \n", header->DATA);
}

int timeout_recvfrom (int sock, char *buf, struct sockaddr* connection, socklen_t *length, int timeout){
	fd_set socks;
	struct timeval t;
	t.tv_sec = 0;
	int numbytes;
	FD_ZERO(&socks);
	FD_SET(sock, &socks);
	t.tv_usec = timeout;
	if (select(sock + 1, &socks, NULL, NULL, &t))
	{
		if (numbytes = recvfrom(sock, buf, MAXBUFLEN-1, 0, connection, length)!=-1)
			return numbytes;
	}
	else
		return -1;
}

int set_timeout(int sockfd, int ms){
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = ms * 000;  //100ms
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("sender: setsockopt");
		return -1;
	}
	return 0;
}

int reliablyTransfer(char* hostname, char * udpPort, char* filename, unsigned long long int bytesToTransfer){
	int sockfd, rv, numbytes;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in bind_addr, sendto_addr;
	struct sockaddr their_addr;
	socklen_t their_addr_len = sizeof(their_addr);
	char buf[MAXBUFLEN];

	//setting up sendto address
	uint16_t sendto_port = (uint16_t)atoi(udpPort);
	memset(&sendto_addr, 0, sizeof(sendto_addr));
	sendto_addr.sin_family = AF_INET;
	sendto_addr.sin_port = htons(sendto_port);
	inet_pton(AF_INET, hostname, &sendto_addr.sin_addr);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(hostname, udpPort, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("sender: socket");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "sender: failed to create socket\n");
		return 2;
	}
	//extract my port number
	uint16_t my_port = ((struct sockaddr_in *)p->ai_addr )->sin_port;
	printf("my port %d\n", my_port);

	freeaddrinfo(servinfo);

	set_timeout(sockfd, 100); //100ms

	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(my_port);
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in)) == -1)
	{
		close(sockfd);
		perror("sender: bind");
		exit(1);
	}

	TCP_hearder my_header, their_header;
	my_header.src_port = htons(my_port);
	my_header.dest_port = htons( (uint16_t)atoi(udpPort) );
	my_header.seq_no = htonl(0);
	my_header.ack_no = htonl(0);
	my_header.SYN = 1;
	my_header.ACK = 0;
	my_header.FIN = 0;
	my_header.DATA = 0;

	enum tcp_state state;

	printf("Starting three way handshake...\n");

	while(1)
	{
		memcpy(buf, &my_header, sizeof(my_header));
		if (numbytes = sendto(sockfd, buf, 16, 0, (struct sockaddr* )&sendto_addr,
							  sizeof(sendto_addr)) == -1) {
			perror("sender: sendto");
			exit(1);
		}
		state = SYN_SENT;
		numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0, &their_addr, &their_addr_len);
		printf("bytes received %d\n",numbytes);
		if(numbytes == 16)
			break;
	}
	printf("their port %d\n", ntohs(((struct sockaddr_in *)&their_addr)->sin_port));

	buf[numbytes] = '\0';
	memcpy(&their_header, buf, numbytes);

	if (their_header.SYN == 1 && their_header.ACK == 1)
		state = ESTABLISHED;

	my_header.SYN = 0;
	my_header.ACK = 1;

	printf("In while 2\n");
	while(1)
	{
		memcpy(buf, &my_header, sizeof(my_header));
		if (numbytes = sendto(sockfd, buf, 16, 0, (struct sockaddr*)&sendto_addr,
							  sizeof(sendto_addr)) == -1) {
			perror("sender: sendto");
			exit(1);
		}
		if(numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,NULL, NULL) == -1)
			break;
	}

	state = ESTABLISHED;
	printf("Connection Established\n");
	print_header(&their_header);

	my_header.ACK = 0;
	my_header.DATA = 1;

	FILE* out_file = fopen(filename, "rb");
	memcpy(buf, &my_header, sizeof(my_header));
	fread(buf + sizeof(my_header), 1, bytesToTransfer, out_file);

	if (numbytes = sendto(sockfd, buf, bytesToTransfer + sizeof(my_header), 0,
						  (struct sockaddr*)&sendto_addr, sizeof(sendto_addr)) == -1) {
		perror("sender: sendto");
		exit(1);
	}

	fclose(out_file);
	close(sockfd);


}

int main(int argc, char** argv){

	unsigned long long int numBytes;

	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	numBytes = atoll(argv[4]);

	reliablyTransfer(argv[1], argv[2], argv[3], numBytes);
}