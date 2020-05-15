#include <fs/nvs.h>
#include <kernel.h>
#include <logging/log.h>
#include <stdlib.h>
#include <zephyr.h>

#include "board.h"
#include "core.h"

LOG_MODULE_REGISTER(storage, 4);

#define NVS_SECTOR_SIZE DT_FLASH_ERASE_BLOCK_SIZE
#define NVS_SECTOR_COUNT 4
#define NVS_STORAGE_OFFSET DT_FLASH_AREA_STORAGE_OFFSET

static struct nvs_fs zt_fs = {
    .sector_size  = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset       = NVS_STORAGE_OFFSET,
};

void store()
{
    u8_t sensor_data = 0;
    board_sensor_a_last_data_get(&sensor_data);
    int rc = nvs_write(&zt_fs, 0, &sensor_data, 1);
    if (rc > 0) { /* item was found, show it */
        LOG_INF("Id: %d", 0);
        LOG_HEXDUMP_INF(&sensor_data, 1, "Stored sensor data: ");
    } else { /* item was not found, add it */
        LOG_INF("Could not store sensor data error: %d", rc);
    }

    u8_t net_data[5] = {0};
    core_last_net_packet_get(net_data);
    rc = nvs_write(&zt_fs, 1, net_data, 5);
    if (rc > 0) { /* item was found, show it */
        LOG_INF("Id: %d", 1);
        LOG_HEXDUMP_INF(&net_data, 5, "Stored net data: ");

    } else { /* item was not found, add it */
        LOG_INF("Could not store net data error: %d", rc);
    }
}

void restore()
{
    u8_t sensor_data = 0;
    int rc           = nvs_read(&zt_fs, 0, &sensor_data, 1);
    if (rc > 0) { /* item was found, show it */
        LOG_INF("Id: %d", 0);
        LOG_HEXDUMP_INF(&sensor_data, 1, "Recovered sensor data: ");
        board_sensor_a_last_data_set(&sensor_data);
    } else { /* item was not found, add it */
        LOG_INF("No values found for sensor data");
    }

    u8_t net_data[5] = {0};
    rc               = nvs_read(&zt_fs, 1, net_data, 5);
    if (rc > 0) { /* item was found, show it */
        LOG_INF("Id: %d", 1);
        LOG_HEXDUMP_INF(&net_data, 5, "Recovered net data: ");
        core_last_net_packet_set(net_data);
    } else { /* item was not found, add it */
        LOG_INF("No values found for net data");
    }
    LOG_DBG("Restoring data from flash");
}
void STORAGE_task(void)
{
    int error = nvs_init(&zt_fs, DT_FLASH_DEV_NAME);
    if (error) {
        LOG_INF("Flash Init failed");
    } else {
        LOG_INF("NVS started...[OK]");
    }
    restore();
    while (1) {
        k_sleep(K_SECONDS(30));
        store();
    }
}

K_THREAD_DEFINE(STORAGE, 512, STORAGE_task, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
