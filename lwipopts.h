#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// lwIP configuration, based on pico-examples/pico_w/wifi/lwipopts_examples_common.h
// (see https://www.nongnu.org/lwip/2_1_x/group__lwip__opts.html for details)

#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0
// we use pico_cyw43_arch_lwip_poll, so libc malloc is safe
#define MEM_LIBC_MALLOC             1
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0
#define LWIP_CHKSUM_ALGORITHM       3
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// ---- SNTP (used to keep the clock correct for alarms) ----
// SNTP needs one extra system timeout
#define MEMP_NUM_SYS_TIMEOUT        (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)
#define SNTP_SERVER_DNS             1
#define SNTP_SERVER_ADDRESS         "pool.ntp.org"
#define SNTP_CHECK_RESPONSE         2
#define SNTP_COMP_ROUNDTRIP         0
#define SNTP_STARTUP_DELAY          1
#define SNTP_STARTUP_DELAY_FUNC     (LWIP_RAND() % 5000)
#define SNTP_RECV_TIMEOUT           15000
// re-sync once an hour
#define SNTP_UPDATE_DELAY           3600000
#define SNTP_RETRY_TIMEOUT          SNTP_RECV_TIMEOUT
#define SNTP_RETRY_TIMEOUT_MAX      (SNTP_RETRY_TIMEOUT * 10)
#define SNTP_RETRY_TIMEOUT_EXP      1

#define SNTP_GET_SYSTEM_TIME(sec, us)    clock_get_utc(&(sec), &(us))
#define SNTP_SET_SYSTEM_TIME_US(sec, us) clock_set_utc(sec, us)

#include <stdint.h>
void clock_set_utc(uint32_t sec, uint32_t us);
void clock_get_utc(uint32_t *sec, uint32_t *us);

#endif /* _LWIPOPTS_H */
