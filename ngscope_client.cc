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
#include "ngscope_ts.h"

using namespace std;

#define BUFFSIZE 15000
#define DELAY_THD 2
#define OFFSET_SIZE 201

extern client_fd_t client_fd;
extern bool go_exit;

extern ngscope_dci_CA_t dci_ca;
extern pthread_mutex_t dci_mutex;
extern ngscope_reordering_buf_t reTx_buffer;
extern int dci_pkt_offset;
extern uint64_t last_tx_pkt_t_us;

bool found_reTx = false;
bool Missing_DL = false;
int stat_miss_reTx = 0;

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
				//printf("pkt_seq: %d adjust us:%d \n", p->pkt_header.sequence_number, adjust_us);
				p->tcp_header.adjust_us = adjust_us;
				p->tcp_header.tx_timestamp 	= curr_t_us * 1000;
				memcpy(buff, &(p->tcp_header), sizeof(TCPHeader));
				sender_socket.senddata(buff, sizeof(TCPHeader), sender_addr);
				p->acked = true;
				last_tx_pkt_t_us = curr_t_us;
			}
		}
    }
	/* Let's make the packet transmission more consist: transmit 1 packet if we haven't send any 
		pkts in the past 1 ms */
	//printf("time diff: %ld\n", curr_t_us - last_tx_pkt_t_us);
	if(curr_t_us - last_tx_pkt_t_us >= 1000){ // 1ms
		TCPHeader header; 		
		header.seq_num 			= -1111;
		header.flow_id 			= 0;
		header.src_id 			= 0;
		header.tx_timestamp 	= curr_t_us;
		memcpy(buff, &(header), sizeof(TCPHeader));
		sender_socket.senddata(buff, sizeof(TCPHeader), sender_addr);
		last_tx_pkt_t_us = curr_t_us;
		//printf("send 1 pkt!\n");
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
		//printf("new DL RETX!\n");
		ngscope_dci_copy_dl_reTx(&dci_ca, recent_dl_reTx_us);
		
		printf("new DL reTx ");
		for(int i=0; i<dci_ca.nof_cell; i++){
			printf("%d ",dci_ca.cell_dci[i].recent_dl_reTx_tti);
		}
		printf("\n");
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

	// update the reTx buffer if 1, we found a new dl reTx 2, there is no active buffer and we missed some dl_reTx
	if(new_dl_reTx || (Missing_DL  && reTx_buffer.ready && reTx_buffer.buffer_active == false)){
		found_reTx = ngscope_reTx_update_buf(&reTx_buffer, dci_all, nof_cell);
		printf("curr_tti:%d reTx buffer start:%d end:%d size:%ld active:%d start_pkt_found:%d recent_dci:%d\n\n", dci_all[0]->ca_header, reTx_buffer.buf_start_tti, 
		reTx_buffer.buf_last_tti, reTx_buffer.buf_size, reTx_buffer.buffer_active, reTx_buffer.start_pkt_found, dci_all[0]->dl_dci.reTx_tti[dci_all[0]->dl_dci.nof_reTx-1]);
		if(found_reTx == false){
			Missing_DL = true;
		}else{
			Missing_DL = false;
		}
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
		}else{
			// we need to timeout if we cannot find the pkt_start
			int tti_diff = tti_distance(reTx_buffer.buf_start_tti, dci_all[0]->ca_header);
			if(tti_diff > 20){
				ngscope_reTx_reset_buf(&reTx_buffer);
				stat_miss_reTx++;
				printf("buf_start_tti:%d ca_header:%d missing reTx:%d \n", reTx_buffer.buf_start_tti, dci_all[0]->ca_header, stat_miss_reTx);
			}
		}
	}

	// reverse the packet delay in the list if the reTx packet is not empty
	if(reTx_buffer.ready && reTx_buffer.buffer_active && reTx_buffer.start_pkt_found){
		ngscope_list_revert_delay(pkt_list, &reTx_buffer);
		//printf("list reverse done!\n");
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

