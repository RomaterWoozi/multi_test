//
// Created by wzx on 2019/3/21.
//
#include <stdio.h>
#include<unistd.h>
#include "ls_telephony_util.h"
#include "zib_dial_test.h"

using namespace android;
static RIL_Call ril_calls[RIL_MAX_CALLS];


static void onNewSms(void *resp) {
    char *sms_content = (char *) resp;
    DBGMSG("newSMS:sms_content=%s  ", sms_content);
    printf("newSMS:sms_content=%s\n", sms_content);
};

static void onNewCall(void *resp) {
    char *number = (char *) resp;
    printf("onNewCall number =%s\n", number);
    DBGMSG("onNewCall number =%s  ", number);
    ls_api_acceptCall();
};


static void onActiveCall(void *resp) {
    printf("onActiveCall..");
    int callIdex = *((int *) resp);
    printf("onActiveCall:callIdex=%d\n", callIdex);
    DBGMSG("onActiveCall:callIdex=%d  ", callIdex);
//    sleep(10);
//    ls_api_hangupConnection(callIdex);
};

static void onHangupCall(void *resp) {
    printf("onHangupCall.. \n");
    int callIdex = *((int *) resp);
    printf("onHangupCall:callIdex=%d\n\n", callIdex);
    DBGMSG("onHangupCall:callIdex=%d\n\n", callIdex);
};

static void onDialing(void *resp) {
    printf("onDialing.. \n");
    int callIdex = *((int *) resp);
    printf("onDialing:callIdex=%d\n\n", callIdex);
    DBGMSG("onDialing:callIdex=%d\n\n", callIdex);
};

static void onAlerting(void *resp) {
    printf("onAlerting.. \n");
    int callIdex = *((int *) resp);
    printf("onAlerting:callIdex=%d\n\n", callIdex);
    DBGMSG("onAlerting:callIdex=%d\n\n", callIdex);
};

static void onWaiting(void *resp) {
    printf("onWaiting.. \n");
    int callIdex = *((int *) resp);
    printf("onWaiting:callIdex=%d\n\n", callIdex);
    DBGMSG("onWaiting:callIdex=%d\n\n", callIdex);
};

static void onHolding(void *resp) {
    printf("onHolding.. \n");
    int callIdex = *((int *) resp);
    printf("onHolding:callIdex=%d\n\n", callIdex);
};
static const UNSOL_ResponseFunctions s_callbacks = {
        onNewSms,
        onNewCall,
        onActiveCall,
        onHangupCall,
        onDialing,
        onAlerting,
        onWaiting,
        onHolding
};


void getImsi_test() {
    int ret;
    DBGMSG("getImsi_test enter..");
    printf("getImsi_test enter..\n");

    char imsi[16] = {0};
    ret = ls_api_getIMSI(&imsi[0]);

    if (0 == ret) {
        DBGMSG("getImsi_test succ,imsi =%s\n", imsi);
        printf("getImsi_test succ,imsi =%s\n", imsi);
    } else {
        DBGMSG("getImsi_test fail");
        printf("getImsi_test fail\n");
    }
    DBGMSG("getImsi_test end..");
    printf("getImsi_test end..\n");
}

