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

/*---------------------------- Macros & Constants ----------------------------*/

/*----------------------------------- State ----------------------------------*/

// Settings
static const int32_t sleep_time_ms = 50; // Target 20 FPS

/*------------------------------ Private Functions ---------------------------*/

/*------------------------------- Public Functions ---------------------------*/

/*-------------------------------- Entry Point -------------------------------*/

int main(void)
{
    while (1)
    {
        k_msleep(sleep_time_ms);
    }
}
