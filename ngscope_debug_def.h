#ifndef NGSCOPE_DEBUG_H
#define NGSCOPE_DEBUG_H

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <signal.h>

#define CLIENT_RX true
#define CLIENT_DELAY true
#define CLIENT_RETX true

#define CLIENT_DL_reTx true  // log when we found a new dl reTx in packet handling
#define CLIENT_UL_reTx false  // log when we found a new dl reTx in packet handling

#define CLIENT_DCI_RX_DL_RETX true
#define CLIENT_DCI_RX_UL_RETX false 

struct client_fd_t{
	FILE* fd_client_rx; // Log the client side packet receving status
	FILE* fd_client_delay; // Log the client side packet delay handlings
	FILE* fd_client_reTx;  // Log the client side re transmission

	FILE* fd_client_DL_reTx;
	FILE* fd_client_UL_reTx;

	FILE* fd_client_DCI_RX_DL_RETX;
	FILE* fd_client_DCI_RX_UL_RETX;
};

int ngscope_debug_client_fd_init(client_fd_t* client_fd);
int ngscope_debug_client_fd_close(client_fd_t* client_fd);

#endif

