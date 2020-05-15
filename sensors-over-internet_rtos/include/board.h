#ifndef __BOARD_H__
#define __BOARD_H__

#include "zephyr/types.h"

int board_sensor_a_last_data_get(u8_t *last_data);

int board_sensor_a_last_data_set(u8_t *last_data);

#endif
