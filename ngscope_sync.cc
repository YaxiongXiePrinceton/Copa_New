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
#include "ngscope_dci.h"
#include "ngscope_packet_list.h"
#include "ngscope_util.h"

#define DELAY_DIFF_THD 1400
//#include "acker.hh"

// The implementation of the dci synchronization 
// array1 -> pkt timestamp vector
// array2 -> dci timestamp vector
int ngscope_sync_dci_pkt(uint64_t* array1, uint64_t* array2, int array_size1, int array_size2, int* offset_vec, int offset_size){
	long dist_min =0;
	int idx = 0;
    for(int i=0; i<offset_size; i++){
        int offset_us 	= offset_vec[i];
		long dist = correlate_2_vec_w_offset(array1, array2, array_size1, array_size2, offset_us);
		//printf("offset_us:%d dist:%ld ", offset_us, dist);
		if(i==0){
			dist_min = dist;
		}else{
			if(dist < dist_min){
				dist_min 	= dist;
				idx 		= i;
			}
		}
	}
	//printf("\n");
	return offset_vec[idx];
}

