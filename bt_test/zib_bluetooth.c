/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/************************************************************************************
 *
 *  Filename:      bluetoothtest.c
 *
 *  Description:   Bluetooth Test application
 *
 ***********************************************************************************/

#include "zib_bluetooth.h"
//#includes <zib_debug.h>



/************************************************************************************
**  Local type definitions
************************************************************************************/

/************************************************************************************
**  Static variables
************************************************************************************/

typedef struct {
    const char *name;
    t_console_cmd_handler *handler;
    const char *help;
    unsigned char is_job;
} t_cmd;



int main_done = 0;
static bt_status_t status;

/* Main API */
static bluetooth_device_t* bt_device;

const bt_interface_t* sBtInterface = NULL;
const btgatt_interface_t *sGattIf = NULL;

static gid_t groups[] = { AID_NET_BT, AID_INET, AID_NET_BT_ADMIN,
                          AID_SYSTEM, AID_MISC, AID_SDCARD_RW,
                          AID_NET_ADMIN, AID_VPN};

typedef char bdstr_t[18];

/* Set to 1 when the Bluedroid stack is enabled */
static unsigned char bt_enabled = 0;


/************************************************************************************
**  Static functions
************************************************************************************/

void process_cmd(char *p, unsigned char is_job);
static void job_handler(void *param);
int  device_find_status=0;
int discovery_state_changed_status=-1;

/************************************************************************************
**  Externs
************************************************************************************/

/************************************************************************************
**  Functions
************************************************************************************/


/************************************************************************************
**  Shutdown helper functions
************************************************************************************/

static void bdt_shutdown(void)
{
    ZIB_DBG("shutdown bdroid test app\n");
    main_done = 1;
}

int str2bd(char *str, bt_bdaddr_t *addr)
{
    int32_t i = 0;
    for (i = 0; i < 6; i++) {
       addr->address[i] = (uint8_t) strtoul(str, (char **)&str, 16);
       str++;
    }

    return 0;
}
char *bd2str(const bt_bdaddr_t *bdaddr, bdstr_t *bdstr)
{
    char *addr = (char *) bdaddr->address;

    sprintf((char*)bdstr, "%02x:%02x:%02x:%02x:%02x:%02x",
                       (int)addr[0],(int)addr[1],(int)addr[2],
                       (int)addr[3],(int)addr[4],(int)addr[5]);
    return (char *)bdstr;
}

/*****************************************************************************
** Android's init.rc does not yet support applying linux capabilities
*****************************************************************************/

void config_permissions(void)
{
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct cap;

    ZIB_DBG("set_aid_and_cap : pid %d, uid %d gid %d", getpid(), getuid(), getgid());

    header.pid = 0;

    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

    setuid(AID_ROOT);
    setgid(AID_NET_BT_STACK);

    header.version = _LINUX_CAPABILITY_VERSION;

    cap.effective = cap.permitted =  cap.inheritable =
                    1 << CAP_NET_RAW |
                    1 << CAP_NET_ADMIN |
                    1 << CAP_NET_BIND_SERVICE |
                    1 << CAP_SYS_RAWIO |
                    1 << CAP_SYS_NICE |
                    1 << CAP_SETGID;

    capset(&header, &cap);
    setgroups(sizeof(groups)/sizeof(groups[0]), groups);
}



/*****************************************************************************
**   Logger API
*****************************************************************************/

void bdt_log(const char *fmt_str, ...)
{
    static char buffer[1024];
    va_list ap;

    va_start(ap, fmt_str);
    vsnprintf(buffer, 1024, fmt_str, ap);
    va_end(ap);

    fprintf(stdout, "%s\n", buffer);
}

/*******************************************************************************
 ** Misc helper functions
 *******************************************************************************/
static const char* dump_bt_status(bt_status_t status)
{
    switch(status)
    {
        CASE_RETURN_STR(BT_STATUS_SUCCESS)
        CASE_RETURN_STR(BT_STATUS_FAIL)
        CASE_RETURN_STR(BT_STATUS_NOT_READY)
        CASE_RETURN_STR(BT_STATUS_NOMEM)
        CASE_RETURN_STR(BT_STATUS_BUSY)
        CASE_RETURN_STR(BT_STATUS_UNSUPPORTED)

        default:
            return "unknown status code";
    }
}

static void hex_dump(char *msg, void *data, int size, int trunc)
{
    unsigned char *p = data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};

    ZIB_DBG("%s  \n", msg);

    /* truncate */
    if(trunc && (size>32))
        size = 32;

    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4x",
               ((unsigned int)p-(unsigned int)data) );
        }

        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) {
            /* line completed */
            ZIB_DBG("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
        ZIB_DBG("[%4.4s]   %-50.50s  %s\n", addrstr, hexstr, charstr);
    }
}

