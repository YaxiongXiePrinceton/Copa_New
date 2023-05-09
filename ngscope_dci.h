#ifndef NGSCOPE_DCI_HH
#define NGSCOPE_DCI_HH
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


#define NOF_LOG_DCI 1000
#define MAX_NOF_RF_DEV 4

// This structure is used for the DCI exchange between NG-Scope and the app receiver
typedef struct{
    uint64_t time_stamp;
    uint16_t tti;

    uint32_t dl_tbs;
    uint8_t  dl_reTx;
	bool 	 dl_rv_flag;

    uint32_t ul_tbs;
    uint8_t  ul_reTx;
	bool 	 ul_rv_flag;
}ue_dci_t;


/* The following three structures are mainly used inside the dci_synchronization */

// The structure that contains all downlink retransmissions and the timestamps of the retransmissionsA
// We organize the data using array, which is easier for later processing

typedef struct{
	// all dci 
    uint32_t tbs[NOF_LOG_DCI];
    uint8_t  reTx[NOF_LOG_DCI];
	bool 	 reTx_rv_flag[NOF_LOG_DCI];

	// here we matain the retransmission statistics
	int nof_reTx;
    uint64_t reTx_t_us[NOF_LOG_DCI];
    uint16_t reTx_tti[NOF_LOG_DCI];
}ue_dl_dci_t;


// TODO currently ul and dl dci are the same, but we expect 
// them to be different after adding all the reTx related
typedef struct{
	// all dci 
    uint32_t tbs[NOF_LOG_DCI];
    uint8_t  reTx[NOF_LOG_DCI];
	bool 	 reTx_rv_flag[NOF_LOG_DCI];

	// here we matain the retransmission statistics
	int nof_reTx;
    uint64_t reTx_t_us[NOF_LOG_DCI];
    uint16_t reTx_tti[NOF_LOG_DCI];
}ue_ul_dci_t;

typedef struct{
	int 		nof_dci;
	int			ca_header;  // in tti
	int			ca_tail; 	// in tti
	uint64_t 	time_stamp_us[NOF_LOG_DCI];
    uint16_t 	tti[NOF_LOG_DCI];

	uint64_t    curr_time;
	ue_dl_dci_t dl_dci;
	ue_ul_dci_t ul_dci;
}ngscope_dci_t;


typedef struct{
	int 		cell_prb;
	int 		header;
	int 		nof_logged_dci;

	uint64_t	recent_dl_reTx_t_us;	
	uint64_t	recent_ul_reTx_t_us;	

	uint16_t 	recent_dl_reTx_tti;
	uint16_t 	recent_ul_reTx_tti;

	// the dci 
	ue_dci_t 	dci[NOF_LOG_DCI];	
}ngscope_dci_cell_t;

// store the dci with Carrier Aggregation Implemented
typedef struct{
	ngscope_dci_cell_t cell_dci[MAX_NOF_RF_DEV];
	bool ca_ready;
	int header;
	int tail;
	int nof_cell;
	uint64_t curr_time;
}ngscope_dci_CA_t;

typedef struct{
	uint16_t* tti;
	uint64_t* rx_t_us[MAX_NOF_RF_DEV];
	uint8_t*  reTx[MAX_NOF_RF_DEV];
	bool* 	  rv_flag[MAX_NOF_RF_DEV];
	uint32_t* tbs[MAX_NOF_RF_DEV];
}ngscope_muti_cell_trace_t;


void  ngscope_dci_print_single_cell(ngscope_dci_t*        q);

void  ngscope_dci_extract_single_cell(ngscope_dci_t*        q, 
										ngscope_dci_CA_t 	dci_ca,
										int 				cell_idx);
// Extract the data from the ring buffer and put the extracted data into array
// Array is much eaiser for later data processing
void  ngscope_dci_extract_multi_cell(ngscope_dci_t*        	q[MAX_NOF_RF_DEV], 
										ngscope_dci_CA_t 	dci_ca);

void  ngscope_dci_sync_multi_cell(ngscope_dci_t* q[MAX_NOF_RF_DEV], int nof_cell, int offset);

// Do we have any new downlink retransmission observed
bool ngscope_dci_new_dl_reTx(ngscope_dci_CA_t* q, uint64_t dl_reTx_us[MAX_NOF_RF_DEV]);
bool ngscope_dci_new_ul_reTx(ngscope_dci_CA_t* q, uint64_t dl_reTx_us[MAX_NOF_RF_DEV]);
// 
void ngscope_dci_copy_dl_reTx(ngscope_dci_CA_t* q, uint64_t dl_reTx_us[MAX_NOF_RF_DEV]);
void ngscope_dci_copy_ul_reTx(ngscope_dci_CA_t* q, uint64_t ul_reTx_us[MAX_NOF_RF_DEV]);

void ngscope_dci_cell_init(ngscope_dci_cell_t* q);
void ngscope_dci_CA_init(ngscope_dci_CA_t* q);
void ngscope_dci_CA_insert(ngscope_dci_CA_t* q, ue_dci_t* ue_dci, uint8_t cell_idx);

uint64_t* ngscope_dci_get_reTx_t(ngscope_dci_t*   q[MAX_NOF_RF_DEV], int nof_cell, int* nof_reTx);
uint16_t* ngscope_dci_get_reTx_tti(ngscope_dci_t*   q[MAX_NOF_RF_DEV], int nof_cell, int* nof_reTx);

#endif
