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

#include "socket.hh"
#include "ngscope_sync.h"
#include "packet_list.h"

#define DELAY_DIFF_THD 1400
//#include "acker.hh"

//int* delay_diff_ms
//uint64_t* recv_t_us
//uint64_t dci_recv_t_us[NOF_LOG_DCI];
//uint8_t  dci_reTx[NOF_LOG_DCI];
//uint32_t dci_tbs[NOF_LOG_DCI];

// Determine whether we have reTx not handled 
bool ngscope_sync_reTx_not_handled(ue_dci_t* dci_vec, 
                            int         last_reTx_idx,
                            int         header,
     __attribute__((unused))int         nof_dci)
{
    int start_idx = last_reTx_idx +1;
    int end_idx;
    if(header < start_idx){
        end_idx = header + NOF_LOG_DCI;
    }else{
        end_idx = header;
    }
    //printf("last_reTx_idx:%d start:%d  end:%d\n", last_reTx_idx, start_idx, end_idx);
    for(int i=start_idx; i<end_idx; i++){
        int idx = i % NOF_LOG_DCI;
        if(dci_vec[idx].reTx > 0){
            return true;
        }
    } 
            
    return false;
}

// Determine if we observe new reTx
bool ngscope_sync_new_reTx(ue_dci_t* dci_vec, 
                            int         last_reTx_idx,
                            int*        curr_reTx_idx,
                            int         header,
                            int         nof_dci)
{
    if((last_reTx_idx == header) || (last_reTx_idx +1 == header)\
                     || (nof_dci < NOF_LOG_DCI) ){
        *curr_reTx_idx = last_reTx_idx;
        return false;
    }
    int start_idx = last_reTx_idx +1;
    int end_idx;
    if(header < start_idx){
        end_idx = header + NOF_LOG_DCI;
    }else{
        end_idx = header;
    }
    //printf("last_reTx_idx:%d start:%d  end:%d\n", last_reTx_idx, start_idx, end_idx);
    for(int i=start_idx; i<end_idx; i++){
        int idx = i % NOF_LOG_DCI;
        //printf("idx:%d ", idx);
        *curr_reTx_idx = idx;
        if(dci_vec[idx].reTx > 0){
            return true;
        }
    } 
            
    //printf("\n");
    return false;
}

int idx_in_circle(int idx){
    idx = (idx + NOF_LOG_DCI) % NOF_LOG_DCI;
    return idx; 
}
int find_ori_tx_subf(uint8_t* reTx, uint32_t* tbs, int idx){
    int tmp_idx = idx_in_circle(idx-8);
    // max of three retransmissions
   for(int i=0; i<3; i++){
        if(reTx[tmp_idx] == 0){
            if(tbs[idx] == 0){
                // sometimes, in the dci, the tbs of the reTx are set to 0
                // We need to map it to its originl transmissions
                tbs[idx] = tbs[tmp_idx]; 
            }
            return tmp_idx;
        }
        tmp_idx = idx_in_circle(tmp_idx-8);
    } 
    printf("Tracing back three reTransmission cannot find original transmission!\n"); 
    return tmp_idx;
}

int update_ori(uint8_t* reTx, int s_idx, int e_idx){
    int updated_ori = s_idx;
    if(e_idx < s_idx){
        e_idx += NOF_LOG_DCI;
    }
    for(int i=s_idx+1; i<e_idx; i++){
        int idx = idx_in_circle(i);
        if(reTx[idx] > 0){
            updated_ori = idx;
        }
    }
    return updated_ori;
}

uint32_t sum_tbs(uint32_t* tbs, int s_idx, int e_idx){
    uint32_t tbs_sum = 0;
    if(e_idx < s_idx){
        e_idx += NOF_LOG_DCI;
    }
    for(int i=s_idx+1; i<=e_idx; i++){
        int idx = idx_in_circle(i);
        tbs_sum += tbs[idx];
    }
    return tbs_sum;
}

long revert_last_pkt_reTx(packet_node* head,
                            long tbs)
{
    long tbs_sum = tbs;
    packet_node* p;
    p = head->next;
    uint64_t min_delay = p->oneway_us;
    uint32_t pkt_seq = 0;
    while(p != NULL){
        if( p->oneway_us < min_delay){
            min_delay = p->oneway_us;
        }
        if(p->next == NULL){
            // last pkt in the list
            if(tbs_sum > 0){
                tbs_sum -= p->pkt_header.recv_len * 8; // recv_len is in bytes
                p->oneway_us_new    = min_delay; 
            }
        }
        p = p->next;
    }
    if(tbs_sum < 0){
        tbs_sum = 0;
    }
    return tbs_sum;
}

