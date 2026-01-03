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

#include <zephyr/app_version.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/dns_sd.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/net_config.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>

#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/netinet/in.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(net_mgr, CONFIG_LOG_DEFAULT_LEVEL);

#define DEFAULT_PORT 9595

/*----------------------------------- State ----------------------------------*/

static char m_hostname[CONFIG_NET_HOSTNAME_MAX_LEN];

/* clang-format off */
static char m_txt_buf[64];
static const char *m_txt_record[] = {
	"version=" APP_VERSION_STRING,
	"commit=" STRINGIFY(APP_BUILD_VERSION),
	"board=" CONFIG_BOARD,
};
// TODO add Zephyr board revision
/* clang-format on */

K_SEM_DEFINE(m_interface_up, 0, 1);
static struct net_mgmt_event_callback m_iface_cb;

/*------------------------------ Private Functions ---------------------------*/

static void iface_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_IF_UP) {
		k_sem_give(&m_interface_up);
	}
}

static void wait_for_iface_up(void)
{
	if (net_if_is_up(net_if_get_default())) {
		k_sem_give(&m_interface_up);
	}

	k_sem_take(&m_interface_up, K_FOREVER);
}

static int welcome(int fd)
{
	static const char msg[] = "Welcome to the uPTP Service\n";

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
	net_mgmt_init_event_callback(&m_iface_cb, iface_event_handler, NET_EVENT_IF_UP);
	net_mgmt_add_event_callback(&m_iface_cb);
	net_config_init_app(NULL, "Initializing network"); // will timeout if interface isn't up
	wait_for_iface_up();

	// Construct TXT record and pass to DNS-SD
	size_t idx = 0;
	for (size_t i = 0; i < ARRAY_SIZE(m_txt_record); i++) {
		size_t record_len = strlen(m_txt_record[i]);
		if (idx + record_len + 1 >= sizeof(m_txt_buf)) {
			break;
		}
		m_txt_buf[idx] = record_len;
		memcpy(&m_txt_buf[idx + 1], m_txt_record[i], record_len);
		idx += record_len + 1;
	}
	m_txt_buf[idx] = '\0';

	strncpy(m_hostname, net_hostname_get(), sizeof(m_hostname));
	DNS_SD_REGISTER_TCP_SERVICE(uptp, m_hostname, "_uptp", "local", m_txt_buf, DEFAULT_PORT);

	while (1) {
		// If service is terminated, relaunch while link is up
		service();

		k_msleep(100);
		wait_for_iface_up();
	}
}

/*----------------------------------------------------------------------------*/
