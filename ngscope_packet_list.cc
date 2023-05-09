#include <assert.h>
#include <errno.h>
#include <stdio.h>
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

#include "ngscope_packet_list.h"
//#include "acker.hh"
#include "ngscope_sync.h"
#include "ngscope_reTx.h"
#include "ngscope_debug_def.h"

extern client_fd_t client_fd;

packet_node* ngscope_list_createNode(){
    packet_node* temp;
    temp        = (packet_node*)malloc(sizeof(struct linkedList_t));
    temp->next  = NULL;
    temp->prev  = NULL;
    return temp;
}

packet_node* ngscope_list_deleteNode(packet_node* head, packet_node* node, packet_node* prev){
    if(head == NULL || prev == NULL){
        return head;
    } 
    //packet_node* tmp;

    // delete the header
    if(head == node){
        if(head->next != NULL){
            // head is not the only element 
            head = head->next;
            free(head);
        }else{
            // head is the only element
            free(head);
            head = NULL;
        }
    }else{
        // head is different from the node
        if(node->next != NULL){
            // node is not the last element
            prev->next = node->next;    
            free(node);
        }else{
            // node is the last element
            prev->next = NULL;    
            free(node);
        }
    }
    return head;
}

packet_node* findLastNode(packet_node* head){
    packet_node* p;
    if(head->next == NULL){
        return head;
    }
    p = head;
    while(p->next != NULL){
        p = p->next;
    }
    return p; 
}

void delete_1st_element(packet_node* head){
    packet_node* p = head->next->next;

    if(head->next == NULL){
        return;
    }

	if(CLIENT_DELAY){
		fprintf(client_fd.fd_client_delay, "%ld\t%ld\t%ld\t%d\t%d\n", head->next->recv_t_us, head->next->oneway_us, \
			head->next->oneway_us_new, head->next->pkt_header.sequence_number, head->next->burst_start);
	}

    free(head->next);

    if(p != NULL){
        head->next = p;
        p->prev = head;
    }else{
        head->next = NULL;
    } 
    //printf("delete 1 element!\n");
    return;
}

// Make sure that the packets in the list are within a window 
// Here, we don't require the current timestamp as the input 
void ngscope_list_checkTimeDelay_woTime(packet_node* head){
    packet_node* last = findLastNode(head);
    //packet_node* p;

    uint64_t delay_thd_us   = NOF_LOG_DCI * 1000;
    uint64_t curr_t_us      = last->recv_t_us; 

    if(head->next == NULL){
        return;
    }

    //p = head->next;
    //printf("delay_thd_us:%d \n", delay_thd_us);
    while((head->next != NULL) || (curr_t_us - head->recv_t_us > delay_thd_us) ){
        // delay larger than the threshold, then delete the header 
        delete_1st_element(head);
    } 

    return;
}
// Re-implement the above function but with current timestamp as input
// Make sure that the packets in the list are within a window 
void ngscope_list_checkTimeDelay_wTime(packet_node* head, uint64_t curr_t_us){
    uint64_t delay_thd_us   = NOF_LOG_DCI * 1000;
    packet_node* p;
    if(head == NULL){
        return;
    }
    //printf("delay_thd_us:%ld \n", delay_thd_us);

    p = head->next;
    // head is empty or the delay of the head is within the window
    while((head->next != NULL) && (p->next != NULL) && \
                (curr_t_us - p->recv_t_us > delay_thd_us) ){
        // delay larger than the threshold, then delete the header 
        delete_1st_element(head);
        p = head->next;
    } 
    return;
}

void ngscope_list_insertNode(packet_node* head, packet_node* node){
    packet_node* p;
    // node cannot be empty
    if(node == NULL || head == NULL){
        return;
    }

    p = findLastNode(head);
    if(p->recv_t_us > node->recv_t_us){
        printf("ERROR: the receiving timestamp of packets must increase monotonically!\n");
        return;
    }
    p->next = node;
    node->prev = p;
    return;
}

int ngscope_list_length(packet_node* head){
    int len = 0;
    packet_node* p; 
    if(head->next == NULL){
        return 0;
    }
   
    p = head->next; 
    while(p->next != NULL){
        len++;
        p = p->next;
    }
    return len;
}

