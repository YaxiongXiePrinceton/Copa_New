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

#include "ngscope_dci.h"
#include "ngscope_util.h"

//using namespace std;
extern bool go_exit;
extern ngscope_dci_CA_t dci_ca;
extern pthread_mutex_t dci_mutex;

void ngscope_dci_cell_init(ngscope_dci_cell_t* q){
	q->header 	= 0;
	q->cell_prb = 50; //default 10MHz
	q->nof_logged_dci 		= 0;
	q->recent_dl_reTx_t_us 	= 0;
	q->recent_ul_reTx_t_us 	= 0;
	for(int i=0; i<NOF_LOG_DCI; i++){
		memset(&q->dci[i], 0, sizeof(ue_dci_t));
	}
	return;
}

void ngscope_dci_CA_init(ngscope_dci_CA_t* q){
	q->nof_cell 	= 1;
	q->header 		= 0;
	q->tail 		= 0;
	q->curr_time 	= 0;
	q->ca_ready 	= false;

	for(int i=0; i<MAX_NOF_RF_DEV; i++){
		ngscope_dci_cell_init(&q->cell_dci[i]);
	}
	return;
}

void print_single_cell_info(ngscope_dci_cell_t* q){
	printf("cell_prb:%d\theader%d\tnof_dci:%d\trecent_dl_reTx:%ld\n",
			q->cell_prb, q->header, q->nof_logged_dci, q->recent_dl_reTx_t_us);
	return;
}

void print_single_cell_dci(ngscope_dci_cell_t* q){
	for(int i=0; i<NOF_LOG_DCI; i++){
		printf("%d|%d ", q->dci[i].dl_tbs, q->dci[i].ul_tbs);
	}
	printf("\n");

	return;
}
void print_ue_dci(ue_dci_t* q, int cell_idx){
	printf("UE-DCI: cell:%d\ttimestamp:%ld\ttti:%d\tdl_tbs:%d\tul_tbs:%d\tul_dl_reTx:%d|%d\n",
		cell_idx, q->time_stamp, q->tti, q->dl_tbs, q->ul_tbs, q->dl_reTx, q->ul_reTx);	
	return;
}

void print_non_empty_ue_dci(ue_dci_t* q, int cell_idx){
	if(q->dl_tbs>0 || q->ul_tbs>0){
		printf("UE-DCI: cell:%d\ttimestamp:%ld\ttti:%d\tdl_tbs:%d\tul_tbs:%d\tul_dl_reTx:%d|%d\n",
			cell_idx, q->time_stamp, q->tti, q->dl_tbs, q->ul_tbs, q->dl_reTx, q->ul_reTx);	
	}
	return;
}

uint16_t find_dci_cell_tail(ngscope_dci_cell_t* q){
	int nof_dci = q->nof_logged_dci;
	int header  = q->header;
	int tail_idx = (header - nof_dci + NOF_LOG_DCI) % NOF_LOG_DCI;
	//printf("nof_dci:%d header:%d | %d tail:%d | %d ", nof_dci, header, q->dci[header].tti, tail_idx, q->dci[tail_idx].tti);
	return q->dci[tail_idx].tti;
}
void ca_update_header(ngscope_dci_CA_t* q){
	int 	nof_cell = q->nof_cell;
	uint16_t header_vec[MAX_NOF_RF_DEV];
	uint16_t tail_vec[MAX_NOF_RF_DEV];
	uint64_t curr_time[MAX_NOF_RF_DEV];
	bool 	ca_ready = true;

	// NOTE::: here the header is the tii, no the index inside the array
	for(int i=0; i<nof_cell; i++){
		int cell_header = (q->cell_dci[i].header - 1) % NOF_LOG_DCI;
		header_vec[i] 	= q->cell_dci[i].dci[cell_header].tti;
		tail_vec[i] 	= find_dci_cell_tail(&q->cell_dci[i]);
		curr_time[i] 	= q->cell_dci[i].dci[cell_header].time_stamp;
		if(q->cell_dci[i].header == 0){
			ca_ready = false;
		}
		//printf("cell header:%d tail:%d ", header_vec[i], tail_vec[i]);
	}
	//printf("\n");

	
	if(q->ca_ready){
		// unwrap tti so that we can compare
		unwrap_tti_array(header_vec, nof_cell);

		uint16_t min_tti = min_in_array_uint16_v(header_vec, nof_cell);
		q->header 		= wrap_tti(min_tti);

		uint16_t max_tti = max_in_array_uint16_v(tail_vec, nof_cell);
		q->tail 		= wrap_tti(max_tti + 1);

		q->curr_time 	= mean_array_uint64(curr_time, nof_cell);

	}else{
		if(ca_ready){
			q->ca_ready = true; 
		}
	}
	//printf("CA Header:%d Tail:%d\n", q->header, q->tail);
	return;
}
void ngscope_dci_CA_insert(ngscope_dci_CA_t* q, ue_dci_t* ue_dci, uint8_t cell_idx){
	// get the pointer to the dci cell 
	ngscope_dci_cell_t* dci_cell = &q->cell_dci[cell_idx];

	int prev_header = (dci_cell->header - 1 + NOF_LOG_DCI) % NOF_LOG_DCI;
	int prev_tti 	= dci_cell->dci[prev_header].tti; 
	int curr_tti 	= ue_dci->tti;

	//printf("prev:%d curr:%d header:%d prev_header:%d\n", prev_tti, curr_tti, dci_cell->header, prev_header);
	if( ((prev_tti + 1) % 10240) != curr_tti){
		printf("We missing one DCI message! prev:%d curr:%d ", prev_tti, curr_tti);
		printf("header:%d prev_header:%d\n",  dci_cell->header, prev_header);
	}

	// print the received ue_dci
	//print_non_empty_ue_dci(ue_dci, cell_idx);

	// print cell related information
	//if(cell_idx == 0){
	//	//print_single_cell_info(dci_cell);
	//	print_single_cell_dci(dci_cell);
	//}

	// copy the dci messages
	memcpy(&dci_cell->dci[dci_cell->header], ue_dci, sizeof(ue_dci_t));

	// update the header
	dci_cell->header++;
    dci_cell->header =  dci_cell->header % NOF_LOG_DCI;

	// update the length	
	if(dci_cell->nof_logged_dci < NOF_LOG_DCI){
		dci_cell->nof_logged_dci++;
	}
	// check if current dci indicates retransmission
	if(ue_dci->dl_reTx == 1){
		dci_cell->recent_dl_reTx_t_us = ue_dci->time_stamp;
	}

	if(ue_dci->ul_reTx == 1){
		dci_cell->recent_ul_reTx_t_us = ue_dci->time_stamp;
	}

	ca_update_header(q);

	return;
}

