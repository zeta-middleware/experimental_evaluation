#include <kernel.h>
#include <logging/log.h>
#include <string.h>
#include <zephyr.h>

#include "core.h"

#define MAX_RING_SIZE 11

LOG_MODULE_REGISTER(core, 4);

extern struct k_msgq net_request_msgq;      // from Net
extern struct k_msgq sensor_a_data_stream;  // from Sensor
extern struct k_msgq sensor_b_data_stream;  // from Sensor
extern struct k_msgq sensor_c_data_stream;  // from Sensor

K_MSGQ_DEFINE(net_uplink_msgq, 5, 10, 8);

K_SEM_DEFINE(core_last_data_sem, 1, 1);

static u8_t __last_data[5] = {0};

static u8_t ring_data_a[MAX_RING_SIZE];
static u8_t ring_data_b[MAX_RING_SIZE];
static u32_t ring_data_c[MAX_RING_SIZE];
static u8_t id_a;
static u8_t id_b;
static u8_t id_c;

int core_last_net_packet_get(u8_t *last_data)
{
    int error = k_sem_take(&core_last_data_sem, K_MSEC(500));
    if (!error) {
        memcpy(last_data, __last_data, 5);
        k_sem_give(&core_last_data_sem);
    }
    return error;
}

int core_last_net_packet_set(u8_t *last_data)
{
    int error = k_sem_take(&core_last_data_sem, K_MSEC(500));
    if (!error) {
        memcpy(__last_data, last_data, 5);
        k_sem_give(&core_last_data_sem);
    }
    return error;
}

static u16_t sensor_a_mean(void)
{
    u16_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_a[i] != 0; i++) {
        sum += ring_data_a[i];
    }

    LOG_DBG("A Mean -> i: %d, sum: %d, mean: %d", i, sum, sum / i);
    return sum / i;
}

static u16_t sensor_b_mean(void)
{
    u16_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_b[i] != 0; i++) {
        sum += ring_data_b[i];
    }

    LOG_DBG("B Mean -> i: %d, sum: %d, mean: %d", i, sum, sum / i);
    return sum / i;
}

static u32_t sensor_c_mean(void)
{
    u32_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_c[i] != 0; i++) {
        sum += ring_data_c[i];
    }

    LOG_DBG("C Mean -> i: %d, sum: %d, mean: %d", i, sum, sum / i);
    return sum / i;
}

static void core_process_sensors()
{
    u8_t data_u8 = {0};
    if (!k_msgq_get(&sensor_a_data_stream, &data_u8, K_NO_WAIT)) {
        ring_data_a[id_a] = data_u8;
        id_a              = (id_a + 1) % MAX_RING_SIZE;
        LOG_DBG("Data received from sensor A: %d", data_u8);
    }
    if (!k_msgq_get(&sensor_b_data_stream, &data_u8, K_NO_WAIT)) {
        ring_data_b[id_b] = data_u8;
        id_b              = (id_b + 1) % MAX_RING_SIZE;
        LOG_DBG("Data received from sensor B: %d", data_u8);
    }
    u32_t data_u32 = {0};
    if (!k_msgq_get(&sensor_c_data_stream, &data_u32, K_NO_WAIT)) {
        ring_data_c[id_c] = data_u32;
        id_c              = (id_c + 1) % MAX_RING_SIZE;
        LOG_DBG("Data received from sensor C: %d", data_u32);
    }
}

void CORE_task()
{
    LOG_DBG("CORE Service has started...[OK]");
    u8_t net_request     = 0;
    u8_t net_response[5] = {0};
    while (1) {
        net_request = 0;
        memset(net_response, 0, 5);
        core_process_sensors();
        if (!k_msgq_get(&net_request_msgq, &net_request, K_NO_WAIT)) {
            LOG_WRN("Net request: 0x%02x", net_request);
            net_response[0] = net_request;
            switch (net_request) {
            case 0xA0:
                net_response[1] = ring_data_a[(id_a == 0) ? MAX_RING_SIZE : id_a - 1];
                break;
            case 0xA1:
                net_response[1] = ring_data_b[(id_b == 0) ? MAX_RING_SIZE : id_b - 1];
                break;
            case 0xA2:
                memcpy(net_response + 1,
                       &ring_data_c[(id_c == 0) ? MAX_RING_SIZE : id_c - 1],
                       sizeof(u32_t));
                break;
            case 0xA3: {
                u16_t a_mean = sensor_a_mean();
                memcpy(net_response + 1, &a_mean, sizeof(u16_t));
            } break;
            case 0xA4: {
                u16_t b_mean = sensor_b_mean();
                memcpy(net_response + 1, &b_mean, sizeof(u16_t));
            } break;
            case 0xA5: {
                u32_t c_mean = sensor_c_mean();
                memcpy(net_response + 1, &c_mean, sizeof(u32_t));
            } break;
            default:
                LOG_ERR("Request code wrong: 0x%02x", net_request);
            }
            core_last_net_packet_set(net_response);
            k_msgq_put(&net_uplink_msgq, net_response, K_MSEC(100));
        }
        k_sleep(K_SECONDS(3));
    }
}

K_THREAD_DEFINE(CORE, 512, CORE_task, NULL, NULL, NULL, 3, 0, K_NO_WAIT);
