#ifndef __TELNET_H__
#define __TELNET_H__

#include <sys/epoll.h>

struct telnet_main {
	int server_fd;
	
	int epoll_fd;
	uint64_t timeout;
	
#define EPOLL_LISTEN_EVENTS 100
	struct epoll_event events[EPOLL_LISTEN_EVENTS];

#define EPOLL_READ_BUFFER 4096
#define EPOLL_WRITE_BUFFER 4096
	char read_buf[EPOLL_READ_BUFFER];
	char write_buf[EPOLL_WRITE_BUFFER];
};

#endif
