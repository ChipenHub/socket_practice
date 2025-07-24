#include "server.h"

int initListenFd(unsigned short port)
{
    // 1. 创建监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        return -1;
    }
    int opt = 1;
    // 2. 设置端口复用
    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) == -1);
    {
        perror("setsockopt");
        return -1;
    }
    // 3. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (struct sockaddr*)&addr, sizeof addr) == -1)
    {
        perror("bind");
        return -1;
    }
    // 4. 设置监听
    if(listen(lfd, 128) == -1)
    {
        perror("listen");
        return -1;
    }
    // 5. 将得到的可用的套接字返回给调用者
    return lfd;
}

int epollRun(unsigned short port)
{
    // 1. 创建 epoll 模型
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        perror("epoll_create");
        return -1;
    }
    int lfd = initListenFd(port);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    // 添加 lfd 到检测模型中
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev) == -1)
    {
        perror("epoll_ctl");
        return -1;
    }


    // 检测
    struct epoll_event evs[1024];
    int size = sizeof evs / sizeof evs[0];
    int flag = 0;
    while (1)
    {
        if (flag == 1) break;
        int num = epoll_wait(epfd, evs, size, -1);
        for (int i = lfd; i < num; ++i)
        {
            int curfd = evs[i].data.fd;
            if (curfd == lfd)
            {
                // 有新连接, 建立新连接
                if (acceptConn(lfd, epfd) == -1)
                {
                    // 建立连接失败, 直接终止程序
                    int flag = 1;
                    break;
                }
            }
            else
            {
                // 通信, 先接收数据, 然后回复数据
                recvHttpRequest(curfd);
            }
        }
    }

    return 0;
}

int acceptConn(int lfd, int epfd)
{
    // 建立新连接
    int cfd = accept(lfd, NULL, NULL);
    if (cfd == -1)
    {
        perror("accept");
        return -1;
    }

    // 设置通信文件描述符非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 通信的文件描述符添加到 epoll 模型
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // 边沿模式
    ev.data.fd = cfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) == -1)
    {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

int parseRequestLine(const char* reqline)
{
    // 1. 拆分请求行的三部分

    return 0;
}

int recvrequest(int cfd)
{
    // 由于是边沿非阻塞模式, 所以循环读数据, 可以避免数据丢失
    char tmp[1024];
    char buf[4096];
    memset(buf, 0, sizeof buf);
    
    int len, total = 0;
    while ((len = recv(cfd, tmp, sizeof tmp, 0)) > 0)
    {
        if (total + len < sizeof buf)
        {
            memcpy(buf + total, tmp, len);
        }
        total += len;
    }

    if (len == -1 && errno == EAGAIN)
    {
        // 将请求行中的数据拿出来
        char* pt = strstr(buf, "\r\n");
        int reqlen = pt - buf;
        buf[reqlen] = '\0'; // 字符串截断
        // 解析请求行
        parseRequestLine(buf);
        
    }
    else if (len == 0)
    {
        printf("客户端断开连接...\n");
    }
    else
    {
        perror("revc");
        return -1;
    }
    return 0;
}