/*******************************************************************************
 ** Console helper functions
 *******************************************************************************/

void skip_blanks(char **p)
{
  while (**p == ' ')
    (*p)++;
}

uint32_t get_int(char **p, int DefaultValue)
{
  uint32_t Value = 0;
  unsigned char   UseDefault;

  UseDefault = 1;
  skip_blanks(p);

  while ( ((**p)<= '9' && (**p)>= '0') )
    {
      Value = Value * 10 + (**p) - '0';
      UseDefault = 0;
      (*p)++;
    }

  if (UseDefault)
    return DefaultValue;
  else
    return Value;
}

int get_signed_int(char **p, int DefaultValue)
{
  int    Value = 0;
  unsigned char   UseDefault;
  unsigned char  NegativeNum = 0;

  UseDefault = 1;
  skip_blanks(p);

  if ( (**p) == '-')
    {
      NegativeNum = 1;
      (*p)++;
    }
  while ( ((**p)<= '9' && (**p)>= '0') )
    {
      Value = Value * 10 + (**p) - '0';
      UseDefault = 0;
      (*p)++;
    }

  if (UseDefault)
    return DefaultValue;
  else
    return ((NegativeNum == 0)? Value : -Value);
}

void get_str(char **p, char *Buffer)
{
  skip_blanks(p);

  while (**p != 0 && **p != ' ')
    {
      *Buffer = **p;
      (*p)++;
      Buffer++;
    }

  *Buffer = 0;
}

uint32_t get_hex(char **p, int DefaultValue)
{
  uint32_t Value = 0;
  unsigned char   UseDefault;

  UseDefault = 1;
  skip_blanks(p);

  while ( ((**p)<= '9' && (**p)>= '0') ||
          ((**p)<= 'f' && (**p)>= 'a') ||
          ((**p)<= 'F' && (**p)>= 'A') )
    {
      if (**p >= 'a')
        Value = Value * 16 + (**p) - 'a' + 10;
      else if (**p >= 'A')
        Value = Value * 16 + (**p) - 'A' + 10;
      else
        Value = Value * 16 + (**p) - '0';
      UseDefault = 0;
      (*p)++;
    }

  if (UseDefault)
    return DefaultValue;
  else
    return Value;
}

void get_bdaddr(const char *str, bt_bdaddr_t *bd) {
    char *d = ((char *)bd), *endp;
    int i;
    for(i = 0; i < 6; i++) {
        *d++ = strtol(str, &endp, 16);
        if (*endp != ':' && i != 5) {
            memset(bd, 0, sizeof(bt_bdaddr_t));
            return;
        }
        str = endp + 1;
    }
}

const t_cmd console_cmd_list[];
static int console_cmd_maxlen = 0;

static void cmdjob_handler(void *param)
{
    char *job_cmd = (char*)param;

    ZIB_DBG("cmdjob starting (%s)", job_cmd);

    process_cmd(job_cmd, 1);

    ZIB_DBG("cmdjob terminating");

    free(job_cmd);
}

static int create_cmdjob(char *cmd)
{
    pthread_t thread_id;
    char *job_cmd;

    job_cmd = malloc(strlen(cmd)+1); /* freed in job handler */
    strcpy(job_cmd, cmd);

    if (pthread_create(&thread_id, NULL,
                       (void*)cmdjob_handler, (void*)job_cmd)!=0)
      perror("pthread_create");

    return 0;
}
/*******************************************************************************
 ** Load stack lib
 *******************************************************************************/

