#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "telnet.h"

/* support by C99 */
#include <stdbool.h>

struct telnet_main tm;

void telnet_set_nonblock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int telnet_config_ip_port(struct sockaddr_in *ip, const char *ip_str, const char *port_str)
{	
	assert (ip);
	assert (ip_str);
	assert (port_str);

	uint16_t port;
    struct hostent *server;
	port = atoi(port_str);
    server = gethostbyname(ip_str);
	
    if (server == NULL)
        return -1;

    bzero((char *) ip, sizeof(struct sockaddr_in));	
    ip->sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
         (char *)&ip->sin_addr.s_addr,
         server->h_length);
    ip->sin_port = htons(port);

	return 0;
}

int telnet_connect_ip_server(struct sockaddr_in *ip)
{
	int sockfd;
	struct telnet_main *tlm = &tm;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
    if (sockfd < 0) {
		
		fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
		return sockfd;
    }

    if (connect(sockfd, (struct sockaddr *)ip, sizeof(struct sockaddr_in)) < 0) {
		
		fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
		return -1;
   	}

	tlm->server_fd = sockfd;

	return sockfd;
}

void __attribute__((constructor))  telnet_main_init()
{
	tm.timeout = 1e6;
}

void __attribute__((destructor))  telnet_free()
{
	struct telnet_main *tlm = &tm;
	close(tlm->server_fd);
	close(tlm->epoll_fd);
}

int telnet_epoll_add_fd(int fd)
{
	struct telnet_main *tlm = &tm;	
	struct epoll_event ev;

	/* use edge trigger */
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = fd;

    if (epoll_ctl(tlm->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
		return -1;
    }

	return 0;
}

int telnet_epoll_add_stdin()
{
	struct telnet_main *tlm = &tm;	
	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = STDIN_FILENO;

    if (epoll_ctl(tlm->epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0) {
		fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
		return -1;
    }

	return 0;
}

int telnet_epoll_add_stdout()
{
	struct telnet_main *tlm = &tm;	
	struct epoll_event ev;

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = STDOUT_FILENO;

    if (epoll_ctl(tlm->epoll_fd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev) < 0) {
		fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
		return -1;
    }

	return 0;
}

int telnet_epoll_init()
{
	/* syscall */
	int fd;
	struct telnet_main *tlm = &tm;	
	
	if ((fd = epoll_create(1000)) < 0)
		return -1;

	tlm->epoll_fd = fd;

	telnet_epoll_add_fd(tlm->server_fd);

	telnet_epoll_add_stdin();
	
	telnet_set_nonblock(tlm->server_fd);
	
	telnet_set_nonblock(STDIN_FILENO);
	
	return 0;
}

void telnet_send_msg(int fd)
{
	int buf_len, len;
	struct telnet_main *tlm = &tm;

	buf_len = 0;

	memset(tlm->write_buf, 0, EPOLL_WRITE_BUFFER);

	while (true) {

		len = read(fd, tlm->write_buf + buf_len, EPOLL_WRITE_BUFFER);
		
		if (len < 0) {

			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				
                break;
            } else {
				fprintf(stderr, "%s:%d, %s\n", __FUNCTION__, __LINE__, strerror(errno));
                break;
            }		
			
			break;
		} else if (len == 0) {
			break;
		}

		buf_len += len;
	}

	/* telnet subfix is \r\n */
	strncat(tlm->write_buf, "\r\n", 2);

	write(tlm->server_fd, tlm->write_buf, buf_len + 2);
}

void telnet_read_msg(int fd)
{
	int buf_len, len;
	struct telnet_main *tlm = &tm;

	buf_len = 0;

	/* don't think about overflow */
	memset(tlm->read_buf, 0, EPOLL_READ_BUFFER);
	
	while (true) {

		len = read(fd, tlm->read_buf + buf_len, EPOLL_READ_BUFFER);
		
		if (len < 0) {

			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				
                break;
            } else {
				fprintf(stderr, "%s:%d, %s\n", __FUNCTION__, __LINE__, strerror(errno));
                break;
            }		
			
			break;
		} else if (len == 0) {
			break;
		}

		buf_len += len;
	}

	fprintf(stdout, "%s", tlm->read_buf);
	fflush(stdout);
}

int telnet_epoll_handle(int nfds)
{
	int i;
	struct telnet_main *tlm = &tm;	
	
	for (i = 0; i < nfds; i++) {
	
		switch (tlm->events[i].data.fd) {
		case STDIN_FILENO:
	
			telnet_send_msg(STDIN_FILENO);
			break;
		case STDOUT_FILENO:			
			
			break;
		default:
			telnet_read_msg(tlm->server_fd);
			
			break;
		}
	}

	return 0;
}

void telnet_run()
{
	struct telnet_main *tlm = &tm;
	int nfds;

	while (true) {
		
		nfds = epoll_wait(tlm->epoll_fd, tlm->events, EPOLL_LISTEN_EVENTS, tlm->timeout);
	
		switch (nfds) {
		case -1:

			fprintf(stderr, "%s:%d %s\n", __FUNCTION__, __LINE__, strerror(errno));
			exit(-1);
			break;
		case 0:
			
			/* timeout */
			continue;
			break;
		}

		telnet_epoll_handle(nfds);
	}
}

int main(int argc, char *argv[]) {

    struct sockaddr_in serv_addr;	

    if (argc < 3) {
        fprintf(stderr, "Usage: %s ipv4 port\n", argv[0]);
        exit(1);
    }

	telnet_config_ip_port(&serv_addr, argv[1], argv[2]);

    if (telnet_connect_ip_server(&serv_addr) < 0)
		exit(-1);

	telnet_epoll_init();

	telnet_run();

    return 0;
}

