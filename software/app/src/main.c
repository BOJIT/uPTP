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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "threads/net_mgr.h"

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/*----------------------------------- State ----------------------------------*/

static const int32_t sleep_time_ms = 500;

K_THREAD_STACK_DEFINE(m_net_mgr_stack, 2048);
struct k_thread m_net_mgr_thread;

/*------------------------------ Private Functions ---------------------------*/

/*------------------------------- Public Functions ---------------------------*/

/*-------------------------------- Entry Point -------------------------------*/

int main(void)
{
	net_mgr_init();

	k_thread_create(&m_net_mgr_thread, m_net_mgr_stack, K_THREAD_STACK_SIZEOF(m_net_mgr_stack),
			net_mgr_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);

	// int i = 0;

	while (1) {
		// i++;
		// LOG_INF("Hello World! %u", k_uptime_get_32());
		// if (i % 10 == 0) {
		// 	LOG_WRN("Random Panic!");
		// }
		k_msleep(sleep_time_ms);
	}
}