int HAL_load(void)
{
    int err = 0;

    hw_module_t* module;
    hw_device_t* device;

    ZIB_DBG("Loading HAL lib + extensions");

    err = hw_get_module(BT_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
    if (err == 0)
    {
        err = module->methods->open(module, BT_HARDWARE_MODULE_ID, &device);
        if (err == 0) {
            bt_device = (bluetooth_device_t *)device;
            sBtInterface = bt_device->get_bluetooth_interface();
        }
    }

    ZIB_DBG("HAL library loaded (%s)", strerror(err));

    return err;
}

int HAL_unload(void)
{
    int err = 0;

    ZIB_DBG("Unloading HAL lib");

    sBtInterface = NULL;

    ZIB_DBG("HAL library unloaded (%s)", strerror(err));

    return err;
}

/*******************************************************************************
 ** HAL test functions & callbacks
 *******************************************************************************/

void setup_test_env(void)
{
    int i = 0;

    while (console_cmd_list[i].name != NULL)
    {
        console_cmd_maxlen = MAX(console_cmd_maxlen, (int)strlen(console_cmd_list[i].name));
        i++;
    }
}

void check_return_status(bt_status_t status)
{
    if (status != BT_STATUS_SUCCESS)
    {
        ZIB_DBG("HAL REQUEST FAILED status : %d (%s)", status, dump_bt_status(status));
    }
    else
    {
        ZIB_DBG("HAL REQUEST SUCCESS");
    }
}
static byte* intToByte(int i)
{
    byte abyte[4];
    abyte[0] = (byte)(0xff & i);
    abyte[1] = (byte)((0xff00 &i) >> 8);
    abyte[2] = (byte) ((0xff0000 & i) >>16);
    abyte[3] = (byte) ((0xff000000 & i) >>24);
    ZIB_DBG("abyte:0x%x,0x%x,0x%x,0x%x",abyte[0],abyte[1],abyte[2],abyte[3]);
    return &abyte;
}
static void set_adapter_discoveryMode()
{
    int scan_mode = 0x02;
    byte *abyte;
    bt_property_t prop;
    prop.type = (bt_property_type_t)BT_PROPERTY_ADAPTER_SCAN_MODE;
    prop.len = 4;
    abyte = intToByte(scan_mode);
    ZIB_DBG("set_adapter_discoveryMode:0x%x,0x%x,0x%x,0x%x",abyte[0],abyte[1],abyte[2],abyte[3]);
    prop.val = abyte;
    ZIB_DBG("prop.type:%d,prop.len:%d,prop.val:0x%x",prop.type,prop.len,prop.val);
    int ret = sBtInterface->set_adapter_property(&prop);
}

static void set_adapter_discoveryTimeout()
{
    int duration = 0;
    byte *abyte = NULL;
    bt_property_t prop;
    prop.type = BT_PROPERTY_ADAPTER_DISCOVERY_TIMEOUT;
    prop.len = 4;
    abyte = intToByte(duration);
    prop.val = abyte;
    ZIB_DBG("prop.type:%d,prop.len:%d,prop.val:0x%x",prop.type,prop.len,*((byte*)prop.val));
    int ret = sBtInterface->set_adapter_property(&prop);

}


static void adapter_state_changed(bt_state_t state)
{
    ZIB_DBG("ADAPTER STATE UPDATED : %s", (state == BT_STATE_OFF)?"OFF":"ON");
    if (state == BT_STATE_ON) {
        bt_enabled = 1;
        set_adapter_discoveryTimeout();
        set_adapter_discoveryMode();
    } else {
        bt_enabled = 0;
    }
}

static void ssp_request_cb(bt_bdaddr_t *remote_bd_addr,bt_bdname_t *bd_name,uint32_t cod,bt_ssp_variant_t pairing_variant, uint32_t pass_key)
{
   ZIB_DBG("pair_state_changed,will auto accept  ");
   sBtInterface->ssp_reply(remote_bd_addr, BT_SSP_VARIANT_PASSKEY_CONFIRMATION,1,pass_key);

}

int get_discovery_state_changed_status()
{
    return discovery_state_changed_status;
}

int get_device_find_status()
{
    return device_find_status;
}

/**
 * device查找回调函数
 * ***/
static void device_found_cb (int num_properties, bt_property_t *properties)
{
    device_find_status=0;
    ZIB_DBG("device found ");
    int i = 0;
    int j = 0;
    char datatmp[256];
    int16_t  name_len = 0;
    bt_bdaddr_t remote_bd_addr;
    bt_bdname_t remote_bd_name;
    bdstr_t remote_bd_str;
    ZIB_DBG("device found 1");
    memset(&remote_bd_addr, 0, sizeof(bt_bdaddr_t));
    memset(&remote_bd_str,  0, sizeof(bdstr_t));
    memset(&remote_bd_name, 0, sizeof(bt_bdname_t));
    for(i = 0; i < num_properties; i++)
    {
        switch(properties[i].type)
        {
            case BT_PROPERTY_BDNAME:
            {
                name_len = properties[i].len;
                memcpy(remote_bd_name.name,properties[i].val, name_len);
                remote_bd_name.name[name_len] = '\0';
            }
            break;
            case BT_PROPERTY_BDADDR:
            {
                bt_bdaddr_t *bd_addr = (bt_bdaddr_t*)properties[i].val;
                memcpy(&remote_bd_addr,(bt_bdaddr_t*)bd_addr, sizeof(bt_bdaddr_t));
                bd2str(&remote_bd_addr, &remote_bd_str);
            }
            break;
            default:
                break;
        }
    }
    ZIB_DBG("device found 2");
    ZIB_DBG("device found bt mac : %s name: %s",remote_bd_str,remote_bd_name.name);

    if(remote_bd_str!=NULL) {
        if (strlen(remote_bd_str)) {
            ZIB_DBG("device found 3");
            device_find_status=1;
        }
    }


}
static void discovery_state_changed_cb (bt_discovery_state_t state)
{
    ZIB_DBG("discovery_state_changed,state:%d",state);
    discovery_state_changed_status=state;
}

static void bond_state_changed_cb(bt_status_t status,
                                 bt_bdaddr_t *remote_bd_addr,
                                 bt_bond_state_t state)
{
    ZIB_DBG("bond_state_changed_callback,state:%d",state);
}

#ifndef HAS_BLUETOOTH_SPRD
static void dut_mode_recv(uint16_t opcode, uint8_t *buf, uint8_t len)
{
    ZIB_DBG("DUT MODE RECV : NOT IMPLEMENTED");
}
#else
static void dut_mode_recv(uint16_t opcode, uint8_t *buf, uint8_t len)
{
    ZIB_DBG("DUT MODE RECV");
    char status = -1;
    char rssi = -1;

    if(opcode == HCI_DUT_GET_RXDATA && len ==2){
        status = *buf;
        rssi = *(buf+1);
        ZIB_DBG("DUT MODE RECV RX DATA : STATUS = %d, RSSI = %d",status,rssi);
    }
}
#endif

static void le_test_mode(bt_status_t status, uint16_t packet_count)
{
    ZIB_DBG("LE TEST MODE END status:%s number_of_packets:%d", dump_bt_status(status), packet_count);
}

#ifdef HAS_BLUETOOTH_SPRD
static void nonsig_test_rx_recv(bt_status_t status, uint8_t rssi, uint32_t pkt_cnt,
                                                uint32_t pkt_err_cnt,uint32_t bit_cnt,uint32_t bit_err_cnt)
{
    ZIB_DBG("nonsig_test_rx_recv status:%s", dump_bt_status(status));
    ZIB_DBG("rssi:0x%x, per:0x%x, ber:0x%x, rbr:0x%x, cnt:0x%x",rssi, pkt_cnt, pkt_err_cnt, bit_cnt, bit_err_cnt);
}
#endif

static bt_callbacks_t bt_callbacks = {
    sizeof(bt_callbacks_t),
    adapter_state_changed,
    NULL, /* adapter_properties_cb */
    NULL, /* remote_device_properties_cb */
    device_found_cb, /* device_found_cb */
    discovery_state_changed_cb, /* discovery_state_changed_cb */
    NULL, /* pin_request_cb  */
    ssp_request_cb, /* ssp_request_cb  */
    bond_state_changed_cb, /* bond_state_changed_cb */
    NULL, /* acl_state_changed_cb */
    NULL, /* thread_evt_cb */
    dut_mode_recv, /*dut_mode_recv_cb */
//    NULL, /*authorize_request_cb */
#if BLE_INCLUDED == TRUE
    le_test_mode, /* le_test_mode_cb */
#else
    NULL,
#endif
#ifdef HAS_BLUETOOTH_SPRD
    nonsig_test_rx_recv /* nonsig_test_rx_recv_cb */
#endif
};

void bdt_init(void)
{
    ZIB_DBG("INIT BT ");
    status = sBtInterface->init(&bt_callbacks);
    check_return_status(status);
}

void bdt_enable(void)
{
    ZIB_DBG("ENABLE BT");
    if (bt_enabled) {
        ZIB_DBG("Bluetooth is already enabled");
        return;
    }
    status = sBtInterface->enable();

    check_return_status(status);
}

void bdt_disable(void)
{
    ZIB_DBG("DISABLE BT");
    if (!bt_enabled) {
        ZIB_DBG("Bluetooth is already disabled");
        return;
    }
    status = sBtInterface->disable();

    check_return_status(status);
}

void bdt_dut_mode_configure(char *p)
{
    int32_t mode = -1;

    ZIB_DBG("BT DUT MODE CONFIGURE");
    if (!bt_enabled) {
        ZIB_DBG("Bluetooth must be enabled for test_mode to work.");
        return;
    }
    mode = get_signed_int(&p, mode);
    if ((mode != 0) && (mode != 1)) {
        ZIB_DBG("Please specify mode: 1 to enter, 0 to exit");
        return;
    }
    status = sBtInterface->dut_mode_configure(mode);

    check_return_status(status);
}

void bdt_le_test_mode(char *p)
{
    int cmd;
    unsigned char buf[3];
    int arg1, arg2, arg3;

    ZIB_DBG("BT LE TEST MODE");
    if (!bt_enabled) {
        ZIB_DBG("Bluetooth must be enabled for le_test to work.");
        return;
    }

    memset(buf, 0, sizeof(buf));
    cmd = get_int(&p, 0);
    switch (cmd)
    {
        case 0x1: /* RX TEST */
           arg1 = get_int(&p, -1);
           if (arg1 < 0) ZIB_DBG("%s Invalid arguments", __FUNCTION__);
           buf[0] = arg1;
           status = sBtInterface->le_test_mode(HCI_LE_RECEIVER_TEST_OPCODE, buf, 1);
           break;
        case 0x2: /* TX TEST */
            arg1 = get_int(&p, -1);
            arg2 = get_int(&p, -1);
            arg3 = get_int(&p, -1);
            if ((arg1 < 0) || (arg2 < 0) || (arg3 < 0))
                ZIB_DBG("%s Invalid arguments", __FUNCTION__);
            buf[0] = arg1;
            buf[1] = arg2;
            buf[2] = arg3;
            status = sBtInterface->le_test_mode(HCI_LE_TRANSMITTER_TEST_OPCODE, buf, 3);
           break;
        case 0x3: /* END TEST */
            status = sBtInterface->le_test_mode(HCI_LE_END_TEST_OPCODE, buf, 0);
           break;
        default:
            ZIB_DBG("Unsupported command");
            return;
            break;
    }
    if (status != BT_STATUS_SUCCESS)
    {
        ZIB_DBG("%s Test 0x%x Failed with status:0x%x", __FUNCTION__, cmd, status);
    }
    return;
}

void bdt_cleanup(void)
{
    ZIB_DBG("CLEANUP");
    sBtInterface->cleanup();
}

/*******************************************************************************
 ** Console commands
 *******************************************************************************/

void do_help(char *p)
{
    int i = 0;
    int max = 0;
    char line[128];
    int pos = 0;

    while (console_cmd_list[i].name != NULL)
    {
        pos = sprintf(line, "%s", (char*)console_cmd_list[i].name);
        ZIB_DBG("%s %s\n", (char*)line, (char*)console_cmd_list[i].help);
        i++;
    }
}

void do_quit(char *p)
{
    bdt_shutdown();
}

/*******************************************************************
 *
 *  BT TEST  CONSOLE COMMANDS
 *
 *  Parses argument lists and passes to API test function
 *
*/

void do_init(char *p)
{
    bdt_init();
}

void do_enable(char *p)
{
    bdt_enable();
}

void do_pair(char *p)
{
    char *addr = NULL;
    bt_bdaddr_t bdaddr;
    if(p != NULL)
    {

       ZIB_DBG("do_pair addr: %s\n", p);
       addr = p;
    }else {
       ZIB_DBG("please input correct MAC addr");
       return;
    }
    str2bd(addr,&bdaddr);
    if(sBtInterface != NULL)
    {
       sBtInterface->create_bond(&bdaddr);
    }
}
void do_start_discovery()
{
    ZIB_DBG("do_start_discovery");
    if(sBtInterface != NULL)
    {
        ZIB_DBG("start_discovery");
       sBtInterface->start_discovery();
    }
}

void do_stop_discovery()
{
    if(sBtInterface != NULL)
    {
       sBtInterface->cancel_discovery();
    }
}


void do_disable(char *p)
{
    bdt_disable();
}
void do_dut_mode_configure(char *p)
{
    bdt_dut_mode_configure(p);
}


void do_le_test_mode(char *p)
{
    bdt_le_test_mode(p);
}

void do_cleanup(char *p)
{
    bdt_cleanup();
}

/******************************************************************
 *
 *  gatt function
 *
*/

// gatt client callback 

void uuid2str(const bt_uuid_t bdaddr)
{
    ZIB_DBG("%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X:%X",
        bdaddr.uu[0],bdaddr.uu[1],bdaddr.uu[2],bdaddr.uu[3],bdaddr.uu[4],bdaddr.uu[5],bdaddr.uu[6],bdaddr.uu[7],
        bdaddr.uu[8],bdaddr.uu[9],bdaddr.uu[10],bdaddr.uu[11],bdaddr.uu[12],bdaddr.uu[13],bdaddr.uu[14],bdaddr.uu[15]);
}

static int CLIENT_IF = -1;
static int SERVER_IF = -1;
uint8_t uuid_service[] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x0, 0x0, 0x80, 0x0, 0x10, 0x0, 0x0, 0xf0, 0xff, 0x0, 0x0};
uint8_t uuid_char[] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x0, 0x0, 0x80, 0x0, 0x10, 0x0, 0x0, 0xf1, 0xff, 0x0, 0x0};
static int CHAR_HANDLE= -1;
static int GATTS_CONN_ID = -1;

