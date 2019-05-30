
#include <netutils/dhcp.h>
#include "includes/commm_util.h"
#include <pthread.h>
#include "type.h"
#include "includes/main.h"

// enable or disable local debug


// 定义扫描固定SSID的信号强度
#define FACTORY_ZIB_WIFI_SSID   "zib"
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


static int             sConnectStatus= 0;
static wifi_ap_t       sAPs[WIFI_MAX_AP];
static int             sStatus       = 0;
static int             sAPNum        = 0;
static sem_t g_sem;
static pthread_mutex_t sMutxEvent    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sMutxDhcp     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sCondEvent    = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  sCondDhcp     = PTHREAD_COND_INITIALIZER;
static volatile int    sEventLooping = 0;
static char mInterfaceCmd[64];
static int             wifi_test_result        = ZIB_FAIL;
static int   wifi_test_over_flag =0;

int wifiClose(void);

int wifiSyncScanAP(struct wifi_ap_t * aps, int maxnum);

void wifiParseLine(const char * begin, const char * end, struct wifi_ap_t * ap)
{
    const char * pcur = begin,* ps;
    char siglev[32]={0},freq[32]={0};
    int  sli = 0,fn = 0;
    unsigned short i = 0;
    int bi;
    // bssid
    while( *pcur != '\t' && pcur < end ) {
        if( i < sizeof(ap->smac) - 1 ) {
            ap->smac[i++] = *pcur;
        }
        pcur++;
    }
    ap->smac[i] = 0;

    pcur++;
    if( '\t' == *pcur ) {
        pcur++;
    }
    // frequency
    while( *pcur != '\t' && pcur < end ) {
	 freq[fn++] = *pcur;
        pcur++;
    }
    siglev[fn] = 0;
    ap->frequency= strtol(freq, NULL, 10);

    pcur++;
    if( '\t' == *pcur ) {
        pcur++;
    }
    // signal level
    while( *pcur != '\t' && pcur < end ) {
		if( sli < 31 ) {
			siglev[sli++] = *pcur;
		}
        pcur++;
    }
    siglev[sli] = 0;
    ap->sig_level = strtol(siglev, NULL, 10);

    pcur++;
    if( '\t' == *pcur ) {
        pcur++;
    }
    // flags
    while( *pcur != '\t' && pcur < end ) {
        pcur++;
    }
    pcur++;
    if( '\t' == *pcur ) {
        pcur++;
    }
    // ssid
    i = 0;
    while( *pcur != '\t' && pcur < end ) {
        if( i < sizeof(ap->name) - 1 ) {
            ap->name[i++] = *pcur;
        }
        pcur++;
    }
    ap->name[i] = 0;
	
    DBGMSG("mac = %s, name = %s, sig = %d, freq = %d", ap->smac, ap->name, ap->sig_level, ap->frequency);
}

static int wcnd_down_network_interface(const char *ifname)
{
    ifc_init();
    if (ifc_down(ifname)){
        ERRMSG("Error downing interface: %s", strerror(errno));
    }
    ifc_close();

    return 0;
}


static char * wifiStrSkipSpace(const char * pfirst, const char * plast)
{
    if( pfirst == NULL || plast == NULL || *pfirst != ' ' )
        return (char*)-1;

    while( *pfirst++ == ' ' && pfirst < plast ) {
        // nothing
    }
    if( pfirst >= plast ) {
        DBGMSG("first = %p, last = %p", pfirst, plast);
        return NULL;
    }

    return (char *)pfirst;
}

//------------------------------------------------------------------------------
static char * wifiStrFindChar(const char * pfirst, const char * plast, char val)
{
    if( pfirst == NULL ||  plast == NULL )
		return (char*)-1;

    while(*pfirst++ != val && pfirst < plast) {
        // nothing
    }
    if( pfirst >= plast ) {
        DBGMSG("first = %p, last = %p\n", pfirst, plast);
        return NULL;
    }
    return (char *)pfirst;
}


