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
#include <stdlib.h>
#include <stdio.h>

#include "ngscope_reTx.h"
#include "ngscope_util.h"
#include "ngscope_debug_def.h"

extern client_fd_t client_fd;

int ngscope_reTx_init_buf(ngscope_reordering_buf_t* q){
	q->ready 			= false;

	q->buffer_active  	= false;
	q->buf_size 		= 0;
	q->buf_start_tti 	= 0;
	q->buf_last_tti		= 0;
	q->buf_start_t 		= 0;
	q->buf_last_t 		= 0;

	q->start_pkt_t 		= 0;
	q->start_pkt_found 	= false;
	q->most_recent_dci_t= 0;
	q->ref_oneway 		= 0;

	return 1;
}

int ngscope_reTx_reset_buf(ngscope_reordering_buf_t* q){
	
	if(CLIENT_RETX){
		fprintf(client_fd.fd_client_reTx, "%ld\t%ld\t%ld\t\n", q->buf_start_t, q->buf_last_t, q->start_pkt_t);
	}

	q->buffer_active  	= false;
	q->buf_size 		= 0;

	// we keep these data for later use dont change them
	//q->buf_start_tti 	= 0;
	//q->buf_last_tti		= 0;
	//q->buf_start_t 		= 0;
	//q->buf_last_t 		= 0;

	//q->start_pkt_t 		= 0;  // save it for later
	q->start_pkt_found 	= false;
	q->most_recent_dci_t= 0;

	//q->ref_oneway 		= 0; // save it for later

	return 1;
}

int ngscope_reTx_activate_buf(ngscope_reordering_buf_t* q){
	
	q->buffer_active  	= true;
	q->buf_size 		= 0;

	//q->start_pkt_t 		= 0;  // save it for later
	q->start_pkt_found 	= true;

	//q->ref_oneway 		= 0; // save it for later

	return 1;
}


uint16_t tti_to_idx(uint16_t tti, uint16_t tail_tti){
	return (tti - tail_tti + MAX_TTI) % MAX_TTI;
}
void free_ca_trace(ngscope_reTx_ca_t* res){
	free(res->reTx);
	free(res->tti);
	free(res->tbs);
	free(res->rx_t_us);

	return;
}

void  extract_ca_trace(ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV], int nof_cell, ngscope_reTx_ca_t* res){
	int header_tti  = dci_all[0]->ca_header;
	int tail_tti  	= dci_all[0]->ca_tail;

	//printf("header:%d tail:%d\n", header_tti, tail_tti);

	int size 	= tti_distance(tail_tti, header_tti);

	res->reTx 	= (bool *)calloc(size, sizeof(bool));
	res->tti 	= (uint16_t *)calloc(size, sizeof(uint16_t));
	res->tbs 	= (uint32_t *)calloc(size, sizeof(uint32_t));
	res->rx_t_us= (uint64_t *)calloc(size, sizeof(uint64_t));

	res->size 	= size;

	for(int i=0; i<size; i++){
		res->tti[i] = wrap_tti(tail_tti + i);	
	}

	for(int i=0; i<nof_cell; i++){
		int ss_idx = find_element_in_array_uint16(dci_all[i]->tti, dci_all[i]->nof_dci, tail_tti);
		int ee_idx = find_element_in_array_uint16(dci_all[i]->tti, dci_all[i]->nof_dci, header_tti);
		for(int j=ss_idx; j<=ee_idx; j++){
			int index 		= tti_to_idx(dci_all[i]->tti[j], tail_tti);
			bool reTx_tmp 		= (dci_all[i]->dl_dci.reTx[j]);
			res->reTx[index] 	=  res->reTx[index] || reTx_tmp;
			res->tbs[index] 	+= dci_all[i]->dl_dci.tbs[j];
			res->rx_t_us[index] +=  dci_all[i]->time_stamp_us[j] / nof_cell; // average time stamp of three cells
		}
	}

	//for(int i=0; i<size; i++){
	//	printf("%d", res->reTx[i]);
	//}
	//printf("header:%d tail:%d size:%d\n", header_tti, tail_tti, size);

	return;
}
// copy ul trace (assume only 1 uplink cell)
void  extract_ca_ul_trace(ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV], int nof_cell, ngscope_reTx_ca_t* res){
	int size 	= dci_all[0]->nof_dci;

	res->reTx 	= (bool *)calloc(size, sizeof(bool));
	res->tti 	= (uint16_t *)calloc(size, sizeof(uint16_t));
	res->tbs 	= (uint32_t *)calloc(size, sizeof(uint32_t));
	res->rx_t_us= (uint64_t *)calloc(size, sizeof(uint64_t));

	res->size 	= size;
	memcpy(res->tti, dci_all[0]->tti, size * sizeof(uint16_t));
	memcpy(res->reTx, dci_all[0]->ul_dci.reTx, sizeof(uint8_t));
	memcpy(res->tbs, dci_all[0]->ul_dci.tbs, sizeof(uint32_t));
	memcpy(res->rx_t_us, dci_all[0]->time_stamp_us, sizeof(uint64_t));
	//for(int i=0; i<size; i++){
	//	printf("%d", res->reTx[i]);
	//}
	//printf("header:%d tail:%d size:%d\n", header_tti, tail_tti, size);

	return;
}

