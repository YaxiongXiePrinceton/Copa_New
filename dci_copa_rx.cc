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
#include "ngscope_packet_list.h"
#include "ngscope_sync.h"
#include "ngscope_sock.h"
#include "ngscope_reTx.h"
#include "ngscope_dci.h"
#include "ngscope_dci_recv.h"

#define BUFFSIZE 15000
#define DELAY_THD 2
#define OFFSET_SIZE 201

using namespace std;
bool go_exit = false;

ngscope_dci_CA_t dci_ca;
pthread_mutex_t dci_mutex = PTHREAD_MUTEX_INITIALIZER;


//UDPSocket sender_socket;
//pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;
//
//packet_node* pkt_list;
//pthread_mutex_t pkt_mutex = PTHREAD_MUTEX_INITIALIZER;


ngscope_reordering_buf_t reTx_buffer;


int dci_pkt_offset =0;

// used to lock socket used to listen for packets from the sender
//mutex socket_lock; 

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

	node                        = ngscope_list_createNode();
	pkt_header.sequence_number  = header->seq_num;
	pkt_header.ack_number       = 0;
	pkt_header.sent_timestamp   = header->tx_timestamp;
	pkt_header.sender_id        = header->src_id;
	pkt_header.recv_len_byte    = received;
	node->pkt_header 	    	= pkt_header;	

	memcpy(&node->tcp_header, header, sizeof(TCPHeader));

	uint64_t oneway_ns 	=  recv_time_ns - header->tx_timestamp;

	node->recv_t_us         = recv_time_ns / 1000; // ns -> us
	node->pkt_header        = pkt_header;
	node->oneway_us         = oneway_ns / 1000;  // ns -> us
	node->oneway_us_new     = oneway_ns / 1000;  // ns -> us
	node->revert_flag       = false;
	node->burst_start       = false;
	node->acked       		= false;
	//printf("before insertnode !\n");
	ngscope_list_insertNode_checkTime(pkt_list, node, fd_delay);
	//printf("after insertnode !\n");
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


//void* ngscope_pkt_send_thread(void* p){
//	char buff[BUFFSIZE];
//	sockaddr_in* sender_addr = (sockaddr_in*)p;
//
//	/***  Packet handling to get the packcets **/
//
//	/*** Socket handling to transmit the packets **/
//	pthread_mutex_lock(&socket_mutex);
//	sender_socket.senddata(buff, sizeof(TCPHeader), sender_addr);
//	pthread_mutex_unlock(&socket_mutex);
//
//
//    pthread_exit(NULL);
//}