uint32_t confirm_start_of_pkt_burst(int index, uint32_t* pkt_seq_vec, uint64_t* recv_t_diff)
{
    if(index < 4){
        return pkt_seq_vec[index];
    }
    for(int i=0; i<=3; i++){
        int idx_tmp = index - i;
        if(recv_t_diff[idx_tmp] < 1000){
            continue;
        }else if(recv_t_diff[idx_tmp] > 6000){
            return pkt_seq_vec[idx_tmp];
        }else{
            return pkt_seq_vec[index];
        }
    } 
    return pkt_seq_vec[index-3];
}
bool match_reTx_pkt_in_time(packet_node* head,
                                int      nof_pkt,
                                uint64_t timestamp, 
                                uint32_t* seq,
                                uint64_t* delay,
                                long*     delay_to_recent)
{
    packet_node* p;
    p = head->next;
    uint64_t min_delay = p->oneway_us;
    uint64_t min_delay_diff = abs( (int)(p->recv_t_us - timestamp));
    //uint32_t pkt_seq = 0;
    uint64_t recent_pkt_recv_t;
    uint64_t delay_of_matched = 0;

    printf("nof_pkt:%d\n", nof_pkt);
    uint64_t* pkt_recv_diff = (uint64_t* )malloc( (nof_pkt + 5) * sizeof(uint64_t));
    uint64_t  recv_anchor = p->recv_t_us;
    uint32_t* pkt_seq_vec = (uint32_t* )malloc((nof_pkt + 5) * sizeof(uint32_t));

    int cnt = 0; int matched_idx = -1;
    while(p != NULL){
        int delay_diff = abs((long)(p->recv_t_us - timestamp));
        if( delay_diff < (long)min_delay_diff){
            min_delay_diff      = delay_diff;
            //pkt_seq             = p->pkt_header.sequence_number;
            delay_of_matched    = p->oneway_us;
            matched_idx         = cnt;
        }  
        if( p->oneway_us < min_delay){
            min_delay = p->oneway_us;
        }
        recent_pkt_recv_t   = p->recv_t_us;
        pkt_seq_vec[cnt]    =   p->pkt_header.sequence_number;
        pkt_recv_diff[cnt]  = abs((long)(p->recv_t_us - recv_anchor));

        recv_anchor = p->recv_t_us;
        cnt++;
        p = p->next;
    }
    
    printf("cnt:%d\n", cnt);
    *delay_to_recent = (long)(recent_pkt_recv_t - timestamp);

    printf("min delay:%ld min_delay_diff: %ld \n", min_delay, min_delay_diff);
    bool matched = true;
    if(min_delay_diff < DELAY_DIFF_THD){
        //*seq    = pkt_seq; 
        //*delay  = min_delay;
        *seq    = confirm_start_of_pkt_burst(matched_idx, pkt_seq_vec, pkt_recv_diff); 
        *delay  = delay_of_matched - 8000;
        matched = true;
    }else{
        *seq    = 0; 
        *delay  = min_delay;
        matched = false;
    } 

    free(pkt_recv_diff);
    free(pkt_seq_vec); 
    return matched;
} 

long revert_pkt_delay_vec(packet_node* head,
                    uint64_t timestamp, 
                    uint32_t pkt_seq,
                    uint64_t min_delay,
                    long     tbs,
                    FILE*    fd)
{
    packet_node* p;
    p = head->next;
    long tbs_sum = tbs;
    int pkt_cnt = 0;
    bool revert_flag = false;

    // now revert the delay
    while(p != NULL){
        if( p->pkt_header.sequence_number == pkt_seq && revert_flag == false){
            fprintf(fd, "%ld\t%ld\n", timestamp, p->recv_t_us);
            if(!p->revert_flag){
                tbs_sum -= p->pkt_header.recv_len * 8; // recv_len is in bytes
                p->oneway_us_new    = min_delay; 
                p->revert_flag      = true; 
                pkt_cnt++;
            }
            revert_flag = true;
        }else{
            if(revert_flag){
                if(!p->revert_flag){
                    tbs_sum -= p->pkt_header.recv_len * 8; // recv_len is in bytes
                    p->oneway_us_new    = min_delay; 
                    p->revert_flag      = true; 
                    pkt_cnt++;
                }
            } 
        }
        if(tbs_sum <= 0){
            printf("Tbs is empty!\n");
            break;
        }
        p = p->next;
    }
    printf("tbs_sum_in:%ld tbs_sum_out:%ld reverted packet:%d\n", tbs, tbs_sum, pkt_cnt);

    if(tbs_sum < 0){
        tbs_sum = 0;
    }
    return tbs_sum;
}


