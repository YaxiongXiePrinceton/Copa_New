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
#include "ngscope_dci_recv.h"
#include "ngscope_util.h"
#include "ngscope_debug_def.h"
extern client_fd_t client_fd;

//using namespace std;
extern bool go_exit;
extern ngscope_dci_CA_t dci_ca;
extern pthread_mutex_t dci_mutex;
extern bool sock_ready;

int nof_dl_reTx_per_cell[4] = {0};
int nof_ul_reTx_per_cell[4] = {0};

int recv_dci_ver1(ue_dci_t* ue_dci, char* recvBuf, int buf_idx, int recvLen){
	uint8_t ul_dl;
	if(buf_idx >= recvLen){
		return -1;
	}
	memcpy(&ul_dl, &recvBuf[buf_idx], sizeof(uint8_t));
	buf_idx += 1;
	//printf("ul_dl:%d buf_idx:%d\n", ul_dl, buf_idx);
	// We have downlink dci transmitted
	if(ul_dl & 0x01){
		if(buf_idx +4 >= recvLen){
			return -1;
		}
		// copy reTx
		memcpy(&ue_dci->dl_reTx, &recvBuf[buf_idx], sizeof(uint8_t));
		buf_idx += 1;

		// downlink transport block size
		uint32_t    tbs = 0;
		memcpy(&tbs, &recvBuf[buf_idx], sizeof(uint32_t));
		tbs     = ntohl(tbs);
		ue_dci->dl_tbs = tbs;
		buf_idx += 4;
	}

	// We have uplink dci transmitted
	if(ul_dl & 0x02){
		if(buf_idx +4 >= recvLen){
			return -1;
		}
		// copy reTx
		memcpy(&ue_dci->ul_reTx, &recvBuf[buf_idx], sizeof(uint8_t));
		buf_idx += 1;

		// downlink transport block size
		uint32_t    tbs = 0;
		memcpy(&tbs, &recvBuf[buf_idx], sizeof(uint32_t));
		tbs     = ntohl(tbs);
		ue_dci->ul_tbs = tbs;
		buf_idx += 4;
	}
	return buf_idx;
}
int recv_config(char* recvBuf, int buf_idx){
	uint8_t nof_cell = 0;

	uint16_t cell_prb[MAX_NOF_RF_DEV];

    memcpy(&nof_cell, &recvBuf[buf_idx], sizeof(uint8_t));
	buf_idx += 1;
	printf("NOF_CELL:%d ", nof_cell);
	
	for(int i=0; i< nof_cell; i++){
		memcpy(&cell_prb[i], &recvBuf[buf_idx], sizeof(uint16_t));
		buf_idx += 2;
		printf("%d-th CELL PRB:%d ", i, cell_prb[i]);
	}
	printf("\n");

	pthread_mutex_lock(&dci_mutex);
	dci_ca.nof_cell = nof_cell;
	for(int i=0; i<nof_cell; i++){
		dci_ca.cell_dci[i].cell_prb = cell_prb[i];
	}
	pthread_mutex_unlock(&dci_mutex);
	return buf_idx;
}
 
