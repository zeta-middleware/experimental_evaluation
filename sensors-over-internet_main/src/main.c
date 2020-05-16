/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fs/nvs.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RING_SIZE 11
#define NVS_ID_SENSOR_A 0
#define NVS_ID_NET_RESPONSE 1

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

static struct nvs_fs fs = {
    .sector_size  = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset       = NVS_STORAGE_OFFSET,
};

/* Ring data and indexes */
static u8_t ring_data_a[MAX_RING_SIZE];
static u8_t ring_data_b[MAX_RING_SIZE];
static u32_t ring_data_c[MAX_RING_SIZE];
static u8_t id_a;
static u8_t id_b;
static u8_t id_c;

/* New data receveid from sensors */
static u8_t new_data_a = 0;
static u8_t new_data_b = 0;
static u32_t new_data_c = 0;

/* New net packets */
static u8_t net_request = 0;
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

    return (i == 0) ? 0:(sum / i);
}

static u16_t sensor_b_mean(void)
{
    u16_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_b[i] != 0; i++) {
        sum += ring_data_b[i];
    }

    return (i == 0) ? 0:(sum / i);
}

static u32_t sensor_c_mean(void)
{
    u32_t sum = 0;
    int i     = 0;
    for (i = 0; i < MAX_RING_SIZE && ring_data_c[i] != 0; i++) {
        sum += ring_data_c[i];
    }

    return (i == 0) ? 0:(sum / i);
}

void get_sensors_data_timer_expiry(struct k_timer *timer_id) {
    new_data_a = get_sensor_a();
    new_data_b = get_sensor_b();
    new_data_c = get_sensor_c();
    k_sem_give(&get_sensors_data_sem);
}

static u8_t save_data_in_flash;

void get_net_request_packet_timer_expiry(struct k_timer *timer_id) {
    k_sem_give(&generate_net_request_packet_sem);
    if (++save_data_in_flash >= 3) {
        save_data_in_flash = 0;
        k_sem_give(&nvs_sem);
    }
}

u8_t generate_random_number(u8_t lower, u8_t upper)
{
    return (rand() % (upper - lower + 1)) + lower;
}

static void handle_net_requests(void)
{
    u8_t num                  = generate_random_number(1, 36);
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

    printk("Getting a virtual packet request from net with id: %02X\n",
            net_request);
    printk("Sending a net packet request to NET_REQUEST channel...\n");
    k_sem_give(&get_net_request_packet_sem);
}

void nvs_save_data(void) {
    int bytes_written = nvs_write(&fs, NVS_ID_SENSOR_A, &ring_data_a[(id_a == 0) ? MAX_RING_SIZE : id_a - 1], sizeof(u8_t));
    if (bytes_written > 0) {
        printk("Sensor A was saved on the flash!\n");
    }
    bytes_written = nvs_write(&fs, NVS_ID_NET_RESPONSE, net_response, sizeof(u8_t) * 5);
    if (bytes_written > 0) {
        printk("Net response was saved on the flash!\n");
    }
}

void recovery_data_from_flash(void) {
    printk("Recovering data from flash [ ]\n");
    int rc = 0;
    u8_t data_a_flash = 0;
    u8_t net_response_flash[5] = {0};

    rc = nvs_read(&fs, NVS_ID_SENSOR_A, &data_a_flash, sizeof(u8_t));
    if (rc > 0) {
        printk("Id: %d, value: %d\n", NVS_ID_SENSOR_A, data_a_flash);
    }

    rc = nvs_read(&fs, NVS_ID_NET_RESPONSE, net_response_flash, 5);
    if (rc > 0) {
        printk("Id: %d, value: ", NVS_ID_NET_RESPONSE);
        for (size_t i = 0; i < 5; i++) {
            printk(" %02X", net_response_flash[i]);
        }
        printk("|");
        for (size_t i = 0; i < 5; i++) {
            if (32 <= net_response_flash[i]
                && net_response_flash[i] <= 126) {
                printk("%c", net_response_flash[i]);
            } else {
                printk(".");
            }
        }        
    }
    printk("\n");
    printk("Recovering data from flash [X]\n");
}

