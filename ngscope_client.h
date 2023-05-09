#ifndef NGSCOPE_CLIENT_HH
#define NGSCOPE_CLIENT_HH
#include <unistd.h>
#include <stdint.h>
#include "tcp-header.hh"
#include "udp-socket.hh"
#include "ngscope_packet_list.h"
#include "ngscope_sync.h"
#include "ngscope_sock.h"
#include "ngscope_reTx.h"
#include "ngscope_dci.h"
#include "ngscope_dci_recv.h"
#include "ngscope_debug_def.h"


using namespace std;
void NAT_punching(UDPSocket &sender_socket, chrono::high_resolution_clock::time_point start_time_point);


bool enqueue_pkt(packet_node* pkt_list, TCPHeader* header, int received, uint64_t recv_time_ns);
void send_pkt_list(UDPSocket &sender_socket, sockaddr_in* sender_addr, packet_node* head);

bool copy_dci_data_from_struct(ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV], 
					uint64_t recent_dl_reTx_us[MAX_NOF_RF_DEV],
					uint64_t recent_ul_reTx_us[MAX_NOF_RF_DEV],
					bool* new_dl_reTx_v,
					bool* new_ul_reTx_v,
					int* nof_cell);
	
int  pkt_handling(ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV],
					packet_node* 		pkt_list, 
					bool 				new_dl_reTx,
					int  				nof_cell);

#endif
