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

#include "packet_list.h"
//#include "acker.hh"
#include "ngscope_sync.h"

packet_node* createNode(){
    packet_node* temp;
    temp        = (packet_node*)malloc(sizeof(struct linkedList_t));
    temp->next  = NULL;
    temp->prev  = NULL;
    return temp;
}

packet_node* deleteNode(packet_node* head, packet_node* node, packet_node* prev){
    if(head == NULL || prev == NULL){
        return head;
    } 
    packet_node* tmp;

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

void delete_1st_element(packet_node* head, FILE* fd){
    packet_node* p = head->next->next;

    if(head->next == NULL){
        return;
    }

    fprintf(fd, "%ld\t%ld\t%ld\t%d\n", head->next->recv_t_us, head->next->oneway_us, \
		head->next->oneway_us_new, head->next->pkt_header.sequence_number);
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
void checkTimeDelay_woTime(packet_node* head){
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
        delete_1st_element(head, NULL);
    } 

    return;
}
// Re-implement the above function but with current timestamp as input
// Make sure that the packets in the list are within a window 
void checkTimeDelay_wTime(packet_node* head, uint64_t curr_t_us, FILE* fd){
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
        delete_1st_element(head, fd);
        p = head->next;
    } 
    return;
}

void insertNode(packet_node* head, packet_node* node){
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

int listLength(packet_node* head){
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

void insertNode_checkTime(packet_node* head, packet_node* node, FILE* fd){
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
    checkTimeDelay_wTime(head, curr_t_us, fd);
    return;
}

void printList(packet_node* head){
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

