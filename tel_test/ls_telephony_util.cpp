/* 
**
** Copyright 2018, The wuzhouxing Open CPU Project
**
*/


#define LOG_TAG "LSTELEPHONY-SO"

#include <binder/Parcel.h>
#include <cutils/jstring.h>
#include "AppClient.h"
#include "ls_telephony.h"
#include <zib_common_debug.h>

using namespace android;

static AppClient* mClientInstance = NULL;
static int s_registerClient = 0;
static char aid_ptr[100]={0};
static char app_label_ptr[100]={0};
static char number[50]={0};
static char name[50]={0};

const char * failCauseToString(RIL_Errno e) {
    switch(e) {
        case RIL_E_SUCCESS: return "E_SUCCESS";
        case RIL_E_RADIO_NOT_AVAILABLE: return "E_RADIO_NOT_AVAILABLE";
        case RIL_E_GENERIC_FAILURE: return "E_GENERIC_FAILURE";
        case RIL_E_PASSWORD_INCORRECT: return "E_PASSWORD_INCORRECT";
        case RIL_E_SIM_PIN2: return "E_SIM_PIN2";
        case RIL_E_SIM_PUK2: return "E_SIM_PUK2";
        case RIL_E_REQUEST_NOT_SUPPORTED: return "E_REQUEST_NOT_SUPPORTED";
        case RIL_E_CANCELLED: return "E_CANCELLED";
        case RIL_E_OP_NOT_ALLOWED_DURING_VOICE_CALL: return "E_OP_NOT_ALLOWED_DURING_VOICE_CALL";
        case RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW: return "E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW";
        case RIL_E_SMS_SEND_FAIL_RETRY: return "E_SMS_SEND_FAIL_RETRY";
        case RIL_E_SIM_ABSENT:return "E_SIM_ABSENT";
        case RIL_E_ILLEGAL_SIM_OR_ME:return "E_ILLEGAL_SIM_OR_ME";
        default: return "<unknown error>";
    }
}

extern "C" char * strdupReadString(Parcel &p) {
    size_t stringlen;
    const char16_t *s16;

    s16 = p.readString16Inplace(&stringlen);

    return strndup16to8(s16, stringlen);
}

static void writeStringToParcel(Parcel &p, const char *s) {
    char16_t *s16;
    size_t s16_len;
    s16 = strdup8to16(s, &s16_len);
    p.writeString16(s16, s16_len);
    free(s16);
}

void register_Client( const UNSOL_ResponseFunctions *callbacks){
    DBGMSG("register_Client start...s_registerClient=%d",s_registerClient);
    mClientInstance = AppClient::getInstance();
    if(s_registerClient > 0){
        DBGMSG("register_Client has been called more than once. Subsequent call ignored");
        return;
    }
    mClientInstance->register_ClientCallback(callbacks);   
    s_registerClient = 1;

    DBGMSG("register_Client end...");
}

void deRegister_Client(){
    DBGMSG("deRegister_Client start");
    if(mClientInstance != NULL){
       delete mClientInstance;
    }
    s_registerClient = 0;

    DBGMSG("deRegister_Client end...");
}

/*
** SMSC to PDU
*/
int smsc_to_pdu(char *smsc, char *pdu_smsc, int pdu_smsc_len){
    if (pdu_smsc_len < 16){
    	return -1;
    }

    smsc++;  // delete "+"
    int  p_len = strlen(smsc);
    char *scb  = pdu_smsc+4;
    memcpy(scb, smsc, p_len);
    if(p_len%2 != 0) {
    	strncpy(scb+p_len, "F", 2);//The length is odd, add 'F' at the end.  "****8613800100500F"
    }

    int i;
    char swap_temp;
    p_len = strlen(scb);
    for (i=0; i<p_len/2; i++) {  //Odd and even bit exchange "****683108100005F0"
    	swap_temp = *(scb+1);
    	*(scb+1) = *scb;
    	*scb = swap_temp;
    	scb += 2;
    }
    memcpy(pdu_smsc, "0891", 4);
    return 0;
}