int recv_one_dci(char* recvBuf, int buf_idx, int recvLen){
    ue_dci_t    ue_dci;
	memset(&ue_dci, 0, sizeof(ue_dci_t));

	//printf("recv dci! buf_idx:%d\n", buf_idx);
	// the preamable part must be at least 12 bytes long
	if(buf_idx + 11 >= recvLen){
		printf("recv_one_dci: not enough bytes!\n");
		return -1;
	}

	uint8_t 	proto_v;
	// get protocol buffer
	memcpy(&proto_v, &recvBuf[buf_idx], sizeof(uint8_t));
	buf_idx += 1;

	// Get the timestamp
	uint32_t lower_t = 0;
	uint32_t upper_t = 0;
	memcpy(&lower_t, &recvBuf[buf_idx], sizeof(uint32_t));
	lower_t     = ntohl(lower_t);
	buf_idx += 4;

	memcpy(&upper_t, &recvBuf[buf_idx], sizeof(uint32_t));
	upper_t     = ntohl(upper_t);
	buf_idx += 4;
	ue_dci.time_stamp = (uint64_t)lower_t + ((uint64_t)upper_t << 32);

	// Get the tti 
	memcpy(&ue_dci.tti, &recvBuf[buf_idx], sizeof(uint16_t));
	//tti     = ntohl(tti);
	buf_idx += 2;

	uint8_t cell_idx;
	memcpy(&cell_idx, &recvBuf[buf_idx], sizeof(uint8_t));
	buf_idx += 1;
	int ret;
	switch(proto_v){
		case 0:
			// protoco version 0 has length of 11 bytes
			//printf("protocol version 1\n");
			ret = recv_dci_ver1(&ue_dci, recvBuf, buf_idx, recvLen);
			if(ret <0){
				printf("recv_one_dci: not enough bytes for protocol version 0!\n");
				return -1;
			}else{
				buf_idx = ret;
			}
			break;
		default:
			printf("ERROR: unknown protocol version!\n");
			break;
	}
	ue_dci.dl_rv_flag = false;	
	ue_dci.ul_rv_flag = false;	

	if(ue_dci.dl_reTx){
		//printf("CELL:%d TTI:%d timestamp:%ld dl_tbs:%d dl_reTx:%d ul_tbs:%d ul_reTx:%d\n", cell_idx, ue_dci.tti, ue_dci.time_stamp, 
	//			ue_dci.dl_tbs, ue_dci.dl_reTx, ue_dci.ul_tbs, ue_dci.ul_reTx);
		if(CLIENT_DCI_RX_DL_RETX){
			fprintf(client_fd.fd_client_DCI_RX_DL_RETX, "%d\t%ld\t\n", cell_idx, ue_dci.time_stamp);
		}
		nof_dl_reTx_per_cell[cell_idx]++;
	}
	if(ue_dci.ul_reTx){
		nof_ul_reTx_per_cell[cell_idx]++;
		if(CLIENT_DCI_RX_UL_RETX){
			fprintf(client_fd.fd_client_DCI_RX_UL_RETX, "%d\t%ld\t\n", cell_idx, ue_dci.time_stamp);
		}
	}
	//if(ue_dci.ul_reTx){
	//	printf("TTI:%d timestamp:%ld dl_tbs:%d dl_reTx:%d ul_tbs:%d ul_reTx:%d\n", ue_dci.tti, ue_dci.time_stamp, 
	//			ue_dci.dl_tbs, ue_dci.dl_reTx, ue_dci.ul_tbs, ue_dci.ul_reTx);

	//}

	//printf("TTI:%d timestamp:%ld dl_tbs:%d dl_reTx:%d ul_tbs:%d ul_reTx:%d\n", ue_dci.tti, ue_dci.time_stamp, 
	//			ue_dci.dl_tbs, ue_dci.dl_reTx, ue_dci.ul_tbs, ue_dci.ul_reTx);
	pthread_mutex_lock(&dci_mutex);
	ngscope_dci_CA_insert(&dci_ca, &ue_dci, cell_idx);
//	printf("tti:%d  timestamp:%ld cell_idx:%d header:%d len:%d\n", ue_dci.tti, ue_dci.time_stamp, cell_idx,
//		dci_ca.cell_dci[cell_idx].header, dci_ca.cell_dci[cell_idx].nof_logged_dci);
    //printf("Data received! header:%d timestamp:%ld tti:%d reTx:%d tbs:%d nof_dci:%d\n", 
    //            dci_header, time_stamp, tti, reTx, tbs, nof_dci);
   	pthread_mutex_unlock(&dci_mutex);

    return buf_idx;
}

int recv_buffer(char* recvBuf, int idx, int recvLen){
	int buf_idx = idx; 

	/* First, we check the preamble to know the types of data*/
	if( recvBuf[buf_idx] == (char)0xAA && recvBuf[buf_idx+1] == (char)0xAA && \
		recvBuf[buf_idx+2] == (char)0xAA && recvBuf[buf_idx+3] == (char)0xAA ){
		//printf("DCI received!\n");
		buf_idx	+= 4;
		int buf_idx_before = buf_idx;
		buf_idx = recv_one_dci(recvBuf, buf_idx, recvLen);

		if(buf_idx_before == buf_idx){
			// the decoding of the dci failed 
			buf_idx -= 4;
			return -1;
		}
	}else if( recvBuf[buf_idx] == (char)0xBB && recvBuf[buf_idx+1] == (char)0xBB && \
		recvBuf[buf_idx+2] == (char)0xBB && recvBuf[buf_idx+3] == (char)0xBB ){
		printf("Configuration received!\n");
		buf_idx	+= 4;
		buf_idx = recv_config(recvBuf, buf_idx);
	}else if( recvBuf[buf_idx] == (char)0xFF && recvBuf[buf_idx+1] == (char)0xFF && \
		recvBuf[buf_idx+2] == (char)0xFF && recvBuf[buf_idx+3] == (char)0xFF ){
		printf("EXIT!\n");
		go_exit = true;
		return -1;
	}else{
		printf("ERROR: Unknown preamble!\n");
		return -1;
	}
	return buf_idx;
}