// determine whether we observe new reTx
bool ngscope_dci_new_dl_reTx(ngscope_dci_CA_t* q, uint64_t dl_reTx_us[MAX_NOF_RF_DEV]){
	for(int i=0; i<q->nof_cell; i++){
		if(dl_reTx_us[i] == 0){ // the reTx is not inited yet
			if(q->cell_dci[i].recent_dl_reTx_t_us == 0){
				// no reTx is observed yet 
				continue;
			}else{
				// init the value
				dl_reTx_us[i]  = q->cell_dci[i].recent_dl_reTx_t_us;
			}
		}else{
			if(dl_reTx_us[i] != q->cell_dci[i].recent_dl_reTx_t_us){
				return true;
			}
		}
	}
	return false;
}

// determine whether we observe new reTx
bool ngscope_dci_new_ul_reTx(ngscope_dci_CA_t* q, uint64_t ul_reTx_us[MAX_NOF_RF_DEV]){
	for(int i=0; i<q->nof_cell; i++){
		if(ul_reTx_us[i] == 0){ // the reTx is not inited yet
			if(q->cell_dci[i].recent_ul_reTx_t_us == 0){
				// no reTx is observed yet 
				continue;
			}else{
				// init the value
				ul_reTx_us[i]  = q->cell_dci[i].recent_ul_reTx_t_us;
			}
		}else{
			if(ul_reTx_us[i] != q->cell_dci[i].recent_ul_reTx_t_us){
				return true;
			}
		}
	}
	return false;
}

void ngscope_dci_copy_dl_reTx(ngscope_dci_CA_t* q, uint64_t dl_reTx_us[MAX_NOF_RF_DEV]){
	for(int i=0; i<q->nof_cell; i++){
		dl_reTx_us[i]  = q->cell_dci[i].recent_dl_reTx_t_us;
	}
	return;
}

void ngscope_dci_copy_ul_reTx(ngscope_dci_CA_t* q, uint64_t ul_reTx_us[MAX_NOF_RF_DEV]){
	for(int i=0; i<q->nof_cell; i++){
		ul_reTx_us[i]  = q->cell_dci[i].recent_ul_reTx_t_us;
	}
	return;
}

