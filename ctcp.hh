#ifndef REMY_TCP_HH
#define REMY_TCP_HH

#include <assert.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "ccc.hh"
#include "remycc.hh"
#include "tcp-header.hh"
#include "udp-socket.hh"

using namespace std;

#define packet_size 1440
#define data_size (packet_size-sizeof(TCPHeader))

template <class T>
class CTCP {
public:
  enum ConnectionType{ SENDER, RECEIVER };

private:
  T congctrl;
  UDPSocket socket;
  ConnectionType conntype;

  string dstaddr;
  int dstport;
  int srcport;

  int train_length;

  double _last_send_time;

  int _largest_ack;

  double tot_time_transmitted;
  double tot_delay;
  int tot_bytes_transmitted;
  int tot_packets_transmitted;

  void tcp_handshake();

public:

  CTCP( T s_congctrl, string ipaddr, int port, int srcport, int train_length ) 
    :   congctrl( s_congctrl ), 
        socket(), 
        conntype( SENDER ),
        dstaddr( ipaddr ),
        dstport( port ),
        srcport( srcport),
        train_length( train_length ),
        _last_send_time( 0.0 ),
        _largest_ack( -1 ),
        tot_time_transmitted( 0 ),
        tot_delay( 0 ),
        tot_bytes_transmitted( 0 ),
        tot_packets_transmitted( 0 )
  {
    socket.bindsocket( ipaddr, port, srcport );
  }

  CTCP( CTCP<T> &other )
    : congctrl( other.congctrl ),
      socket(),
      conntype( other.conntype ),
      dstaddr( other.dstaddr ),
      dstport( other.dstport ),
      srcport( other.srcport ),
      _last_send_time( 0.0 ),
      _largest_ack( -1 ),
      tot_time_transmitted( 0 ),
      tot_delay( 0 ),
      tot_bytes_transmitted( 0 ),
      tot_packets_transmitted( 0 )
  {
    socket.bindsocket( dstaddr, dstport, srcport );
  }

  //duration in milliseconds
  void send_data ( double flow_size, bool byte_switched, int flow_id, int src_id );

  void listen_for_data ( );
};

#include <string.h>
#include <stdio.h>

#include "configs.hh"

using namespace std;

uint64_t timestamp_ns()
{
  struct timespec ts;

  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;
  return ret;
}

double current_timestamp( chrono::high_resolution_clock::time_point &start_time_point ){
  using namespace chrono;
  high_resolution_clock::time_point cur_time_point = high_resolution_clock::now();
  // convert to milliseconds, because that is the scale on which the
  // rats have been trained
  return duration_cast<duration<double>>(cur_time_point - start_time_point).count()*1000;
}

template<class T>
void CTCP<T>::tcp_handshake() {
  TCPHeader header, ack_header;

  // this is the data that is transmitted. A sizeof(TCPHeader) header followed by a sring of dashes
  char buf[packet_size];
  memset(buf, '-', sizeof(char)*packet_size);
  buf[packet_size-1] = '\0';

  header.seq_num = -1;
  header.flow_id = -1;
  header.src_id = -1;
  header.sender_timestamp = -1;
  header.receiver_timestamp = -1;


  sockaddr_in other_addr;
  double rtt;
  chrono::high_resolution_clock::time_point start_time_point;
  start_time_point = chrono::high_resolution_clock::now();
  double last_send_time = -1e9;
  bool multi_send = false;
 
  if(socket.get_ip() == "0.0"){
	cout<< "Remote unknown! Punching the NAT first!\n" << endl; 
  	while ( true ) {
		int ret = socket.receivedata( buf, packet_size, 200, other_addr );
    		if (ret == 0) {
			cerr << "Waiting sender but could not see anything!" << endl;
      			continue;
		}else if(ret > 0){
			string ip;
			int port;
			UDPSocket::decipher_socket_addr(other_addr, ip, port);
			cout << "Get data from "<< ip << " port: " << port << endl;
			socket.set_remote_ip(ip, port);
			break;
		}
	}
  } 
  while ( true ) {
    double cur_time = current_timestamp(start_time_point);
    if (last_send_time < cur_time - 200) {
      memcpy( buf, &header, sizeof(TCPHeader) );
      socket.senddata( buf, sizeof(TCPHeader) * 2, NULL );

      if (last_send_time != -1e9)
        multi_send = true;
      last_send_time = cur_time;
    }
    if (socket.receivedata( buf, packet_size, 200, other_addr ) == 0) {
      cerr << "Could not establish connection" << endl;
      continue;
    }
    memcpy(&ack_header, buf, sizeof(TCPHeader));
    if (ack_header.seq_num != -1 || ack_header.flow_id != -1)
      continue;
    if (ack_header.sender_timestamp != -1 || ack_header.src_id != -1)
      continue;
    rtt = current_timestamp(start_time_point) - last_send_time;
    break;
  }
  // Set min_rtt only if we are sure we have the right rtt
  if (!multi_send)
    congctrl.set_min_rtt(rtt);
  cout << "Connection Established." << endl; 
}

