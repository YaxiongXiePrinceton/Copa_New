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
#include "ngscope_debug_def.h"
#include "ngscope_client.h"
#include "ngscope_ts.h"

#define BUFFSIZE 15000
#define DELAY_THD 2
#define OFFSET_SIZE 201

using namespace std;
bool go_exit = false;

ngscope_dci_CA_t dci_ca;
pthread_mutex_t dci_mutex = PTHREAD_MUTEX_INITIALIZER;
ngscope_reordering_buf_t reTx_buffer;
int dci_pkt_offset =0;
uint64_t last_tx_pkt_t_us;

/* Logging system */
client_fd_t client_fd;

bool sock_ready = false;

//UDPSocket sender_socket;
//pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;
//
//packet_node* pkt_list;
//pthread_mutex_t pkt_mutex = PTHREAD_MUTEX_INITIALIZER;
// used to lock socket used to listen for packets from the sender
//mutex socket_lock; 

void sig_int_handler(int signo)
{
  printf("SIGINT received. Exiting...\n");
  if (signo == SIGINT) {
    go_exit = true;
  } else if (signo == SIGSEGV) {
    exit(1);
  }
}

void echo_packets(UDPSocket &sender_socket) {
//void echo_packets() {
	char buff[BUFFSIZE];
	sockaddr_in sender_addr;

	// init the file descriptor
	ngscope_debug_client_fd_init(&client_fd);

	ngscope_reTx_init_buf(&reTx_buffer);

	// Building the connection between this program and the NG-Scope 
	//int ngscope_server_sock, ngscope_client_sock;
	//int portNum = 6767;
	//// accept the connection from the clients
	//int nof_sock = accept_slave_connect(&ngscope_server_sock, &ngscope_client_sock, portNum);
    //printf("\n %d ngscope client connected\n\n", nof_sock);

	// Create the thread that receives CSI from NG-Scope-USRP
    //_ue_dci_status.remote_sock = _ngscope_client_sock;
	pthread_t ngscope_recv_t;
    //pthread_create(&ngscope_recv_t, NULL, ngscope_dci_recv_udp_thread, (void *)&ngscope_client_sock);
    pthread_create(&ngscope_recv_t, NULL, ngscope_dci_recv_udp_thread, NULL);
	sleep(3);

	while(!go_exit){
		if(sock_ready){
			break;
		}else{
			sleep(1);
		}
	}

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

	last_tx_pkt_t_us = timestamp_ns() / 1000;

	while (!go_exit) {
		int received __attribute((unused)) = -1;
		/* We try to receive the packet without blocking */
		received = sender_socket.receivedata_w_time(buff, BUFFSIZE, 0, &recv_time_ns, sender_addr);

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

			if(CLIENT_RX){
				fprintf(client_fd.fd_client_rx, "%ld\t%ld\t%d\n", recv_time_ns, oneway_ns, header->seq_num);
			}

			// Make a copy of the received DCI
			copy_dci_data_from_struct(dci_all, recent_dl_reTx_us, recent_ul_reTx_us, &new_dl_reTx, &new_ul_reTx, &nof_cell);

			/**************** Enque the packets ****************/
			enqueue_pkt(pkt_list, header, received, recv_time_ns);
			
			/****************   handling recevied pkts ****************/
			pkt_handling(dci_all, pkt_list, new_dl_reTx, nof_cell);
		}
		send_pkt_list(sender_socket, &sender_addr, pkt_list);
	}
	go_exit = true;

	printf("get out of whiel !\n");
	pthread_join(ngscope_recv_t, NULL);

	//close(ngscope_server_sock);
	//close(ngscope_client_sock);

	ngscope_debug_client_fd_close(&client_fd);
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