void set_uuid(uint8_t* uuid, uint64_t uuid_msb, uint64_t uuid_lsb)
{
    int i;
    for (i = 0; i != 8; ++i)
    {
        uuid[i]     = (uuid_lsb >> (8 * i)) & 0xFF;
        uuid[i + 8] = (uuid_msb >> (8 * i)) & 0xFF;
    }
}

void btgattc_register_app_cb(int status, int clientIf, bt_uuid_t *app_uuid)
{
    ZIB_DBG("%s::%d, status = %d, clientIf = %d",
        __FUNCTION__, __LINE__, status, clientIf);
    if (status == 0)
    {
        CLIENT_IF = clientIf;
    }
}

void btgattc_scan_result_cb(bt_bdaddr_t* bda, int rssi, uint8_t* adv_data)
{
    bdstr_t bdstr;
    unsigned char *addr = (unsigned char *) bda->address;
    snprintf((unsigned char*)bdstr,sizeof(bdstr_t), "%02X:%02X:%02X:%02X:%02X:%02X",
        addr[0],addr[1],addr[2],
        addr[3],addr[4],addr[5]);

    ZIB_DBG("btgattc_scan_result_cb test: bt address<%s>", bdstr);
}

void btgattc_open_cb(int conn_id, int status, int clientIf, bt_bdaddr_t* bda)
{
    ZIB_DBG("btgattc_open_cb conn_id %d, status = %d, clientIf = %d\n",conn_id, status,clientIf);
}

