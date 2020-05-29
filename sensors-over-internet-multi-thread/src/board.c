#include <logging/log.h>
#include <zephyr.h>
#include "kernel.h"

#include "board.h"

LOG_MODULE_REGISTER(board, 4);

K_MSGQ_DEFINE(sensor_a_data_stream, 1, 10, 1);
K_MSGQ_DEFINE(sensor_b_data_stream, 1, 10, 1);
K_MSGQ_DEFINE(sensor_c_data_stream, 4, 10, 4);

K_SEM_DEFINE(sensor_a_sem, 1, 1);

static u8_t __sensor_a_last_data = 0;

int board_sensor_a_last_data_get(u8_t *last_data)
{
    int error = k_sem_take(&sensor_a_sem, K_MSEC(500));
    if (!error) {
        *last_data = __sensor_a_last_data;
        k_sem_give(&sensor_a_sem);
    }
    return error;
}

int board_sensor_a_last_data_set(u8_t *last_data)
{
    int error = k_sem_take(&sensor_a_sem, K_MSEC(500));
    if (!error) {
        __sensor_a_last_data = *last_data;
        k_sem_give(&sensor_a_sem);
    }
    return error;
}

static u8_t get_sensor_a(void)
{
    static u8_t data_a = 20;

    data_a++;
    if (data_a > 40) {
        data_a = 20;
    }

    return data_a;
}

static u8_t get_sensor_b(void)
{
    static u8_t data_b = 90;

    data_b--;
    if (data_b < 40) {
        data_b = 90;
    }

    return data_b;
}

static u32_t get_sensor_c(void)
{
    static u32_t data_c = 101325;

    data_c += 27;
    if (data_c > 102000) {
        data_c = 101325;
    }

    return data_c;
}

void BOARD_task()
{
    LOG_DBG("BOARD Service has started...[OK]");
    u8_t sensor_a_data  = 0;
    u8_t sensor_b_data  = 0;
    u32_t sensor_c_data = 0;
    while (1) {
        sensor_a_data = get_sensor_a();
        board_sensor_a_last_data_set(&sensor_a_data);
        k_msgq_put(&sensor_a_data_stream, &sensor_a_data, K_MSEC(500));
        sensor_b_data = get_sensor_b();
        k_msgq_put(&sensor_b_data_stream, &sensor_b_data, K_MSEC(500));
        sensor_c_data = get_sensor_c();
        k_msgq_put(&sensor_c_data_stream, &sensor_c_data, K_MSEC(500));
        k_sleep(K_SECONDS(3));
    }
}

K_THREAD_DEFINE(BOARD, 512, BOARD_task, NULL, NULL, NULL, 3, 0, 0);