int wifiGetStatus( void )
{
    DBGMSG("status = %d", sStatus);
    return sStatus;
}

//------------------------------------------------------------------------------
int wifiCommand(const char * cmder, char * replyBuf, int replySize)
{
    size_t replyLen;
    int fail = -1;
    int i = 0;

    if( cmder == NULL ||replyBuf == NULL || replySize <= 0 )
	return -1;

    snprintf(mInterfaceCmd, sizeof(mInterfaceCmd), "IFNAME=wlan0 %s", cmder);
    DBGMSG("'mInterfaceCmd=%s!", mInterfaceCmd);
    for(i = 0; i < WIFI_CMD_RETRY_NUM; ++i ) {
        replyLen = (size_t)(replySize - 1);
        if( wifi_command(mInterfaceCmd, replyBuf, &replyLen) != 0 ) {
            ERRMSG("'%s'(%d): error", mInterfaceCmd, i);
            sleep(WIFI_CMD_RETRY_TIME);
            continue;
        } else {
            fail = 0;
            break;
        }
    }

    if( fail ) {
        DBGMSG("'%s' retry %d, always fail!", cmder, WIFI_CMD_RETRY_NUM);
        return -1;
    }

    replyBuf[replyLen] = 0;

    return replyLen;
}


/*
 *
 */
void * wifiScanThread( void *param )
{
    int retryNum = 3;
	int i=0;
	int found_ssid=0;
		
	pthread_detach(pthread_self()); 	//free by itself
    while( retryNum-- ) {
		sAPNum = wifiSyncScanAP(sAPs, WIFI_MAX_AP);
		if( 0 == sAPNum ) {
			usleep(1400 * 1000);
			DBGMSG("---- wifi retry scan ----");
		} else {
			for(i=0;i<sAPNum;i++){
				// 间隔时间太短，或者热点太多，导致获取到的热点没有指定SSID
				if(strcmp(sAPs[i].name,FACTORY_ZIB_WIFI_SSID)==0){
					found_ssid =1;
					break;
				}	
			}
			
			if(found_ssid) {
				break;
			}else{
				usleep(1400 * 1000);	
			}
		}
    }

    sStatus &= ~WIFI_STATUS_SCANNING;
    if( sAPNum > 0 ) {
		sStatus |= WIFI_STATUS_SCAN_END;
    } else {

    }

    DBGMSG("---- wifiScanThread exit: num = %d,sStatus=%d ----", sAPNum,sStatus);
    sem_post(&g_sem);
	
		
    return NULL;
}


/*
 * 获取wifi扫描结果
 */
int wifiGetScanResult(struct wifi_ap_t * aps, int maxnum)
{
    char reply[4096];
    int  len;
    int num = 0;
    char * pcur,* pend,* line;

    if(aps == NULL || maxnum <= 0 )
		return -1;

    len = wifiCommand("SCAN_RESULTS", reply, sizeof(reply));
    if( (len <= 0) || (NULL == strstr(reply, "bssid")) ) {
        ERRMSG("scan results fail: %s", reply);
        return -2;
    }
    pcur = reply;
    pend = reply + len;
    // skip first line(end with '\n'): bssid / frequency / signal level / flags / ssid
    while( *pcur++ != '\n' && pcur < pend ) {
        // nothing
    }
    for( ; (num < maxnum) && (pcur < pend); ++num ) {
        line = pcur;
        while( *pcur != '\n' && pcur++ < pend ) {
            // nothing
        }
        wifiParseLine(line, pcur, aps+num);
        // skip \n
        pcur++;
    }
    DBGMSG("---- wifiGetScanResult exit. num = %d",num);
	
    return num;
}