void btgattc_close_cb(int conn_id, int status, int clientIf, bt_bdaddr_t* bda)
{
    ZIB_DBG("btgattc_close_cb conn_id %d, status = %d, clientIf = %d",conn_id, status,clientIf);
}

void continue_search(int conn_id, int status)
{
}

void btgattc_search_complete_cb(int conn_id, int status)
{
}

void btgattc_search_result_cb(int conn_id, btgatt_srvc_id_t *srvc_id)
{
}

void btgattc_get_characteristic_cb(int conn_id, int status,
        btgatt_srvc_id_t *srvc_id, btgatt_gatt_id_t *char_id, int char_prop)
{
}

void btgattc_get_descriptor_cb(int conn_id, int status,
        btgatt_srvc_id_t *srvc_id, btgatt_gatt_id_t *char_id, btgatt_gatt_id_t *descr_id)
{
}

void btgattc_get_included_service_cb(int conn_id, int status,
        btgatt_srvc_id_t *srvc_id, btgatt_srvc_id_t *incl_srvc_id)
{
}

void btgattc_register_for_notification_cb(int conn_id, int registered, int status,
        btgatt_srvc_id_t *srvc_id, btgatt_gatt_id_t *char_id)
{
    ZIB_DBG("%s::%d, registered = %d, conn_id = %d, status = %ld is_primary = %d, id.inst_id = %d",
        __FUNCTION__,__LINE__, registered, conn_id, status, srvc_id->is_primary, srvc_id->id.inst_id);
    uuid2str(srvc_id->id.uuid);
    uuid2str(char_id->uuid);
}

