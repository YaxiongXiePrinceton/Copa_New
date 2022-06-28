#ifndef UDP_SOCKET_HH
#define UDP_SOCKET_HH

#include <string>

#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

class UDPSocket{
public:
	typedef sockaddr_in SockAddress;
private:
	int udp_socket;

	std::string ipaddr;
	int port;
  	int srcport;

	bool bound;
public:
	UDPSocket() : udp_socket(-1), ipaddr(), port(), srcport(), bound(false) {
		udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
		/* Ask for timestamps */
		int ts_opt = 1;
		if ( setsockopt( udp_socket, SOL_SOCKET, SO_TIMESTAMPNS, &ts_opt, sizeof( ts_opt ) )
									< 0 ) {
			perror( "setsockopt" );
			exit( 1 );
		}
	}

	int bindsocket(std::string ipaddr, int port, int srcport);
	int bindsocket(int port);
	ssize_t senddata(const char* data, ssize_t size, SockAddress *s_dest_addr);
	ssize_t senddata(const char* data, ssize_t size, std::string dest_ip, int dest_port);
	int receivedata(char* buffer, int bufsize, int timeout, SockAddress &other_addr);
	int receivedata_w_time(char* buffer, int bufsize, int timeout, uint64_t* timestamp, SockAddress &other_addr);

	static void decipher_socket_addr(SockAddress addr, std::string& ip_addr, int& port);
	static std::string decipher_socket_addr(SockAddress addr);
	void set_remote_ip(std::string remote_ip, int remote_port);
	std::string get_ip();
};

#endif
