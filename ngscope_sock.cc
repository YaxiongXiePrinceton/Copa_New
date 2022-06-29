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

#include "ngscope_sock.h"
//#include "acker.hh"
#include "ngscope_sync.h"

extern bool go_exit;
extern ue_dci_t    dci_vec[NOF_LOG_DCI];
extern int         dci_header;
extern int         nof_dci;
extern pthread_mutex_t dci_mutex;

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

    //int start = time(NULL);
    //int ellaps = 0;
    //while (true){
    //    /*等待客户端连接请求到达*/
    //    client_sockfd=accept(server_sockfd,(struct sockaddr *)&remote_addr, &sin_size);
    //    if(client_sockfd > 0){
    //        printf("accept client %s\n",inet_ntoa(remote_addr.sin_addr));
    //        client_fd_vec[nof_sock] = client_sockfd;
    //        nof_sock += 1;
    //        break;
    //    }
    //    //ellaps = time(NULL);
    //    //if( (ellaps - start > MAX_WAIT_TIME_S) || (nof_sock >= MAX_USRP_NUM ))
    //     //   break;
    //}
    return nof_sock;
}

int recv_one_dci(char* recvBuf, int buf_idx){
    ue_dci_t    ue_dci;
    // Time stamp
    uint32_t lower_t = 0;
    uint32_t upper_t = 0;
    memcpy(&lower_t, &recvBuf[buf_idx], sizeof(uint32_t));
    lower_t     = ntohl(lower_t);
    buf_idx += 4;

    memcpy(&upper_t, &recvBuf[buf_idx], sizeof(uint32_t));
    upper_t     = ntohl(upper_t);
    buf_idx += 4;
    uint64_t time_stamp = (uint64_t)lower_t + ((uint64_t)upper_t << 32);

    uint16_t tti = 0;
    memcpy(&tti, &recvBuf[buf_idx], sizeof(uint16_t));
    //tti     = ntohl(tti);

    buf_idx += 2;

    // Retransmissions
    uint8_t reTx    = 0;
    memcpy(&reTx, &recvBuf[buf_idx], sizeof(uint8_t));
    buf_idx += 1;

    // Transport block size
    uint32_t    tbs = 0;
    memcpy(&tbs, &recvBuf[buf_idx], sizeof(uint32_t));
    tbs     = ntohl(tbs);
    buf_idx += 4;
    ue_dci.time_stamp = time_stamp;
    ue_dci.tbs        = tbs;
    ue_dci.reTx       = reTx;
    ue_dci.tti        = tti;

    pthread_mutex_lock(&dci_mutex);
    memcpy(&dci_vec[dci_header], &ue_dci, sizeof(ue_dci_t));
    dci_header++;
    dci_header = dci_header % NOF_LOG_DCI;
    if(nof_dci < NOF_LOG_DCI){
        nof_dci++;
    }
    //printf("Data received! header:%d timestamp:%ld tti:%d reTx:%d tbs:%d nof_dci:%d\n", \
                dci_header, time_stamp, tti, reTx, tbs, nof_dci);

    //uint64_t delay = dci_vec[dci_header].time_stamp;
    //for(int i=0; i<NOF_LOG_DCI;i++){
    //    int idx = dci_header + i;
    //    idx     = idx % NOF_LOG_DCI;
    //    printf("%d %ld| ", dci_vec[idx].tti, dci_vec[idx].time_stamp - delay);
    //}
    //printf("\n");
    pthread_mutex_unlock(&dci_mutex);

    return buf_idx;
}

void* recv_ngscope_dci(void* p){
    //ue_status_t* ue_status  = (ue_status_t*)p;
    //int sock  = ue_status->remote_sock;
    int sock  = *(int *)p;
    int buf_size = 19;
    char recvBuf[19];
    ue_dci_t    ue_dci;
    while(true){
        if(go_exit) break;
        int recvLen   = recv(sock, recvBuf, buf_size, 0);
        int buf_idx   = 4;
        if(recvLen > 0){
            if( recvBuf[0] == (char)0xAA && recvBuf[1] == (char)0xAA && \
                recvBuf[2] == (char)0xAA && recvBuf[3] == (char)0xAA ){
                //printf("recvLen: %d\n", recvLen);
                recv_one_dci(recvBuf, buf_idx);
            }
        }
    }
    pthread_exit(NULL);
}