//扫描wifi热点
int wifiSyncScanAP(struct wifi_ap_t * aps, int maxnum)
{
    char reply[64];
    int  len;
    int ret = 0;
    struct timespec to;

    if( aps == NULL ||  maxnum <=  0 )
        return -1;

#if 0
    reply[0] = 0;
    len = sizeof(reply);
    //-- don't care result anli 2013-03-05i
    /*modify by sam.sun,20140702, reason: for solve coverity issue, just check the return value*/
    if( (wifiCommand("DRIVER SCAN-ACTIVE", reply, len) <= 0) || (NULL == strstr(reply, "OK")) ) {
        LOGD("scan passive fail: %s", reply);
         //return -1;
    }
#endif

    reply[0] = 0;
    len = sizeof(reply);
    if( (wifiCommand("SCAN", reply, len) <= 0) || (NULL == strstr(reply, "OK")) ) {
        ERRMSG("scan fail: %s", reply);
        return -2;
    }
    to.tv_nsec = 0;
    to.tv_sec  = time(NULL) + 8;

    pthread_mutex_lock(&sMutxEvent);
    if( 0 == pthread_cond_timedwait(&sCondEvent, &sMutxEvent, &to) ) {
        DBGMSG("wait done.");
    } else {
        ERRMSG("wait timeout or error!");
    }
    pthread_mutex_unlock(&sMutxEvent);

    ret = wifiGetScanResult(aps, maxnum);
	
    return ret;
}




int eng_wifi_get_scan_result( void )
{
		
    if( !(sStatus & WIFI_STATUS_OPENED) ) {
        ERRMSG("wifi not opened");
        return -1;
    }
    if( sStatus & WIFI_STATUS_SCANNING ) {
        sem_post(&g_sem);
        DBGMSG("already scannning...");
        return 0;
    }

    sStatus |= WIFI_STATUS_SCANNING;
    sStatus &= ~WIFI_STATUS_SCAN_END;

    pthread_t      ptid;
    pthread_attr_t attr;
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&ptid, &attr, wifiScanThread, NULL);

    return 0;
}

int wifiClose( void )
{
	DBGMSG("wifiClose");

    sEventLooping = 0;

    wifi_stop_supplicant(0);
    wcnd_down_network_interface("wlan0");
    wifi_unload_driver();
    wifi_close_supplicant_connection();
    DBGMSG("---- wifi close done ----");
    sStatus = 0;

    memset(sAPs, 0, WIFI_MAX_AP * sizeof(wifi_ap_t));
    sAPNum = 0;
	
    return 0;
}



static void wifi_result(void)
{
	DBGMSG("wifi_result");

	int i = 0;

	for(i = 0; i < sAPNum; i++){
		if(strcmp(sAPs[i].name,FACTORY_ZIB_WIFI_SSID)==0){
			DBGMSG( "find ssid =%s ,sig_level= %d ",sAPs[i].name,sAPs[i].sig_level);
			DBGMSG( "\n name:%s mac:%s ",sAPs[i].name, sAPs[i].smac);
			DBGMSG( "\n sig_level:%d HZ:%d",sAPs[i].sig_level,sAPs[i].frequency);
			if(sAPs[i].sig_level>FACTORY_ZIB_SIG_LEVEL) {
				wifi_test_result =ZIB_PASS;
			} else {
				wifi_test_result = ZIB_FAIL;
			}
		}	
	}

}

static void wifi_show_result()
{
	DBGMSG("wifi_show_result");

    int i = 0;
	
    for(i = 0; i < sAPNum; i++){
        DBGMSG( "name:%s mac:%s ",sAPs[i].name, sAPs[i].smac);
        DBGMSG( "sig_level:%d HZ:%d",sAPs[i].sig_level,sAPs[i].frequency);
    }
}


