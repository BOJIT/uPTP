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
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/netinet/in.h>

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(net_mgr, CONFIG_LOG_DEFAULT_LEVEL);

// #define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED |
// NET_EVENT_IPV4_ADDR_ADD)
#define DEFAULT_PORT 0

/*----------------------------------- State ----------------------------------*/

// static struct net_mgmt_event_callback l4_cb;
// static K_SEM_DEFINE(network_connected, 0,
// 		    1); // TODO this may need to be sorted to handle disconnects

/*------------------------------ Private Functions ---------------------------*/

// static void print_dhcp_info(struct net_if *iface)
// {
// 	for (size_t i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
// 		char buf[NET_IPV4_ADDR_LEN];

// 		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
// 			continue;
// 		}

// 		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
// 			net_addr_ntop(AF_INET,
// 				      &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr, buf,
// 				      sizeof(buf)));
// 		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
// 			net_addr_ntop(AF_INET, &iface->config.ip.ipv4->unicast[i].netmask, buf,
// 				      sizeof(buf)));
// 		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
// 			net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
// 		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
// 			iface->config.dhcpv4.lease_time);
// 	}
// }

// static void l4_event_handler(struct net_mgmt_event_callback *cb, uint64_t event,
// 			     struct net_if *iface)
// {
// 	LOG_INF("Got Event: %llu", event);
// 	switch (event) {
// 	case NET_EVENT_L4_CONNECTED:
// 		LOG_INF("Network connectivity established and IP address assigned");
// 		k_sem_give(&network_connected);
// 		break;
// 	case NET_EVENT_L4_DISCONNECTED:
// 		break;
// 	case NET_EVENT_IPV4_ADDR_ADD:
// 		print_dhcp_info(iface);
// 		break;
// 	default:
// 		break;
// 	}
// }

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static int welcome(int fd)
{
	static const char msg[] = "Bonjour, Zephyr world!\n";

	return send(fd, msg, sizeof(msg), 0);
}

// static void wait_for_network(void)
// {
// 	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
// 	net_mgmt_add_event_callback(&l4_cb);
// 	conn_mgr_mon_resend_status();

// 	net_if_foreach(start_dhcpv4_client, NULL);

// 	k_sem_take(&network_connected, K_FOREVER);
// 	LOG_INF("Network Connected");
// }

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

#if DEFAULT_PORT == 0
	/* The advanced use case: ephemeral port */
	// #if defined(CONFIG_NET_IPV6)
	// 	DNS_SD_REGISTER_SERVICE(uptp, CONFIG_NET_HOSTNAME, "_uptp", "_tcp", "local",
	// 				DNS_SD_EMPTY_TXT,
	// 				&((struct sockaddr_in6 *)&server_addr)->sin6_port);
	// #elif defined(CONFIG_NET_IPV4)
	DNS_SD_REGISTER_SERVICE(uptp, CONFIG_NET_HOSTNAME, "_uptp", "_tcp", "local",
				DNS_SD_EMPTY_TXT, &((struct sockaddr_in *)&server_addr)->sin_port);
// #endif
#else
	/* The simple use case: fixed port */
	DNS_SD_REGISTER_TCP_SERVICE(zephyr, CONFIG_NET_HOSTNAME, "_uptp", "local", DNS_SD_EMPTY_TXT,
				    DEFAULT_PORT);
#endif

	LOG_WRN("SRV MARKSS");

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
	// wait_for_network();
	net_if_foreach(start_dhcpv4_client, NULL);

	service();

	while (1) {
		LOG_WRN("Second Thread");
		k_msleep(1000);
	}
}

/*----------------------------------------------------------------------------*/
