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

#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "threads/net_mgr.h"

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define LED_STRIP_NODE DT_NODELABEL(led_strip)

/*----------------------------------- State ----------------------------------*/

// K_THREAD_STACK_DEFINE(m_net_mgr_stack, 2048);
// struct k_thread m_net_mgr_thread;

/*------------------------------ Private Functions ---------------------------*/

/*------------------------------- Public Functions ---------------------------*/

/*-------------------------------- Entry Point -------------------------------*/

int main(void)
{
	static const struct device *strip = DEVICE_DT_GET_OR_NULL(LED_STRIP_NODE);

	struct led_rgb colour = {
		.r = 0x50,
		.g = 0x00,
		.b = 0x00,
	};

	if (!strip || !device_is_ready(strip)) {
		LOG_WRN("LED strip not ready");
	}

	if (strip) {
		led_strip_update_rgb(strip, &colour, 1);
	}

	// net_mgr_init();
	// k_thread_create(&m_net_mgr_thread, m_net_mgr_stack,
	// K_THREAD_STACK_SIZEOF(m_net_mgr_stack), 		net_mgr_thread, NULL, NULL, NULL, 5,
	// 0, K_NO_WAIT);

	while (1) {
		k_msleep(1000);
	}
}
