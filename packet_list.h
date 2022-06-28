#ifndef PACKET_LIST_HH
#define PACKET_LIST_HH
//#include "payload.hh"
#include "tcp-header.hh"


typedef struct{
    uint32_t    sequence_number;
    uint32_t    ack_number;
    uint64_t    sent_timestamp;
    int         sender_id;
    uint32_t    recv_len; // packet size
}pkt_header_t;

struct linkedList_t{
    pkt_header_t    pkt_header;
    TCPHeader       tcp_header;
    uint64_t        oneway_us;
    uint64_t        recv_t_us;
    uint64_t        oneway_us_new;

    bool            revert_flag;
    struct linkedList_t* next;
    struct linkedList_t* prev;
};

typedef struct linkedList_t packet_node;

packet_node* createNode();
packet_node* deleteNode(packet_node* head, packet_node* node, packet_node* prev);
void checkTimeDelay_woTime(packet_node* head);
void checkTimeDelay_wTime(packet_node* head, uint64_t curr_t_us);
void insertNode(packet_node* head, packet_node* node);
void insertNode_checkTime(packet_node* head, packet_node* node, FILE* fd);

int listLength(packet_node* head);
void printList(packet_node* head);
int listDelayDiff(packet_node* head, int listLen, int* delay_diff_ms, uint64_t* recv_t);
#endif 

