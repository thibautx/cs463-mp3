#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include "packet_header.h"

#define NO_MSG_WAIT_ALL 0

//int  sequencenumber;
uint32_t sequencenumber;
//int  buf_len;
uint32_t buf_len;
//off_t offst = 0;
uint32_t offst = 0;

unsigned int lossCtr = 0;

enum state {
	GOOD_STATE = 1,
	LOSSY_STATE = 2
};

int rv;
int recv_sockfd;
struct addrinfo hints, *servinfo, *p;
struct sockaddr their_addr;
//struct sockaddr_in si_me;
struct sockaddr_in si_other;
struct sockaddr_in peer_addr;
//socklen_t peer_addr_len = sizeof(struct sockaddr_in);
socklen_t their_addr_len = sizeof(their_addr);
unsigned short int port_num;
packet_t *recv_packet;

enum state getState(int sequencenum, off_t off){
	if ((sequencenum == (sequencenumber + 1)) && (off == offst)) {
		return GOOD_STATE;
	} else {
		return LOSSY_STATE;
	}
}

int setup_network(char* UDPport){
//	int numbytes;
//	char buf[MAXBUFLEN];
//	socklen_t their_addr_len = sizeof(their_addr);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, UDPport, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((recv_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("receiver: socket");
			continue;
		}
		if (bind(recv_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(recv_sockfd);
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
	printf("receiver: setup_network complete\n");
	return 1;
//	if ((recv_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
//	{
//		perror("Socket call failed\n");
//		exit(1);
//	}
//
//	memset((char *) &si_me, 0, sizeof(si_me));
//	si_me.sin_family = AF_INET;
//	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
//	si_me.sin_port = htons(UDPport);
//	if (bind(recv_sockfd, (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
//		perror("Bind Error");
//		exit(1);
//	}
//	port_num = UDPport;
//
//	return 0;
}

void* producer(void* arg){
	char* destfile = (char *)arg;
	ack_t  ack_now;
//	int numBytes;
	ssize_t numBytes;
	FILE*  dest;
	ssize_t recvbytes;
//	long writebytes = 0;
//	long totalRecv = 0;
	uint64_t writebytes = 0;
	uint64_t totalRecv;
	int state = 0;
	int prev_state = 0;
	recv_packet = malloc(sizeof(packet_t)+MSS);
	dest = fopen(destfile, "wb");

	if (dest == NULL) {
		perror("Fopen failed\n");
		exit(1);
	}

	offst = 0;
	while(1){
		if((recvbytes = recvfrom(recv_sockfd, recv_packet, sizeof(packet_t)+MSS, NO_MSG_WAIT_ALL,
								 &their_addr, &their_addr_len)) == -1){
			perror("received: Packet receive error");
			exit(1);
		}
		if(recvbytes != 1458){
			printf("receiver: received %lu bytes, type=%d\n", recvbytes, ntohl(recv_packet->packet_type));
		}
		printf("received: total received %lu\n", totalRecv);
		totalRecv += recvbytes;
		if (ntohl(recv_packet->packet_type) == EOF_PKT) {
			eof_packet_t *ep = malloc(sizeof(eof_packet_t));;
			memcpy(ep, &recv_packet, sizeof(eof_packet_t));
			printf("Total bytes written %ld File size %d EOF %d Final Local State %d Loss Ctr %d\n",
				   writebytes, ntohl(ep->file_sz), ntohl(ep->eof), state, lossCtr);
			fflush(dest);
			fclose(dest);
			free(ep);
			break;
		}

		switch(getState(ntohl(recv_packet->sequencenumber), ntohl(recv_packet->f_offset))) {
			case GOOD_STATE:
			{
				sequencenumber++;
				buf_len = ntohl(recv_packet->buf_bytes);
				if((numBytes = fwrite(recv_packet->data, 1, buf_len, dest)) == -1) {
					perror("fwrite");
					exit(1);
				}
				offst = ftell(dest);
				writebytes += numBytes;
				ack_now.buf_len = recv_packet->buf_bytes;
				ack_now.sequencenumber = htonl(sequencenumber);
				ack_now.tot_bytes = htonl(writebytes);
				ack_now.f_offset = htonl(ftell(dest));
				if (peer_addr.sin_port != htons(port_num)) {
					peer_addr.sin_port = htons(port_num);
				}
//				if((numBytes = sendto(recv_sockfd, &ack_now, sizeof(ack_t), NO_MSG_WAIT_ALL,
//								 (struct sockaddr *)&peer_addr, peer_addr_len)) == -1){
//					printf("Peer Addr %s Peer port %d Peer AF %d\n",
//						   inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port),
//						   ntohs(peer_addr.sin_family));
//					perror("receiver: send ack error 1");
//					exit(1);
//				}

//				printf("receiver: send ack now\n");
				if((numBytes = sendto(recv_sockfd, &ack_now, sizeof(ack_t), NO_MSG_WAIT_ALL,
								 &their_addr, their_addr_len)) == -1){
//					printf("sender: their_addr=%s, their_port=%d, their_af=%d\n",
//						   inet_ntoa(their_addr.sin_addr),
//						   ntohs(their_addr.sin_port),
//						   ntohs(their_addr.sin_family));
					perror("receiver: send ack error 1 (GOOD_STATE)");
					exit(1);
				}

			}
				state = GOOD_STATE;
				prev_state = GOOD_STATE;
				break;
			case LOSSY_STATE:
			{
				if (prev_state == GOOD_STATE) {
					lossCtr++;
				}

				ack_now.buf_len = htonl(buf_len);
				ack_now.tot_bytes = htonl(writebytes);
				ack_now.sequencenumber = htonl(sequencenumber);
				ack_now.f_offset = htonl(ftell(dest));

//				if((numBytes = sendto(recv_sockfd, &ack_now, sizeof(ack_t), 0,
//								 (struct sockaddr *)&peer_addr, peer_addr_len)) == -1)
//				{
//					perror("receiver: Send ack error 1 (LOSSY_STATE)");
//					exit(1);
//				}

				if((numBytes = sendto(recv_sockfd, &ack_now, sizeof(ack_t), 0,
									  &their_addr, their_addr_len)) == -1){
					perror("receiver: Send ack error 1 (LOSSY_STATE)");
					exit(1);
				}
				state = LOSSY_STATE;
				prev_state = LOSSY_STATE;
			}
				break;
		}
	}
	printf("Producer done and exiting\n");
	return NULL;
}

void init(char* udpPort) {
	setup_network(udpPort);
	tcp_handshake();
	sequencenumber = 0;
}

void reliablyReceive(char* udpPort, char* destinationFile){
	init(udpPort);
	pthread_t produceid;
	char* file = malloc(strlen(destinationFile) + 1);

	memset(file, 0, strlen(destinationFile)+1);
	memcpy(file, destinationFile, strlen(destinationFile));

	if (pthread_create(&produceid, NULL, &producer, file) < 0) {
		printf("PThread create error\n");
		perror("");
	}

	pthread_join(produceid, NULL);
	return;
}

int tcp_handshake(){
	return 0;
//	int numbytes;
//	char buf[4];
//	buf[3] = '\0';
//
//	if ((numbytes = recvfrom(recv_sockfd, buf, 3, 0, &their_addr, &their_addr_len)) == -1) {
//		perror("recvfrom");
//		exit(1);
//	}
//
//	if (memcmp(buf,"SYN", 3) != 0) {
//		printf("Received: %s\n", buf);
//		return -1; // indicates termination
//	}
//
//	if ((numbytes = sendto(recv_sockfd, "ACK", 3, 0, &their_addr, their_addr_len)) == -1) {
//		perror("sender: sendto");
//		exit(1);
//	}
//	printf("receiver: tcp_handshake complete\n");
//	return 0;
}

int main(int argc, char** argv){
	if(argc != 3){
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	reliablyReceive(argv[1], argv[2]);
	return 0;
}