int wifi_Dhcp(){
    DBGMSG("---- wifi_Dhcp enter ----\n");
    int result;

    DBGMSG("---- wifi_Dhcp dhcp_stop begin----\n");
    result = dhcp_stop("wlan0");
    if (result != 0) {
        INFMSG("dhcp_stop failed : wlan0");
    }else{
        INFMSG("dhcp_stop success : wlan0");
    }
    DBGMSG("---- wifi_Dhcp dhcp_stop end----\n");

    char  ipaddr[PROPERTY_VALUE_MAX];
    uint32_t prefixLength;
    char gateway[PROPERTY_VALUE_MAX];
    char    dns1[PROPERTY_VALUE_MAX];
    char    dns2[PROPERTY_VALUE_MAX];
    char    dns3[PROPERTY_VALUE_MAX];
    char    dns4[PROPERTY_VALUE_MAX];
    char *dns[5] = {dns1, dns2, dns3, dns4, NULL};
    char  server[PROPERTY_VALUE_MAX];
    uint32_t lease;
    char vendorInfo[PROPERTY_VALUE_MAX];
    char domains[PROPERTY_VALUE_MAX];
    char mtu[PROPERTY_VALUE_MAX];
    DBGMSG("---- wifi_Dhcp dhcp_do_request begin----\n");
    result = dhcp_do_request("wlan0", ipaddr, gateway, &prefixLength,
                             dns, server, &lease, vendorInfo, domains, mtu);
    DBGMSG("---- wifi_Dhcp dhcp_do_request end----\n");
    if (result != 0) {
        INFMSG("dhcp_do_request failed : wlan0");
    }else{
        INFMSG("wifi_Connect_Network dhcp_do_request  ipaddr = %s\n", ipaddr);
        INFMSG("wifi_Connect_Network dhcp_do_request  gateway = %s\n", gateway);
        INFMSG("wifi_Connect_Network dhcp_do_request  dns1 = %s\n", dns[0]);
        INFMSG("wifi_Connect_Network dhcp_do_request  dns2 = %s  length = %d\n", dns[1], strlen(dns[1]));
        INFMSG("wifi_Connect_Network dhcp_do_request  server = %s\n", server);
        INFMSG("wifi_Connect_Network dhcp_do_request  vendorInfo = %s\n", vendorInfo);
        INFMSG("wifi_Connect_Network dhcp_do_request  mtu = %s\n", mtu);
    }
    DBGMSG("---- wifi_Dhcp exit ----\n");

    char cmd[256];
    system("ndc resolver setdefaultif wlan0");
    if(strlen(dns1)>1)
    {
        snprintf(cmd,256,"ndc resolver setifdns wlan0 \"\" %s",dns1);
    }
    if(strlen(dns2)>1)
    {
        snprintf(cmd,256,"%s %s",cmd,dns2);
    }
    if(strlen(dns3)>1)
    {
        snprintf(cmd,256,"%s %s",cmd,dns3);
    }
    if(strlen(dns4)>1)
    {
        snprintf(cmd,256,"%s %s",cmd,dns4);
    }
    system(cmd);
    memset(cmd,0,256);
    if(strlen(gateway)>1)
    {
        snprintf(cmd,128,"ip route add default via %s dev wlan0 table main",gateway);
    }
    system(cmd);

    return result;
}

void * wifiEventLoop( void *param )
{
    char evt[EVT_MAX_LEN + 1];
    int len = 0;

    pthread_detach(pthread_self()); 	//free by itself
    evt[EVT_MAX_LEN] = 0;
    while( sEventLooping ) {
        evt[0] = 0;
        len = wifi_wait_for_event(evt,EVT_MAX_LEN);

        if( (len > 0) &&(NULL != strstr(evt, "SCAN-RESULTS")) ) {
            pthread_mutex_lock(&sMutxEvent);
            pthread_cond_signal(&sCondEvent);
            pthread_mutex_unlock(&sMutxEvent);
        }


        if( (len > 0) &&(NULL != strstr(evt, "CTRL-EVENT-CONNECTED")) ) {
            DBGMSG(".... connect complete ....\n");
            if(sConnectStatus != CONNECTING){
                wifi_Dhcp();
            }
            sConnectStatus = CONNECTED;
            pthread_mutex_lock(&sMutxDhcp);
            pthread_cond_signal(&sCondDhcp);
            pthread_mutex_unlock(&sMutxDhcp);
        }
        if( (len > 0) &&(NULL != strstr(evt, "pre-shared key may be incorrect")) ) {
            DBGMSG(".... connect pre-shared key may be incorrect ....\n");
            sConnectStatus = DISABLED_AUTH_FAILURE;
            pthread_mutex_lock(&sMutxDhcp);
            pthread_cond_signal(&sCondDhcp);
            pthread_mutex_unlock(&sMutxDhcp);
        }
        if( (len > 0) &&(NULL != strstr(evt, "ASSOC-REJECT")) ) {
            DBGMSG(".... connect ASSOC-REJECT ....\n");
            sConnectStatus = DISABLED_ASSOCIATION_REJECT;
            pthread_mutex_lock(&sMutxDhcp);
            pthread_cond_signal(&sCondDhcp);
            pthread_mutex_unlock(&sMutxDhcp);
        }


        if( NULL != strstr(evt, "TERMINATING") ) {
            break;
        }
    }

    return NULL;
}


