#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include<cutils/log.h>
#include <private/android_filesystem_config.h>
#include <android/log.h>
#include <hardware/hardware.h>
#include <hardware/bluetooth.h>
#include <hardware/bt_gatt.h>
#include "commm_util.h"
/************************************************************************************
**  Constants & Macros
************************************************************************************/

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define CASE_RETURN_STR(const) case const: return #const;

#define HCI_DUT_SET_TXPWR   0xFCE1
#define HCI_DUT_SET_RXGIAN  0xFCE2
#define HCI_DUT_GET_RXDATA  0xFCE3
#define HCI_LE_RECEIVER_TEST_OPCODE 0x201D
#define HCI_LE_TRANSMITTER_TEST_OPCODE 0x201E
#define HCI_LE_END_TEST_OPCODE 0x201F
#define is_cmd(str) ((strlen(str) == strlen(cmd)) && strncmp((const char *)&cmd, str, strlen(str)) == 0)
#define if_cmd(str)  if (is_cmd(str))
#define PID_FILE "/data/.bdt_pid"


typedef unsigned char  byte;
typedef void (t_console_cmd_handler) (char *p);



#ifdef __cplusplus
extern "C" {
#endif


int get_discovery_state_changed_status();

int get_device_find_status();

void do_enable(char *p);

void do_pair(char *p);

void do_start_discovery();

void do_stop_discovery();

void do_disable(char *p);

void do_btgattc_register_app();

void do_btgattc_unregister_app();

void do_btgatts_start_advertise();

void do_btgatts_init_server();

void do_btgatts_send_notification();

void do_help(char *p);

void do_quit(char *p);

void setup_test_env(void);

void bdt_log(const char *fmt_str, ...);

void config_permissions(void);

int HAL_load(void);

void bdt_init(void);

void gatt_init(void);

void process_cmd(char *p, unsigned char is_job);

int HAL_unload(void);

int zb_bluetooth_test();

#ifdef __cplusplus
}
#endif