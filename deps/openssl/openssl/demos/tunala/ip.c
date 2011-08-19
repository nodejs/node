#include "tunala.h"

#ifndef NO_IP

#define IP_LISTENER_BACKLOG 511 /* So if it gets masked by 256 or some other
				   such value it'll still be respectable */

/* Any IP-related initialisations. For now, this means blocking SIGPIPE */
int ip_initialise(void)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGPIPE, &sa, NULL) != 0)
		return 0;
	return 1;
}

int ip_create_listener_split(const char *ip, unsigned short port)
{
	struct sockaddr_in in_addr;
	int fd = -1;
	int reuseVal = 1;

	/* Create the socket */
	if((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		goto err;
	/* Set the SO_REUSEADDR flag - servers act weird without it */
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)(&reuseVal),
				sizeof(reuseVal)) != 0)
		goto err;
	/* Prepare the listen address stuff */
	in_addr.sin_family = AF_INET;
	memcpy(&in_addr.sin_addr.s_addr, ip, 4);
	in_addr.sin_port = htons(port);
	/* Bind to the required port/address/interface */
	if(bind(fd, (struct sockaddr *)&in_addr, sizeof(struct sockaddr_in)) != 0)
		goto err;
	/* Start "listening" */
	if(listen(fd, IP_LISTENER_BACKLOG) != 0)
		goto err;
	return fd;
err:
	if(fd != -1)
		close(fd);
	return -1;
}

int ip_create_connection_split(const char *ip, unsigned short port)
{
	struct sockaddr_in in_addr;
	int flags, fd = -1;

	/* Create the socket */
	if((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		goto err;
	/* Make it non-blocking */
	if(((flags = fcntl(fd, F_GETFL, 0)) < 0) ||
			(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0))
		goto err;
	/* Prepare the connection address stuff */
	in_addr.sin_family = AF_INET;
	memcpy(&in_addr.sin_addr.s_addr, ip, 4);
	in_addr.sin_port = htons(port);
	/* Start a connect (non-blocking, in all likelihood) */
	if((connect(fd, (struct sockaddr *)&in_addr,
			sizeof(struct sockaddr_in)) != 0) &&
			(errno != EINPROGRESS))
		goto err;
	return fd;
err:
	if(fd != -1)
		close(fd);
	return -1;
}

static char all_local_ip[] = {0x00,0x00,0x00,0x00};

int ip_parse_address(const char *address, const char **parsed_ip,
		unsigned short *parsed_port, int accept_all_ip)
{
	char buf[256];
	struct hostent *lookup;
	unsigned long port;
	const char *ptr = strstr(address, ":");
	const char *ip = all_local_ip;

	if(!ptr) {
		/* We assume we're listening on all local interfaces and have
		 * only specified a port. */
		if(!accept_all_ip)
			return 0;
		ptr = address;
		goto determine_port;
	}
	if((ptr - address) > 255)
		return 0;
	memset(buf, 0, 256);
	memcpy(buf, address, ptr - address);
	ptr++;
	if((lookup = gethostbyname(buf)) == NULL) {
		/* Spit a message to differentiate between lookup failures and
		 * bad strings. */
		fprintf(stderr, "hostname lookup for '%s' failed\n", buf);
		return 0;
	}
	ip = lookup->h_addr_list[0];
determine_port:
	if(strlen(ptr) < 1)
		return 0;
	if(!int_strtoul(ptr, &port) || (port > 65535))
		return 0;
	*parsed_ip = ip;
	*parsed_port = (unsigned short)port;
	return 1;
}

int ip_create_listener(const char *address)
{
	const char *ip;
	unsigned short port;

	if(!ip_parse_address(address, &ip, &port, 1))
		return -1;
	return ip_create_listener_split(ip, port);
}

int ip_create_connection(const char *address)
{
	const char *ip;
	unsigned short port;

	if(!ip_parse_address(address, &ip, &port, 0))
		return -1;
	return ip_create_connection_split(ip, port);
}

int ip_accept_connection(int listen_fd)
{
	return accept(listen_fd, NULL, NULL);
}

#endif /* !defined(NO_IP) */