// Extract dci into multiple vectors
void extract_single_dci(ngscope_dci_t* q,
					ngscope_dci_cell_t dci_cell,
					int q_idx,
					int p_idx)
{
	q->tti[q_idx] 		 	= dci_cell.dci[p_idx].tti;
	q->time_stamp_us[q_idx] = dci_cell.dci[p_idx].time_stamp;

	// Downlink 
	q->dl_dci.tbs[q_idx] 	= dci_cell.dci[p_idx].dl_tbs;
	q->dl_dci.reTx[q_idx] 	= dci_cell.dci[p_idx].dl_reTx;

	q->dl_dci.reTx_rv_flag[q_idx] 	= dci_cell.dci[p_idx].dl_rv_flag;
	q->ul_dci.reTx_rv_flag[q_idx] 	= dci_cell.dci[p_idx].ul_rv_flag;

	if(q->dl_dci.reTx[q_idx]){
		q->dl_dci.reTx_t_us[q->dl_dci.nof_reTx] = dci_cell.dci[p_idx].time_stamp;
		q->dl_dci.reTx_tti[q->dl_dci.nof_reTx] 	= dci_cell.dci[p_idx].tti;
		q->dl_dci.nof_reTx++;
	}

	// Downlink 
	q->ul_dci.tbs[q_idx] 	= dci_cell.dci[p_idx].ul_tbs;
	q->ul_dci.reTx[q_idx] 	= dci_cell.dci[p_idx].ul_reTx;

	if(q->ul_dci.reTx[q_idx]){
		q->ul_dci.reTx_t_us[q->ul_dci.nof_reTx] = dci_cell.dci[p_idx].time_stamp;
		q->ul_dci.reTx_tti[q->ul_dci.nof_reTx] 	= dci_cell.dci[p_idx].tti;
		q->ul_dci.nof_reTx++;
	}

	return;
}

void  ngscope_dci_extract_single_cell(ngscope_dci_t* 		q,
										ngscope_dci_CA_t 	dci_ca,
										int 				cell_idx)
{
	// empty the structure
	memset(q, 0, sizeof(ngscope_dci_t));
	ngscope_dci_cell_t dci_cell = dci_ca.cell_dci[cell_idx];
	int header 	= dci_cell.header;
	int nof_dci = dci_cell.nof_logged_dci;

	q->nof_dci 	= nof_dci;
	q->ca_header= dci_ca.header;
	q->ca_tail 	= dci_ca.tail;
	q->curr_time= dci_ca.curr_time;

	// when the array is not full, then we should start copy from the begining
	if(nof_dci < NOF_LOG_DCI){
		for(int i=0; i<nof_dci; i++){
			extract_single_dci(q, dci_cell, i, i);
		}
	}else if(nof_dci == NOF_LOG_DCI){
		for(int i=0; i<nof_dci; i++){
			int p_idx = (header + i) % NOF_LOG_DCI;
			//printf("%d %d| ", i, p_idx); 
			extract_single_dci(q, dci_cell, i, p_idx);
		}
		//printf("\n");
	}else{
		printf("ERROR: the nof logged dci is corrupted!\n\n");
	}
   	//printf("end of extract single cell!\n"); 
    return;
}

void ngscope_dci_print_single_cell(ngscope_dci_t* 		q){
	int nof_reTx = q->dl_dci.nof_reTx;
	printf("nof_reTx:%d ", nof_reTx);
	for(int i=0; i<nof_reTx; i++){
		printf("%d ", q->dl_dci.reTx_tti[i]);
	}
	printf("\n");
	return;
}


void  ngscope_dci_extract_multi_cell(ngscope_dci_t* 		q[MAX_NOF_RF_DEV],
										ngscope_dci_CA_t 	dci_ca)
{
	for(int i=0; i<dci_ca.nof_cell; i++){
		ngscope_dci_extract_single_cell(q[i], dci_ca, i);
	}
   	//printf("end of extract multiple cell!\n"); 
	return;
}

void  ngscope_dci_sync_single_cell(ngscope_dci_t* q, int offset){
	for(int i=0; i<q->nof_dci; i++){
		q->time_stamp_us[i] += offset;
	}

	for(int i=0; i<q->dl_dci.nof_reTx; i++){
		q->dl_dci.reTx_t_us[i] += offset;
	}

	for(int i=0; i<q->ul_dci.nof_reTx; i++){
		q->ul_dci.reTx_t_us[i] += offset;
	}

	return;
}
void  ngscope_dci_sync_multi_cell(ngscope_dci_t* q[MAX_NOF_RF_DEV], int nof_cell, int offset){
	for(int i=0; i<nof_cell; i++){
		ngscope_dci_sync_single_cell(q[i], offset);
	}
	return;
}
uint64_t* ngscope_dci_get_reTx_t(ngscope_dci_t* 	q[MAX_NOF_RF_DEV], int nof_cell, int* nof_reTx){
	// get total nof reTx
	int total_nof_reTx = 0;
	for(int i=0; i<nof_cell; i++){
		total_nof_reTx += q[i]->dl_dci.nof_reTx;
	}
	uint64_t* array = (uint64_t*)calloc(total_nof_reTx, sizeof(uint64_t));
	int cnt = 0;
	for(int i=0; i<nof_cell; i++){
		for(int j=0; j<q[i]->dl_dci.nof_reTx; j++){
			array[cnt] = q[i]->dl_dci.reTx_t_us[j];
			cnt += 1;
		}
	}
	//qsort(array, total_nof_reTx, sizeof(uint64_t), cmpfunc_uint64);
	quick_sort_uint64(array, total_nof_reTx);
	*nof_reTx = total_nof_reTx;
	return array;
	//*nof_reTx = q[0]->dl_dci.nof_reTx;
	//return q[0]->dl_dci.reTx_t_us;
}

