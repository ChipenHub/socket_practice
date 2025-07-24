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


int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));

	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(8888);

	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr)))
	{
		perror("bind");
		return -1;
	}

	printf("start listen\n");

	listen(sockfd, 10);

#if 1 // select

	fd_set rfds, rset;

	FD_ZERO(&rfds);
	FD_SET(sockfd, &rfds);

	int maxfd = sockfd;
	while (1)
	{
		rset = rfds;
		int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(sockfd, &rset))
		{

			struct sockaddr_in clientaddr;
			socklen_t len = sizeof(struct sockaddr_in);

			int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);

			printf("sockfd: %d\n", clientfd);

			FD_SET(clientfd, &rfds);
			maxfd = clientfd;

		}


		int i = 0;
		for (i = sockfd + 1; i <= maxfd; i++)
		{
			if (FD_ISSET(i, &rset))
			{
				char buffer[128] = { 0 };
				int count = recv(i, buffer, 128, 0);
				if (0 == count)
				{
					printf("disconnect\n");
					close(i);

					FD_CLR(i, &rfds);
					break;
				}

				send(i, buffer, count, 0);
				printf("clientfd: %d, count: %d, buffer: %s\n", i, count, buffer);

			}
		}
	}

#elif 0 // poll

	struct pollfd fds[1024] = { 0 };

	fds[sockfd].fd = sockfd;
	fds[sockfd].events = POLLIN;

	int maxfd = sockfd;

	while (1)
	{
		int nready = poll(fds, maxfd + 1, -1);

		if (fds[sockfd].revents & POLLIN)
		{
			struct  sockaddr_in clientaddr;
			int len = sizeof(struct sockaddr_in);

			int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
			
			printf("hello, %d\n", clientfd);
			fds[clientfd].fd = clientfd;
			fds[clientfd].events = POLLIN;

			maxfd = clientfd;

		}
		int i = 0;
		for (i = sockfd + 1; i <= maxfd; i++)
		{
			if (fds[i].revents & POLLIN)
			{
				char buffer[128] = { 0 };
				int count = recv(fds[i].fd, buffer, 128, 0);
				if (count == 0)
				{
					printf("disconnect\n");
					close(i);
					fds[i].fd = -1;
					fds[i].events = 0;

					continue;
				}

				send(i, buffer, count, 0);
				printf("clientfd: %d, sount: %d, lbuffer: %s\n", i, count, buffer);

			}
		}
	}
#elif 1 // epoll 水平触发、面向 IO
	int epfd = epoll_create(1);

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;


	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

	struct epoll_event events[1024] = { 0 };

	while (1)
	{
		int nready = epoll_wait(epfd, events, 1024, -1);

		int i = 0;
		for (i = 0; i < nready; i++)
		{
			int curfd = events[i].data.fd;
			if (sockfd == curfd)
			{
				struct sockaddr_in clientaddr;
				socklen_t len = sizeof(struct sockaddr_in);

				int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);

				ev.events = EPOLLIN;
				ev.data.fd = clientfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);

				printf("clientfd: %d\n", clientfd);
			}
			else if (events[i].events & EPOLLIN)
			{
				char buffer[10] = { 0 }; // 只要有数据就会一直触发，因此会回复多次
				int count = recv(curfd, buffer, 10, 0);
				if (count == 0)
				{
					printf("disconnect\n");
					close(curfd);
					epoll_ctl(epfd, EPOLL_CTL_DEL, curfd, NULL);

					continue;
				}

				send(curfd, buffer, count, 0);
				printf("clientfd: %d, sount: %d, buffer: %s\n", curfd, count, buffer);
			}
			
		}

	}

#endif
	return 0;
}
