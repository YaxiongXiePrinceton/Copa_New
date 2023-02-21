#ifndef NGSCOPE_RETX_HH
#define NGSCOPE_RETX_HH
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>


#include "ngscope_dci.h"
#define ALIGN_T_THD 4000 // 4000 us
#define TIME_DIFF_THD 6500
#define ONEWAY_DIFF_THD 3500

#define BURST_FIND_SEARCH_PKT 8
typedef struct{
	bool*     reTx;
	uint16_t* tti;
	uint32_t* tbs;
	uint64_t* rx_t_us;
	int 	  size;
}ngscope_reTx_ca_t;

typedef struct{
	bool 	 ready; // set to false if we never seen any dci reTx 

	uint64_t start_pkt_t;
	bool 	 start_pkt_found; 

	bool 	 buffer_active;
	long 	 buf_size;
	uint16_t buf_start_tti;
	uint16_t buf_last_tti;
	uint64_t buf_start_t;
	uint64_t buf_last_t;

	
	uint64_t ref_oneway;
	uint64_t most_recent_dci_t;
}ngscope_reordering_buf_t;

typedef struct{
	bool 		ready;
	uint16_t 	tti;
}ngscope_dci_tracker_t;

int ngscope_reTx_init_buf(ngscope_reordering_buf_t* q);
int ngscope_reTx_reset_buf(ngscope_reordering_buf_t* q);

int ngscope_reTx_update_buf(ngscope_reordering_buf_t* q,
							ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV],
							int nof_cell);
int ngscope_reTx_update_ul_buf(ngscope_reordering_buf_t* q,
							ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV],
							int nof_cell);


// we try to identify the starting point of the burst inside the packet statistics 
int ngscope_reTx_find_burst_start(uint64_t* pkt_t_us, uint64_t* oneway, int pkt_num, uint64_t dci_reTx_t_us);

#endif