static void handleGetCurrentCallList(Parcel &p,void* callLists){
    char * number_tmp = NULL;
    char * name_tmp = NULL;    
    int num;

    p.readInt32(&num);
    DBGMSG("handleGetCurrentCallList num = %d", num);

    RIL_Call (*ril_rp)[RIL_MAX_CALLS] = (RIL_Call (*)[RIL_MAX_CALLS])callLists;

    for (int i = 0; i < num && i< RIL_MAX_CALLS;i++){
        int value;
        int unused;
        p.readInt32(&value);
        ril_rp[i]->state = (RIL_CallState)value;
        p.readInt32(&ril_rp[i]->index);
        p.readInt32(&ril_rp[i]->toa);
        p.readInt32(&ril_rp[i]->isMpty);
        p.readInt32(&ril_rp[i]->isMT);
        p.readInt32(&ril_rp[i]->als);
        p.readInt32(&ril_rp[i]->isVoice);
	p.readInt32(&ril_rp[i]->isVoicePrivacy);
        number_tmp = strdupReadString(p);
        if (number_tmp != NULL){
            memset(number,0,sizeof(number));
            memcpy(number,number_tmp,strlen(number_tmp));
            free(number_tmp);
        }
        ril_rp[i]->number = number;
        p.readInt32(&ril_rp[i]->numberPresentation);
        name_tmp = strdupReadString(p);
        if (name_tmp != NULL){
            memset(name,0,sizeof(name));
            memcpy(name,name_tmp,strlen(name_tmp));
            free(name_tmp);
        }
        ril_rp[i]->name = name;
        p.readInt32(&ril_rp[i]->namePresentation);
        p.readInt32(&unused);
        ril_rp[i]->uusInfo = NULL; /* UUS Information is absent */
	DBGMSG("handleGetCurrentCallList:state:%d, index:%d, isMT:%d, isVoice:%d, number:%s",ril_rp[i]->state, ril_rp[i]->index, ril_rp[i]->isMT,ril_rp[i]->isVoice, ril_rp[i]->number);
    }
}

static void handleIccCardStatus(Parcel &p, RIL_CardStatus * cardstatus){
    char * aid_ptr_tmp = NULL;
    char * app_label_ptr_tmp = NULL;
     DBGMSG("handleIccCardStatus enter..."); 
    p.readInt32(&(cardstatus->card_state));
    p.readInt32(&(cardstatus->universal_pin_state));
    p.readInt32(&(cardstatus->gsm_umts_subscription_app_index));
    p.readInt32(&(cardstatus->cdma_subscription_app_index));
    p.readInt32(&(cardstatus->ims_subscription_app_index));
    p.readInt32(&(cardstatus->num_applications));
    DBGMSG("handleIccCardStatus:card_state=%d",cardstatus->card_state); 

    for(int index = 0; index < cardstatus->num_applications; index++){
        p.readInt32(&(cardstatus->applications[index].app_type));
        p.readInt32(&(cardstatus->applications[index].app_state));
        p.readInt32(&(cardstatus->applications[index].perso_substate));
        //cardstatus->applications[index].aid_ptr = strdupReadString(p);
        aid_ptr_tmp = strdupReadString(p);
        DBGMSG("handleIccCardStatus:aid_ptr_tmp=%s",aid_ptr_tmp); 
        if(aid_ptr_tmp != NULL){
            memcpy(aid_ptr,aid_ptr_tmp,strlen(aid_ptr_tmp));
            free(aid_ptr_tmp);
        }
        cardstatus->applications[index].aid_ptr = aid_ptr;
        app_label_ptr_tmp = strdupReadString(p);
        if(app_label_ptr_tmp != NULL){
            memcpy(app_label_ptr,app_label_ptr_tmp,strlen(app_label_ptr_tmp));
            free(app_label_ptr_tmp);
        }
        cardstatus->applications[index].app_label_ptr = app_label_ptr;     
        p.readInt32(&(cardstatus->applications[index].pin1_replaced));
        p.readInt32(&(cardstatus->applications[index].pin1));
        p.readInt32(&(cardstatus->applications[index].pin2));
    }
}

