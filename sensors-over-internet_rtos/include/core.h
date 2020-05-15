#ifndef __CORE_H__
#define __CORE_H__

#include "zephyr/types.h"

int core_last_net_packet_get(u8_t *last_data);

int core_last_net_packet_set(u8_t *last_data);

#endif
