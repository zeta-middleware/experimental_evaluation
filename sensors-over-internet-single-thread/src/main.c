/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/flash.h>
#include <fs/nvs.h>
#include <logging/log.h>
#include <storage/flash_map.h>
#include <string.h>
#include <zephyr.h>

#define MAX_RING_SIZE 11
#define NVS_ID_SENSOR_A 0
#define NVS_ID_NET_RESPONSE 1

LOG_MODULE_REGISTER(system, 4);

void get_sensors_data_timer_expiry(struct k_timer *timer_id);
void get_net_request_packet_timer_expiry(struct k_timer *timer_id);

K_TIMER_DEFINE(get_sensors_data_timer, get_sensors_data_timer_expiry, NULL);
K_TIMER_DEFINE(get_net_request_packet_timer, get_net_request_packet_timer_expiry, NULL);
K_SEM_DEFINE(nvs_sem, 0, 1);
K_SEM_DEFINE(get_sensors_data_sem, 0, 1);
K_SEM_DEFINE(get_net_request_packet_sem, 0, 1);
K_SEM_DEFINE(generate_net_request_packet_sem, 0, 1);
K_SEM_DEFINE(get_net_response_packet_sem, 0, 1);

#define NVS_SECTOR_SIZE DT_FLASH_ERASE_BLOCK_SIZE
#define NVS_SECTOR_COUNT 4
#define NVS_STORAGE_OFFSET DT_FLASH_AREA_STORAGE_OFFSET

static struct nvs_fs fs;
/* Ring data and indexes */
static u8_t ring_data_a[MAX_RING_SIZE];
static u8_t ring_data_b[MAX_RING_SIZE];
static u32_t ring_data_c[MAX_RING_SIZE];
static u8_t id_a;
static u8_t id_b;
static u8_t id_c;

/* New data receveid from sensors */
static u8_t new_data_a  = 0;
static u8_t new_data_b  = 0;
static u32_t new_data_c = 0;

/* New net packets */
static u8_t net_request     = 0;
static u8_t net_response[5] = {0};

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

static u16_t sensor_a_mean(void)
{
    u16_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_a[i] != 0; i++) {
        sum += ring_data_a[i];
    }

    return (i == 0) ? 0 : (sum / i);
}

static u16_t sensor_b_mean(void)
{
    u16_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_b[i] != 0; i++) {
        sum += ring_data_b[i];
    }

    return (i == 0) ? 0 : (sum / i);
}

static u32_t sensor_c_mean(void)
{
    u32_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_c[i] != 0; i++) {
        sum += ring_data_c[i];
    }

    return (i == 0) ? 0 : (sum / i);
}

void get_sensors_data_timer_expiry(struct k_timer *timer_id)
{
    new_data_a = get_sensor_a();
    new_data_b = get_sensor_b();
    new_data_c = get_sensor_c();
    k_sem_give(&get_sensors_data_sem);
}

static u8_t save_data_in_flash;

void get_net_request_packet_timer_expiry(struct k_timer *timer_id)
{
    k_sem_give(&generate_net_request_packet_sem);
    if (++save_data_in_flash >= 3) {
        save_data_in_flash = 0;
        k_sem_give(&nvs_sem);
    }
}

u8_t generate_random_number(u8_t lower, u8_t upper)
{
    return (sys_rand32_get() % (upper - lower + 1)) + lower;
}

static void handle_net_requests(void)
{
    u8_t num    = generate_random_number(1, 36);
    net_request = 0;
    if (num <= 6) {
        net_request = 0xA0;
    } else if (num <= 12) {
        net_request = 0xA1;
    } else if (num <= 18) {
        net_request = 0xA2;
    } else if (num <= 24) {
        net_request = 0xA3;
    } else if (num <= 30) {
        net_request = 0xA4;
    } else if (num <= 36) {
        net_request = 0xA5;
    }

    LOG_DBG("Getting a virtual packet request from net with id: %02X", net_request);
    LOG_DBG("Sending a net packet request to NET_REQUEST channel...");
    k_sem_give(&get_net_request_packet_sem);
}

void nvs_save_data(void)
{
    int bytes_written =
        nvs_write(&fs, NVS_ID_SENSOR_A,
                  &ring_data_a[(id_a == 0) ? MAX_RING_SIZE : id_a - 1], sizeof(u8_t));
    if (bytes_written > 0) {
        LOG_DBG("Sensor A was saved on the flash!");
    }
    bytes_written = nvs_write(&fs, NVS_ID_NET_RESPONSE, net_response, sizeof(u8_t) * 5);
    if (bytes_written > 0) {
        LOG_DBG("Net response was saved on the flash!");
    }
}

void recovery_data_from_flash(void)
{
    LOG_DBG("Recovering data from flash [ ]");
    int rc                     = 0;
    u8_t data_a_flash          = 0;
    u8_t net_response_flash[5] = {0};

    rc = nvs_read(&fs, NVS_ID_SENSOR_A, &data_a_flash, sizeof(u8_t));
    if (rc > 0) {
        LOG_DBG("Id: %d, value: %d", NVS_ID_SENSOR_A, data_a_flash);
    }

    rc = nvs_read(&fs, NVS_ID_NET_RESPONSE, net_response_flash, 5);
    if (rc > 0) {
        LOG_DBG("Id: %d", NVS_ID_NET_RESPONSE);
        LOG_HEXDUMP_DBG(net_response_flash, 5, "Value:");
        /* for (size_t i = 0; i < 5; i++) { */
        /*     LOG_DBG(" %02X", net_response_flash[i]); */
        /* } */
        /* LOG_DBG("|"); */
        /* for (size_t i = 0; i < 5; i++) { */
        /*     if (32 <= net_response_flash[i] && net_response_flash[i] <= 126) { */
        /*         LOG_DBG("%c", net_response_flash[i]); */
        /*     } else { */
        /*         LOG_DBG("."); */
        /*     } */
        /* } */
    }
    LOG_DBG("Recovering data from flash [X]");
}