//wifi open
int eng_wifi_open( void )
{
    int cnn_num = 5;
    int cnn_ret = -1;
    int ret = 0;
    pthread_t      ptid;
    pthread_attr_t attr;

    if(is_wifi_driver_loaded() ) {
        wifiClose();
    }

    if((wifi_load_driver() )!= 0 ) {
        ERRMSG("wifi_load_driver fail!");
        return -1;
    }
    DBGMSG("------ start supplicant ------");

    if(wifi_start_supplicant(0) != 0 ) {
        wifi_unload_driver();
        ERRMSG("wifi_start_supplicant fail!");
    }
    DBGMSG("------ supplicant start finish ------");

    while( cnn_num-- ) {
        usleep(200 * 1000);
        if( wifi_connect_to_supplicant() != 0 ) {
            ERRMSG("wifi_connect_to_supplicant the %d th fail!",cnn_num);
            continue;
        } else {
            cnn_ret = 0;
            break;
        }
    }
    DBGMSG("------ after supplicant connect ------");
    if( 0 != cnn_ret ) {
        wifi_stop_supplicant(0);
        wifi_unload_driver();
        ERRMSG("wifi_connect_to_supplicant finally fail!");
        return -3;
    }

    sStatus       = 0;
    sAPNum        = -1;
    sEventLooping = 1;
    wifi_test_result = ZIB_FAIL;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&ptid, &attr, wifiEventLoop, NULL);

    sStatus |= WIFI_STATUS_OPENED;
    sem_post(&g_sem);

    return 0;
}


int eng_wifi_scan_start(void)
{
	DBGMSG( "eng_wifi_scan_start");

	int wifiStatus = 0,ret=0;

	sem_init(&g_sem, 0, 0);

	wifiStatus = wifiGetStatus();
	 if( !(WIFI_STATUS_OPENED & wifiStatus) ) {
            if( eng_wifi_open() < 0 ) {
                ret = ZIB_FAIL;
		   		return -1;
            }
	}

	sem_wait(&g_sem);
	wifiStatus = wifiGetStatus();
	if( !(WIFI_STATUS_SCAN_END & wifiStatus) ) {
            if( !(WIFI_STATUS_SCANNING & wifiStatus) ) {
                 if( eng_wifi_get_scan_result() < 0){
					return -1;
                 }
            }
	}

	return 0;
}

int test_wifi_start(void)
{
	int i=0;
	int ret=0;
	int wifiStatus = 0;

   DBGMSG("test_wifi_start");
	
	if(eng_wifi_scan_start() < 0){
		ret = ZIB_FAIL;
		return ret;
	}

	sem_wait(&g_sem);
	wifiStatus = wifiGetStatus();
	if(WIFI_STATUS_SCAN_END & wifiStatus) {
		if( sAPNum > 0){
			wifi_result();
			if(wifi_test_result==ZIB_PASS) {
				wifi_show_result();
				ret= 1;
			} else {
				wifi_show_result();
				ret= 0;
			}	
		}else{
			ret= 0;
		}
	}
	wifi_test_over_flag =1;
	return ret;
}