static int sendRequest(int request,const void *data, size_t dataSize,void * result){
    if (dataSize > MAX_COMMAND_BYTES) {
        DBGMSG("sendRequest: packet larger than %u (%u)",MAX_COMMAND_BYTES, (unsigned int )dataSize);
        return -1;
    }    
    if(mClientInstance != NULL){
        mClientInstance->sendTransact(request, data,dataSize,result);
    }else{
        DBGMSG("sendRequest:no register Client Instance"); 
    }
    return 0;
}

extern "C" void RIL_onRequestComplete(const Parcel& p,void * result) {      
    int requestId,err;
    p.readInt32(&requestId);
    p.readInt32(&err);
    p.setDataPosition(p.dataPosition());
    size_t availDatalen = p.dataAvail();
    const void *response = p.readInplace(availDatalen);
    
    Parcel reqCmplete;
    reqCmplete.setData((uint8_t *) response, availDatalen);   
    DBGMSG ("RIL_onRequestComplete: requestId %d,error=%d",requestId,err);
    char * imsi = NULL;
    char * smsc = NULL;
    char pdu_smsc[50] = {0};
    char * atRsp = NULL;
    if(err == 0){
        switch(requestId){
            case RIL_REQUEST_GET_SIM_STATUS:
                DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_GET_SIM_STATUS");
                handleIccCardStatus(reqCmplete,(RIL_CardStatus*)result);
                break;
            case RIL_REQUEST_GET_CURRENT_CALLS:
                 DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_GET_CURRENT_CALLS");
                 handleGetCurrentCallList(reqCmplete,result);
                 break;            
            case RIL_REQUEST_DIAL:
                 DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_DIAL");
                 break;
           case RIL_REQUEST_GET_IMSI:
                imsi = strdupReadString(reqCmplete);
                DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_GET_IMSI : imsi=%s",imsi);
                if(imsi != NULL){
                    memcpy(result, imsi, 15);
                    free(imsi);
                }
                break;
            case RIL_REQUEST_HANGUP:
                DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_HANGUP");
                break;
           case RIL_REQUEST_SEND_SMS:
                DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_SEND_SMS");
                break;
           case RIL_REQUEST_ANSWER:
                DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_ANSWER");
                break;                
            case RIL_REQUEST_GET_SMSC_ADDRESS:
                 smsc = strdupReadString(reqCmplete);
                 DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_GET_SMSC_ADDRESS : smsc=%s",smsc);
                 smsc_to_pdu(smsc,&pdu_smsc[0],sizeof(pdu_smsc));
                 if(smsc != NULL){ 
                     free(smsc);
                 }                 
                 if(pdu_smsc != NULL){
                     memcpy(result, pdu_smsc, strlen(pdu_smsc));
                 }
                 break;
           case RIL_REQUEST_SEND_AT:
                atRsp = strdupReadString(reqCmplete);
                DBGMSG ("RIL_onRequestComplete: case RIL_REQUEST_SEND_AT : atRsp=%s",atRsp);
                if(atRsp != NULL){
                    memcpy(result, atRsp, strlen(atRsp));
                    free(atRsp);
                }
                break;
            default:
                DBGMSG("RIL_onRequestComplete no match requestId");
                break;
        }
    }else{
        DBGMSG ("RIL_onRequestComplete: fails by %s",failCauseToString((RIL_Errno)err));
    }
}

int ls_api_getIMSI(char * imsi){
    int ret;
    ret = sendRequest(RIL_REQUEST_GET_IMSI,NULL,0,imsi);
    if (ret < 0) {
        DBGMSG("ls_api_getIMSI:send RIL_REQUEST_GET_IMSI Fail!");
	return -1;
    }

    DBGMSG("ls_api_getIMSI:send RIL_REQUEST_GET_IMSI Succ!");
    return 0;       
}