long match_and_revert_pkt_reTx(packet_node* head,
                                uint64_t timestamp, 
                                long tbs,
                                FILE* fd)
{
    long tbs_sum = tbs;
    packet_node* p;
    p = head->next;
    uint64_t min_delay = p->oneway_us;
    uint64_t min_delay_diff = abs( (int)(p->recv_t_us - timestamp));

    uint64_t consec_delay_diff = 0, last_recv_t = 0;

    uint32_t pkt_seq = 0;
    while(p != NULL){
        int delay_diff = abs((int)(p->recv_t_us - timestamp));
        if( delay_diff < (long)min_delay_diff){
            min_delay_diff = delay_diff;
            pkt_seq = p->pkt_header.sequence_number;
        }  
        if( p->oneway_us < min_delay){
            min_delay = p->oneway_us;
        }

        //consec_delay_diff = abs((int)(last_recv_t - p->recv_t_us));
        //if(consec_delay_diff > 4000 && delay_diff < DELAY_DIFF_THD){
        //    pkt_seq = p->pkt_header.sequence_number;
        //    revert_flag = true;
        //    break;
        //}
        p = p->next;
    }

    // now revert the delay
    int pkt_cnt = 0;
    if(min_delay_diff < DELAY_DIFF_THD){
        p = head->next;
        bool revert_flag = false;
        while(p != NULL){
            if( p->pkt_header.sequence_number == pkt_seq && revert_flag == false){
                fprintf(fd, "%ld\t%ld\n", timestamp, p->recv_t_us);
                if(!p->revert_flag){
                    tbs_sum -= p->pkt_header.recv_len * 8; // recv_len is in bytes
                    p->oneway_us_new    = min_delay; 
                    p->revert_flag      = true; 
                    pkt_cnt++;
                }
                revert_flag = true;
            }else{
                if(revert_flag){
                    if(!p->revert_flag){
                        tbs_sum -= p->pkt_header.recv_len * 8; // recv_len is in bytes
                        p->oneway_us_new    = min_delay; 
                        p->revert_flag      = true; 
                        pkt_cnt++;
                    }
                } 
            }
            if(tbs_sum <= 0){
                printf("Tbs is empty!\n");
                break;
            }
            p = p->next;
        }
    }
    printf("min delay:%ld min_delay_diff: %ld tbs_sum:%ld pkt_cnt:%d\n", min_delay, min_delay_diff, tbs_sum, pkt_cnt);
    if(tbs_sum < 0){
        tbs_sum = 0;
    }
    return tbs_sum;
}

bool circular_a_larger_b(uint16_t a, uint16_t b){
    int diff = abs((int) (a - b));

    if(diff >= 4 * NOF_LOG_DCI){
        // the TTI is wrapped up around the 10240
        if(a > b){
            return false;
        }else{
            return true;
        }
    }else{
        if(a > b){
            return true;
        }else{
            return false;
        }
    } 
}
//
//int circular_a_minus_b(int a, int b){
//	return 0;
//}

int find_i_th_reTx(uint8_t*     reTx,
                    uint16_t*   tti,
                    int         last_reTx_tti,
                    int         nof_dci,
                    int         index)
{
    int cnt=0;
    for(int i=0; i<nof_dci; i++){
        if( (reTx[i] > 0) && (circular_a_larger_b(tti[i], last_reTx_tti))){
            cnt++;
            if(cnt == index){
                return i;
            }
        }
    }
    return -1;

}

