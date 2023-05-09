#ifndef PACKET_LIST_HH
#define PACKET_LIST_HH
#include <unistd.h>
#include <stdint.h>


//#include "payload.hh"
#include "tcp-header.hh"
#include "ngscope_reTx.h"

#define RE_TX_DELAY_THD 7

typedef struct{
    uint32_t    sequence_number;
    uint32_t    ack_number;
    uint64_t    sent_timestamp;
    int         sender_id;
    uint32_t    recv_len_byte; // packet size
}pkt_header_t;

struct linkedList_t{
    pkt_header_t    pkt_header;
   	TCPHeader       tcp_header;
    uint64_t        oneway_us;
    uint64_t        recv_t_us;
    uint64_t        oneway_us_new;

    bool            revert_flag;
    bool            acked;

	bool 			burst_start;

    struct linkedList_t* next;
    struct linkedList_t* prev;
};

typedef struct linkedList_t packet_node;


/* Functions related to the list itself,
 * including create, delete the list, inserting one node, etc */
packet_node* ngscope_list_createNode();
packet_node* ngscope_list_deleteNode(packet_node* head, packet_node* node, packet_node* prev);

void ngscope_list_checkTimeDelay_woTime(packet_node* head);
void ngscope_list_checkTimeDelay_wTime(packet_node* head, uint64_t curr_t_us);
void ngscope_list_insertNode(packet_node* head, packet_node* node);
void ngscope_list_insertNode_checkTime(packet_node* head, packet_node* node);
void ngscope_list_revert_delay(packet_node* head, ngscope_reordering_buf_t* q);

uint64_t ngscope_list_ave_oneway(uint64_t* oneway, int nof_pkt, int index, int len);

int ngscope_list_length(packet_node* head);
void ngscope_list_printList(packet_node* head);
//int listDelayDiff(packet_node* head, int listLen, int* delay_diff_ms, uint64_t* recv_t);
uint64_t* ngscope_list_get_reTx(packet_node* head, uint64_t* recv_t_us, uint64_t* oneway_us, int listLen, int* nof_reTx_pkt);
#endif 

