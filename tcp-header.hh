#ifndef TCP_HEADER_HH
#define TCP_HEADER_HH
struct TCPHeader{
	int seq_num;
	int flow_id;
	int src_id;
	double sender_timestamp;
	uint64_t tx_timestamp;
	double receiver_timestamp;
	int adjust_us;
};
#endif