int wifiSaveConfig(){
    char reply[64];
    int  len;
    reply[0] = 0;
    len = sizeof reply;
    if((wifiCommand("SAVE_CONFIG", reply, len) <= 0) || (NULL == strstr(reply, "OK"))) {
        ERRMSG("wifiAddNetwork SAVE_CONFIG fail: reply = %s\n", reply);
        return -1;
    }else{
        INFMSG("wifiAddNetwork SAVE_CONFIG success: reply = %s  len = %d\n", reply, len);
        return 0;
    }
}


int wifiAddNetwork( char * ssid, char * psk )
{
    FUN_ENTER;
    AT_ASSERT( ssid != NULL );
    AT_ASSERT( psk != NULL );

    char reply[64];
    int  len;
    int netId = -1;

    //add_network
    reply[0] = 0;
    len = sizeof reply;
    len = wifiCommand("ADD_NETWORK", reply, len);
    netId = atoi(reply);
    if( len >= 0 && netId >= 0 ) {
        INFMSG("wifiAddNetwork success: reply = %s  len = %d netId = %d\n", reply, len, netId);
    }else{
        INFMSG("wifiAddNetwork fail: reply = %s\n", reply);
        return -1;
    }
    char netIdchar[4];
    sprintf(netIdchar,"%d",netId);
    char * setnetwork = "SET_NETWORK ";


    //set_network for ssid
    reply[0] = 0;
    len = sizeof reply;
    char * SSID = " ssid ";
    char cmdssid[64] = "";
    strcat(cmdssid,setnetwork);
    strcat(cmdssid,netIdchar);
    strcat(cmdssid,SSID);
    strcat(cmdssid,ssid);
    INFMSG("wifiAddNetwork cmdssid = %s\n", cmdssid);
    if((wifiCommand(cmdssid, reply, len) <= 0) || (NULL == strstr(reply, "OK"))) {
        ERRMSG("wifiAddNetwork cmdssid fail: reply = %s\n", reply);
        return -2;
    }else{
        INFMSG("wifiAddNetwork cmdssid success: reply = %s  len = %d\n", reply, len);
    }

    //set_network for psk
    reply[0] = 0;
    len = sizeof reply;
    char * PSK = " psk ";
    char cmdpsk[64] = "";
    strcat(cmdpsk,setnetwork);
    strcat(cmdpsk,netIdchar);
    strcat(cmdpsk,PSK);
    strcat(cmdpsk,psk);
    INFMSG("wifiAddNetwork cmdpsk = %s\n", cmdpsk);
    if((wifiCommand(cmdpsk, reply, len) <= 0) || (NULL == strstr(reply, "OK"))) {
        ERRMSG("wifiAddNetwork cmdpsk fail: reply = %s\n", reply);
        return -3;
    }else{
        INFMSG("wifiAddNetwork cmdpsk success: reply = %s  len = %d\n", reply, len);
    }


    //enable_network
    reply[0] = 0;
    len = sizeof reply;
    char * enablenetwork = "ENABLE_NETWORK ";
    char cmdenable[64] = "";
    strcat(cmdenable,enablenetwork);
    strcat(cmdenable,netIdchar);
    INFMSG("wifiAddNetwork cmdenable = %s\n", cmdenable);
    if((wifiCommand(cmdenable, reply, len) <= 0) || (NULL == strstr(reply, "OK"))) {
        ERRMSG("wifiAddNetwork cmdenable fail: reply = %s\n", reply);
        return -4;
    }else{
        INFMSG("wifiAddNetwork cmdenable success: reply = %s  len = %d\n", reply, len);
    }

    //save config
    if(0 != wifiSaveConfig()) {
        ERRMSG("wifiAddNetwork SAVE_CONFIG fail");
        return -5;
    }else{
        INFMSG("wifiAddNetwork SAVE_CONFIG success");
    }

    FUN_EXIT;
    return netId;
}