void main(void)
{
    struct flash_pages_info info;
    fs.offset = FLASH_AREA_OFFSET(storage);
    int rc    = flash_get_page_info_by_offs(
        device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL), fs.offset, &info);
    if (rc) {
        printk("Unable to get page info");
    }
    fs.sector_size  = info.size;
    fs.sector_count = 3U;

    int nvs_err = nvs_init(&fs, DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);
    if (nvs_err) {
        LOG_DBG("Nvs init failed");
    } else {
        LOG_DBG("Nvs started...");
        recovery_data_from_flash();
    }

    // Setup code
    k_timer_start(&get_sensors_data_timer, K_SECONDS(3), K_SECONDS(3));
    k_timer_start(&get_net_request_packet_timer, K_SECONDS(10), K_SECONDS(10));

    while (1) {  // Loop code
        if (!k_sem_take(&get_sensors_data_sem, K_NO_WAIT)) {
            ring_data_a[id_a] = new_data_a;
            id_a              = (id_a + 1) % MAX_RING_SIZE;
            LOG_DBG("Data received from sensor A: %d", new_data_a);
            ring_data_b[id_b] = new_data_b;
            id_b              = (id_b + 1) % MAX_RING_SIZE;
            LOG_DBG("Data received from sensor B: %d", new_data_b);
            ring_data_c[id_c] = new_data_c;
            id_c              = (id_c + 1) % MAX_RING_SIZE;
            LOG_DBG("Data received from sensor C: %d", new_data_c);
        }
        if (!k_sem_take(&get_net_request_packet_sem, K_NO_WAIT)) {
            memset(net_response, 0, 5);
            net_response[0] = net_request;
            if (net_request == 0xA0) {  // Requesting last A data
                net_response[1] = ring_data_a[(id_a == 0) ? MAX_RING_SIZE : id_a - 1];
            } else if (net_request == 0xA1) {  // Requesting last B data
                net_response[1] = ring_data_b[(id_b == 0) ? MAX_RING_SIZE : id_b - 1];
            } else if (net_request == 0xA2) {  // Requesting last C data
                memcpy(net_response + 1,
                       &ring_data_c[(id_c == 0) ? MAX_RING_SIZE : id_c - 1],
                       sizeof(u32_t));
            } else if (net_request == 0xA3) {  // Requesting A mean
                u16_t a_mean = sensor_a_mean();
                memcpy(net_response + 1, &a_mean, sizeof(u16_t));
            } else if (net_request == 0xA4) {  // Requesting B mean
                u16_t b_mean = sensor_b_mean();
                memcpy(net_response + 1, &b_mean, sizeof(u16_t));
            } else if (net_request == 0xA5) {  // Requesting C mean
                u32_t c_mean = sensor_c_mean();
                memcpy(net_response + 1, &c_mean, sizeof(u32_t));
            } else {
                LOG_DBG("Net request sent is invalid!");
            }
            LOG_DBG("Net request received with ID: %02X", net_request);
            LOG_DBG("Sending a net response...");
            k_sem_give(&get_net_response_packet_sem);
        }
        if (!k_sem_take(&get_net_response_packet_sem, K_NO_WAIT)) {
            LOG_DBG("Net response received...");
            if (net_response[0] == 0xA0) {  // Requesting last A data
                LOG_DBG("Last sensor A data saved: %d", net_response[1]);
            } else if (net_response[0] == 0xA1) {  // Requesting last B data
                LOG_DBG("Last sensor B data saved: %d", net_response[1]);
            } else if (net_response[0] == 0xA2) {  // Requesting last C data
                u32_t c_value = 0;
                memcpy(&c_value, net_response + 1, sizeof(u32_t));
                LOG_DBG("Last sensor C data saved: %d", c_value);
            } else if (net_response[0] == 0xA3) {  // Requesting A mean
                u16_t a_mean = 0;
                memcpy(&a_mean, net_response + 1, sizeof(u16_t));
                LOG_DBG("Current A mean: %d", a_mean);
            } else if (net_response[0] == 0xA4) {  // Requesting B mean
                u16_t b_mean = 0;
                memcpy(&b_mean, net_response + 1, sizeof(u16_t));
                LOG_DBG("Current B mean: %d", b_mean);
            } else if (net_response[0] == 0xA5) {  // Requesting C mean
                u32_t c_mean = 0;
                memcpy(&c_mean, net_response + 1, sizeof(u32_t));
                LOG_DBG("Current C mean: %d", c_mean);
            } else {
                LOG_DBG("Net response sent is invalid!");
            }
            u32_t per = cpu_stats_non_idle_and_sched_get_percent();
            LOG_WRN("CPU usage: %u%%", per);
            cpu_stats_reset_counters();
        }
        if (!k_sem_take(&generate_net_request_packet_sem, K_NO_WAIT)) {
            handle_net_requests();
        }
        if (!k_sem_take(&nvs_sem, K_NO_WAIT) && !nvs_err) {
            nvs_save_data();
        }

        k_sleep(K_MSEC(50));
    }
}
