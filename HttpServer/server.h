#pragma once
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 服务器要处理的业务逻辑
// 初始化监听的文件描述符
int iniListenFd(unsigned short port);

// 启动 epoll 模型
int epollRun(unsigned short port);
int acceptConn(int lfd, int epfd);
int recvHttpRequest(int cfd);