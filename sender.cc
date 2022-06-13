#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "congctrls.hh"
#include "remycc.hh"
#include "ctcp.hh"
#include "kernelTCP.hh"
#include "markoviancc.hh"
#include "traffic-generator.hh"

// see configs.hh for details
double TRAINING_LINK_RATE = 4000000.0/1500.0;
bool LINK_LOGGING = false;
std::string LINK_LOGGING_FILENAME;

int accept_slave_connect(int* server_fd, int* client_fd_vec, int portNum, char* ip){
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
	strcpy(ip, inet_ntoa(remote_addr.sin_addr));
        client_fd_vec[nof_sock] = client_sockfd;
        nof_sock += 1;
    }else{
        printf("Cannot find any clients!\n");
    }
    return nof_sock;
}

int main( int argc, char *argv[] ) {
	Memory temp;
	WhiskerTree whiskers;
	bool ratFound = false;

	string serverip = "";
	int serverport=8888;
  	int sourceport=0;
	int offduration=5000, onduration=5000;
	string traffic_params = "";
	// for MarkovianCC
	string delta_conf = "";
	// length of packet train for estimating bottleneck bandwidth
	int train_length = 1;

	enum CCType { REMYCC, TCPCC, KERNELCC, PCC, NASHCC, MARKOVIANCC } cctype = REMYCC;

	for ( int i = 1; i < argc; i++ ) {
		std::string arg( argv[ i ] );
		if ( arg.substr( 0, 3 ) == "if=" ) {
			if( cctype != REMYCC ) {
				fprintf( stderr, "Warning: ignoring parameter 'if=' as cctype is not 'remy'.\n" );
				continue;
			}

			std::string filename( arg.substr( 3 ) );
			int fd = open( filename.c_str(), O_RDONLY );
			if ( fd < 0 ) {
				perror( "open" );
				exit( 1 );
			}

			RemyBuffers::WhiskerTree tree;
			if ( !tree.ParseFromFileDescriptor( fd ) ) {
				fprintf( stderr, "Could not parse %s.\n", filename.c_str() );
				exit( 1 );
			}
	
			whiskers = WhiskerTree( tree );
			ratFound = true;

			if ( close( fd ) < 0 ) {
				perror( "close" );
				exit( 1 );
			}
		}
		else if( arg.substr( 0, 9 ) == "serverip=" )
			serverip = arg.substr( 9 );
		else if( arg.substr( 0, 11 ) == "serverport=" )
			serverport = atoi( arg.substr( 11 ).c_str() );
		else if( arg.substr( 0, 11 ) == "sourceport=" )
			sourceport = atoi( arg.substr( 11 ).c_str() );
		else if( arg.substr( 0, 12 ) == "offduration=" )
			offduration	= atoi( arg.substr( 12 ).c_str() );
		else if( arg.substr( 0, 11 ) == "onduration=" )
			onduration = atoi( arg.substr( 11 ).c_str() );
		else if( arg.substr( 0, 9 ) == "linkrate=" ) 
			TRAINING_LINK_RATE = atof( arg.substr( 9 ).c_str() );
		else if( arg.substr( 0, 8 ) == "linklog=" ) {
			LINK_LOGGING_FILENAME = arg.substr( 8 );
			LINK_LOGGING = true;
		}
		else if( arg.substr( 0, 15 ) == "traffic_params=")
			traffic_params = arg.substr( 15 );
		else if (arg.substr( 0, 11) == "delta_conf=")
			delta_conf = arg.substr( 11 );
		else if (arg.substr( 0, 13 ) == "train_length=")
			train_length = atoi(arg.substr( 13 ).c_str());
		else if( arg.substr( 0, 7 ) == "cctype=" ) {
			std::string cctype_str = arg.substr( 7 );
			if( cctype_str == "remy" )
				cctype = CCType::REMYCC;
			else if( cctype_str == "tcp" )
				cctype = CCType::TCPCC;
			else if ( cctype_str == "kernel" )
				cctype = CCType::KERNELCC;
			else if ( cctype_str == "pcc" )
				cctype = CCType::PCC;
			else if ( cctype_str == "nash" )
				cctype = CCType::NASHCC;
			else if (cctype_str == "markovian")
				cctype = CCType::MARKOVIANCC;
			else
				fprintf( stderr, "Unrecognised congestion control protocol '%s'.\n", cctype_str.c_str() );
		}
		else {
			fprintf( stderr, "Unrecognised option '%s'.\n", arg.c_str() );
		}
	}
	
		
        char send_buf[20];
        char recv_buf[20];
        int server_fd = 0, client_fd = 0, portNum = 6767;
        send_buf[0] = (char)0xAA;
        send_buf[1] = (char)0xAA;
        send_buf[2] = (char)0xAA;
        send_buf[3] = (char)0xAA;
	char ip[32];
        accept_slave_connect(&server_fd, &client_fd, portNum, ip);
	serverip = ip;

        while(true){
                int recvLen = recv(server_fd, recv_buf, 20, 0);
                if(recvLen > 0){
                        if(recv_buf[0] == (char)0xAA && recv_buf[0] == (char)0xAA &&
                                recv_buf[0] == (char)0xAA && recv_buf[0] == (char)0xAA){
				printf("Recv from Client!\n");
                                break;
                        }
                }
        }
        send(server_fd, send_buf, 20, 0);
        close(server_fd);
        close(client_fd);
	
	if ( serverip == "" ) {
		fprintf( stderr, "Usage: sender serverip=(ipaddr) [if=(ratname)] [offduration=(time in ms)] [onduration=(time in ms)] [cctype=remy|kernel|tcp|markovian] [delta_conf=(for MarkovianCC)] [traffic_params=[exponential|deterministic],[byte_switched],[num_cycles=]] [linkrate=(packets/sec)] [linklog=filename] [serverport=(port)]\n");
		exit(1);
	}

	if( not ratFound and cctype == CCType::REMYCC ) {
		fprintf( stderr, "Please specify remy specification file using if=<filename>\n" );
		exit(1);
	}

	if( cctype == CCType::REMYCC) {
		fprintf( stdout, "Using RemyCC.\n" );
		RemyCC congctrl( whiskers );
		CTCP< RemyCC > connection( congctrl, serverip, serverport, sourceport, train_length );
		TrafficGenerator<CTCP<RemyCC>> traffic_generator( connection, onduration, offduration, traffic_params );
		traffic_generator.spawn_senders( 1 );
	}
	else if( cctype == CCType::TCPCC ) {
		fprintf( stdout, "Using UDT's TCP CC.\n" );
		DefaultCC congctrl;
		CTCP< DefaultCC > connection( congctrl, serverip, serverport, sourceport, train_length );
		TrafficGenerator< CTCP< DefaultCC > > traffic_generator( connection, onduration, offduration, traffic_params );
		traffic_generator.spawn_senders( 1 );
	}
	else if ( cctype == CCType::KERNELCC ) {
		fprintf( stdout, "Using the Kernel's TCP using sockperf.\n");
		KernelTCP connection( serverip, serverport );
		TrafficGenerator< KernelTCP > traffic_generator( connection, onduration, offduration, traffic_params );
		traffic_generator.spawn_senders( 1 );
	}
	else if ( cctype == CCType::PCC ) {
		fprintf( stdout, "Using PCC.\n" );
		fprintf( stderr, "PCC not supported.\n" );
		assert( cctype != CCType::PCC );
		//PCC_TCP connection( serverip, serverport );
		//TrafficGenerator< PCC_TCP > traffic_generator( connection, onduration, offduration, traffic_params );
		//traffic_generator.spawn_senders( 1 );
	}
	else if ( cctype == CCType::NASHCC ) {
		fprintf ( stderr, "NashCC Deprecated. Use MarkovianCC.\n" );
		assert( cctype != CCType::NASHCC );
	}
	else if ( cctype == CCType::MARKOVIANCC ){
		fprintf( stdout, "Using MarkovianCC.\n");
		MarkovianCC congctrl(1.0);
		assert(delta_conf != "");
		congctrl.interpret_config_str(delta_conf);
		CTCP< MarkovianCC > connection( congctrl, serverip, serverport, sourceport, train_length );
		TrafficGenerator< CTCP< MarkovianCC > > traffic_generator( connection, onduration, offduration, traffic_params );
		traffic_generator.spawn_senders( 1 );
	}
	else{
		assert( false );
	}
}