int ngscope_sync_oneway_revert(packet_node* head,
                                int         nof_pkt,
                                uint64_t*   recv_t_us,
                                uint32_t*   tbs,
                                uint8_t*    reTx,
                                uint16_t*   tti,
                                long        tbs_in,
                                long*       tbs_out,
                                int         delay_in,
                                int*        delay_out,
                                int         last_reTx_tti,
                                int         offset,
         __attribute__((unused))int         header,
                                int         nof_dci,
                                FILE*       fd)
{
    int curr_reTx_idx = -1; 
    long tbs_left = 0;

    int last_tti = last_reTx_tti;
    int nof_reTx = 1;

    if(tbs_in != 0){
        printf("ERROR: we don't allow non-zero tbs left when finding new reTx\n");
        return tbs_in;
    }
    printf("last_reTx_tti:%d current tti:%d\n", last_reTx_tti, tti[nof_dci-1]);
    for(int i=0; i<nof_dci; i++){
        //int idx = find_i_th_reTx(reTx, tti, last_reTx_tti, nof_dci, nof_reTx);
        if( (reTx[i] > 0) && (circular_a_larger_b(tti[i], last_reTx_tti))){
            // 1, find ori retransmission
            int ori_idx         = find_ori_tx_subf(reTx, tbs, i);
            int ori_idx_new     = update_ori(reTx, ori_idx, i);
            // count the tbs
            long tbs_sum        = (long)sum_tbs(tbs, ori_idx_new, i); 

            // shift the timestamp    
            uint64_t timestamp  = recv_t_us[i] + offset; 

            //fprintf(fd, "%ld\t%d\t%ld\n", recv_t_us[i], offset, timestamp);
            // revert the delay 
            uint32_t matched_seq =0;
            uint64_t min_delay =0;
            long delay_to_recent_pkt =0;

            printf("nof_dci:%d tti-ori:%d updated tti_ori:%d tti-reTx:%d time:%ld tbs:%ld \n", \
                                        nof_dci, tti[ori_idx], tti[ori_idx_new], tti[i], timestamp, tbs_sum); 

            bool matched = match_reTx_pkt_in_time(head, nof_pkt, timestamp, &matched_seq, &min_delay, &delay_to_recent_pkt);
            
            if(ori_idx_new != ori_idx && delay_in > 0){
                // it is within a burst
                min_delay = delay_in;
            }
            
            *delay_out = min_delay;

            if(matched){
                printf("Matched!\n");
                // Get matched packet sequence
                tbs_left = revert_pkt_delay_vec(head, timestamp, matched_seq, min_delay, tbs_sum, fd);
                curr_reTx_idx = i;
                if(tbs_left == 0){
                    // we finished our job --> do nothing and loop to the next 
                    printf("we finished our job!\n");
                }else if(tbs_left < tbs_sum){
                    // we do part of the job, but not finished
                    *tbs_out = tbs_left; 
                    return curr_reTx_idx;   
                }else if(tbs_left == tbs_sum){
                    // we do nothing, which is incorrect!
                    printf("ERROR we find a match but we do nothing!\n");
                    *tbs_out = tbs_left; 
                    return curr_reTx_idx;   
                }else{
                    printf("ERROR: tbs_left value not recovnized!\n\n\n");
                }
            }else{
                printf("No match found!\n");
                // Cannot find a match!
                if(delay_to_recent_pkt < 0){
                    // the packet delay is behind the dci, we need to wait a little bit longer 
                    printf("Packet timestamp is behind the dci messages!\n");
                    return curr_reTx_idx;
                }else if(delay_to_recent_pkt > 40000){
                    // seems like that we are not going to find such a match anyway, skip
                    printf("We skipp one DCI!\n");
                    curr_reTx_idx = i;
                }else{
                    printf("No match and do nothing!\n");
                }
            }
        }    
    }
    if(tbs_left < 0){    
        tbs_left = 0;
    }
    *tbs_out = tbs_left; 
    return curr_reTx_idx;
}

// Extract dci into multiple vectors
void ngscope_sync_extract_data_from_dci(ue_dci_t* dci_vec, 
                            uint64_t* recv_t_us,    
                            uint8_t* reTx,  
                            uint32_t* tbs,
                            uint16_t* tti,
                            int header,
                            int nof_dci)
{
    for(int i=0; i<nof_dci; i++){
        int idx         = header + i;
        idx             = idx % NOF_LOG_DCI;
        recv_t_us[i]    = dci_vec[idx].time_stamp;
	//printf("%ld ",dci_vec[idx].time_stamp);

        tbs[i]          = dci_vec[idx].tbs;
        reTx[i]         = dci_vec[idx].reTx;
        tti[i]          = dci_vec[idx].tti;
    }
    return;
}

