#include <logging/log.h>
#include <stdlib.h>
#include <zephyr.h>
#include "kernel.h"

LOG_MODULE_REGISTER(net, 4);

extern struct k_msgq net_uplink_msgq;  // from Core

K_MSGQ_DEFINE(net_request_msgq, 1, 10, 1);

u8_t generate_random_number(u8_t lower, u8_t upper)
{
    return (sys_rand32_get() % (upper - lower + 1)) + lower;
}

static void handle_net_requests(void)
{
    u8_t num            = generate_random_number(1, 36);
    u8_t packet_request = 0;
    if (num <= 6) {
        packet_request = 0xA0;
    } else if (num <= 12) {
        packet_request = 0xA1;
    } else if (num <= 18) {
        packet_request = 0xA2;
    } else if (num <= 24) {
        packet_request = 0xA3;
    } else if (num <= 30) {
        packet_request = 0xA4;
    } else if (num <= 36) {
        packet_request = 0xA5;
    }
    LOG_DBG("Getting a virtual packet request from net with id: %02X", packet_request);
    LOG_DBG("Sending a net packet request to NET_REQUEST channel...");
    k_msgq_put(&net_request_msgq, &packet_request, K_MSEC(500));
}

static void net_handle_channel_callback(u8_t *net_response)
{
    LOG_DBG("Net response received: %02X", net_response[0]);
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
}

void NET_task()
{
    LOG_DBG("NET Service has started...[OK]");
    u8_t net_packet[5] = {0};
    while (1) {
        k_sleep(K_SECONDS(9));
        handle_net_requests();
        k_sleep(K_SECONDS(1));
        if (!k_msgq_get(&net_uplink_msgq, net_packet, K_NO_WAIT)) {
            net_handle_channel_callback(net_packet);
        }
        u32_t per = cpu_stats_non_idle_and_sched_get_percent();
        LOG_WRN("CPU usage: %u%%\n", per);
        cpu_stats_reset_counters();
    }
}

K_THREAD_DEFINE(NET, 512, NET_task, NULL, NULL, NULL, 2, 0, 0);