void main(void)
{
    int nvs_err = nvs_init(&fs, DT_FLASH_DEV_NAME);
    if (nvs_err) {
        printk("Nvs init failed\n");
    }
    else {
        printk("Nvs started...\n");
        recovery_data_from_flash();
    }

        
    // Setup code
    k_timer_start(&get_sensors_data_timer, K_SECONDS(3), K_SECONDS(3));
    k_timer_start(&get_net_request_packet_timer, K_SECONDS(10), K_SECONDS(10));

    
    
    while(1) { // Loop code
        if (!k_sem_take(&get_sensors_data_sem, K_NO_WAIT)) {
            ring_data_a[id_a] = new_data_a;
            id_a              = (id_a + 1) % MAX_RING_SIZE;
            printk("Data received from sensor A: %d\n", new_data_a);
            ring_data_b[id_b] = new_data_b;
            id_b              = (id_b + 1) % MAX_RING_SIZE;
            printk("Data received from sensor B: %d\n", new_data_b);
            ring_data_c[id_c] = new_data_c;
            id_c              = (id_c + 1) % MAX_RING_SIZE;
            printk("Data received from sensor C: %d\n", new_data_c);
        }
        if (!k_sem_take(&get_net_request_packet_sem, K_NO_WAIT)) {
            memset(net_response, 0, 5);
            net_response[0] = net_request;
            if (net_request == 0xA0) {  // Requesting last A data
                net_response[1] =
                    ring_data_a[(id_a == 0) ? MAX_RING_SIZE : id_a - 1];
            } else if (net_request == 0xA1) {  // Requesting last B data
                net_response[1] =
                    ring_data_b[(id_b == 0) ? MAX_RING_SIZE : id_b - 1];
            } else if (net_request == 0xA2) {  // Requesting last C data
                memcpy(net_response + 1,
                       &ring_data_c[(id_c == 0) ? MAX_RING_SIZE : id_c - 1], sizeof(u32_t));
            } else if (net_request == 0xA3) {  // Requesting A mean
                u16_t a_mean = sensor_a_mean();
                memcpy(net_response + 1, &a_mean, sizeof(u16_t));
            } else if (net_request == 0xA4) {  // Requesting B mean
                u16_t b_mean = sensor_b_mean();
                memcpy(net_response + 1, &b_mean, sizeof(u16_t));
            } else if (net_request == 0xA5) {  // Requesting C mean
                u32_t c_mean = sensor_c_mean();
                memcpy(net_response + 1, &c_mean, sizeof(u32_t));
            }
            else {
                printk("Net request sent is invalid!\n");
            }
            printk("Net request received with ID: %02X\n", net_request);
            printk("Sending a net response...\n");
            k_sem_give(&get_net_response_packet_sem);
        }
        if (!k_sem_take(&get_net_response_packet_sem, K_NO_WAIT)) {
            printk("Net response received...\n");
            if (net_response[0] == 0xA0) {  // Requesting last A data
                printk("Last sensor A data saved: %d\n", net_response[1]);
            } else if (net_response[0] == 0xA1) {  // Requesting last B data
                printk("Last sensor B data saved: %d\n", net_response[1]);
            } else if (net_response[0] == 0xA2) {  // Requesting last C data
                u32_t c_value = 0;
                memcpy(&c_value, net_response + 1, sizeof(u32_t));
                printk("Last sensor C data saved: %d\n", c_value);
            } else if (net_response[0] == 0xA3) {  // Requesting A mean
                u16_t a_mean = 0;
                memcpy(&a_mean, net_response + 1, sizeof(u16_t));
                printk("Current A mean: %d\n", a_mean);
            } else if (net_response[0] == 0xA4) {  // Requesting B mean
                u16_t b_mean = 0;
                memcpy(&b_mean, net_response + 1, sizeof(u16_t));
                printk("Current B mean: %d\n", b_mean);
            } else if (net_response[0] == 0xA5) {  // Requesting C mean
                u32_t c_mean = 0;
                memcpy(&c_mean, net_response + 1, sizeof(u32_t));
                printk("Current C mean: %d\n", c_mean);
            } else {
                printk("Net response sent is invalid!\n");
            }
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
