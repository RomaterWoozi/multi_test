//
// Created by wzx on 2019/5/31.
//

#ifndef SAMPLE1_ZIB_WIFI_H
#define SAMPLE1_ZIB_WIFI_H

#endif //SAMPLE1_ZIB_WIFI_H

// 定义扫描固定SSID的信号强度
#define FACTORY_ZIB_WIFI_SSID   "sz_zib"
#define FACTORY_ZIB_SIG_LEVEL   (-80)
/******************wifi*************/
typedef struct wifi_ap_t {
    char smac[32]; // string mac
    char name[64]; //wifi name
    int  sig_level;
    int frequency;
}wifi_ap_t;

#define WIFI_STATUS_UNKNOWN     0x0001
#define WIFI_STATUS_OPENED      0x0002
#define WIFI_STATUS_SCANNING    0x0004
#define WIFI_STATUS_SCAN_END    0x0008

#define WIFI_CMD_RETRY_NUM      3
#define WIFI_CMD_RETRY_TIME     1 // second
#define DISABLED_DHCP_FAILURE       2
#define DISABLED_UNKNOWN_REASON     0
#define DISABLED_AUTH_FAILURE       3
#define CONNECTED                   5
#define CONNECTING                  6
#define DISABLED_ASSOCIATION_REJECT 4
#define WIFI_MAX_AP             20
#define EVT_MAX_LEN 127


/******************wifi end*************/