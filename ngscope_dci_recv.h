#ifndef NGSCOPE_DCI_RECV_HH
#define NGSCOPE_DCI_RECV_HH
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

void* ngscope_dci_recv_thread(void* p);
void* ngscope_dci_recv_udp_thread(void* p);

#endif
