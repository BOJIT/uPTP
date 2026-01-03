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

#include "threads/net_mgr.h"

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define LED_STRIP_NODE DT_NODELABEL(led_strip)

struct led_rgb m_colours[3] = {
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
};

/*----------------------------------- State ----------------------------------*/

K_THREAD_STACK_DEFINE(m_net_mgr_stack, 2048);
struct k_thread m_net_mgr_thread;

/*------------------------------ Private Functions ---------------------------*/

/*------------------------------- Public Functions ---------------------------*/

/*-------------------------------- Entry Point -------------------------------*/

#define ADJ_FREQ_BASE_ADDEND (uint32_t)(UINT32_MAX * (50000000 / (float)(144 * 4)))

int main(void)
{
	static const struct device *strip = DEVICE_DT_GET_OR_NULL(LED_STRIP_NODE);

	if (!strip || !device_is_ready(strip)) {
		LOG_ERR("LED device not ready");
		return -EIO;
	}

	net_mgr_init();
	k_thread_create(&m_net_mgr_thread, m_net_mgr_stack, K_THREAD_STACK_SIZEOF(m_net_mgr_stack),
			net_mgr_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);

	size_t i = 0;
	while (1) {
		if (strip) {
			led_strip_update_rgb(strip, &m_colours[i], 1);
			i = (i + 1) % ARRAY_SIZE(m_colours);
		}

		// LOG_INF("PTP_REGS: %u, %u", ETH->PTPTSHR, ETH->PTPTSLR);

		k_msleep(1000);
	}
}