void btgattc_notify_cb(int conn_id, btgatt_notify_params_t *p_data)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
    uuid2str(p_data->char_id.uuid);
    uuid2str(p_data->srvc_id.id.uuid);
}

void btgattc_read_characteristic_cb(int conn_id, int status, btgatt_read_params_t *p_data)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgattc_write_characteristic_cb(int conn_id, int status, btgatt_write_params_t *p_data)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgattc_execute_write_cb(int conn_id, int status)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgattc_read_descriptor_cb(int conn_id, int status, btgatt_read_params_t *p_data)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgattc_write_descriptor_cb(int conn_id, int status, btgatt_write_params_t *p_data)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgattc_remote_rssi_cb(int client_if,bt_bdaddr_t* bda, int rssi, int status)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgattc_advertise_cb(int status, int client_if)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

static const btgatt_client_callbacks_t sGattClientCallbacks = {
    btgattc_register_app_cb,
    btgattc_scan_result_cb,
    btgattc_open_cb,
    btgattc_close_cb,
    btgattc_search_complete_cb,
    btgattc_search_result_cb,
    btgattc_get_characteristic_cb,
    btgattc_get_descriptor_cb,
    btgattc_get_included_service_cb,
    btgattc_register_for_notification_cb,
    btgattc_notify_cb,
    btgattc_read_characteristic_cb,
    btgattc_write_characteristic_cb,
    btgattc_read_descriptor_cb,
    btgattc_write_descriptor_cb,
    btgattc_execute_write_cb,
    btgattc_remote_rssi_cb,
    btgattc_advertise_cb
};

// gatt server callback 

void btgatts_register_app_cb(int status, int server_if, bt_uuid_t *uuid)
{
    ZIB_DBG("%s:%d,status = %d, server_if = %d\n", __FUNCTION__, __LINE__, status, server_if);
    if (status == 0) {
        SERVER_IF = server_if;
    }
}

void btgatts_connection_cb(int conn_id, int server_if, int connected, bt_bdaddr_t *bda)
{
    ZIB_DBG("%s:%d, connected = %d\n", __FUNCTION__, __LINE__, connected);
    if (connected == 1)
    {
        GATTS_CONN_ID = conn_id;
    }
    else
    {
        GATTS_CONN_ID = -1;
    }
}