// Check whether the i-th reTx is a retransmission or not
bool ngscope_reTx_is_new_burst(bool* reTx_array, int array_size, int index){
	if(index >= array_size){
		printf("Index is outside the array!\n");
		return false;
	}
	bool ret = true;
	// if index is smaller than 8, which means the start of the array, for sure it is a new burst
	if(index < 8){
		for(int i=0; i<index; i++){
			if(reTx_array[i])
				ret = false;
		}
	}else{
		for(int i=index - 8; i<index; i++){
			if(reTx_array[i])
				ret = false;
		}
	}
	return ret;
}

// if we  know the tti of the reTx, we need to find the index of this reTx inside the 
// tti_array (tti_array contains the tti of all the subframes  
int ngscope_reTx_find_index_of_reTx(uint16_t tti_array[NOF_LOG_DCI], uint16_t tti){
	for(int i=0; i< NOF_LOG_DCI; i++){
		if(tti_array[i] == tti){
			return i;
		}
	}
	return -1;
}

// Calculate the total bits that are stored inside the re-ordering buffer
uint32_t ngscope_reTx_sum_tbs_new_burst(uint32_t* tbs, int tbs_size, int index){
	if(index >= tbs_size){
		printf("TBS array size is smaller than the index!\n");
		return 0;
	}

	uint32_t tbs_sum=0;
	if(index < 8){
		for(int i=0; i<index; i++){
			tbs_sum += tbs[i];	
		}
	}else{
		for(int i=index-8; i<index; i++){
			tbs_sum += tbs[i];	
		}
	}
	return tbs_sum;
}

// Calculate the total bits that are stored inside the re-ordering buffer
uint32_t ngscope_reTx_sum_tbs_in_burst(uint32_t* tbs, int tbs_size, int idx_last, int idx_curr){
	if(idx_last >= tbs_size || idx_curr >= tbs_size){
		printf("TBS array size is smaller than the index!\n");
		return 0;
	}

	uint32_t tbs_sum=0;
	/*  idx_last +++++  idx_curr
	*   the idx_last and idx_curr are not summed */
	for(int i=idx_last+1; i<idx_curr; i++){
		tbs_sum += tbs[i];	
	}
	return tbs_sum;
}


void set_reTx_start_tti(ngscope_reordering_buf_t* q,
				ngscope_reTx_ca_t* ca_trace, int idx){
	
	if (q->buffer_active == true)
		printf("ERROR: found new reTx but the reTx buffer is stiall active\n");

	//printf("We found a start!\n");

	q->buffer_active 	= true;
	q->ready 			= true;

	q->buf_size 		= (long) ngscope_reTx_sum_tbs_new_burst(ca_trace->tbs, ca_trace->size, idx) + 11520 * 4;

	q->buf_start_tti 	= ca_trace->tti[idx];
	q->buf_last_tti 	= ca_trace->tti[idx];

	q->buf_start_t		= ca_trace->rx_t_us[idx];
	q->buf_last_t 		= ca_trace->rx_t_us[idx];

	//printf("We found a start headr:%d end:%d!\n", q->buf_start_tti, q->buf_last_tti);
	return;
}
void set_reTx_tti_inBurst(ngscope_reordering_buf_t* q,
				ngscope_reTx_ca_t* ca_trace, int idx){
	
	if (q->buffer_active == false)
		printf("ERROR: found new reTx in burst but the reTx buffer is inactive\n");
	
	q->buffer_active 	= true;

	int idx_last 		= find_element_in_array_uint16(ca_trace->tti, ca_trace->size, q->buf_last_tti);
	q->buf_size 		+= (long) ngscope_reTx_sum_tbs_in_burst(ca_trace->tbs, ca_trace->size, idx_last, idx) + 11520 * 4;

	q->buf_last_tti 	= ca_trace->tti[idx]; // last seen tti
	q->buf_last_t 		= ca_trace->rx_t_us[idx];
	//printf("We found a in-burst headr:%d end:%d!\n", q->buf_start_tti, q->buf_last_tti);

	return;
}
				
bool update_reTx_buf(ngscope_reordering_buf_t* q,
				ngscope_reTx_ca_t* ca_trace){
	bool found_reTx = false;
	for(int i=0; i<ca_trace->size; i++){
		// skip the tti if it is smaller than the last tti of the reTx 
		if(tti_a_se_b(wrap_tti(ca_trace->tti[i]), wrap_tti(q->buf_last_tti)) && q->ready){
			continue;
		}
		if(ca_trace->reTx[i]){
			found_reTx = true;
			bool new_burst = ngscope_reTx_is_new_burst(ca_trace->reTx, ca_trace->size, i);
			// if the buffer is active
			if(q->buffer_active){
				if(new_burst){
					printf("ERROR: reTx within 8 tti of the last burst but identified as new burst!\n");
				}else{
					set_reTx_tti_inBurst(q, ca_trace, i);
				}
			}else{
				if(new_burst || (q->ready == false) ){ 
					set_reTx_start_tti(q, ca_trace, i);
				}else{
					printf("Warning: burst inactive but TTI detected to be in burst. Maybe the packet drains too quickly.\n");
					ngscope_reTx_activate_buf(q);
					set_reTx_tti_inBurst(q, ca_trace, i);
				}
			}
		}
		if(q->buffer_active){
			// if we havn't seen any reTx within the 8 TTI of last tti of the burst
			// Exit the loop [+ + +] <--reTx outsize the burst
			if(tti_a_l_b(wrap_tti(ca_trace->tti[i]), wrap_tti(q->buf_last_tti + 8))){
				break;
			}
		}
	}
	printf("UPDATE reTx Buf: do we found_reTx:%d ?\n", found_reTx);
	return found_reTx;
}