int ls_api_getSimStatus(RIL_CardStatus * cardstatus){
    int ret;
    ret = sendRequest(RIL_REQUEST_GET_SIM_STATUS,NULL,0,cardstatus);
    if (ret < 0) {
        DBGMSG("ls_api_getSimStatus:send RIL_REQUEST_GET_SIM_STATUS Fail!");
	return -1;
    }

    DBGMSG("ls_api_getSimStatus:send RIL_REQUEST_GET_SIM_STATUS Succ!");
    return 0;       
}

int ls_api_sendAT(char* atCmd,char *atRsp){
    int ret;
    Parcel p;
    writeStringToParcel( p, atCmd );
    ret = sendRequest(RIL_REQUEST_SEND_AT,p.data(), p.dataSize(),atRsp);
    if (ret < 0) {
        DBGMSG("ls_api_sendAT:send RIL_REQUEST_SEND_AT Fail!");
	return -1;
    }

    DBGMSG("ls_api_sendAT:send RIL_REQUEST_SEND_AT Succ!");
    return 0;    
}

int ls_api_getSMSC(char * smsc){
    int ret;
    ret = sendRequest(RIL_REQUEST_GET_SMSC_ADDRESS,NULL,0,smsc);
    if (ret < 0) {
        DBGMSG("ls_api_getSMSC:send RIL_REQUEST_GET_SMSC_ADDRESS Fail!");
	return -1;
    }

    DBGMSG("ls_api_getSMSC:send RIL_REQUEST_GET_SMSC_ADDRESS Succ!");
    return 0;       
}

int ls_api_sendsms( const char* smscPDU, const char* sms_content)
{
    int ret;
    Parcel p;

    p.writeInt32(2);
    writeStringToParcel( p, smscPDU );
    writeStringToParcel( p, sms_content );
	
    ret = sendRequest(RIL_REQUEST_SEND_SMS, p.data(), p.dataSize(),NULL);
    if (ret < 0) {
        DBGMSG("ls_api_sendsms: send RIL_REQUEST_SEND_SMS Fail!");
	return -1;
    }

    DBGMSG("ls_api_sendsms:send RIL_REQUEST_SEND_SMS Succ!");
    return 0;
}

int ls_api_dial( const char* number )
{
    int ret;
    Parcel p;

    writeStringToParcel( p, number );
    p.writeInt32(0);//clir mode
    p.writeInt32(0);//uusinfo
    ret = sendRequest(RIL_REQUEST_DIAL, p.data(), p.dataSize(),NULL);
    if (ret < 0) {
        DBGMSG("ls_api_dial:send RIL_REQUEST_DIAL Fail!");
	return -1;
    }

    DBGMSG("ls_api_dial:send RIL_REQUEST_DIAL Succ!");
    return 0;
}

 int ls_api_getCurrentCalls(void * currentCalls){
    int ret;
    ret = sendRequest(RIL_REQUEST_GET_CURRENT_CALLS,NULL,0,currentCalls);
    if (ret < 0) {
        DBGMSG("ls_api_getCurrentCalls:send RIL_REQUEST_GET_CURRENT_CALLS Fail!");
        return -1;
    }

    DBGMSG("ls_api_getCurrentCalls:send RIL_REQUEST_GET_CURRENT_CALLS Succ!");
    return 0;  
}

 int ls_api_acceptCall(){
    int ret;
    ret = sendRequest(RIL_REQUEST_ANSWER,NULL,0,NULL);
    if (ret < 0) {
        DBGMSG("ls_api_acceptCall:send RIL_REQUEST_ANSWER Fail!");
        return -1;
    }

    DBGMSG("ls_api_acceptCall:send RIL_REQUEST_ANSWER Succ!");
    return 0;  
}

  int ls_api_hangupConnection( int index){
    int ret;
    Parcel p;

    p.writeInt32(1);//count
    p.writeInt32(index);//index
    
    ret = sendRequest(RIL_REQUEST_HANGUP,p.data(), p.dataSize(),NULL);
    if (ret < 0) {
        DBGMSG("ls_api_hangupConnection:send RIL_REQUEST_HANGUP Fail!");
        return -1;
    }

    DBGMSG("ls_api_hangupConnection:send RIL_REQUEST_HANGUP Succ!");
    return 0;  
}

