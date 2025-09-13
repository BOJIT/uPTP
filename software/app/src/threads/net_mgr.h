/**
 * @file net_mgr.h
 * @author James Bennion-Pedley
 * @brief Manages network-related tasks (outside of existing Zephyr services)
 * @date 13/09/2025
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef __THREADS_NET_MGR_H__
#define __THREADS_NET_MGR_H__

/*--------------------------------- Includes ---------------------------------*/

/*--------------------------------- Datatypes --------------------------------*/

/*--------------------------------- Functions --------------------------------*/

int net_mgr_init(void);

void net_mgr_thread(void *arg1, void *arg2, void *arg3);

/*----------------------------------------------------------------------------*/

#endif /* __THREADS_NET_MGR_H__ */