void getSimStatus_test() {
    int ret;
    DBGMSG("getSimStatus_test enter..");
    printf("getSimStatus_test enter..\n");
    RIL_CardStatus cardstatus;
    memset(&cardstatus, 0, sizeof(cardstatus));
    ret = ls_api_getSimStatus(&cardstatus);

    if (0 == ret) {
        printf("getSimStatus_test succ,card_state =%d,card_num_applications=%d\n", cardstatus.card_state,
               cardstatus.num_applications);
        switch (cardstatus.card_state) {
            case RIL_CARDSTATE_ABSENT:
                printf("=== CARD ABSENT===\n");
                DBGMSG("=== CARD ABSENT===\n");
                break;
            case RIL_CARDSTATE_PRESENT:
                printf("=== CARD PRESENT===\n");
                DBGMSG("=== CARD PRESENT===\n");
                break;
            case RIL_CARDSTATE_ERROR:
                printf("=== CARD ERROR===\n");
                DBGMSG("=== CARD ERROR===\n");
                break;
            default:
                printf("=== get card state error===\n");
                DBGMSG("=== get card state error===\n");
                break;
        }

        if (cardstatus.num_applications > 0) {
            for (int index = 0; index < cardstatus.num_applications; index++) {
                printf("getSimStatus_test applications[%d] app_state=%d\n", index,
                       cardstatus.applications[index].app_state);
                switch (cardstatus.applications[index].app_state) {
                    case RIL_APPSTATE_UNKNOWN:
                        printf("=== SIM  RIL_APPSTATE_UNKNOWN===\n");
                        break;
                    case RIL_APPSTATE_DETECTED:
                        printf("=== SIM RIL_APPSTATE_DETECTED===\n");
                        break;
                    case RIL_APPSTATE_PIN:
                        printf("=== SIM RIL_APPSTATE_PIN===\n");
                        break;
                    case RIL_APPSTATE_PUK:
                        printf("=== SIM RIL_APPSTATE_PUK===\n");
                        break;
                    case RIL_APPSTATE_SUBSCRIPTION_PERSO:
                        printf("=== SIM RIL_APPSTATE_SUBSCRIPTION_PERSO===\n");
                        break;
                    case RIL_APPSTATE_READY:
                        printf("=== SIM RIL_APPSTATE_READY===\n");
                        DBGMSG("=== SIM RIL_APPSTATE_READY===\n");
                        break;
                    case RIL_APPSTATE_BLOCKED:
                        printf("=== SIM RIL_APPSTATE_BLOCKED===\n");
                        DBGMSG("=== SIM RIL_APPSTATE_BLOCKED===\n");
                        break;
                    case RIL_APPSTATE_NOT_READY:
                        printf("=== SIM RIL_APPSTATE_NOT_READY===\n");
                        DBGMSG("=== SIM RIL_APPSTATE_NOT_READY===\n");
                        break;
                    default:
                        printf("=== get sim state error===\n");
                        DBGMSG("=== get sim state error===\n");
                        break;
                }
            }
        }
    } else {
        printf("getSimStatus_test fail\n");
        DBGMSG("getSimStatus_test fail\n");
    }
    printf("getSimStatus_test end..\n");
    DBGMSG("getSimStatus_test end..\n");

}

void airplaneMode_test() {
    DBGMSG("start");

    char closePS[256] = {0};
    ls_api_sendAT("AT+SFUN=5", &closePS[0]);
    printf("close procol stack = %s\n", closePS);
    sleep(10);
    char openPS[256] = {0};
    ls_api_sendAT("AT+SFUN=4", &openPS[0]);
    DBGMSG("open procol stack = %s\n ", openPS);
    printf("open procol stack = %s\n", openPS);

}

void send_sms_test(void) {
    int ret;

    printf("send sms start...\n");
    ///test send sms
    /// http://www.multisilicon.com/blog/a22201774~/pdu.htm  转码网站
    ///
    //const char *smsc = "0891683110304105F0"; // 短信中心号码(需要转码) +8613010314500
    const char *message = "11000b818128212537f80008ff1000680065006c006c006f002c4f60597d";//pdu hello,ţۃ18821252738

    char pdu_smsc[50] = {0};
    ret = ls_api_getSMSC(&pdu_smsc[0]);
    if (ret < 0) {
        printf("ls_api_getSMSC err\n");
        return;
    }
    printf("ls_api_getSMSC smsc pdu is %s\n", pdu_smsc);

    ret = ls_api_sendsms(pdu_smsc, message);
    if (ret < 0) {
        printf("ls_api_sendsms err\n");
        return;
    }
    printf("send sms end...\n");
}

int dial_test(char *number) {
    int ret = 0;
    printf("dial_test enter.. %s \n", number);
    DBGMSG("dial_test enter..%s ", number);
    ret = ls_api_dial(number);
    if (0 == ret) {
        DBGMSG("ls_api_dial succ");
        printf("ls_api_dial succ\n");
        //success init run_time
        return 0;
    } else {
        printf("ls_api_dial fail\n");
        DBGMSG("ls_api_dial fail");
    }
    return -1;
}


void *zb_dial_test1(void *arg) {
    register_Client(&s_callbacks);
            LOGD("===start zb_dial_test1====");
    memset(ril_calls, 0, sizeof(ril_calls));

    getImsi_test();
    getSimStatus_test();
    //send_sms_test();

//     int ret =dial_test("112");
    //airplaneMode_test();

    while (1) {
        sleep(0x00ffffff);
    }


            LOGD("===end zb_dial_test1====");
    printf("===end test====\n");
    deRegister_Client();
    return 0;
}


/**
 * 打电话测试
 * **/
int zib_dial_test(char *number) {
    printf("===start test====\n");
    register_Client(&s_callbacks);
    DBGMSG("===start dialcall test====");
    memset(ril_calls, 0, sizeof(ril_calls));

    getImsi_test();
    getSimStatus_test();
    //send_sms_test();
    int ret = dial_test(number);
    //airplaneMode_test();
    if (ret) {
        DBGMSG("dial 10086 success");
    }
    sleep(0x16);
    DBGMSG("===end dialcall test====");
    printf("===end test====\n");
    deRegister_Client();
    return ret;
}
