void btgatts_service_added_cb(int status, int server_if,
        btgatt_srvc_id_t *srvc_id, int srvc_handle)
{
    ZIB_DBG("%s:%d status = %d server_if = %d, srvc_handle = %d\n",
        __FUNCTION__, __LINE__, status,server_if, srvc_handle);
    bt_uuid_t uuid;
    int i = 0;
    for (i = 0; i < 16; i++)
    {
        uuid.uu[i] = uuid_char[i];
    }

    sGattIf->server->add_characteristic(SERVER_IF, srvc_handle,
        &uuid, 26, 36881);
}

void btgatts_included_service_added_cb(int status, int server_if,
        int srvc_handle, int incl_srvc_handle)
{
    ZIB_DBG("%s:%d\n", __FUNCTION__, __LINE__);
}

void btgatts_characteristic_added_cb(int status, int server_if, bt_uuid_t *char_id,
        int srvc_handle, int char_handle)
{
    ZIB_DBG("%s:%d status = %d server_if = %d, srvc_handle = %d, char_handle = %d\n",
        __FUNCTION__, __LINE__, status,server_if, srvc_handle, char_handle);
    if (status == 0)
    {
        CHAR_HANDLE = char_handle;
        sGattIf->server->start_service(server_if, srvc_handle, 2);
    }
}

void btgatts_descriptor_added_cb(int status, int server_if,
        bt_uuid_t *descr_id, int srvc_handle, int descr_handle)
{
    ZIB_DBG("%s:%d status = %d server_if = %d, srvc_handle = %d",
       __FUNCTION__, __LINE__, status, server_if, srvc_handle);
}

void btgatts_service_started_cb(int status, int server_if, int srvc_handle)
{
    ZIB_DBG("%s:%d", __FUNCTION__, __LINE__);
}

void btgatts_service_stopped_cb(int status, int server_if, int srvc_handle)
{
    ZIB_DBG("%s:%d\n", __FUNCTION__, __LINE__);
}

void btgatts_service_deleted_cb(int status, int server_if, int srvc_handle)
{
    ZIB_DBG("%s:%d\n", __FUNCTION__, __LINE__);
}

void btgatts_request_read_cb(int conn_id, int trans_id, bt_bdaddr_t *bda,
        int attr_handle, int offset, bool is_long)
{
    ZIB_DBG("%s:%d\n", __FUNCTION__, __LINE__);
}

void btgatts_request_write_cb(int conn_id, int trans_id,
        bt_bdaddr_t *bda, int attr_handle, int offset, int length,
        bool need_rsp, bool is_prep, uint8_t* value)
{
    ZIB_DBG("%s:%d, offset = %d, attr_handle = %d, length = %d, value = %s\n",
        __FUNCTION__, __LINE__, offset, attr_handle, length, value);
    btgatt_response_t response;

    response.attr_value.handle = attr_handle;
    response.attr_value.auth_req = 0;
    response.attr_value.offset = offset;
    response.attr_value.len = length;

    if (value != NULL)
    {   
        int i = 0;
        for (i = 0; i != length; ++i)
            response.attr_value.value[i] = (uint8_t) value[i];
    }

    sGattIf->server->send_response(conn_id, trans_id, 0, &response);
}

void btgatts_request_exec_write_cb(int conn_id, int trans_id,
        bt_bdaddr_t *bda, int exec_write)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

void btgatts_response_confirmation_cb(int status, int handle)
{
    ZIB_DBG("%s:%d,", __FUNCTION__, __LINE__);
}

static const btgatt_server_callbacks_t sGattServerCallbacks = {
    btgatts_register_app_cb,
    btgatts_connection_cb,
    btgatts_service_added_cb,
    btgatts_included_service_added_cb,
    btgatts_characteristic_added_cb,
    btgatts_descriptor_added_cb,
    btgatts_service_started_cb,
    btgatts_service_stopped_cb,
    btgatts_service_deleted_cb,
    btgatts_request_read_cb,
    btgatts_request_write_cb,
    btgatts_request_exec_write_cb,
    btgatts_response_confirmation_cb
};

static const btgatt_callbacks_t sGattCallbacks = {
    sizeof(btgatt_callbacks_t),
    &sGattClientCallbacks,
    &sGattServerCallbacks
};

void gatt_init(void)
{
    if ((sGattIf = (btgatt_interface_t *)
          sBtInterface->get_profile_interface(BT_PROFILE_GATT_ID)) == NULL) {
        ZIB_DBG("Failed to get Bluetooth GATT Interface");
        return;
    }

    if ((status = sGattIf->init(&sGattCallbacks)) != BT_STATUS_SUCCESS) {
        ZIB_DBG("Failed to initialize Bluetooth GATT, status: %d", status);
        sGattIf = NULL;
    }
}

