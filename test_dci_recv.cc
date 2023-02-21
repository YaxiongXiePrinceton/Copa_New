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

#include "ngscope_sock.h"
#include "ngscope_dci.h"
#include "ngscope_dci_recv.h"

bool go_exit = false;

ngscope_dci_CA_t dci_ca;
pthread_mutex_t dci_mutex = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char* argv[]) {
	int ngscope_server_sock, ngscope_client_sock;
	int portNum = 6767;
	pthread_t ngscope_recv_t;


	// accept the connection from the clients
	int nof_sock = accept_slave_connect(&ngscope_server_sock, &ngscope_client_sock, portNum);
    printf("\n %d ngscope client connected\n\n", nof_sock);

    //_ue_dci_status.remote_sock = _ngscope_client_sock;
    pthread_create(&ngscope_recv_t, NULL, ngscope_dci_recv_thread, (void *)&ngscope_client_sock);
	sleep(3);

	pthread_join(ngscope_recv_t, NULL);
	close(ngscope_server_sock);
	close(ngscope_client_sock);

}
