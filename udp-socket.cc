#include <arpa/inet.h>
#include <cassert>
#include <errno.h>
#include <iostream>
#include <string.h>

#include "udp-socket.hh"

using namespace std;
void UDPSocket::set_remote_ip(std::string remote_ip, int remote_port){
	ipaddr 	= remote_ip;
	port 	= remote_port;
}
int UDPSocket::bindsocket(string s_ipaddr, int s_port, int sourceport){
	ipaddr = s_ipaddr;
	port = s_port;
	srcport = sourceport;
	if (sourceport == 0) {
		bound = true;
		return 0;
	}
	struct sockaddr_in srcaddr;
	memset(&srcaddr, 0, sizeof(srcaddr));
	srcaddr.sin_family = AF_INET;
	srcaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	srcaddr.sin_port = htons(srcport);
	if (bind(udp_socket, (struct sockaddr *)&srcaddr, sizeof(srcaddr)) < 0) {
		perror("bind");
		return -1;
	} else {
		bound = true;
		return 0;
  	}
}

int UDPSocket::bindsocket(int s_port)
{
	ipaddr = "";
	port = s_port;
 	sockaddr_in addr_struct;
	memset((char *) &addr_struct, 0, sizeof(addr_struct));

    	addr_struct.sin_family = AF_INET;
    	addr_struct.sin_port = htons(port);
    	addr_struct.sin_addr.s_addr = htonl(INADDR_ANY);
 	if (bind(udp_socket , (struct sockaddr*)&addr_struct, sizeof(addr_struct) ) != 0){
 		std::cerr<<"Error while binding socket. Code: "<<errno<<endl;
 		return -1;
 	}
 	bound = true;
 	return 0;
}

