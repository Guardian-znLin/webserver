#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <libgen.h>
#include "../header/locker.h"
#include "../header/threadpool.hpp"
#include "../header/http_conn.h"
#include <signal.h>

#define MAX_FD 65535           // 最大文件描述符个数
#define MAX_EVENT_NUMBER 10000 //最大事件数量

void addsig(int sig, void(hander)(int))
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hander;
    //设置临时阻塞信号集，全阻塞，即信号处理过程中，收到信号则阻塞
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}
//添加文件描述符到epoll
extern int addfd(int epollfd, int fd, bool one_shot);
//从epoll中删除文件描述符
extern int removefd(int epollfd, int fd);
//修改文件描述符
extern int modfd(int epollfd, int fd, int ev);
int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        printf("按照如下格式运行 %s port number\n", basename(argv[0]));
        return -1;
    }
    int port = atoi(argv[1]);
    //对SIGPIPE信号处理
    addsig(SIGPIPE, SIG_IGN);
    //创建线程池，初始化线程池
    threadpool<http_conn> *pool = nullptr;
    try
    {
        pool = new threadpool<http_conn>();
    }
    catch (...)
    {
        exit(-1);
    }

    //创建数组用于保存所有客户端信息
    http_conn *users = new http_conn[MAX_FD]; // 文件描述符作为索引

    //网络
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("listenfd");
        exit(-1);
    }
    //设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    //绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    int ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    if (ret < 0)
    {
        perror("bind");
        exit(-1);
    }
    //监听
    listen(listenfd, 5);

#pragma region EPOLL
    //创建epoll对象，事件数组，添加
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);

    //将监听文件描述符添加到epoll中
    addfd(epollfd, listenfd, false); //监听的文件描述符不用设置one shot
    http_conn::m_epollfd = epollfd;

    while (true)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((num < 0) && (errno != EINTR))
        {
            printf("epoll failure \n");
            break;
        }
        //循环遍历
        for (int i = 0; i < num; ++i)
        {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd)
            {
                //客户端连接
                struct sockaddr_in client_address;
                socklen_t client_len = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_len);
                if (connfd < 0)
                {
                    throw std::exception();
                }
                if (http_conn::m_user_count >= MAX_FD)
                {
                    //目前连接数满了，
                    //给客户端写一个信息，服务器正忙。。。
                    close(connfd);
                    continue;
                }
                // 新的客户的数据初始化，放到http_conn数组中
                users[connfd].init(connfd, client_address);
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //对方一场端开或者错误
                users[sockfd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].read())
                {
                    //读取数据
                    
                    pool->append(users + sockfd);
                }
                else
                {
                    //读取失败，关闭连接
                    users[sockfd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!users[sockfd].write())
                {
                    //写入失败，关闭连接
                    users[sockfd].close_conn();
                }
            }
        }
    }

#pragma endregion
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}