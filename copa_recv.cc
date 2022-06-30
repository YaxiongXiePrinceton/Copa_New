#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <signal.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "tcp-header.hh"
#include "udp-socket.hh"
#include "packet_list.h"
#include "ngscope_sync.h"
#include "ngscope_sock.h"

#define BUFFSIZE 15000
#define DELAY_THD 2
using namespace std;
bool go_exit = false;

ue_dci_t    dci_vec[NOF_LOG_DCI];
int         dci_header  = 0;
int         nof_dci     = 0;
pthread_mutex_t dci_mutex = PTHREAD_MUTEX_INITIALIZER;


// used to lock socket used to listen for packets from the sender
mutex socket_lock; 
uint64_t timestamp_ns()
{
  struct timespec ts;

  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;
  return ret;
}

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

bool enqueue_pkt(packet_node* pkt_list, TCPHeader* header, int received, uint64_t recv_time_ns, FILE* fd_delay){
	packet_node* node; 
	pkt_header_t pkt_header;

	node                        = createNode();
	pkt_header.sequence_number  = header->seq_num;
	pkt_header.ack_number       = 0;
	pkt_header.sent_timestamp   = header->tx_timestamp;
	pkt_header.sender_id        = header->src_id;
	pkt_header.recv_len         = received;
	node->pkt_header 	    = pkt_header;	

	memcpy(&node->tcp_header, header, sizeof(TCPHeader));

	uint64_t oneway_ns 	=  recv_time_ns - header->tx_timestamp;

	node->recv_t_us         = recv_time_ns / 1000; // ns -> us
	node->pkt_header        = pkt_header;
	node->oneway_us         = oneway_ns / 1000;  // ns -> us
	node->oneway_us_new     = oneway_ns / 1000;  // ns -> us
	node->revert_flag       = false;
	node->acked       	= false;
	insertNode_checkTime(pkt_list, node, fd_delay);
	return true;
}
void send_pkt_w_time(UDPSocket &sender_socket, sockaddr_in* sender_addr, packet_node* head, uint64_t timestamp){
	packet_node* p;
	if(head->next == NULL){
		return;
	}
	p = head;
	while(p->next != NULL){
		p = p->next;
		//printf("recv_t: %ld time_anchor:%ld \n", p->recv_t_us, timestamp);
		if( (p->acked == false) && p->recv_t_us < (timestamp - DELAY_THD)){
			int offset 	= (int)((long)p->oneway_us - (long)p->oneway_us_new);
			p->tcp_header.adjust_us = offset;
			sender_socket.senddata((char *)&p->tcp_header, sizeof(TCPHeader), sender_addr); 
			p->acked  = true;
		}
	}
	return;
}


// For each packet received, acks back the pseudo TCP header with the 
// current  timestamp
void echo_packets(UDPSocket &sender_socket) {
	char buff[BUFFSIZE];
	sockaddr_in sender_addr;
	
	FILE *fd_ack;
	fd_ack = fopen("./data/client_ack_log","w+");
        fclose(fd_ack);
        fd_ack = fopen("./data/client_ack_log","a+");

	uint64_t dci_recv_t_us[NOF_LOG_DCI];
	uint8_t  dci_reTx[NOF_LOG_DCI];
	uint32_t dci_tbs[NOF_LOG_DCI];
	uint16_t dci_tti[NOF_LOG_DCI];
	printf("echo packets!\n");

	// ** NAT Punching related **/
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
	
        uint64_t recv_time_ns;
	while (1) {
		if(go_exit) break;
		int received __attribute((unused)) = -1;
		while (received <= 0) {
			if(go_exit) break;
			//received = sender_socket.receivedata(buff, BUFFSIZE, -1, sender_addr);
			received = sender_socket.receivedata_w_time(buff, BUFFSIZE, 1000, &recv_time_ns, sender_addr);
			assert( received != -1 );
		}

		TCPHeader *header = (TCPHeader*)buff;
		
		header->receiver_timestamp = \
			chrono::duration_cast<chrono::duration<double>>(
				chrono::high_resolution_clock::now() - start_time_point
			).count()*1000; //in milliseconds
		
		sender_socket.senddata(buff, sizeof(TCPHeader), &sender_addr);
		uint64_t oneway_ns      =  recv_time_ns - header->tx_timestamp;
		fprintf(fd_ack, "%ld\t%ld\t%d\n", recv_time_ns, oneway_ns, header->seq_num);
	}
	fclose(fd_ack);
}


int main(int argc, char* argv[]) {
	int port = 9004;
	if (argc == 2)
		port = atoi(argv[1]);

	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigprocmask(SIG_UNBLOCK, &sigset, NULL);
	signal(SIGINT, sig_int_handler);


	UDPSocket sender_socket;
	sender_socket.bindsocket(port);
	
	//thread nat_thread(punch_NAT, nat_ip_addr, ref(sender_socket));
	echo_packets(sender_socket);

	return 0;
}
