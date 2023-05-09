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

#include "ngscope_ts.h"
#include "ngscope_debug_def.h"

client_fd_t client_fd;


#define BUFFSIZE 15000
#define DELAY_THD 2

using namespace std;
bool go_exit = false;

bool sock_ready = false;
ngscope_dci_CA_t dci_ca;
pthread_mutex_t dci_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

ngscope_reordering_buf_t reTx_buffer;
int dci_pkt_offset =0;
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

bool enqueue_pkt(packet_node* pkt_list, TCPHeader* header, int received, uint64_t recv_time_ns){
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
	ngscope_list_insertNode_checkTime(pkt_list, node);
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


// For each packet received, acks back the pseudo TCP header with the 
// current  timestamp
void echo_packets(UDPSocket &sender_socket) {
	char buff[BUFFSIZE];
	sockaddr_in sender_addr;

	FILE *fd_sync, *fd_offset, *fd_delay, *fd_ack;
	system("mkdir ./data");	

	// init the file descriptor
	ngscope_debug_client_fd_init(&client_fd);


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

//	uint64_t dci_recv_t_us[NOF_LOG_DCI];
//	uint8_t  dci_reTx[NOF_LOG_DCI];
//	uint32_t dci_tbs[NOF_LOG_DCI];
//	uint16_t dci_tti[NOF_LOG_DCI];
 
	int ngscope_server_sock, ngscope_client_sock;
	int portNum = 6767;
	pthread_t ngscope_recv_t;

	// accept the connection from the clients
	int nof_sock = accept_slave_connect(&ngscope_server_sock, &ngscope_client_sock, portNum);
    printf("\n %d ngscope client connected\n\n", nof_sock);

    //_ue_dci_status.remote_sock = _ngscope_client_sock;
    pthread_create(&ngscope_recv_t, NULL, ngscope_dci_recv_thread, (void *)&ngscope_client_sock);
	sleep(3);

	printf("echo packets!\n");

	// ** NAT Punching related **/
	chrono::high_resolution_clock::time_point start_time_point = \
		chrono::high_resolution_clock::now();
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
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);

	packet_node* pkt_list = ngscope_list_createNode();
	//packet_node* node;

//	int _last_reTx_idx = -1;
//    int _last_reTx_tti = 0; 
//	long tbs_res = 0;
//	int  last_delay_anchor = -1;
//	uint64_t curr_time, prev_time;
	uint64_t recv_time_ns;
	int _pkt_received = 0;

	// prepare the offset
	//int offset 	= 0; // used for storing the offset(result) 
	int offset_size = 201;
	int offset_vec[201] = {0};
    for(int i=0; i<= 200; i++){
        offset_vec[i]   = (i-100) * 100;
	}

	int nof_cell = 0;
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
		//uint64_t t1, t2;
		//t1 = timestamp_ns();

		while ( (received <= 0) && !go_exit) {
			//received = sender_socket.receivedata(buff, BUFFSIZE, -1, sender_addr);
			received = sender_socket.receivedata_w_time(buff, BUFFSIZE, 1000, &recv_time_ns, sender_addr);
			assert( received != -1 );
		}
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

		// Insert the packet into the list
		enqueue_pkt(pkt_list, header, received, recv_time_ns);

		/*************** Here we copy the data ***************/
		//uint64_t time_anchor = 0;
		pthread_mutex_lock(&dci_mutex);
		//printf("Enter the loop --> reTx idx:%d tti:%d \n", _last_reTx_idx, _last_reTx_tti);
		new_dl_reTx = ngscope_dci_new_dl_reTx(&dci_ca, recent_dl_reTx_us);
		if(new_dl_reTx ){
			//printf("new RETX!\n");
 			ngscope_dci_extract_multi_cell(dci_all, dci_ca);
			ngscope_dci_copy_dl_reTx(&dci_ca, recent_dl_reTx_us);
		}

		new_ul_reTx = ngscope_dci_new_dl_reTx(&dci_ca, recent_ul_reTx_us);
		if(new_ul_reTx ){
			printf("new ul RETX!\n");
			ngscope_dci_copy_ul_reTx(&dci_ca, recent_ul_reTx_us);
		}

		nof_cell = dci_ca.nof_cell;
		//dci_curr_time_us = dci_ca.curr_time;

		//time_anchor 	= dci_ca.cell_dci[0].dci[dci_ca.cell_dci[0].header - 1].time_stamp; 
		pthread_mutex_unlock(&dci_mutex);

		//printf("dci:%ld recv:%ld diff:%ld ms\n", dci_curr_time_us, recv_time_ns/1000, abs((long)(dci_curr_time_us - recv_time_ns/1000) /1000));

		uint64_t* pkt_reTx_rx_t_us;  // time of reTx before pruning of intersections
		uint64_t* pkt_rx_t_us; 
		uint64_t* pkt_oneway_us; 

		uint64_t* dci_reTx_rx_t_us;  // time of reTx before pruning of intersections
		uint16_t* dci_reTx_tti;  // time of reTx before pruning of intersections

		// get the retransmission from the received packets
		int nof_reTx_pkt = 0;  
		int nof_reTx_dci = 0;  


		int nof_pkt = ngscope_list_length(pkt_list); 
		//Extract the pkt recv time and the pkt recv time of the retransmission
		pkt_rx_t_us 		= (uint64_t *)malloc(nof_pkt * sizeof(uint64_t));
		pkt_oneway_us 		= (uint64_t *)malloc(nof_pkt * sizeof(uint64_t));

		pkt_reTx_rx_t_us 	= ngscope_list_get_reTx(pkt_list, pkt_rx_t_us, pkt_oneway_us, nof_pkt, &nof_reTx_pkt);
   
		if(new_dl_reTx){
			//ngscope_dci_print_single_cell(dci_all[0]);

			dci_reTx_rx_t_us 	= ngscope_dci_get_reTx_t(dci_all, nof_cell, &nof_reTx_dci);
			dci_reTx_tti 		= ngscope_dci_get_reTx_tti(dci_all, nof_cell, &nof_reTx_dci);

			dci_pkt_offset = ngscope_sync_dci_pkt(pkt_reTx_rx_t_us, dci_reTx_rx_t_us, nof_reTx_pkt, nof_reTx_dci, offset_vec, offset_size);

			ngscope_reTx_update_buf(&reTx_buffer, dci_all, nof_cell);
			fprintf(fd_offset, "%d\n", dci_pkt_offset);

			//for(int i=0; i<nof_reTx_pkt;i++){
			//	fprintf(fd_sync, "%ld\t", pkt_reTx_rx_t_us[i]);
			//}
			//fprintf(fd_sync,"\n"); 

			//for(int i=0; i<dci_all[0]->dl_dci.nof_reTx; i++){
			//	fprintf(fd_sync, "%ld\t", dci_all[0]->dl_dci.reTx_t_us[i]);
			//}
			//fprintf(fd_sync,"\n"); 

			////offset = ngscope_sync_dci_pkt(dci_all, pkt_list, nof_cell, fd_sync); 
			//fprintf(fd_offset, "%d\t%d\t%d\n", offset, nof_reTx_pkt, nof_reTx_dci);

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

		if(reTx_buffer.ready && reTx_buffer.buffer_active && reTx_buffer.start_pkt_found){
			ngscope_list_revert_delay(pkt_list, &reTx_buffer);
		}

		free(pkt_reTx_rx_t_us);
		free(pkt_rx_t_us);
		free(pkt_oneway_us);

		//t2 = timestamp_ns();
		//printf("time spend:%ld ms\n", (t2-t1)/1000000);
//		printf(" tti: dci_header:%d nof_dci:%d\n",dci_header, nof_dci);
//		if(tbs_res > 0){
//			tbs_res = revert_last_pkt_reTx(pkt_list, tbs_res);
//		}else{
//			if(new_reTx && _pkt_received > 100){
//				offset = ngscope_sync_dci_delay(delay_diff_ms, recv_t_us, 
//						listLen, dci_reTx, dci_recv_t_us, logged_dci, fd_sync);
//				//printf("Time on ngscope_sync_dci_delay:%ld\n", (t2-t1) / 1000);
//				//fprintf(fd_offset, "%d\n", offset);
//				long tbs_out = 0;
//				int new_delay_anchor = 0;
//				int tmp_idx = ngscope_sync_oneway_revert(pkt_list, listLen, dci_recv_t_us, dci_tbs, 
//                        		dci_reTx, dci_tti, tbs_res, &tbs_out, last_delay_anchor, &new_delay_anchor, 
//					_last_reTx_tti, offset, dci_header, nof_dci, fd_offset);
//				last_delay_anchor = new_delay_anchor;
//				printf("tmp_idx:%d\n", tmp_idx);
//				tbs_res = tbs_out;
//				if(tmp_idx > 0){
//					_last_reTx_idx = (dci_header + tmp_idx) % NOF_LOG_DCI;
//					_last_reTx_tti = dci_tti[tmp_idx];
//				}
//				//printf("dci_header:%d after revert, reTx idx: %d reTx tti: %d\n", dci_header, _last_reTx_idx, _last_reTx_tti)
//				//_last_reTx_tti = ; // update tti
//			}
//		}
//		//printf("time_anchor:%ld offset:%d\n", time_anchor, 

		//time_anchor += offset;
		//send_pkt_w_time(sender_socket, &sender_addr, pkt_list, time_anchor);	
		
		sender_socket.senddata(buff, sizeof(TCPHeader), &sender_addr);
	}

	ngscope_debug_client_fd_init(&client_fd);

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

	return 0;
}