int ngscope_reTx_update_buf(ngscope_reordering_buf_t* q,
							ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV],
							int nof_cell){
	ngscope_reTx_ca_t trace;

	// extract the trace that includes mutiple cells
	extract_ca_trace(dci_all, nof_cell, &trace);

	//printf("size:%d ", trace.size);
	//for(int i=0; i<trace.size; i++){
	//	printf("%d", trace.reTx[i]);
	//}
	//printf("\n");

	// update the retransmission buffer
	bool found_reTx = update_reTx_buf(q, &trace);

	
	return found_reTx;
}



int ngscope_reTx_update_ul_buf(ngscope_reordering_buf_t* q,
							ngscope_dci_t* 		dci_all[MAX_NOF_RF_DEV],
							int nof_cell){
	ngscope_reTx_ca_t trace;

	// extract the trace that includes mutiple cells
	extract_ca_ul_trace(dci_all, nof_cell, &trace);

	//printf("size:%d ", trace.size);
	//for(int i=0; i<trace.size; i++){
	//	printf("%d", trace.reTx[i]);
	//}
	//printf("\n");

	// update the retransmission buffer
	update_reTx_buf(q, &trace);

	
	return 1;
}


// let's try to judge whether this is a burst start or not:
// 1, the time difference with its previous pkt is larger than 8 millisecond
// 2, the oneway difference difference is also larger than 8 millisecond 
// (we ignore this for now) 
//bool is_burst_start(uint64_t* pkt_t_us, uint64_t* oneway_us, int pkt_num, int index){
bool is_burst_start(uint64_t* pkt_t_us, uint64_t* oneway, int pkt_num, int index){
	if(index >= pkt_num){
		printf("Index outside of the array!\n");
		return false;
	}
	if(index == 0){
		return true;
	}else{
		// Judege the burst-start
		if( abs( (long)pkt_t_us[index]  - (long)pkt_t_us[index-1]) >= TIME_DIFF_THD &&
					(long)oneway[index]  - (long)oneway[index-1] > ONEWAY_DIFF_THD){
			return true;
		}
	}
	return false;

}

bool judge_dci_pkt_time_align(uint64_t pkt_t_us, uint64_t dci_t_us){
	if( abs ((long)(pkt_t_us - dci_t_us)) <= ALIGN_T_THD)
		return true;
	else
		return false;
}
// we try to identify the starting point of the burst inside the packet statistics 
int ngscope_reTx_find_burst_start(uint64_t* pkt_t_us, uint64_t* oneway, int pkt_num, uint64_t dci_reTx_t_us){
	// step-1, find the nearest neighbour according to the timestamp
	int index = nearest_neighbour_index(pkt_t_us, pkt_num, dci_reTx_t_us);
	//if(!judge_dci_pkt_time_align(pkt_t_us[index], dci_reTx_t_us))
//		return -1;

	if(index < BURST_FIND_SEARCH_PKT){
		for(int i=index; i>=0; i--){
			if(is_burst_start(pkt_t_us, oneway, pkt_num, i)){
				//return pkt_t_us[i];
				return i;
			}
		}
	}else{
		for(int i=index;i>index-BURST_FIND_SEARCH_PKT; i--){
			if(is_burst_start(pkt_t_us, oneway, pkt_num, i)){
				//return pkt_t_us[i];
				return i;
			}
		}
	}

	if(index + BURST_FIND_SEARCH_PKT < pkt_num){
		for(int i=index+1; i<index + BURST_FIND_SEARCH_PKT; i++){
			if(is_burst_start(pkt_t_us, oneway, pkt_num, i)){
				//return pkt_t_us[i];
				return i;
			}
		}
	}else{
		for(int i=index+1;i<pkt_num; i++){
			if(is_burst_start(pkt_t_us, oneway, pkt_num, i)){
				return i;
			}
		}
	}
	long time_diff = abs((long)(dci_reTx_t_us - pkt_t_us[pkt_num-1]));
	//printf("time diff :%ld\n", time_diff);

	// now we cannot find any matching pkt
	// if there is one pkt that's close enough, we also count
	//if(!judge_dci_pkt_time_align(pkt_t_us[index], dci_reTx_t_us))
//		return -1;

	//return pkt_t_us[index];
	//return index;
	if(time_diff < 10000){
		return -1;
	}else{
		return index;
	}
}