bool tx_pkt_ornot(packet_node* p){
	uint64_t reTx_dci_t_us = reTx_buffer.buf_last_t;
	bool 	 reTx_empty = reTx_buffer.buf_size>0? 0:1;
	bool 	 reTx_ready = reTx_buffer.ready;

	// yes if the reTx is not ready 
	if(reTx_ready == 0){
		return true;
	} 

	// if this packet has been reverted
	if(p->revert_flag){
		return true;
	}

	// if the reTx buffer is empty, then highly likely we don't have any reTx left
	if(reTx_empty){
		if(p->recv_t_us > reTx_dci_t_us){
			return true;
		}
	}else{
		// if the buffer is not empty, then we are highly possible got something
		if((p->recv_t_us - reTx_dci_t_us) > 10000){
			return true;
		}
	}

	return false;	
}
void send_pkt_list(UDPSocket &sender_socket, sockaddr_in* sender_addr, packet_node* head){
    packet_node* p;
	char buff[BUFFSIZE];
    if(head->next == NULL){
        return;
    }
    p = head; 

	uint64_t curr_t_us = timestamp_ns() / 1000;
    while((p->next != NULL) && !go_exit){
        p = p->next;
		// only handle the un-acked packets
		if(p->acked == false){
			if(tx_pkt_ornot(p)){
				// adjust the delay
				int adjust_us = (int)(p->oneway_us_new - p->oneway_us) - (int)(curr_t_us - p->recv_t_us) ;
				printf("pkt_seq: %d adjust us:%d \n", p->pkt_header.sequence_number, adjust_us);
				p->tcp_header.adjust_us = adjust_us;
				p->tcp_header.tx_timestamp 	= curr_t_us * 1000;
				memcpy(buff, &(p->tcp_header), sizeof(TCPHeader));
				sender_socket.senddata(buff, sizeof(TCPHeader), sender_addr);
				p->acked = true;
			}
		}
    }

	return;
}
/* We copy data from the struct dci_ca ---> dci_all */
bool copy_dci_data_from_struct(ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV], 
					uint64_t recent_dl_reTx_us[MAX_NOF_RF_DEV],
					uint64_t recent_ul_reTx_us[MAX_NOF_RF_DEV],
					bool* new_dl_reTx_v,
					bool* new_ul_reTx_v,
					int* nof_cell){
	bool new_dl_reTx = false;
	bool new_ul_reTx = false;

	/*************** Here we copy the dci data ***************/
	pthread_mutex_lock(&dci_mutex);
	// Check whether we found new reTx in both dl and ul
	new_dl_reTx = ngscope_dci_new_dl_reTx(&dci_ca, recent_dl_reTx_us);
	new_ul_reTx = ngscope_dci_new_ul_reTx(&dci_ca, recent_ul_reTx_us);

	// if we found any reTx
	if(new_dl_reTx || new_ul_reTx){
		ngscope_dci_extract_multi_cell(dci_all, dci_ca);
	}

	// if we found dl reTx, update the recent dl reTx
	if(new_dl_reTx){
		printf("new DL RETX!\n");
		ngscope_dci_copy_dl_reTx(&dci_ca, recent_dl_reTx_us);
	}

	// if we found ul reTx, update the recent ul reTx
	if(new_ul_reTx ){
		//printf("new ul RETX!\n");
		ngscope_dci_copy_ul_reTx(&dci_ca, recent_ul_reTx_us);
	}
	*nof_cell = dci_ca.nof_cell;
	pthread_mutex_unlock(&dci_mutex);

	*new_dl_reTx_v = new_dl_reTx;
	*new_ul_reTx_v = new_ul_reTx;

	if(new_dl_reTx || new_ul_reTx)
		return true;
	else 
		return false;
}