int shift_recv_buffer(char* buf, int buf_idx, int recvLen){
	if(buf_idx > recvLen){
		printf("ERROR: SOCKET: recv buffer length configuration error!\n");
		return 0;
	}

	// shift the data inside the buffer  
	int cnt = 0;
	for(int i=buf_idx; i<recvLen; i++){
		buf[cnt] = buf[i];
		cnt++;
	}
	
	return cnt;
}

void* ngscope_dci_recv_thread(void* p){
    //ue_status_t* ue_status  = (ue_status_t*)p;
    //int sock  = ue_status->remote_sock;
    int sock  = *(int *)p;
    int buf_size = 100;
    char recvBuf[100];

	ngscope_dci_CA_init(&dci_ca);

    while(!go_exit){
    	int buf_idx = 0;
        int recvLen = recv(sock, recvBuf, buf_size, 0);
        if(recvLen > 0){
			while(!go_exit){
				int ret = recv_buffer(recvBuf, buf_idx, recvLen);
				if(ret < 0){
					// ignore the buffer
					buf_idx = recvLen; 
					break;
				}else if(ret == recvLen){
					break;
				}else{
					buf_idx = ret;
				}
			}
			//offset = shift_recv_buffer(recvBuf, buf_idx, recvLen);
			printf("recvLen: %d buf_idx: %d \n\n", recvLen, buf_idx);
        }
    }
	for(int i=0; i<4; i++){
		printf("cell:%d DL-reTx:%d UL-reTx:%d \n", i, nof_dl_reTx_per_cell[i], nof_ul_reTx_per_cell[i]);
	}
    pthread_exit(NULL);
}

void* ngscope_dci_recv_udp_thread(void* p){
    //ue_status_t* ue_status  = (ue_status_t*)p;
    //int sock  = ue_status->remote_sock;
    int sockfd;
    char recvBuf[1400];
	struct sockaddr_in servaddr, cliaddr;
	int PORT = 6767;

	ngscope_dci_CA_init(&dci_ca);

	// Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
	
	// Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    
    // Bind socket to server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

	// Receive message from client
    unsigned int len = 0;
	int n;
    len = sizeof(cliaddr);
	// we try to build the connection first: wati for the client to send the connection request to us
	int nof_cli_req = 0;
	while(!go_exit){
		n = recvfrom(sockfd, (char *)recvBuf, 1400, MSG_WAITALL, (struct sockaddr*) &cliaddr, &len);
		if(n > 0){
			printf("recv len:%d \n", n);
			if( recvBuf[0] == (char)0xCC && recvBuf[1] == (char)0xCC && \
				recvBuf[2] == (char)0xCC && recvBuf[3] == (char)0xCC ){
				nof_cli_req++;
			}
		}
		if(nof_cli_req >= 2){
			break;
		}
	}

	// tell the client that we recevied their request
	recvBuf[0] = (char)0xAA;recvBuf[1] = (char)0xAA;
	recvBuf[2] = (char)0xAA;recvBuf[3] = (char)0xAA;

	printf("CONNECTION BUILD!\n");
	sendto(sockfd, (char *)recvBuf, 4, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
	sendto(sockfd, (char *)recvBuf, 4, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);

	// notify others that socket is ready
	sock_ready = true;

    while(!go_exit){
    	int buf_idx = 0;
        int recvLen = 0;

		recvLen = recvfrom(sockfd, (char *)recvBuf, 1400, MSG_WAITALL, (struct sockaddr*) &cliaddr, &len);
        if(recvLen > 0){
			while(!go_exit){
				int ret = recv_buffer(recvBuf, buf_idx, recvLen);
				if(ret < 0){
					// ignore the buffer
					printf("recvLen: %d buf_idx: %d \n\n", recvLen, buf_idx);
					buf_idx = recvLen; 
					break;
				}else if(ret == recvLen){
					break;
				}else{
					buf_idx = ret;
				}
			}
			//offset = shift_recv_buffer(recvBuf, buf_idx, recvLen);
			//printf("recvLen: %d buf_idx: %d \n\n", recvLen, buf_idx);
        }
    }
	for(int i=0; i<4; i++){
		printf("cell:%d DL-reTx:%d UL-reTx:%d \n", i, nof_dl_reTx_per_cell[i], nof_ul_reTx_per_cell[i]);
	}

	close(sockfd);
    pthread_exit(NULL);
}


