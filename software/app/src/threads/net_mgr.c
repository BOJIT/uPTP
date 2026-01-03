/**
 * @file net_mgr.c
 * @author James Bennion-Pedley
 * @brief Manages network-related tasks (outside of existing Zephyr services)
 * @date 13/09/2025
 *
 * @copyright Copyright (c) 2025
 *
 */

/*--------------------------------- Includes ---------------------------------*/

#include "net_mgr.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/dns_sd.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_config.h>

#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netinet/in.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(net_mgr, CONFIG_LOG_DEFAULT_LEVEL);

#define DEFAULT_PORT 9595

/*----------------------------------- State ----------------------------------*/

/*------------------------------ Private Functions ---------------------------*/

static int welcome(int fd)
{
	static const char msg[] = "Bonjour, Zephyr world!\n";

	return send(fd, msg, sizeof(msg), 0);
}

static void service(void)
{
	int r;
	int server_fd;
	int client_fd;
	socklen_t len;
	void *addrp;
	uint16_t *portp;
	struct sockaddr client_addr;
	char addrstr[INET6_ADDRSTRLEN];
	uint8_t line[64];

	static struct sockaddr server_addr;

	k_msleep(5000);

	DNS_SD_REGISTER_TCP_SERVICE(uptp, CONFIG_NET_HOSTNAME, "_uptp", "local", DNS_SD_EMPTY_TXT,
				    DEFAULT_PORT);

	if (IS_ENABLED(CONFIG_NET_IPV6)) {
		net_sin6(&server_addr)->sin6_family = AF_INET6;
		net_sin6(&server_addr)->sin6_addr = in6addr_any;
		net_sin6(&server_addr)->sin6_port = sys_cpu_to_be16(DEFAULT_PORT);
	} else if (IS_ENABLED(CONFIG_NET_IPV4)) {
		net_sin(&server_addr)->sin_family = AF_INET;
		net_sin(&server_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
		net_sin(&server_addr)->sin_port = sys_cpu_to_be16(DEFAULT_PORT);
	} else {
		__ASSERT(false, "Neither IPv6 nor IPv4 are enabled");
	}

	r = socket(server_addr.sa_family, SOCK_STREAM, 0);
	if (r == -1) {
		LOG_INF("socket() failed (%d)", errno);
		return;
	}

	server_fd = r;
	LOG_INF("server_fd is %d", server_fd);

	r = bind(server_fd, &server_addr, sizeof(server_addr));
	if (r == -1) {
		LOG_INF("bind() failed (%d)", errno);
		close(server_fd);
		return;
	}

	if (server_addr.sa_family == AF_INET6) {
		addrp = &net_sin6(&server_addr)->sin6_addr;
		portp = &net_sin6(&server_addr)->sin6_port;
	} else {
		addrp = &net_sin(&server_addr)->sin_addr;
		portp = &net_sin(&server_addr)->sin_port;
	}

	inet_ntop(server_addr.sa_family, addrp, addrstr, sizeof(addrstr));
	LOG_INF("bound to [%s]:%u", addrstr, ntohs(*portp));

	r = listen(server_fd, 1);
	if (r == -1) {
		LOG_INF("listen() failed (%d)", errno);
		close(server_fd);
		return;
	}

	for (;;) {
		len = sizeof(client_addr);
		r = accept(server_fd, (struct sockaddr *)&client_addr, &len);
		if (r == -1) {
			LOG_INF("accept() failed (%d)", errno);
			continue;
		}

		client_fd = r;

		inet_ntop(server_addr.sa_family, addrp, addrstr, sizeof(addrstr));
		LOG_INF("accepted connection from [%s]:%u", addrstr, ntohs(*portp));

		/* send a banner */
		r = welcome(client_fd);
		if (r == -1) {
			LOG_INF("send() failed (%d)", errno);
			close(client_fd);
			return;
		}

		for (;;) {
			/* echo 1 line at a time */
			r = recv(client_fd, line, sizeof(line), 0);
			if (r == -1) {
				LOG_INF("recv() failed (%d)", errno);
				close(client_fd);
				break;
			}
			len = r;

			r = send(client_fd, line, len, 0);
			if (r == -1) {
				LOG_INF("send() failed (%d)", errno);
				close(client_fd);
				break;
			}
		}
	}
}

/*------------------------------- Public Functions ---------------------------*/

int net_mgr_init(void)
{
	return 0;
}

void net_mgr_thread(void *arg1, void *arg2, void *arg3)
{
	net_config_init_app(NULL, "Initializing network"); // Waits for interface up initially

	service();
	LOG_ERR("DNS-SD Service Terminated Prematurely!");
	// TODO ensure service is only up when link is up

	while (1) {
		k_msleep(1000);
	}
}

/*----------------------------------------------------------------------------*/