// takes flow_size in milliseconds (byte_switched=false) or in bytes (byte_switched=true) 
template<class T>
void CTCP<T>::send_data( double flow_size, bool byte_switched, int flow_id, int src_id ){
  TCPHeader header, ack_header;

  // this is the data that is transmitted. A sizeof(TCPHeader) header followed by a sring of dashes
  char buf[packet_size];
  memset(buf, '-', sizeof(char)*packet_size);
  buf[packet_size-1] = '\0';

  // for link logging
  ofstream link_logfile;
  if( LINK_LOGGING )
    link_logfile.open( LINK_LOGGING_FILENAME, ios::out | ios::app );

  // for flow control
  int seq_num = 0;
  _largest_ack = -1;

  // for estimating bottleneck link rate
  double link_rate_estimate = 0.0;
  double last_recv_time = 0.0;

  // for maintaining performance statistics
  double delay_sum = 0;
  int num_packets_transmitted = 0;
  int transmitted_bytes = 0;

  cout << "Assuming training link rate of: " << TRAINING_LINK_RATE << " pkts/sec" << endl;

  // Get min_rtt from outside
  // const char* min_rtt_c = getenv("MIN_RTT");
  // if (min_rtt_c != 0)
  //   congctrl.set_min_rtt(atof(min_rtt_c));

  // For computing timeouts
  tcp_handshake();

  chrono::high_resolution_clock::time_point start_time_point = chrono::high_resolution_clock::now();
  double cur_time = current_timestamp( start_time_point );
  _last_send_time = cur_time;
  double last_ack_time = cur_time;

  cur_time = current_timestamp( start_time_point );
  congctrl.set_timestamp(cur_time);
  congctrl.init();
  system("mkdir ./data");
  FILE* fd_rtt = fopen("./data/rtt.txt","w+");
  FILE* fd_time = fopen("./data/pkt_time.txt","w+");
  fclose(fd_rtt);
  fclose(fd_time);
  fd_rtt = fopen("./data/rtt.txt","a+");
  fd_time = fopen("./data/pkt_time.txt","a+");

  uint64_t recv_time_ns;
 
	while ((byte_switched?(num_packets_transmitted*data_size):cur_time) < flow_size) {
		cur_time = current_timestamp( start_time_point );
		if (cur_time - last_ack_time > 2000) {
		  std::cerr << "Timeout" << std::endl;
		  if ((byte_switched?(num_packets_transmitted*data_size):cur_time) >= flow_size) break;
		  congctrl.set_timestamp(cur_time);
		  congctrl.init();
		  _largest_ack = seq_num - 1;
		  _last_send_time = cur_time;
		  last_ack_time = cur_time; // So we don't timeout repeatedly
		}
		int nof_pkt = 0;
		uint64_t t1 = timestamp_ns();

		double wind_size = congctrl.get_the_window();
		int nof_pkt_out = seq_num - _largest_ack + 1;

		bool b1 = (seq_num < _largest_ack + 1 + congctrl.get_the_window());
		bool b2 = (_last_send_time + congctrl.get_intersend_time() * train_length <= cur_time);


		// transmit the packet 
		//printf("Reason: %d %d wind_size:%f int_send_t:%f nof_pkt:%d\n", b1, b2, wind_size, congctrl.get_intersend_time(), nof_pkt_out);
		// Warning: The number of unacknowledged packets may exceed the congestion window by num_packets_per_link_rate_measurement
		while (((seq_num < _largest_ack + 1 + 5 * congctrl.get_the_window()) &&
				(_last_send_time + congctrl.get_intersend_time() * train_length <= cur_time) &&
				(byte_switched?(num_packets_transmitted*data_size):cur_time) < flow_size ) ||
			   (seq_num % train_length != 0)) {

		  header.seq_num = seq_num;
		  header.flow_id = flow_id;
		  header.src_id = src_id;
		  header.sender_timestamp = cur_time;
		  header.receiver_timestamp = 0;

		  uint64_t timestamp = timestamp_ns();

		//      uint32_t lower_t = htonl( (uint32_t)timestamp);
		//      uint32_t upper_t = htonl( timestamp >> 32);
		  //header.tx_timestamp = (uint64_t)lower_t + ((uint64_t)upper_t << 32);
		  header.tx_timestamp = timestamp;

		  //std::cout << "Seq: " << seq_num << " timestamp" << timestamp << " net:" << header.tx_timestamp<< endl;

		  header.adjust_us = 0;
		  memcpy( buf, &header, sizeof(TCPHeader) );
		  printf("size of header:%ld\n", sizeof(TCPHeader));

		  socket.senddata( buf, packet_size, NULL );
		  _last_send_time += congctrl.get_intersend_time();

		  if (seq_num % train_length == 0) {
			congctrl.set_timestamp(cur_time);
			congctrl.onPktSent( header.seq_num / train_length );
		  }
		  seq_num++;
		  nof_pkt++;
		}
		uint64_t t2 = timestamp_ns();
		if (cur_time - _last_send_time >= congctrl.get_intersend_time() * train_length ||
			seq_num >= _largest_ack + congctrl.get_the_window()) {
		  // Hopeless. Stop trying to compensate.

		  header.seq_num = -1;
		  header.flow_id = -1;
		  header.src_id = src_id;
		  header.sender_timestamp = cur_time;
		  header.receiver_timestamp = 0;

		  uint64_t timestamp = timestamp_ns();

		//      uint32_t lower_t = htonl( (uint32_t)timestamp);
		//      uint32_t upper_t = htonl( timestamp >> 32);
		  //header.tx_timestamp = (uint64_t)lower_t + ((uint64_t)upper_t << 32);
		  header.tx_timestamp = timestamp;


		  header.adjust_us = 0;
		  memcpy( buf, &header, sizeof(TCPHeader) );
		  //printf("size of header:%ld\n", sizeof(TCPHeader));
		  socket.senddata( buf, sizeof(TCPHeader), NULL );
	
		  _last_send_time = cur_time;
		}

		cur_time = current_timestamp( start_time_point );
		double timeout = _last_send_time + 1000; //congctrl.get_timeout(); // everything in milliseconds
		if(congctrl.get_the_window() > 0)
		  timeout = min( 1000.0, _last_send_time + congctrl.get_intersend_time()*train_length - cur_time );

		sockaddr_in other_addr;

		//if(socket.receivedata(buf, packet_size, timeout, other_addr) == 0) {
		if(socket.receivedata_w_time(buf, packet_size, timeout, &recv_time_ns, other_addr) == 0) {
		  cur_time = current_timestamp(start_time_point);
		  if(cur_time > _last_send_time + congctrl.get_timeout())
			congctrl.onTimeout();
		  continue;
		}

		memcpy(&ack_header, buf, sizeof(TCPHeader));
		ack_header.seq_num++; // because the receiver doesn't do that for us yet
		//std::cout << "adjust us:" <<ack_header.adjust_us << endl;

		if (ack_header.src_id != src_id || ack_header.flow_id != flow_id){
		  if(ack_header.src_id != src_id ){
			std::cerr<<"Received incorrect ack for src "<<ack_header.src_id<<" to "<<src_id<<" for flow "<<ack_header.flow_id<<" to "<<flow_id<<endl;
		  }
		  continue;
		}
		cur_time = current_timestamp( start_time_point );
		last_ack_time = cur_time;

		// Estimate link rate
		if ((ack_header.seq_num - 1) % train_length != 0 && last_recv_time != 0.0) {
		  double alpha = 1 / 16.0;
		  if (link_rate_estimate == 0.0)
			link_rate_estimate = 1 * (cur_time - last_recv_time);
		  else
			link_rate_estimate = (1 - alpha) * link_rate_estimate + alpha * (cur_time - last_recv_time);
		  // Use estimate only after enough datapoints are available
		  if (ack_header.seq_num > 2 * train_length)
			congctrl.onLinkRateMeasurement(1e3 / link_rate_estimate );
		}
		last_recv_time = cur_time;

		// Track performance statistics
		delay_sum += cur_time - ack_header.sender_timestamp;
		this->tot_delay += cur_time - ack_header.sender_timestamp;

		transmitted_bytes += data_size;
		this->tot_bytes_transmitted += data_size;

		num_packets_transmitted += 1;
		this->tot_packets_transmitted += 1;

		if ((ack_header.seq_num - 1) % train_length == 0) {

		  double curr_rtt = cur_time - ack_header.sender_timestamp - ((double)ack_header.adjust_us / 1000);
		  uint64_t oneway_ns = recv_time_ns - ack_header.tx_timestamp;

		  fprintf(fd_rtt, "%f\t%f\t%ld\t%d\n", cur_time, curr_rtt, oneway_ns, ack_header.seq_num);

		  congctrl.set_timestamp(cur_time);
		  congctrl.onACK(ack_header.seq_num / train_length,
						 ack_header.receiver_timestamp,
				 ack_header.sender_timestamp,
				 ack_header.adjust_us );
		}

		uint64_t t3 = timestamp_ns();
		if(nof_pkt > 0){
		fprintf(fd_time,"%ld\t%ld\t%ld\t", t1, t2, t3);
		fprintf(fd_time,"%f\t%d\n", timeout,nof_pkt);
	}
#ifdef SCALE_SEND_RECEIVE_EWMA
    //assert(false);
#endif

    _largest_ack = max(_largest_ack, ack_header.seq_num);
  }

  cur_time = current_timestamp( start_time_point );

  congctrl.set_timestamp(cur_time);
  congctrl.close();

  this->tot_time_transmitted += cur_time;

  double throughput = transmitted_bytes/( cur_time / 1000.0 );
  double delay = (delay_sum / 1000) / num_packets_transmitted;

  std::cout<<"\n\nData Successfully Transmitted\n\tThroughput: "<<throughput<<" bytes/sec\n\tAverage Delay: "<<delay<<" sec/packet\n\tCompletion time: " << cur_time / 1000.0 << "sec\n";

  double avg_throughput = tot_bytes_transmitted / ( tot_time_transmitted / 1000.0);
  double avg_delay = (tot_delay / 1000) / tot_packets_transmitted;
  std::cout<<"\n\tAvg. Throughput: "<<avg_throughput<<" bytes/sec\n\tAverage Delay: "<<avg_delay<<" sec/packet\n";

  fclose(fd_rtt);
  fclose(fd_time);

  if( LINK_LOGGING )
    link_logfile.close();
}

template<class T>
void CTCP<T>::listen_for_data ( ){

}

#endif
