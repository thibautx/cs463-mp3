#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>

#define MAXBUFLEN 1400

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

void print_header(TCP_hearder * header)
{
	printf( "src_port = %d \n", ntohs(header->src_port));
	printf( "dest_port = %d \n", ntohs(header->dest_port));
	printf( "seq_no = %d \n", ntohl(header->seq_no));
	printf( "ack_no = %d \n", ntohl(header->ack_no));
	printf( "SYN = %d \n", header->SYN);
	printf( "ACK = %d \n", header->ACK);
	printf( "FIN = %d \n", header->FIN);
	printf( "DATA = %d \n", header->DATA);
}

int set_timeout(int sockfd, int ms)
{
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = ms * 000;  //100ms
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		perror("sender: setsockopt");
		return -1;
	}
	return 0;
}

int reliablyReceive(char * udpPort, char* destinationFile) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr their_addr;
	char buf[MAXBUFLEN];
	socklen_t their_addr_len = sizeof(their_addr);
	char s[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, udpPort, &hints, &servinfo)) != 0) {
    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    	return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
    	if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
    		perror("receiver: socket");
    		continue;
    	}

    	if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
    		close(sockfd);
    		perror("receiver: bind");
    		continue;
    	}

    	break;
    }

    if (p == NULL) {
    	fprintf(stderr, "receiver: failed to bind socket\n");
    	return 2;
    }
    freeaddrinfo(servinfo);

    enum tcp_state state;
    state = LISTEN;
    printf("receiver: waiting to recvfrom...\n");

    TCP_hearder my_header, their_header;
    my_header.src_port = htons( (uint16_t)atoi(udpPort) );
    my_header.seq_no = htonl(0);
    my_header.ack_no = htonl(0);
    my_header.SYN = 1;
    my_header.ACK = 1;
    my_header.FIN = 0;
    my_header.DATA = 0;

    numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, 
    	&their_addr_len);
    buf[numbytes] = '\0';
    
    memcpy(&their_header, buf, numbytes);
    my_header.dest_port = their_header.src_port;
    if (their_header.SYN == 1 && their_header.DATA == 0)
    	state = SYN_RCVD;

	set_timeout(sockfd, 100); //100ms

	while(1)
	{
		memcpy(buf, &my_header, sizeof(my_header));
		if ((numbytes = sendto(sockfd, buf, 16, 0, (struct sockaddr *)&their_addr, 
			their_addr_len)) == -1) 
		{
			perror("receiver: sendto");
			exit(2);
		}
		state = SYN_SENT;
		numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, NULL, NULL);
		
		if (numbytes == 16)
			break;
	}
	
	buf[numbytes] = '\0';
	memcpy(&their_header, buf, numbytes);
	state = ESTABLISHED;
	printf("Connection Established\n");
	print_header(&their_header);

	while(1)
	{
		numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, NULL, NULL);

		if(numbytes > 15)
		{
			buf[numbytes] = '\0';
			memcpy(&their_header, buf, sizeof(their_header));
			if (their_header.DATA == 1)
				break;
		}	
	}
	printf("bytes received %d\n", (int)(numbytes - sizeof(their_header)));

	FILE *recv_file = fopen(destinationFile, "wb");
	fwrite(buf+ sizeof(their_header), 1, numbytes - sizeof(their_header), recv_file);

	fclose(recv_file);



	close(sockfd);



}

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	reliablyReceive(argv[1], argv[2]);
}