uint16_t* ngscope_dci_get_reTx_tti(ngscope_dci_t* 	q[MAX_NOF_RF_DEV], int nof_cell, int* nof_reTx){
	// get total nof reTx
	int total_nof_reTx = 0;
	for(int i=0; i<nof_cell; i++){
		total_nof_reTx += q[i]->dl_dci.nof_reTx;
	}
	uint16_t* array = (uint16_t*)calloc(total_nof_reTx, sizeof(uint16_t));

	int cnt = 0;
	for(int i=0; i<nof_cell; i++){
		for(int j=0; j<q[i]->dl_dci.nof_reTx; j++){
			array[cnt] = q[i]->dl_dci.reTx_tti[j];
			cnt += 1;
		}
	}
	//qsort(array, total_nof_reTx, sizeof(uint16_t), cmpfunc_uint16);
	quick_sort_uint16(array, total_nof_reTx);
	*nof_reTx = total_nof_reTx;
	return array;
	//*nof_reTx = q[0]->dl_dci.nof_reTx;
	//return q[0]->dl_dci.reTx_t_us;
}

int get_overlapping_tti(ngscope_dci_t* 	q[MAX_NOF_RF_DEV], int nof_cell, uint16_t* s_idx, uint16_t* e_idx){

	uint16_t start_idx[MAX_NOF_RF_DEV];
	uint16_t end_idx[MAX_NOF_RF_DEV];

	for(int i=0; i<nof_cell;i++){
		start_idx[i] = q[i]->tti[0];
		end_idx[i] = q[i]->tti[q[i]->nof_dci];
	}

	uint16_t start_tti =0;
	uint16_t end_tti =0;

	start_tti 	= max_tti_in_array(start_idx, nof_cell);
	end_tti 	= max_tti_in_array(end_idx, nof_cell);

	*s_idx 	= start_tti;
	*e_idx 	= end_tti;

	return 1;
}
int allocate_memory_for_multi_cell_trace(ngscope_muti_cell_trace_t* q, int size, int nof_cell){
	q->tti = (uint16_t*)malloc(size * sizeof(uint16_t));
	for(int i=0; i<nof_cell; i++){
		q->rx_t_us[i] 	= (uint64_t*)malloc(size * sizeof(uint64_t));
		q->reTx[i] 		= (uint8_t*)malloc(size * sizeof(uint8_t));
		q->rv_flag[i] 	= (bool*)malloc(size * sizeof(bool));
		q->tbs[i] 		= (uint32_t*)malloc(size * sizeof(uint32_t));
	}

	return 0;
}

int free_memory_for_multi_cell_trace(ngscope_muti_cell_trace_t* q, int size, int nof_cell){
	free(q->tti);
	for(int i=0; i<nof_cell; i++){
		free(q->rx_t_us[i]);
		free(q->reTx[i]);
		free(q->rv_flag[i]);
		free(q->tbs[i]);
	}
	return 0;
}

int extract_mutli_cell_trace(ngscope_dci_t* 	q[MAX_NOF_RF_DEV], int nof_cell, uint16_t s_tti, uint16_t e_tti){
	int vec_len = tti_distance(s_tti, e_tti) + 1; // calculate the length

	// allocate the memory for the cell trace
	ngscope_muti_cell_trace_t cell_trace;
	allocate_memory_for_multi_cell_trace(&cell_trace, vec_len, nof_cell);
	if(s_tti > e_tti) e_tti += MAX_TTI;

	int ss_idx[MAX_NOF_RF_DEV];
	for(int i=0; i<nof_cell; i++){
		ss_idx[i] = find_element_in_array_uint16(q[i]->tti, q[i]->nof_dci, s_tti);
	}
	for(int i=0; i<=vec_len; i++){
		cell_trace.tti[i] = wrap_tti(i + s_tti);

		for(int j=0; j<nof_cell; j++){
			// We find the corresponding tti in the array
			if(q[j]->tti[ss_idx[j]+i] == wrap_tti(i + s_tti)){
				//cell_trace.rx_t_us[j][i] = q[j]->rx_t_us[ss_idx[j]+i];
			}else{
				continue;
			}

		}
	}

	return 1;
}

/* 
* Now let's get the overlapping TTIs
*
*/
int ngscope_dci_multi_cell_trace(ngscope_dci_t* q[MAX_NOF_RF_DEV], int nof_cell){
	uint16_t s_idx = 0, e_idx = 0;

	get_overlapping_tti(q, nof_cell, &s_idx, &e_idx);

	return 1;
}