int wifi_Connect_Network(int netId){
    FUN_ENTER;
    AT_ASSERT( netId != NULL );

    sConnectStatus = CONNECTING;
    char reply[64];
    int  len;
    char netIdchar[4];
    sprintf(netIdchar,"%d",netId);

    //select_network
    reply[0] = 0;
    len = sizeof reply;
    char * selectnetwork = "SELECT_NETWORK ";
    char cmdselect[64] = "";
    strcat(cmdselect,selectnetwork);
    strcat(cmdselect,netIdchar);
    INFMSG("wifi_Connect_Network cmdselect = %s\n", cmdselect);
    if((wifiCommand(cmdselect, reply, len) <= 0) || (NULL == strstr(reply, "OK"))) {
        ERRMSG("wifi_Connect_Network cmdselect fail: reply = %s\n", reply);
        return -1;
    }else{
        INFMSG("wifi_Connect_Network cmdselect success: reply = %s  len = %d\n", reply, len);
    }

    //reconnect
    reply[0] = 0;
    len = sizeof reply;
    if((wifiCommand("RECONNECT", reply, len) <= 0) || (NULL == strstr(reply, "OK"))) {
        ERRMSG("wifi_Connect_Network RECONNECT fail: reply = %s\n", reply);
        return -2;
    }else{
        INFMSG("wifi_Connect_Network RECONNECT success: reply = %s  len = %d\n", reply, len);
    }

    struct timespec to;
    to.tv_nsec = 0;
    to.tv_sec  = time(NULL) + 10;

    pthread_mutex_lock(&sMutxDhcp);
    if( 0 == pthread_cond_timedwait(&sCondDhcp, &sMutxDhcp, &to) ) {
        DBGMSG("wait done.\n");
        if(sConnectStatus != CONNECTED){
            return sConnectStatus;
        }
    } else {
        WRNMSG("wait timeout or error: %s!\n", strerror(errno));
        return DISABLED_UNKNOWN_REASON;
    }
    pthread_mutex_unlock(&sMutxDhcp);

    int retryNum = 3;
    while( retryNum-- ) {
        if( 0 != wifi_Dhcp() ) {
            usleep(1400 * 1000);
            DBGMSG("---- wifi retry dhcp ----\n");
        } else {
            break;
        }
    }

    if(retryNum == 0){
        return DISABLED_DHCP_FAILURE;
    }
    FUN_EXIT;
    return CONNECTED;
}







int get_wifi_test_result(void)
{
	DBGMSG("get_wifi_test_result");

	int wifiStatus = 0;
	int ret =0;

	while(!wifi_test_over_flag) {
		usleep(100*1000);
	}
	
	if(wifi_test_result==ZIB_PASS) {
			DBGMSG("wifi_test_result ZIB_PASS");
			ret= 1;
	} else {
			DBGMSG("wifi_test_result ZIB_FAIL");
			ret= 0;
	}	
	
	return ret;
}


/**
 * 测试wifi
 * */
void eng_wifi_start(void)
{
    DBGMSG("==== eng_wifi_start ====");
    pthread_detach(pthread_self());		//free by itself


    //eng_wifi_scan_start();
  int ret=  test_wifi_start();
  if(ret>=1)
  {

      char * ssid = "chendong";
      char * psk  = "dongchen";
      //the netid is for wifi_Connect_Network
      int netid = wifiAddNetwork(ssid, psk);
      DBGMSG("wifiAddNetwork netid : %d ", netid);
      wifi_Connect_Network(netid);
  }

    DBGMSG("==== eng_bt_wifi_end ====");
}







int wifi_test()
{
    DBGMSG("wifi test start");
    pthread_t wifi_init_thread;
    if(pthread_create(&wifi_init_thread, NULL, (void *) eng_wifi_start, NULL)){
        DBGMSG("wifi  init thread success");
    } else{
        DBGMSG("wifi   init thread failed");
    }
    return 0;
}








