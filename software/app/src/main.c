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

/*---------------------------- Macros & Constants ----------------------------*/

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/*----------------------------------- State ----------------------------------*/

// Settings
static const int32_t sleep_time_ms = 500;

/*------------------------------ Private Functions ---------------------------*/

/*------------------------------- Public Functions ---------------------------*/

/*-------------------------------- Entry Point -------------------------------*/

int main(void)
{
	int i = 0;

	while (1) {
		i++;
		LOG_INF("Hello World! %u", k_uptime_get_32());
		if (i % 10 == 0) {
			LOG_WRN("Random Panic!");
		}
		k_msleep(sleep_time_ms);
	}
}
