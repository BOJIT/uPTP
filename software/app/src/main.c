/**
 * @file main.c
 * @author James Bennion-Pedley
 * @brief Main entry point for application
 * @date 01/06/2025
 *
 * @copyright Copyright (c) 2025
 *
 */

/*--------------------------------- Includes ---------------------------------*/

#include <errno.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/hostname.h>

#if CONFIG_PTP
#include <zephyr/drivers/ptp_clock.h>

#include "ptp/clock.h"
#include "ptp/port.h"
#endif /* CONFIG_PTP */

#include "threads/net_mgr.h"

#if CONFIG_SOC_FAMILY_CH32V
#include <hal_ch32fun.h>
#endif

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define LED_STRIP_NODE DT_NODELABEL(led_strip)

struct led_rgb m_colours[5] = {
	{
		.r = 0x50,
		.g = 0x50,
		.b = 0x00,
	},
	{
		.r = 0x00,
		.g = 0x50,
		.b = 0x50,
	},
	{
		.r = 0x50,
		.g = 0x00,
		.b = 0x50,
	},
	{
		.r = 0x00,
		.g = 0x50,
		.b = 0x00,
	},
	{
		.r = 0x50,
		.g = 0x00,
		.b = 0x00,
	},
};

#if CONFIG_PTP
typedef enum _ptp_status {
	PTP_STATE_FAULT = 0,
	PTP_STATE_TRANSMITTER = 1,
	PTP_STATE_RECEIVER = 2,
	PTP_STATE_LISTENING = 3,

	PTP_STATE_UNKNOWN = 4,
} ptp_status_t;
#endif /* CONFIG_PTP */

/*----------------------------------- State ----------------------------------*/

K_THREAD_STACK_DEFINE(m_net_mgr_stack, 2048);
struct k_thread m_net_mgr_thread;

/*------------------------------ Private Functions ---------------------------*/

#if CONFIG_PTP
static ptp_status_t get_ptp_status(void)
{
	struct ptp_port *port;
	sys_slist_t *ports_list = ptp_clock_ports_list();

	if (!ports_list || sys_slist_len(ports_list) == 0) {
		return -EINVAL;
	}

	port = CONTAINER_OF(sys_slist_peek_head(ports_list), struct ptp_port, node);

	if (!port) {
		return -EINVAL;
	}

	switch (ptp_port_state(port)) {
	case PTP_PS_INITIALIZING:
	case PTP_PS_FAULTY:
	case PTP_PS_DISABLED:
	case PTP_PS_PRE_TIME_TRANSMITTER:
	case PTP_PS_PASSIVE:
	case PTP_PS_UNCALIBRATED:
		return PTP_STATE_FAULT;
	case PTP_PS_TIME_TRANSMITTER:
	case PTP_PS_GRAND_MASTER:
		return PTP_STATE_TRANSMITTER;
	case PTP_PS_TIME_RECEIVER:
		return PTP_STATE_RECEIVER;
	case PTP_PS_LISTENING:
		return PTP_STATE_LISTENING;
	}

	return PTP_STATE_UNKNOWN;
}
#endif /* CONFIG_PTP */

/*------------------------------- Public Functions ---------------------------*/

/*-------------------------------- Entry Point -------------------------------*/

int main(void)
{
	LOG_INF("Device Hostname: %s", net_hostname_get());

	static const struct device *strip = DEVICE_DT_GET_OR_NULL(LED_STRIP_NODE);

	if (!strip || !device_is_ready(strip)) {
		LOG_ERR("LED device not ready");
		return -EIO;
	}

	net_mgr_init();
	k_thread_create(&m_net_mgr_thread, m_net_mgr_stack, K_THREAD_STACK_SIZEOF(m_net_mgr_stack),
			net_mgr_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);

	int64_t target_ms = ((k_uptime_get() + MSEC_PER_SEC) / MSEC_PER_SEC) * MSEC_PER_SEC;
	size_t i = 0;
	while (1) {
		target_ms += MSEC_PER_SEC;
		k_sleep(K_TIMEOUT_ABS_MS(target_ms));

#if CONFIG_PTP
		static const struct device *ptp_device =
			DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ptp_clock));
		struct net_ptp_time t_now = {0};

		if (ptp_device) {
			ptp_clock_get(ptp_device, &t_now);
		}

		ptp_status_t ptp_status = get_ptp_status();
		LOG_INF("PTP_STATUS: %llu, %u, %u", t_now.second, t_now.nanosecond, ptp_status);
		led_strip_update_rgb(strip, &m_colours[ptp_status], 1);
#else
		led_strip_update_rgb(strip, &m_colours[i], 1);
#endif /* CONFIG_PTP */

		i = (i + 1) % ARRAY_SIZE(m_colours);
	}
}
