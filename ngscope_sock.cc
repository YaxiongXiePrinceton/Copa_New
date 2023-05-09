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