int  pkt_handling(ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV],
					packet_node* 		pkt_list, 
					bool 				new_dl_reTx,
					int  				nof_cell){
	uint64_t* pkt_reTx_rx_t_us;  // time of reTx before pruning of intersections
	uint64_t* pkt_rx_t_us; 
	uint64_t* pkt_oneway_us; 

	uint64_t* dci_reTx_rx_t_us;  // time of reTx before pruning of intersections
	uint16_t* dci_reTx_tti;  // time of reTx before pruning of intersections

	// get the retransmission from the received packets
	int nof_reTx_pkt = 0;  
	int nof_reTx_dci = 0;  
	int nof_pkt 	 = 0;

	int dci_pkt_offset =0;

	int offset_vec[OFFSET_SIZE] = {0};
    for(int i=0; i<= 200; i++){
        offset_vec[i]   = (i-100) * 100;
	}

	nof_pkt = ngscope_list_length(pkt_list); 

	//Extract the pkt recv time and the pkt recv time of the retransmission
	pkt_rx_t_us 		= (uint64_t *)malloc(nof_pkt * sizeof(uint64_t));
	pkt_oneway_us 		= (uint64_t *)malloc(nof_pkt * sizeof(uint64_t));

	// extract the data from the pkt list
	pkt_reTx_rx_t_us 	= ngscope_list_get_reTx(pkt_list, pkt_rx_t_us, pkt_oneway_us, nof_pkt, &nof_reTx_pkt);

	if(new_dl_reTx){
		dci_reTx_rx_t_us 	= ngscope_dci_get_reTx_t(dci_all, nof_cell, &nof_reTx_dci); // NOTE: we allocate the memory there remember to free
		dci_reTx_tti 		= ngscope_dci_get_reTx_tti(dci_all, nof_cell, &nof_reTx_dci); // NOTE: we allocate the memory there remember to free

		dci_pkt_offset = ngscope_sync_dci_pkt(pkt_reTx_rx_t_us, dci_reTx_rx_t_us, nof_reTx_pkt, nof_reTx_dci, offset_vec, OFFSET_SIZE);

		// we must free the allocated array 
		free(dci_reTx_rx_t_us);
		free(dci_reTx_tti);
	}		

	// adjust the timestamp according to the offset
	ngscope_dci_sync_multi_cell(dci_all, nof_cell, dci_pkt_offset);

	// update the reTx buffer
	if(new_dl_reTx){
		ngscope_reTx_update_buf(&reTx_buffer, dci_all, nof_cell);
		printf("reTx buffer start:%d end:%d size:%ld active:%d recent_dci:%d\n", reTx_buffer.buf_start_tti, reTx_buffer.buf_last_tti, \
				reTx_buffer.buf_size, reTx_buffer.buffer_active, dci_all[0]->dl_dci.reTx_tti[dci_all[0]->dl_dci.nof_reTx-1]);
	}

	// if our reTx buffer hasn't found the starting pkt yet, e.g., a new reTx found
	// then we need to try to find the starting packet
	if(reTx_buffer.ready && reTx_buffer.buffer_active && !reTx_buffer.start_pkt_found){
		int index= ngscope_reTx_find_burst_start(pkt_rx_t_us, pkt_oneway_us, nof_pkt, reTx_buffer.buf_start_t);
		if(index > 0){
			printf("found pkt start! index:%d nof_pkt:%d\n", index, nof_pkt);
			reTx_buffer.start_pkt_found = true;
			reTx_buffer.start_pkt_t 	= pkt_rx_t_us[index];
			reTx_buffer.ref_oneway 		= ngscope_list_ave_oneway(pkt_oneway_us, nof_pkt, index, 5);
			printf("Delay ref done\n");
		}
	}

	// reverse the packet delay in the list if the reTx packet is not empty
	if(reTx_buffer.ready && reTx_buffer.buffer_active && reTx_buffer.start_pkt_found){
		ngscope_list_revert_delay(pkt_list, &reTx_buffer);
	}

	free(pkt_reTx_rx_t_us);
	free(pkt_rx_t_us);
	free(pkt_oneway_us);

	return 0;
}

