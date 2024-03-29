#ifndef NGSCOPE_SYNC_HH
#define NGSCOPE_SYNC_HH
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
#include "ngscope_sync.h"
#include "ngscope_packet_list.h"
#include "ngscope_util.h"

typedef struct{
	uint64_t start;
	uint64_t end;
}time_seg_t;

int ngscope_sync_dci_pkt(uint64_t* array1, uint64_t* array2, int array_size1, int array_size2, int* offset_vec, int offset_size);
 
#endif