void ngscope_list_insertNode_checkTime(packet_node* head, packet_node* node){
    packet_node* p;
    uint64_t curr_t_us;
    // node cannot be empty
    if(node == NULL || head == NULL){
        printf("Head and node to be inserted cannot be null! Return!\n");
        return;
    }

    // if the list is empty, insert and leave
    if(head->next == NULL){
        head->next = node;
        node->prev = head;
        printf("The only node in the list!\n");
        return;
    }

    //find the current timestamp
    curr_t_us = node->recv_t_us;
    // the timestamp in the list must increase  monotonically
    p = findLastNode(head);
    if(p->recv_t_us > node->recv_t_us){
        printf("ERROR: the receiving timestamp of packets must increase monotonically!\n");
        return;
    }

    // insert the node
    p->next = node;
    node->prev = p;
    // Since the tree is not empty, lets check the time delay 
    ngscope_list_checkTimeDelay_wTime(head, curr_t_us);
    return;
}

void ngscope_list_revert_delay(packet_node* head, ngscope_reordering_buf_t* q){
    packet_node* p;
    p = head->next; 
	//uint64_t prev_delay = p->oneway_us;
    while(p != NULL){
		// found the burst start pkt
		if((p->recv_t_us == q->start_pkt_t) && (p->revert_flag == false) && q->buffer_active){
			//p->oneway_us_new 	= p->oneway_us - 8000;
			p->oneway_us_new 	= q->ref_oneway;
			p->revert_flag 		= true;
			p->burst_start 		= true;

			//q->ref_oneway 	 	= p->oneway_us_new;
			q->buf_size 		-= p->pkt_header.recv_len_byte * 8;
			//printf("found the first pkt: ref_oneway:%ld buf_size:%ld delete size:%d\n", q->ref_oneway, q->buf_size, p->pkt_header.recv_len_byte * 8);
		}

		// pkt later than the burst start pkt
		if((p->recv_t_us > q->start_pkt_t) && (p->revert_flag == false) && q->buffer_active){
			p->oneway_us_new 	= q->ref_oneway;
			p->revert_flag 		= true;
			q->buf_size 		-= p->pkt_header.recv_len_byte * 8;
			//printf("Handling following pkts: ref_oneway:%ld buf_size:%ld delete size:%d\n", q->ref_oneway, q->buf_size, p->pkt_header.recv_len_byte * 8);
		}

		if(q->buf_size <= 0){
			printf("reset the reTx buffer!\n");
			ngscope_reTx_reset_buf(q);
			break;
		}
		//prev_delay = p->oneway_us;
    	p = p->next;
    }
	return;
}

void ngscope_list_printList(packet_node* head){
    packet_node* p;
    p = head->next; 
    while(p != NULL){
        printf("%ld ", p->oneway_us);
        p = p->next;
    }
    printf("\n");
}

int listDelayDiff(packet_node* head, int listLen, int* delay_diff_ms, uint64_t* recv_t_us){
    packet_node* p  = head->next;
    if(p == NULL) 
        return 0;
    delay_diff_ms[0]   = 0; 
    recv_t_us[0]       = p->recv_t_us;
    uint64_t last_oneway = p->oneway_us;
    uint64_t next_oneway;

    //printf("listDelayDiff:listLen :%d ---> ", listLen);
    for(int i=1; i<listLen; i++){
        p  = p->next;
        if(p == NULL){
            printf("Something Wrong in calculating the list length!\n");
            break;
        }
        next_oneway         = p->oneway_us;
        recv_t_us[i]        = p->recv_t_us;
        delay_diff_ms[i]    = (int)abs((long)next_oneway - (long)last_oneway) / 1000;  // ms
        last_oneway         = next_oneway;
        //printf("%d  %ld |", delay_diff_ms[i], recv_t_us[i]);
    }
    //printf("\n");

    return listLen;
}


int get_nof_reTx_pkt(int* delay_diff_ms, int list_len){
    int cnt = 0; 
	/* We think that there is a packet retransmission,
	 * If the delay difference is larger than RE_TX_DELY_THD */
 
    for(int i=0; i<list_len; i++){
        if(delay_diff_ms[i] >= RE_TX_DELAY_THD){
            cnt++;
        }        
    }
    return cnt;
} 


