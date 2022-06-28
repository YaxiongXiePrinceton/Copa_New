#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string.h>
#include <thread>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "tcp-header.hh"
#include "udp-socket.hh"
#include "packet_list.h"

#define BUFFSIZE 15000

using namespace std;

// used to lock socket used to listen for packets from the sender
mutex socket_lock; 

bool enqueue_pkt(packet_node* pkt_list, TCPHeader* header, int received, uint64_t oneway_us, FILE* fd_delay){
	packet_node* node; 
	pkt_header_t pkt_header;

	node                        = createNode();

	//copy the TCP header 
	memcpy(&node->pkt_header, header, sizeof(TCPHeader));

	pkt_header.sequence_number  = header->seq_num;
	pkt_header.ack_number       = 0;
	pkt_header.sent_timestamp   = header->sender_timestamp;
	pkt_header.sender_id        = header->src_id;
	pkt_header.recv_len         = received;
	node->pkt_header 	    = pkt_header;	

	//printf("recv:%d\n", recv_len);
	node->recv_t_us         = header->receiver_timestamp / 1000; // ns -> us
	node->pkt_header        = pkt_header;
	node->oneway_us         = oneway_us ;  
	node->oneway_us_new     = oneway_us;  
	node->revert_flag       = false;
	insertNode_checkTime(pkt_list, node, fd_delay);
	return true;
}


// For each packet received, acks back the pseudo TCP header with the 
// current  timestamp
void echo_packets(UDPSocket &sender_socket) {
	char buff[BUFFSIZE];
	sockaddr_in sender_addr;

	system("mkdir ./data");	

	FILE* fd_delay;
	fd_delay = fopen("./data/cleaned_delay","w+");
    	fclose(fd_delay);
    	fd_delay = fopen("./data/cleaned_delay","a+");


	printf("echo packets!\n");
	chrono::high_resolution_clock::time_point start_time_point = \
		chrono::high_resolution_clock::now();

	TCPHeader *header = (TCPHeader*)buff;
	header->receiver_timestamp = \
		chrono::duration_cast<chrono::duration<double>>(
			chrono::high_resolution_clock::now() - start_time_point
		).count()*1000; //in milliseconds

	
	// hardcode the AWS server address	
	sockaddr_in dest_addr;
	dest_addr.sin_port = htons(9004);
    	if (inet_aton("3.22.79.149", &dest_addr.sin_addr) == 0)
    	{
		std::cerr<<"inet_aton failed while sending data. Code: "<<errno<<endl;
    	}
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);

	packet_node* pkt_list = createNode();
	packet_node* node;
	uint64_t recv_time_ns;
	while (1) {
		int received __attribute((unused)) = -1;
		while (received == -1) {
			//received = sender_socket.receivedata(buff, BUFFSIZE, -1, sender_addr);
			received = sender_socket.receivedata_w_time(buff, BUFFSIZE, -1, &recv_time_ns, sender_addr);
			assert( received != -1 );
		}

		TCPHeader *header = (TCPHeader*)buff;
		
		header->receiver_timestamp = \
			chrono::duration_cast<chrono::duration<double>>(
				chrono::high_resolution_clock::now() - start_time_point
			).count()*1000; //in milliseconds

		header->adjust_us = 222;

		uint64_t tx_timestamp = header->tx_timestamp;
	     	uint32_t lower_t  = tx_timestamp & 0xFFFFFFFF;
      		lower_t = ntohl(lower_t);
      		uint32_t upper_t  = (tx_timestamp >> 32) & 0xFFFFFFFF;
      		upper_t = ntohl(upper_t);
		uint64_t tx_time_ns = (uint64_t)lower_t + ((uint64_t)upper_t << 32);

		pkt_header_t pkt_header;
		node                        = createNode();
		pkt_header.sequence_number  = header->seq_num;
		pkt_header.ack_number       = 0;
		pkt_header.sent_timestamp   = header->tx_timestamp;
		pkt_header.sender_id        = header->src_id;
		pkt_header.recv_len         = received;
		node->pkt_header 	    = pkt_header;	

		memcpy(&node->pkt_header, header, sizeof(TCPHeader));

		uint64_t oneway_ns = tx_time_ns - recv_time_ns;
		std::cout << "seq:" << header->seq_num << " tx timestamp: " << header->tx_timestamp 
			<< " tx timestamp ns: " << tx_time_ns  << "rx timestamp:" << recv_time_ns << endl;

		node->recv_t_us         = recv_time_ns / 1000; // ns -> us
		node->pkt_header        = pkt_header;
		node->oneway_us         = oneway_ns / 1000;  // ns -> us
		node->oneway_us_new     = oneway_ns / 1000;  // ns -> us
		node->revert_flag       = false;
		insertNode_checkTime(pkt_list, node, fd_delay);

            	int  listLen = listLength(pkt_list);
		std::cout<< "rx t:" << header->receiver_timestamp << " tx: "<< header->sender_timestamp
			<< " oneway us:" << oneway_ns / 1000 << " listLen: "<< listLen << endl; 
		sender_socket.senddata(buff, sizeof(TCPHeader), &sender_addr);
	}
	//free(pkt_list);
    	fclose(fd_delay);
}


int main(int argc, char* argv[]) {
	int port = 9004;
	if (argc == 2)
		port = atoi(argv[1]);

	UDPSocket sender_socket;
	sender_socket.bindsocket(port);
	
	//thread nat_thread(punch_NAT, nat_ip_addr, ref(sender_socket));
	echo_packets(sender_socket);

	return 0;
}