// Sends data to the desired address. Returns number of bytes sent if
// successful, -1 if not.
ssize_t UDPSocket::senddata(const char* data, ssize_t size, sockaddr_in *s_dest_addr){
	sockaddr_in dest_addr;
	memset((char *) &dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	if(s_dest_addr == NULL){
		assert(bound); // Socket not bound to an address. Please use 'bindsocket'

	    dest_addr.sin_port = htons(port);

	    if (inet_aton(ipaddr.c_str(), &dest_addr.sin_addr) == 0) 
	    {
	        std::cerr<<"inet_aton failed while sending data. Code: "<<errno<<endl;
	    }
	}
	else{
	    dest_addr.sin_port = ((struct sockaddr_in *)s_dest_addr)->sin_port;
	    dest_addr.sin_addr = ((struct sockaddr_in *)s_dest_addr)->sin_addr;
	}

	int res = sendto(udp_socket, data, size, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
	
	if ( res == -1 ){
		std::cerr<<"Error while sending datagram. Code: "<<errno<<std::endl;
		//assert(false);
		return -1;
	}
	
	return res; // no of bytes sent
}

ssize_t UDPSocket::senddata(const char* data, ssize_t size, string dest_ip, int dest_port){
	sockaddr_in dest_addr;
	memset((char *) &dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(dest_port);
    if (inet_aton(dest_ip.c_str(), &dest_addr.sin_addr) == 0) 
    {
        std::cerr<<"inet_aton failed while sending data. Code: "<<errno<<endl;
    }

    return senddata(data, size, &dest_addr);
}

// Modifies buffer to contain a null terminated string of the received 
// data and returns the received buffer size (or -1 or 0, see below)
//
// Takes timeout in milliseconds. If timeout, returns -1 without 
// changing the buffer. If data arrives before timeout, modifies buffer
// with the received data and returns the places the sender's address
// in other_addr
// 
// If timeout is negative, infinite timeout will be used. If it is 0,
// function will return immediately. Timeout will be rounded up to 
// kernel time granularity, and kernel scheduling delays may cause 
// actual timeout to exceed what is specified
int UDPSocket::receivedata(char* buffer, int bufsize, int timeout, sockaddr_in &other_addr){
	assert(bound); // Socket not bound to an address. Please either use 'bind' or 'sendto'

	unsigned int other_len;

	struct pollfd pfds[1];
	pfds[0].fd = udp_socket;
	pfds[0].events = POLLIN;

	int poll_val = poll(pfds, 1, timeout);
	if( poll_val == 1){
		if(pfds[0].revents & POLLIN){
			other_len = sizeof(other_addr);
			int res = recvfrom( udp_socket, buffer, bufsize, 0, (struct sockaddr*) &other_addr, &other_len );
			if ( res == -1 ){
				std::cerr<<"Error while receiving datagram. Code: "<<errno<<std::endl;
			}
			buffer[res] = '\0'; //terminating null character is not added by default

			return res;
		}
		else{
			std::cerr<<"There was an error while polling. Value of event field: "<<pfds[0].revents<<endl;
			return -1;
		}
	}
	else if ( poll_val == 0){
		return 0; //there was a timeout
	}
	else if ( poll_val == -1 ){
		if ( errno == 4 )
			return receivedata(buffer, bufsize, timeout, other_addr); //to make gprof work
		std::cerr<<"There was an error while polling. Code: "<<errno<<endl;
		return -1;
	}
	else{
		assert( false ); //should never come here
	}
}

int recv_msg_from_sock(int sock, char* buffer, int bufsize, int timeout, uint64_t* timestamp_ns, sockaddr_in* other_addr){

  /* data structure to receive timestamp, source address, and payload */
  struct sockaddr_in remote_addr;
  struct msghdr header;
  struct iovec msg_iovec;

  const int BUF_SIZE = 2048;

  //char msg_payload[ BUF_SIZE ];
  char msg_control[ BUF_SIZE ];
  header.msg_name = other_addr;
  header.msg_namelen = sizeof( remote_addr );
  //msg_iovec.iov_base = msg_payload;
  msg_iovec.iov_base = buffer;
  msg_iovec.iov_len = bufsize;
  header.msg_iov = &msg_iovec;
  header.msg_iovlen = 1;
  header.msg_control = msg_control;
  header.msg_controllen = BUF_SIZE;
  header.msg_flags = 0;

  ssize_t received_len = recvmsg( sock, &header, 0 );
  if ( received_len < 0 ) {
	return received_len;
  }
  if ( received_len > BUF_SIZE ) {
    fprintf( stderr, "Received oversize datagram (size %d) and limit is %d\n",
             static_cast<int>( received_len ), BUF_SIZE );
    exit( 1 );
  }
  /* verify presence of timestamp */
  struct cmsghdr *ts_hdr = CMSG_FIRSTHDR( &header );
  assert( ts_hdr );
  assert( ts_hdr->cmsg_level == SOL_SOCKET );
  assert( ts_hdr->cmsg_type == SO_TIMESTAMPNS );

  struct timespec ts = *(struct timespec *)CMSG_DATA( ts_hdr );
  *timestamp_ns = ts.tv_sec * 1000000000 + ts.tv_nsec;
  return received_len;

}
int UDPSocket::receivedata_w_time(char* buffer, int bufsize, int timeout, uint64_t* timestamp_ns, sockaddr_in &other_addr){
	assert(bound); // Socket not bound to an address. Please either use 'bind' or 'sendto'

	unsigned int other_len;

	struct pollfd pfds[1];
	pfds[0].fd = udp_socket;
	pfds[0].events = POLLIN;

	int poll_val = poll(pfds, 1, timeout);
	if( poll_val == 1){
		if(pfds[0].revents & POLLIN){
			//other_len = sizeof(other_addr);
			//int res = recvfrom( udp_socket, buffer, bufsize, 0, (struct sockaddr*) &other_addr, &other_len );
			int res = recv_msg_from_sock( udp_socket, buffer, bufsize, 0, timestamp_ns, &other_addr);
			if ( res == -1 ){
				std::cerr<<"Error while receiving datagram. Code: "<<errno<<std::endl;
			}
			buffer[res] = '\0'; //terminating null character is not added by default

			return res;
		}
		else{
			std::cerr<<"There was an error while polling. Value of event field: "<<pfds[0].revents<<endl;
			return -1;
		}
	}
	else if ( poll_val == 0){
		return 0; //there was a timeout
	}
	else if ( poll_val == -1 ){
		if ( errno == 4 )
			return receivedata(buffer, bufsize, timeout, other_addr); //to make gprof work
		std::cerr<<"There was an error while polling. Code: "<<errno<<endl;
		return -1;
	}
	else{
		assert( false ); //should never come here
	}
}


void UDPSocket::decipher_socket_addr(sockaddr_in addr, std::string& ip_addr, int& port) {
	ip_addr = inet_ntoa(addr.sin_addr);
	port = ntohs(addr.sin_port);
}

string UDPSocket::decipher_socket_addr(sockaddr_in addr) {
	string ip_addr; int port;
	UDPSocket::decipher_socket_addr(addr, ip_addr, port);
	return ip_addr + ":" + to_string(port);
 }

string UDPSocket::get_ip(){
	return ipaddr;
}