int get_nof_reTx_pkt(int* delay_diff_ms, int list_len){
    int cnt = 0; 
    //count the nof of reTx
    for(int i=0; i<list_len; i++){
        if(delay_diff_ms[i] >= reTx_delay_thd){
            cnt++;
        }        
    }
    return cnt;
} 


// Here we allocate the memory, remember to free them
uint64_t* extract_reTx_from_pkt(int*         delay_diff_ms, 
                            uint64_t*   recv_t_us, 
                            int         list_len,
                            int*        nof_pkt)
{
    int cnt = 0; 
    for(int i=0; i<list_len; i++){
        if(delay_diff_ms[i] >= reTx_delay_thd){
            cnt++;
        }        
    }
    //printf("extract_reTx_from_pkt list_len:%d reTx:%d\n", list_len, cnt);
    uint64_t *reTx_recv_t_us = (uint64_t *)malloc(cnt * sizeof(uint64_t));

    cnt = 0;
    for(int i=0; i<list_len; i++){
        if(delay_diff_ms[i] >= reTx_delay_thd){
            reTx_recv_t_us[cnt] = recv_t_us[i];
            //printf("reTx_recv_t_us:%ld\n", reTx_recv_t_us[cnt]);
            cnt++;
        }        
    }
    *nof_pkt = cnt;
    return reTx_recv_t_us;
} 
int get_nof_reTx_dci(uint8_t*     dci_reTx){
    int cnt = 0; 
    //count the nof of reTx
    for(int i=0; i<NOF_LOG_DCI; i++){
        if(dci_reTx[i] > 0){
            cnt++;
        }        
    }
    return cnt;
}
 
// Here we allocate the memory, remember to free them
uint64_t* extract_reTx_from_dci(uint8_t*     dci_reTx, 
                            uint64_t*   dci_recv_t_us, 
                            int         nof_log_dci,
                            int*        nof_dci)
{
    int cnt = 0; 
    // find and fill 

    //printf("extract_reTx_from_dci -> nof_log_dci:%d\n", nof_log_dci);

    for(int i=0; i<nof_log_dci; i++){
        if(dci_reTx[i] > 0){
            cnt++;
        }        
    }
    uint64_t *reTx_recv_t_us = (uint64_t *)malloc(cnt * sizeof(uint64_t));
    
    cnt = 0;
    for(int i=0; i<nof_log_dci; i++){
        if(dci_reTx[i] > 0){
            reTx_recv_t_us[cnt] = dci_recv_t_us[i];
            cnt++;
        }        
    }
    
    *nof_dci = cnt;
    return reTx_recv_t_us;
} 
int find_vec_interset_time(uint64_t* t1_in, 
                            uint64_t* t2_in,
                            int nof_t1,
                            int nof_t2,
                            uint64_t* start_time,
                            uint64_t* end_time,
                            int* nof_t1_out,
                            int* nof_t2_out)
{
    // figure out the relationship between two vec    
    uint64_t t1_start, t1_end; 
    uint64_t t2_start, t2_end; 
 
    if( (nof_t1 <= 0) || (nof_t2 <= 0) ){
        printf("two vector cannot be empty!\n");
        return -1;
    }
    // set the starting and ending timestamp   
    t1_start    = t1_in[0]; 
    t1_end      = t1_in[nof_t1-1];
    t2_start    = t2_in[0]; 
    t2_end      = t2_in[nof_t2-1];

    if( (t1_start >t2_end) || (t2_start > t1_end)){
        printf("two vector of timestamp has no overlap t1_start:%ld end:%ld t2 start:%ld end:%ld\n",
                    t1_start, t1_end, t2_start, t2_end);
        return -1;
    }

    uint64_t start_t =0, end_t = 0;
    if(t2_end <= t1_end){
        end_t = t2_end;        
    }else{
        end_t = t1_end;
    }

    if(t2_start > t1_start){
        start_t = t2_start;
    }else{
        start_t = t1_start;
    }

    //printf("start:%ld end:%ld !\n", start_t, end_t);

    *start_time = start_t;
    *end_time   = end_t;
    int cnt = 0; 
    for(int i=0; i<nof_t1; i++){
        if( (t1_in[i] >= start_t) && (t1_in[i] <= end_t)){
            cnt++;
        }
    } 
    *nof_t1_out = cnt;

    cnt = 0; 
    for(int i=0; i<nof_t2; i++){
        if( (t2_in[i] >= start_t) && (t2_in[i] <= end_t)){
            cnt++;
        }
    } 
    *nof_t2_out = cnt;
    return 0;   
} 

