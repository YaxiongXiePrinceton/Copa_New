#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string.h>
#include <thread>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "tcp-header.hh"
#include "udp-socket.hh"

#define BUFFSIZE 15000

using namespace std;

// used to lock socket used to listen for packets from the sender
mutex socket_lock; 

// Periodically probes a server with a well known IP address and looks for
// senders that want to connect with this application. In case such a 
// sender exists, tries to punch holes in the NAT (assuming that this 
// process is a NAT).
//
// To be run as a separate thread, so the rest of the code can assume that 
// packets arrive as if no NAT existed.
//
// Currently is only robust if the sender is not behind a NAT, but only the 
// sender needs to be changed to fix this. 

void punch_NAT(string serverip, UDPSocket &sender_socket) {
	const int buffsize = 2048;
	char buff[buffsize];
	UDPSocket::SockAddress addr_holder;
	UDPSocket server_socket;

	server_socket.bindsocket(serverip, 4839, 0);
	while (1) {
		this_thread::sleep_for( chrono::seconds(5) );

		server_socket.senddata(string("Listen").c_str(), 6, serverip, 4839);
		server_socket.receivedata(buff, buffsize, -1, addr_holder);

		char ip_addr[32];
		int port;
		sscanf(buff, "%s:%d", ip_addr, &port);

		int attempt_count = 0;
		const int num_attempts = 500;
		while(attempt_count < num_attempts) {
			socket_lock.lock();
				sender_socket.senddata("NATPunch", 8, string(ip_addr), port);
				// keep the timeout short so that acks are sent in time
				int received = sender_socket.receivedata(buff, buffsize, 5, 
															addr_holder);
			socket_lock.unlock();
			if (received == 0) break;
			++ attempt_count;
		}
		cout << "Could not connect to sender at " << UDPSocket::decipher_socket_addr(addr_holder) << endl;
	}
}

// For each packet received, acks back the pseudo TCP header with the 
// current  timestamp
void echo_packets(UDPSocket &sender_socket) {
	char buff[BUFFSIZE];
	sockaddr_in sender_addr;

	printf("echo packets!\n");

	chrono::high_resolution_clock::time_point start_time_point = \
		chrono::high_resolution_clock::now();

	TCPHeader *header = (TCPHeader*)buff;
	header->receiver_timestamp = \
		chrono::duration_cast<chrono::duration<double>>(
			chrono::high_resolution_clock::now() - start_time_point
		).count()*1000; //in milliseconds

	
	// hardcode the AWS server address	
	sockaddr_in dest_addr;
	dest_addr.sin_port = htons(9004);
    	if (inet_aton("3.22.79.149", &dest_addr.sin_addr) == 0)
    	{
		std::cerr<<"inet_aton failed while sending data. Code: "<<errno<<endl;
    	}
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);
	sender_socket.senddata(buff, sizeof(TCPHeader), &dest_addr);

	while (1) {
		int received __attribute((unused)) = -1;
		while (received == -1) {
			//socket_lock.lock();
				received = sender_socket.receivedata(buff, BUFFSIZE, -1, \
					sender_addr);
			//socket_lock.unlock();
			assert( received != -1 );
		}

		TCPHeader *header = (TCPHeader*)buff;
		header->receiver_timestamp = \
			chrono::duration_cast<chrono::duration<double>>(
				chrono::high_resolution_clock::now() - start_time_point
			).count()*1000; //in milliseconds

		//socket_lock.lock();
			sender_socket.senddata(buff, sizeof(TCPHeader), &sender_addr);
		//socket_lock.unlock();
	}
}

int accept_slave_connect(int* server_fd, int* client_fd_vec, int portNum){
    int server_sockfd;//服务器端套接字
    int client_sockfd;//客户端套接字
    int nof_sock = 0;
    struct sockaddr_in my_addr;   //服务器网络地址结构体
    struct sockaddr_in remote_addr; //客户端网络地址结构体
    unsigned int sin_size;
    memset(&my_addr,0,sizeof(my_addr)); //数据初始化--清零
    my_addr.sin_family=AF_INET; //设置为IP通信
    my_addr.sin_addr.s_addr=INADDR_ANY;//服务器IP地址--允许连接到所有本地地址上
    //my_addr.sin_port=htons(6767); //服务器端口号
    my_addr.sin_port=htons(portNum); //服务器端口号

    /*创建服务器端套接字--IPv4协议，面向连接通信，TCP协议*/
    if((server_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket error");
        return 0;
    }

    *server_fd = server_sockfd;
    //int flags = fcntl(server_sockfd, F_GETFL, 0);
    //fcntl(server_sockfd, F_SETFL, flags | O_NONBLOCK);

    /*将套接字绑定到服务器的网络地址上*/
    if(bind(server_sockfd,(struct sockaddr *)&my_addr,sizeof(struct sockaddr))<0)
    {
        perror("bind error");
        return 0;
    }

    int option = 1;
    if(setsockopt(server_sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
    {
        perror("set sock error");
        return 0;
    }


   /*监听连接请求--监听队列长度为5*/
    if(listen(server_sockfd,5)<0)
    {
        perror("listen error");
        return 0;
    };

    sin_size=sizeof(struct sockaddr_in);
    printf("Waiting for client!\n");
    client_sockfd=accept(server_sockfd,(struct sockaddr *)&remote_addr, &sin_size);
    if(client_sockfd > 0){
        printf("accept client %s\n",inet_ntoa(remote_addr.sin_addr));
        client_fd_vec[nof_sock] = client_sockfd;
        nof_sock += 1;
    }else{
        printf("Cannot find any clients!\n");
    }
    return nof_sock;
}

int connectServer(){
    int client_sockfd;
    struct sockaddr_in remote_addr; //服务器端网络地址结构体
    memset(&remote_addr,0,sizeof(remote_addr)); //数据初始化--清零
    remote_addr.sin_family=AF_INET; //设置为IP通信
   // if(masterIP == NULL){
   //     remote_addr.sin_addr.s_addr=inet_addr("192.168.2.20");//服务器IP地址
   // }else{
   //     remote_addr.sin_addr.s_addr=inet_addr( masterIP );//服务器IP地址
   // }
    remote_addr.sin_addr.s_addr=inet_addr("3.22.79.149");//服务器IP地址
    //remote_addr.sin_addr.s_addr=inet_addr("192.168.1.40");//服务器IP地址
    remote_addr.sin_port=htons(6767); //服务器端口号

    printf("\n\n\n\n\n");
    printf("we are configuring the socket inside WebRTC\n");
    printf("\n\n\n\n\n");

    /*创建客户端套接字--IPv4协议，面向连接通信，TCP协议*/
    if((client_sockfd=socket(PF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket error");
        return 0;
    }
    int prioriety = 7;
    int ret = setsockopt(client_sockfd, SOL_SOCKET, SO_PRIORITY, (void *)&prioriety, sizeof(int));

    if(ret == -1){
        printf("SET socket prioriety failed! ERRNO:%s\n", strerror(errno));
    }

    /*将套接字绑定到服务器的网络地址上*/
    if(connect(client_sockfd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr))<0)
    {
        perror("connect error");
        return 0;
    }
    return client_sockfd;
}

int main(int argc, char* argv[]) {
	int port = 9004;
	if (argc == 2)
		port = atoi(argv[1]);


	UDPSocket sender_socket;
	sender_socket.bindsocket(port);
	
	//thread nat_thread(punch_NAT, nat_ip_addr, ref(sender_socket));
	echo_packets(sender_socket);

	return 0;
}