void do_btgattc_register_app()
{
    ZIB_DBG("%s::%d",__FUNCTION__,__LINE__);
    
    bt_uuid_t uuid;

    if (!sGattIf) return;
    set_uuid(uuid.uu, 0, 0);
    sGattIf->client->register_client(&uuid);
}

void do_btgattc_unregister_app()
{
    ZIB_DBG("%s::%d",__FUNCTION__,__LINE__);
    if (!sGattIf) return;
    sGattIf->client->unregister_client(CLIENT_IF);
}



void btgatts_start_advertising()
{
    ZIB_DBG("%s::%d",__FUNCTION__,__LINE__);
    if (!sGattIf) return;
    sGattIf->client->set_adv_data(CLIENT_IF, false, true, false,
        200, 200, 0, 0, NULL,
        0, NULL, 0,NULL);
    sGattIf->client->listen(CLIENT_IF, true);
}

void btgatts_register_server()
{
    bt_uuid_t uuid;
    if (!sGattIf) return;
    set_uuid(uuid.uu, 0, 0);
    sGattIf->server->register_server(&uuid);
}

void do_btgatts_start_advertise()
{
    btgatts_start_advertising();
    btgatts_register_server();
}

void do_btgatts_init_server()
{
    btgatt_srvc_id_t srvc_id;
    srvc_id.id.inst_id = 0;
    srvc_id.is_primary = 1;
    int i = 0;
        
    for (i = 0; i < 16; i++)
    {
        srvc_id.id.uuid.uu[i] = uuid_service[i];
    }
    sGattIf->server->add_service(SERVER_IF, &srvc_id, 4);
}

void do_btgatts_send_notification() {
    ZIB_DBG("%s::%d",__FUNCTION__,__LINE__);
    char hello[10] = "hello";
    sGattIf->server->send_indication(SERVER_IF, CHAR_HANDLE, GATTS_CONN_ID, 5,
                                     0, hello);
}
/*******************************************************************
 *
 *  CONSOLE COMMAND TABLE
 *
*/

const t_cmd console_cmd_list[] =
{
    /*
     * INTERNAL
     */

    { "h", do_help, "lists all available console commands", 0 },
    { "q", do_quit, "", 0},

    /*
     * API CONSOLE COMMANDS
     */

     /* Init and Cleanup shall be called automatically */
    { "1", do_enable, " enables bluetooth", 0 },
    { "2", do_disable, " disables bluetooth", 0 },
    { "3", do_start_discovery,"do_start_discovery",0},
    { "4", do_stop_discovery, "do_stop_discovery", 0},
    { "5", do_pair," Pair Device,after 'p' is correct Mac ",0},
    { "6", do_btgattc_register_app, "do_btgattc_register_app", 0},
    { "7", do_btgatts_start_advertise, "do_btgatts_start_advertise", 0},
    { "8", do_btgatts_init_server, "do_btgatts_init_server", 0},
    { "9", do_btgatts_send_notification, "do_btgatts_send_notification", 0},
    /* last entry */
    {NULL, NULL, "", 0},
};

/*
 * Main console command handler
*/

void process_cmd(char *p, unsigned char is_job)
{
    char cmd[64];
    int i = 0;
    char *p_saved = p;

    get_str(&p, cmd);

    /* table commands */
    while (console_cmd_list[i].name != NULL)
    {
        if (is_cmd(console_cmd_list[i].name))
        {
            if (!is_job && console_cmd_list[i].is_job)
                create_cmdjob(p_saved);
            else
            {
                console_cmd_list[i].handler(p);
            }
            return;
        }
        i++;
    }
    ZIB_DBG("%s : unknown command\n", p_saved);
    do_help(NULL);
}


/**
 *
 * 蓝牙测试
 * **/
int eng_bt_scan_start(void)
{

    ZIB_DBG(" bluetooth scan start");
    int opt;
    char cmd[128];
    int args_processed = 0;
    int pid = -1;

    config_permissions();

    ZIB_DBG("****Bluetooth Test app starting****");

    if (HAL_load() < 0 ) {
        ZIB_DBG("HAL failed to initialize, exit\n");
        unlink(PID_FILE);
        exit(0);
    }
    setup_test_env();
   /* Automatically perform the init */
    bdt_init();
    gatt_init();
    while(!main_done)
    {
        char line[128];

        /* command prompt */
        printf( ">" );
        fflush(stdout);

        fgets (line, 128, stdin);

        if (line[0]!= '\0')
        {
            /* remove linefeed */
            line[strlen(line)-1] = 0;

            process_cmd(line, 0);
            memset(line, '\0', 128);
        }
    }

    /* FIXME: Commenting this out as for some reason, the application does not exit otherwise*/
    //bdt_cleanup();

    HAL_unload();

    ZIB_DBG("****Bluetooth Test app terminating****");
    return  0;
}