void NAT_punching(UDPSocket &sender_socket, chrono::high_resolution_clock::time_point start_time_point){
	char buff[BUFFSIZE];
	// ** NAT Punching related **/
	//chrono::high_resolution_clock::time_point start_time_point = 
//		chrono::high_resolution_clock::now();
	TCPHeader *header = (TCPHeader*)buff;
	header->receiver_timestamp = \
		chrono::duration_cast<chrono::duration<double>>(
			chrono::high_resolution_clock::now() - start_time_point
		).count()*1000; //in milliseconds
	printf("SIZE of TCP header:%ld\n", sizeof(TCPHeader));
	// hardcode the AWS server address	
	sockaddr_in dest_addr;
	dest_addr.sin_port = htons(9004);

	if (inet_aton("3.22.79.149", &dest_addr.sin_addr) == 0)
	{
		std::cerr<<"inet_aton failed while sending data. Code: "<<errno<<endl;
	}

	for(int i=0; i<5; i++){
		sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	}

	return;
}
// For each packet received, acks back the pseudo TCP header with the 
// current  timestamp
void echo_packets(UDPSocket &sender_socket) {
//void echo_packets() {
	char buff[BUFFSIZE];
	sockaddr_in sender_addr;

	FILE *fd_sync, *fd_offset, *fd_delay, *fd_ack;
	system("mkdir ./data");	

	fd_sync = fopen("./data/dci_sync_delay_sum","w+");
	fclose(fd_sync);
	fd_sync = fopen("./data/dci_sync_delay_sum","a+");

	fd_offset = fopen("./data/dci_sync_offset","w+");
	fclose(fd_offset);
	fd_offset = fopen("./data/dci_sync_offset","a+");

	fd_delay = fopen("./data/cleaned_delay","w+");
	fclose(fd_delay);
	fd_delay = fopen("./data/cleaned_delay","a+");

	fd_ack = fopen("./data/client_ack_log","w+");
	fclose(fd_ack);
	fd_ack = fopen("./data/client_ack_log","a+");

	ngscope_reTx_init_buf(&reTx_buffer);

	// Building the connection between this program and the NG-Scope 
	int ngscope_server_sock, ngscope_client_sock;
	int portNum = 6767;
	// accept the connection from the clients
	int nof_sock = accept_slave_connect(&ngscope_server_sock, &ngscope_client_sock, portNum);
    printf("\n %d ngscope client connected\n\n", nof_sock);

	// Create the thread that receives CSI from NG-Scope-USRP
    //_ue_dci_status.remote_sock = _ngscope_client_sock;
	pthread_t ngscope_recv_t;
    pthread_create(&ngscope_recv_t, NULL, ngscope_dci_recv_thread, (void *)&ngscope_client_sock);
	sleep(3);

	chrono::high_resolution_clock::time_point start_time_point = 
		chrono::high_resolution_clock::now();

	// NAT punching related functions
	NAT_punching(sender_socket, start_time_point);

	// init the packet list
	packet_node* pkt_list = ngscope_list_createNode();

	uint64_t recv_time_ns;
	int _pkt_received = 0;
	int nof_cell = 0;
	// the struct we use to store the dci messages
 	ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV];
	for(int i=0; i<MAX_NOF_RF_DEV; i++){
		dci_all[i] = (ngscope_dci_t*)malloc(sizeof(ngscope_dci_t));
	}

	uint64_t recent_dl_reTx_us[MAX_NOF_RF_DEV] = {0};
	uint64_t recent_ul_reTx_us[MAX_NOF_RF_DEV] = {0};

	//uint64_t dci_curr_time_us = 0;
	bool new_dl_reTx;
	bool new_ul_reTx;

	while (!go_exit) {
		int received __attribute((unused)) = -1;

		/* We try to receive the packet without blocking */
		received = sender_socket.receivedata_w_time(buff, BUFFSIZE, -1, &recv_time_ns, sender_addr);

		/* if we receive something */
		if(received > 0){
			
			// we receive one packet
			_pkt_received 		+= 1;
			TCPHeader *header 	= (TCPHeader*)buff;

			//printf("recv:%d\n", received);
			// get the current timestamp	
			header->receiver_timestamp = \
				chrono::duration_cast<chrono::duration<double>>(
					chrono::high_resolution_clock::now() - start_time_point ).count()*1000; //in milliseconds

			uint64_t oneway_ns  	=  recv_time_ns - header->tx_timestamp;

			fprintf(fd_ack, "%ld\t%ld\t%d\n", recv_time_ns, oneway_ns, header->seq_num);

			// Make a copy of the received DCI
			copy_dci_data_from_struct(dci_all, recent_dl_reTx_us, recent_ul_reTx_us, &new_dl_reTx, &new_ul_reTx, &nof_cell);

			/**************** Enque the packets ****************/
			enqueue_pkt(pkt_list, header, received, recv_time_ns, fd_delay);
			
			/****************   handling recevied pkts ****************/
			pkt_handling(dci_all, pkt_list, new_dl_reTx, nof_cell);
		}
		send_pkt_list(sender_socket, &sender_addr, pkt_list);
	}


	pthread_join(ngscope_recv_t, NULL);
	close(ngscope_server_sock);
	close(ngscope_client_sock);

	fclose(fd_delay);
	fclose(fd_sync);
	fclose(fd_offset);
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
	//echo_packets();

	return 0;
}
