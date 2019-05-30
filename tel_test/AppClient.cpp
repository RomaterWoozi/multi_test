
/* 
**
** Copyright 2018, The Longsung Open CPU Project
**
*/

#define LOG_TAG "LSTELEPHONY-Client"

#include <cutils/jstring.h>
#include "AppClient.h"

AppClient* AppClient::mInstance = NULL;
static int s_ClientRegCalled = 0;
UNSOL_ResponseFunctions s_ClientCallbacks;
ProcessThread *p_thread = NULL;

extern char * strdupReadString(Parcel &p);

static void handleEvent(int event, void* data){
	switch(event){
            case RIL_UNSOL_RESPONSE_NEW_SMS:
                s_ClientCallbacks.onNewSms(data);
                break;
            case RIL_CALL_INCOMING:
                s_ClientCallbacks.onNewCall(data);
                break;
            case RIL_CALL_ACTIVE:
                s_ClientCallbacks.onActiveCall(data);
                break;
            default:
                break;
	}
}

void ProcessHandler::handleMessage(const Message& message){
	handleEvent(m_Body->getMessageEvent(), m_Body->getMessageData());
}

AppClient* AppClient::getInstance(){
	if(mInstance == NULL){
            mInstance = new AppClient();
	}else{
            RLOGD("getInstance return exist AppClient");
        }
	return mInstance;
}

AppClient::AppClient(){
    int retry_count = 0;

    sp<IServiceManager> sm = defaultServiceManager();
    mDeathRecipient = new PhoneServiceDeathRecipient();
    is_serviceready = 0;

    do{
        pservice = sm->getService(String16("longsung.telephonyserver"));
        if(pservice != NULL) {
            RLOGD("connected service success!");
            is_serviceready = 1;
            pservice->linkToDeath(mDeathRecipient);
            break;
        }else{
            RLOGD("NO find service sleep and retrying...retry_count=%d",retry_count);
            sleep(1); //wait some time;
            retry_count++;
            if(retry_count > 10) {
                RLOGD("can't create new AppClient");
                return;
            }
            continue;
        }
    }while(1);

    callback = new ClientCallback();
    addClient();
    creatProcessThread();
    RLOGD("create new AppClient");
}

AppClient::~AppClient(){
    mDeathRecipient = NULL;
    RLOGD("delete  AppClient instance");
}

int AppClient::reconnectPhoneService(){
	int retry_count = 0;
	sp<IServiceManager> sm = defaultServiceManager();

	pservice->unlinkToDeath(mDeathRecipient);
	is_serviceready = 0;
	do{
		pservice = sm->getService(String16("longsung.telephonyserver"));
		if(pservice != NULL){
			RLOGD("reconnected service success!");
			is_serviceready = 1;
			pservice->linkToDeath(mDeathRecipient);
			addClient();
			break;
		}else{
			//printf("retry connected!\n");
			sleep(3);
			retry_count++;
			//wait for iot phoneserver;
			if(retry_count > 10){
				return 0;
			}
		}
	}while(1);

	return 1;
}

void* reconnectThread(void *param){
	int ret;

	ret = AppClient::getInstance()->reconnectPhoneService();
	return NULL;
}

void AppClient::PhoneServiceDeathRecipient::binderDied(const wp<IBinder> &who){
	pthread_t tid;
	RLOGD("server exit!");
	if(pthread_create(&tid, NULL, reconnectThread, NULL) < 0){
            RLOGD("can't reconnected service ");
	}
}

int AppClient::isServiceReady(){
	return is_serviceready;
}

static void* startListen(void *){
	ProcessState::self()->startThreadPool();
	IPCThreadState::self()->joinThreadPool();
	return NULL;
}

void AppClient::addClient(){
	Parcel in,out;
	int ret;
	
	in.writeStrongBinder(sp<IBinder> (callback));
	ret = pservice->transact(IF_REGISTER_CLIENT_ID, in, &out, 0);
}

void AppClient::creatProcessThread(){
    p_thread = new ProcessThread();
    p_thread->run("PROCESS_THREAD", PRIORITY_NORMAL);
}

void AppClient::register_ClientCallback(const UNSOL_ResponseFunctions *callbacks ){
        if (callbacks == NULL) {
             RLOGE("register_Callback: no callback funtions ");
             return;
        }
        if (s_ClientRegCalled > 0) {
            RLOGE("register_Callback has been called more than once. Subsequent call ignored");
            return;
        }
        memset(&s_ClientCallbacks,0,sizeof(s_ClientCallbacks));
        memcpy(&s_ClientCallbacks, callbacks, sizeof (UNSOL_ResponseFunctions));
         s_ClientRegCalled = 1;
}

ClientCallback::ClientCallback(){
	pthread_t tid;
	int ret;

	ret = pthread_create(&tid, NULL, startListen, NULL);
}


status_t ClientCallback::onTransact(uint32_t code, const Parcel& p, Parcel* reply, uint32_t flags){
    RLOGD("onTransact:code=%d",code);

    switch(code){
        case RIL_UNSOL_RESPONSE_NEW_SMS:
            {
                char *sms_content = NULL;
                sms_content = strdupReadString(const_cast<Parcel &>(p));
                sp<MessageBody> msg = new MessageBody(RIL_UNSOL_RESPONSE_NEW_SMS,sms_content);
                sp<ProcessHandler> handler = new ProcessHandler(msg);
                p_thread->sendMessage(handler);

                break;
            }
        case RIL_CALL_INCOMING:
            {
                RLOGD("onTransact:RIL_CALL_INCOMING");
                char *callNumber = NULL;
                callNumber = strdupReadString(const_cast<Parcel &>(p));
                sp<MessageBody> msg = new MessageBody(RIL_CALL_INCOMING,callNumber);
                sp<ProcessHandler> handler = new ProcessHandler(msg);
                p_thread->sendMessage(handler);
                break;
            }
        case RIL_CALL_ACTIVE:
            {
                RLOGD("onTransact:RIL_CALL_ACTIVE");
                int *callId = (int *)malloc(sizeof(int));
                *callId = 0;
                p.readInt32(callId);
                sp<MessageBody> msg = new MessageBody(RIL_CALL_ACTIVE,callId);
                sp<ProcessHandler> handler = new ProcessHandler(msg);
                p_thread->sendMessage(handler);
                break;
            }
        default:
            break;  
    }   
    return NO_ERROR;

}

void AppClient::sendTransact(int requestId,const void *data, size_t datalen,void * result){
    Parcel in,out;
    int ret = 0;
    RLOGD("sendTransact:requestId=%d,datalen=%d",requestId,datalen);
    if(!isServiceReady()) {
        RLOGD("sendTransact:service is not ready");
        return;
    }

    if(data != NULL && datalen > 0){
        in.write(data,datalen);
    }
    ret = pservice->transact(requestId, in, &out, 0);
    RLOGD("sendTransact:pservice->transact result=%d",ret);
    if(ret == 0){
        RIL_onRequestComplete(out,result);
    }else if(ret == 100){
        //service not support cmd!
        RLOGD("sendTransact:service not support cmd!");
    }
}

