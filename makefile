pMEMORY_STYLE := ./protobufs-default

CXX := g++
CXXFLAGS := -DHAVE_CONFIG_H -std=c++11 -pthread -pedantic -Wno-comment -Wno-unused-result -Wno-unused-parameter -Wno-unused-variable -Wall -Wextra -Weffc++ -Werror -Wno-deprecated-copy -fno-default-inline -g -O2 -fPIC
INCLUDES :=	-I./protobufs-default -I./udt

LIBS     := -ljemalloc -lm -pthread -lprotobuf -lpthread -ljemalloc
#$(MEMORY_STYLE)/libremyprotos.a

OBJECTS  := random.o memory.o memoryrange.o rat.o whisker.o whiskertree.o udp-socket.o traffic-generator.o remycc.o markoviancc.o estimators.o rtt-window.o 

#NGSCOPE :=ngscope_lib/ngscope_packet_list.o ngscope_lib/ngscope_sync.o ngscope_lib/ngscope_sock.o ngscope_lib/ngscope_dci_recv.o ngscope_lib/ngscope_dci.o ngscope_lib/ngscope_util.o ngscope_lib/ngscope_reTx.o

#protobufs-default/dna.pb.o  

all: sender  test_dci_recv receiver copa_rx copa_rx_fix dci_copa_rx
#all: sender receiver 
#all: sender receiver copa_rx

python_bindings: pygenericcc.so

.PHONY: all python_bindings

protobufs-default/dna.pb.cc: protobufs-default/dna.proto
	protoc --cpp_out=. protobufs-default/dna.proto

protobufs-default/dna.pb.o: protobufs-default/dna.pb.cc
	$(CXX) -I.. -I. -O2 -fPIC -c protobufs-default/dna.pb.cc -o protobufs-default/dna.pb.o

sender: $(OBJECTS) sender.o protobufs-default/dna.pb.o # $(MEMORY_STYLE)/libremyprotos.a
	$(CXX) $(inputs) -o $(output) $(LIBS)

prober: prober.o udp-socket.o
	$(CXX) $(inputs) -o $(output) $(LIBS)

#ngscope_lib/ngscope_packet_list.o: ngscope_lib/ngscope_packet_list.cc
#ngscope_lib/ngscope_sync.o: ngscope_lib/ngscope_sync.cc
#ngscope_lib/ngscope_sock.o: ngscope_lib/ngscope_sock.cc
#ngscope_lib/ngscope_dci_recv.o: ngscope_lib/ngscope_dci_recv.cc
#ngscope_lib/ngscope_dci.o: ngscope_lib/ngscope_dci.cc
#ngscope_lib/ngscope_util.o: ngscope_lib/ngscope_util.cc
#ngscope_lib/ngscope_reTx.o: ngscope_lib/ngscope_reTx.cc
#ngscope_lib/.o: ngscope_lib/.cc

test_dci_recv: test_dci_recv.o  ngscope_sock.o  ngscope_dci.o ngscope_dci_recv.o ngscope_util.o ngscope_reTx.o 
	$(CXX) $(inputs) -Wno-unused-variable  -Wno-unused-parameter -o $(output) $(LIBS) 


receiver: receiver.o udp-socket.o ngscope_packet_list.o ngscope_sync.o  socket.o ngscope_sock.o ngscope_dci_recv.o ngscope_dci.o ngscope_util.o ngscope_reTx.o
	$(CXX) $(inputs) -Wno-unused-variable  -Wno-unused-parameter -o $(output) $(LIBS) 

dci_copa_rx: dci_copa_rx.o udp-socket.o ngscope_packet_list.o ngscope_sync.o  socket.o ngscope_sock.o ngscope_dci_recv.o ngscope_dci.o ngscope_util.o ngscope_reTx.o
	$(CXX) $(inputs) -Wno-unused-variable  -Wno-unused-parameter -o $(output) $(LIBS) 


copa_rx: copa_recv.o udp-socket.o ngscope_packet_list.o ngscope_sync.o socket.o ngscope_sock.o ngscope_dci_recv.o ngscope_dci.o ngscope_util.o ngscope_reTx.o
	$(CXX) $(inputs) -Wno-unused-variable  -Wno-unused-parameter -o $(output) $(LIBS) 

copa_rx_fix: copa_rx_fix.o udp-socket.o ngscope_packet_list.o ngscope_sync.o socket.o ngscope_sock.o ngscope_dci_recv.o ngscope_dci.o ngscope_util.o ngscope_reTx.o
	$(CXX) $(inputs) -Wno-unused-variable  -Wno-unused-parameter -o $(output) $(LIBS) 



python-wrapper.o: python-wrapper.cc
	$(CXX) -I/usr/include/python2.7 $(INCLUDES) -fPIC $(CXXFLAGS) -c python-wrapper.cc -o python-wrapper.o

%.o: %.cc
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $(input) -o $(output)

pygenericcc.so:
	$(CXX) -shared -Wl,--export-dynamic -Wl,--no-undefined python-wrapper.o $(OBJECTS) protobufs-default/dna.pb.o -o pygenericcc.so -lpython2.7 -lboost_python $(LIBS)

pcc-tcp.o: pcc-tcp.cc
	$(CXX) -DHAVE_CONFIG_H -I. -I./udt -std=c++11 -pthread         -fno-default-inline -g -O2 -MT pcc-tcp.o -MD -MP -c -o pcc-tcp.o pcc-tcp.cc