// Here we allocate the memory, remember to free them
int extract_reTx_from_pkt(int* 			delay_diff_ms, 
                            uint64_t*   recv_t_us, 
                            uint64_t*   reTx_recv_t_us, 
                            int         list_len)
{

    int cnt = 0; 
      //printf("extract_reTx_from_pkt list_len:%d reTx:%d\n", list_len, cnt);
    //uint64_t *reTx_recv_t_us = (uint64_t *)malloc(cnt * sizeof(uint64_t));

    for(int i=0; i<list_len; i++){
        if(delay_diff_ms[i] >= RE_TX_DELAY_THD){
            reTx_recv_t_us[cnt] = recv_t_us[i];
            //printf("reTx_recv_t_us:%ld\n", reTx_recv_t_us[cnt]);
            cnt++;
        }        
    }
    return 0;
} 


/* Target: get the timestamp of retransmitted packets
 * step 1: Calculate the delay difference between two consecutive packets
 * step 2: find the packets with delay difference is larger than the threshold
 * step 3: those found packets are the packets with retransmissions */
uint64_t* ngscope_list_get_reTx(packet_node* head, uint64_t* recv_t_us, uint64_t* oneway_us, int listLen, int* nof_reTx_pkt){
    packet_node* p  = head->next;
    if(p == NULL)  return NULL; // list cannot be empty
 	
	// how many packets in the list
	//int  listLen 		= ngscope_list_length(head); 
	int* delay_diff_ms  = (int *)malloc(listLen * sizeof(int));
	//uint64_t* recv_t_us = (uint64_t *)malloc(listLen * sizeof(uint64_t));

    uint64_t last_oneway = p->oneway_us;
    uint64_t next_oneway;

	// Get the first element in the array
    delay_diff_ms[0]    = 0;
    recv_t_us[0]        = p->recv_t_us;
	oneway_us[0] 	= p->oneway_us;
    //printf("listDelayDiff:listLen :%d ---> ", listLen);
    for(int i=1; i<listLen; i++){
        p  = p->next;
        if(p == NULL){
            printf("Something Wrong in calculating the list length!\n");
            break;
        }
        next_oneway         = p->oneway_us;
        recv_t_us[i]        = p->recv_t_us;
        delay_diff_ms[i]    = (int)abs((long)next_oneway - (long)last_oneway) / 1000;  // ms
        last_oneway         = next_oneway;
		oneway_us[i] 		= p->oneway_us;
    }

	// count the nof retranmissions
	int nof_reTx = get_nof_reTx_pkt(delay_diff_ms, listLen);
	*nof_reTx_pkt = nof_reTx;

	// allocate the memory for storing the timestamp of the retransmitted packets
    uint64_t *reTx_recv_t_us = (uint64_t *)malloc(nof_reTx * sizeof(uint64_t));

	// extract the timestamps of the retransmitted packets
	extract_reTx_from_pkt(delay_diff_ms, recv_t_us, reTx_recv_t_us, listLen);

	
	free(delay_diff_ms);
	//free(recv_t_us);
	return reTx_recv_t_us;
}

uint64_t* ngscope_list_get_pkt_t(packet_node* head, int*  nof_pkt){
    packet_node* p  = head->next;
    if(p == NULL)  return NULL; // list cannot be empty
 	
	// how many packets in the list
	int  listLen 		= ngscope_list_length(head); 
	*nof_pkt 	= listLen; 
	uint64_t* recv_t_us = (uint64_t *)malloc(listLen * sizeof(uint64_t));
	
	for(int i=1; i<listLen; i++){
        p  = p->next;
        if(p == NULL){
            printf("Something Wrong in calculating the list length!\n");
            break;
        }
        recv_t_us[i]        = p->recv_t_us;
    }
	return recv_t_us;
}

uint64_t ngscope_list_ave_oneway(uint64_t* oneway, int nof_pkt, int index, int len){
	if(index >= nof_pkt || len > nof_pkt){
		printf("WRONG config when ave oneway!\n");
	}
	uint64_t sum 	= 0;
	int cnt 		= 0;
	if(index - len <0){
		for(int i=0; i<index-1; i++){
			sum += oneway[i];
			cnt += 1;
		}
	}else{
		for(int i=index -len; i<index-1; i++){
			sum += oneway[i];
			cnt += 1;
		}
	}
	return sum / cnt;	
}

