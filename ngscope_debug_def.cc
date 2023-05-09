#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <signal.h>

#include "ngscope_debug_def.h"


int ngscope_debug_client_fd_init(client_fd_t* client_fd){
	system("mkdir ./data");	

	if(CLIENT_RX){
		client_fd->fd_client_rx = fopen("./data/client_pkt_rx","w+");
		fclose(client_fd->fd_client_rx);
		client_fd->fd_client_rx = fopen("./data/client_pkt_rx","a+");
	}
	
	if( CLIENT_DELAY){
		client_fd->fd_client_delay = fopen("./data/client_pkt_delay_fix","w+");
		fclose(client_fd->fd_client_delay);
		client_fd->fd_client_delay = fopen("./data/client_pkt_delay_fix","a+");
	}

	if(CLIENT_RETX){
		client_fd->fd_client_reTx = fopen("./data/client_dci_pkt_reTx","w+");
		fclose(client_fd->fd_client_reTx);
		client_fd->fd_client_reTx = fopen("./data/client_dci_pkt_reTx","a+");
	}

	if(CLIENT_DL_reTx){
		client_fd->fd_client_DL_reTx = fopen("./data/client_DL_reTx","w+");
		fclose(client_fd->fd_client_DL_reTx);
		client_fd->fd_client_DL_reTx = fopen("./data/client_DL_reTx","a+");
	}

	if(CLIENT_UL_reTx){
		client_fd->fd_client_UL_reTx = fopen("./data/client_UL_reTx","w+");
		fclose(client_fd->fd_client_UL_reTx);
		client_fd->fd_client_UL_reTx = fopen("./data/client_UL_reTx","a+");
	}

	if(CLIENT_DCI_RX_DL_RETX){
		client_fd->fd_client_DCI_RX_DL_RETX = fopen("./data/client_DCI_rx_DL_reTx","w+");
		fclose(client_fd->fd_client_DCI_RX_DL_RETX);
		client_fd->fd_client_DCI_RX_DL_RETX = fopen("./data/client_DCI_rx_DL_reTx","a+");
	}

	if(CLIENT_DCI_RX_UL_RETX){
		client_fd->fd_client_DCI_RX_UL_RETX = fopen("./data/client_DCI_rx_UL_reTx","w+");
		fclose(client_fd->fd_client_DCI_RX_UL_RETX);
		client_fd->fd_client_DCI_RX_UL_RETX = fopen("./data/client_DCI_rx_UL_reTx","a+");
	}

	return 1;
}


int ngscope_debug_client_fd_close(client_fd_t* client_fd){
	
	if (CLIENT_RX)
		fclose(client_fd->fd_client_rx);

	if(CLIENT_DELAY)
		fclose(client_fd->fd_client_delay);

	if(CLIENT_RETX)
		fclose(client_fd->fd_client_reTx);

	if(CLIENT_DL_reTx){
		fclose(client_fd->fd_client_DL_reTx);
	}

	if(CLIENT_UL_reTx){
		fclose(client_fd->fd_client_UL_reTx);
	}

	if(CLIENT_DCI_RX_DL_RETX){
		fclose(client_fd->fd_client_DCI_RX_DL_RETX);
	}

	if(CLIENT_DCI_RX_UL_RETX){
		fclose(client_fd->fd_client_DCI_RX_UL_RETX);
	}

	return 1;
}



