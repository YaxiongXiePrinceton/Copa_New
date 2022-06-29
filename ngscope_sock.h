#ifndef NGSCOPE_SOCK_HH
#define NGSCOPE_SOCK_HH

int accept_slave_connect(int* server_fd, int* client_fd_vec, int portNum);
void* recv_ngscope_dci(void* p);
#endif