uint64_t* cut_vec_with_time_edge(uint64_t* t_in,
                                    int nof_in,
                                    uint64_t start_t,
                                    uint64_t end_t,
                                    int nof_out)
{
    int cnt = 0; 
    uint64_t* t_out =(uint64_t *)malloc(nof_out * sizeof(uint64_t));
    for(int i=0; i<nof_in; i++){
        if( (t_in[i] >= start_t) && (t_in[i] <= end_t)){
            t_out[cnt] = t_in[i];
            cnt++;
        }
    }  
    return t_out;
}

int match_dci_pkt_delay_w_offset(uint64_t* dci_t_us, 
                            uint64_t* pkt_t_us, 
                            int  offset_us,
                            int nof_pkt,
                            int nof_dci)
{
    int delay_sum = 0;
    for(int i=0; i<nof_pkt; i++){
        int min_offset = 0;
        for(int j=0; j<nof_dci; j++){
            if( (j==0) && (pkt_t_us[i] < dci_t_us[j]) ){
                break;
            }
            int delay_diff = (int)abs((long)pkt_t_us[i] - (long)(dci_t_us[j] + offset_us));
            if(j ==0) min_offset = delay_diff;
            if(delay_diff < min_offset){
                min_offset = delay_diff;
            }
        } 
        delay_sum += min_offset;
    }
    return delay_sum;
}

int min_in_vec(int* vec, int num){
    int min_v = vec[0];
    int     min_i  = 0;
    for(int i=0; i<num; i++){
        if(vec[i] < min_v){
            min_v = vec[i];
            min_i = i;
        }
    }
    return min_i;
}

