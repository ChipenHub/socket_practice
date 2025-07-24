#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/poll.h>

#define ENABLE_HTTP_RESPONSE	1



#define BUFFER_LENGTH	1024



int set_cb(int fd, int event, int flag);
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);


typedef int (* RCALLBACK)(int fd);

// save buffer data
struct conn_item
{
	int fd;
	char rbuffer[BUFFER_LENGTH];
	int rlen;
	char wbuffer[BUFFER_LENGTH];
	int wlen;

	union
	{
		RCALLBACK accept_callback;
		RCALLBACK recv_callback;
	}recv_t;
	RCALLBACK send_callback;
};

struct conn_item connlist[1024];

#if 1
int epfd;
#elif
struct reactor
{
	int epfd;
	struct conn_item connlist[1024];
};
#endif

#if ENABLE_HTTP_RESPONSE

typedef struct conn_item connection_t;

#include <time.h>

int http_response(connection_t *conn)
{
    const char *html_body = "<html><head><title>chipen.com</title></head><body><h1>chipen</h1></body></html>";
    int content_length = strlen(html_body);

    // 生成符合 HTTP 标准格式的 Date 字符串
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    char date_str[128];
    strftime(date_str, sizeof(date_str), "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", gmt);

    // 构建完整的 HTTP 响应
    conn->wlen = snprintf(conn->wbuffer, BUFFER_LENGTH,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %d\r\n"
        "Accept-Ranges: bytes\r\n"
        "%s"
        "\r\n"  // Header 与 Body 的分隔符
        "%s",   // HTML 内容
        content_length,
        date_str,
        html_body
    );

    return conn->wlen;
}


#endif





// main loop
int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(2048);

	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr)))
	{
		perror("bind");
		return -1;
	}

	listen(sockfd, 10);

	connlist[sockfd].fd = sockfd;
	connlist[sockfd].recv_t.accept_callback = accept_cb;
	// epoll 边缘触发

	/*

	对于 IO 处理：随着时间增多会越来越复杂
	if（listenfd)
	{
	...
	}
	else // clientfd
	{
	...
	}

	对于事件处理 -> reactor 对事件反应更缓和
	if (events & EPOLLIN)
	{
	...
	}
	else if (events & EPOLLOUT)
	{
	...
	}

	*/

	epfd = epoll_create(1); // int size
	set_cb(sockfd, EPOLLIN, 1);

	struct epoll_event events[1024] = { 0 };

	while (1)
	{
		int nready = epoll_wait(epfd, events, 1024, -1);

		int i = 0;
		for (i = 0; i < nready; i++)
		{
			int curfd = events[i].data.fd;
			// if (sockfd == curfd)
			// {
			// 	// accept_cb()
			// 	// int clientfd = accept_cb(sockfd);
			// 	int clientfd = connlist[sockfd].recv_t.accept_callback(sockfd);
			// 	printf("client: %d\n", clientfd);
				
			// }
			if (events[i].events & EPOLLIN)
			{
				// int count = recv_cb(curfd);
				int count = connlist[curfd].recv_t.recv_callback(curfd);

				if (count != -1)
				printf("recv <- clientfd: %d, count: %d, buffer: %s\n", curfd, count, connlist[curfd].rbuffer);

			}
			else if (events[i].events & EPOLLOUT)
			{
				// int count = send_cb(curfd);
				int count = connlist[curfd].send_callback(curfd);
				printf("send -> clientfd: %d, count: %d, buffer: %s\n", curfd, count, connlist[curfd].wbuffer);

			}
			
		}

	}

	return 0;
}




/*
1. listenfd 触发 EPOLLIN 事件 -> 执行 accept_cb
2. client 触发 EPOLLIN 事件 -> recv_cb
3. client 触发 EPOLLOUT 事件 -> send_cb
*/

// ADD: flag == 1 else 0
int set_cb(int fd, int event, int flag)
{
	if (flag)
	{
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
	
	
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
		
	}
	else
	{
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;

		epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

	}
}


int accept_cb(int fd)
{
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(struct sockaddr_in);

	int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);

	set_cb(clientfd, EPOLLIN, 1);

	
	// set connlist
	connlist[clientfd].fd = clientfd;
	memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].wlen = 0;
	memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
	connlist[clientfd].rlen = 0;

	// set callback
	connlist[clientfd].recv_t.recv_callback = recv_cb;
	connlist[clientfd].send_callback = send_cb;

	
	printf("clientfd: %d\n", clientfd);
	return clientfd;
}

int recv_cb(int fd)
{
	char * buffer = connlist[fd].rbuffer;
	int index = connlist[fd].rlen;

	int count = recv(fd, buffer + index, BUFFER_LENGTH - index, 0);
	
	if (count == 0)
	{
		printf("disconnect\n");
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);

		return -1;
	}

	connlist[fd].rlen += count;

#if ENABLE_HTTP_RESPONSE

http_response(&connlist[fd]);

#else

memcpy(connlist[fd].wbuffer, connlist[fd].rbuffer, connlist[fd].rlen);
connlist[fd].wlen = connlist[fd].rlen;

#endif

	// event
	set_cb(fd, EPOLLOUT, 0);

	return count;
}

int send_cb(int fd)
{
	char * buffer = connlist[fd].wbuffer;
	int index = connlist[fd].wlen;

	int count = send(fd, buffer, index, 0);


	// 事件来一次执行一次

	set_cb(fd, EPOLLIN, 0);

	return count;
}