void sync_2_recv_t(uint64_t* dci_recv_t_us, 
                    uint64_t* pkt_recv_t_us,
                    int nof_dci,
                    int nof_pkt)
{
    uint64_t delay_anchor;
    if(dci_recv_t_us[0] > pkt_recv_t_us[0]){
        delay_anchor =  pkt_recv_t_us[0];
    }else{
        delay_anchor =  dci_recv_t_us[0];
    }
   
    //printf("dci:%ld pkt:%ld delay_anchor:%ld\n", dci_recv_t_us[0], pkt_recv_t_us[0], delay_anchor); 
 
    //printf("PKT delay:--->");
    for(int i=0; i<nof_pkt; i++){
        pkt_recv_t_us[i] = pkt_recv_t_us[i] - delay_anchor; 
        //printf("%ld ", pkt_recv_t_us[i]);
    }
    //printf("\n");

    //printf("DCI delay:--->");
    for(int i=0; i<nof_dci; i++){
        dci_recv_t_us[i] = dci_recv_t_us[i] - delay_anchor; 
        //printf("%ld ", dci_recv_t_us[i]);
    } 
    //printf("\n");

    //printf("RANGE dci: [%ld, %ld] pkt:[%ld, %ld]\n", dci_recv_t_us[0], dci_recv_t_us[nof_dci-1],
    //                    pkt_recv_t_us[0], pkt_recv_t_us[nof_pkt-1]);
    return;
}
void print_vec(uint64_t* vec, int num){
    for(int i=0; i<num; i++){
        printf("%ld ", vec[i]);
    }
    
    printf("\n");
}
int ngscope_sync_dci_delay(int*         delay_diff_ms, 
                            uint64_t*   recv_t_us, 
                            int         list_len,  
                            uint8_t*    dci_reTx, 
                            uint64_t*   dci_recv_t_us,
                            int         nof_log_dci,
     __attribute__((unused))FILE* fd)
{
    uint64_t* dci_reTx_recv_t_us;  // time of reTx before pruning of intersections
    uint64_t* pkt_reTx_recv_t_us;  // time of reTx before pruning of intersections
    uint64_t* dci_t_us;  // time of reTx after pruning of intersections
    uint64_t* pkt_t_us; // time of reTx after pruning of intersections

    int nof_reTx_pkt = 0;  // before pruning
    int nof_reTx_dci = 0;  // before pruning
    int nof_dci = 0;  // after pruning
    int nof_pkt = 0;
 
    /* Enable this function for debugging purposes.
     * this function synchronizes two vector of timestamps and make the timestamp start from 0
     * so it is easier to debug the timestamps */ 
    //sync_2_recv_t(dci_recv_t_us, recv_t_us, nof_log_dci, list_len);
   
    // Extract the list of reTx timestamps and its recving timestamp from packet statisics 
    pkt_reTx_recv_t_us = extract_reTx_from_pkt(delay_diff_ms, recv_t_us, list_len, &nof_reTx_pkt);

    //printf("%d pkt ReTx recv_t: ->", nof_reTx_pkt);
    //print_vec(pkt_reTx_recv_t_us, nof_reTx_pkt);
    
    // Extract the list of reTx timestamps from dci statisics 
    dci_reTx_recv_t_us = extract_reTx_from_dci(dci_reTx, dci_recv_t_us, nof_log_dci, &nof_reTx_dci);
    //printf("%d dci ReTx recv_t: ->", nof_reTx_dci);
    //print_vec(dci_reTx_recv_t_us, nof_reTx_dci);

    // we need to exit if we didn't observe any retransmissions
    if( (nof_reTx_pkt <=0) || (nof_reTx_dci <=0)){
        printf("We don't find any retransmissions!\n");
        return -1;
    }

    // now we have two vectors of retransmissions, we need to find the intersections of these two vectors
    //printf("Find interset idx reTx-dci:%d reTx-pkt:%d\n", nof_reTx_pkt, nof_reTx_dci);
    uint64_t start_t = 0, end_t = 0;
    if(find_vec_interset_time(dci_reTx_recv_t_us, pkt_reTx_recv_t_us, nof_reTx_dci, nof_reTx_pkt,\
                                &start_t, &end_t, &nof_dci, &nof_pkt) < 0){
        return 0;
    }
    
    // Cut the vectors according to the time window  
    dci_t_us = cut_vec_with_time_edge(dci_reTx_recv_t_us, nof_reTx_dci, start_t, end_t, nof_dci);
    pkt_t_us = cut_vec_with_time_edge(pkt_reTx_recv_t_us, nof_reTx_pkt, start_t, end_t, nof_pkt);

    //log these two vectors
    //for(int i=0; i<nof_dci; i++){
    //    fprintf(fd, "%ld\t", dci_t_us[i]);
    //}
    //fprintf(fd, "\n");
    //
    //for(int i=0; i<nof_pkt; i++){
    //    fprintf(fd, "%ld\t", pkt_t_us[i]);
    //}
    //fprintf(fd, "\n");


    // generally, there will be more ReTx in dci
    int delay_sum[300];
    int offset_vec[300];

    //uint64_t t1, t2;
    //t1 = Socket::timestamp();
    for(int i=0; i<= 200; i++){
        int offset_us   = (i-100) * 100;
        offset_vec[i]   = offset_us;
        //delay_sum[i]    = match_dci_pkt_delay_w_offset(dci_reTx_recv_t_us, pkt_reTx_recv_t_us, offset_us,
                            //nof_reTx_pkt, nof_reTx_dci);

        delay_sum[i]    = match_dci_pkt_delay_w_offset(dci_t_us, pkt_t_us, offset_us,\
                            nof_pkt, nof_dci);

        //fprintf(fd, "%d\t", delay_sum[i]);
    }
    //fprintf(fd, "\n");
    //t2 = Socket::timestamp();
    //printf("Time on ngscope_sync_dci_delay -> searching:%ld\n", (t2-t1) / 1000);

    
    int min_idx = min_in_vec(delay_sum, 201);
    int offset   = offset_vec[min_idx];
    //printf("nof reTx dci:%d pkt:%d offset:%d\n", nof_reTx_dci, nof_reTx_pkt, offset);
      
    free(dci_reTx_recv_t_us);
    free(pkt_reTx_recv_t_us);
    free(dci_t_us);
    free(pkt_t_us);
    
    return offset;
}
