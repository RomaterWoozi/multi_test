//
// Created by wzx on 2019/3/26.
//

#include <system/audio.h>
#include <media/AudioSystem.h>
#include <pthread.h>
#include "zib_common_type.h"
#include "../includes/commm_util.h"
#include "zib_mediautils.h"

using namespace android;



#define DELAY_TIME 15

#define LS_PCM_RECORD_TEST_FILE "/data/record.pcm"
static int rec_stop = 0;
#define RCRD_CNT 160000


void zib_audio_recordplay_test(void) {
    DBGMSG("start record play");
    FILE *fd = NULL;

    uchar *rcrd = (uchar *) malloc(RCRD_CNT);
    fd = fopen(LS_PCM_RECORD_TEST_FILE, "rb");
    //申请一块能装下整个文件的空间
    memset(rcrd, 0x00, RCRD_CNT);
    unsigned int temp;
    if (fread(rcrd, sizeof(uchar), RCRD_CNT, fd)) {
//        unsigned int temp;
        audPlayer_Open(AUD_OUTDEV_SPEAKER, 16000, 0, 0);
        int ret = audPlayer_Play(rcrd, RCRD_CNT);
        DBGMSG("record play status %d", ret);
        sleep(DELAY_TIME);
        audPlayer_Close();
        DBGMSG("pcm play finish.\n");
    }
    //关闭流
    fclose(fd);
}


void *zb_Music_Play(void *temp) {
    DBGMSG("播放mictest 录音文件");
    char path[]="mictest";
    int ret = zb_media_play(path);
    if (ret == 0) {
        DBGMSG("music play success");
    } else {
        DBGMSG("music play error");
    }
    DBGMSG("music play %d seconds waitting for .....",DELAY_TIME);
    sleep(DELAY_TIME);
    zb_meida_play_stop();
    return NULL;
}


void zb_Create_MusicThread(void) {
    DBGMSG("music thread start play mictest.mp3");
    pthread_t re_thread;
    pthread_attr_t re_attr;
    (void) pthread_attr_init(&re_attr);
    (void) pthread_attr_setdetachstate(&re_attr, PTHREAD_CREATE_DETACHED);
    if (0 != pthread_create(&re_thread,
                            &re_attr,
                            zb_Music_Play,
                            NULL)) {
        DBGMSG("Create pthread Error!\n");
    }

}

void zib_audio_record_test(void) {
    unsigned char *rcrd = new unsigned char[RCRD_CNT];
    rec_stop = 0;
    FILE *fp_r =NULL;
    //删除录音文件
    fp_r = fopen(LS_PCM_RECORD_TEST_FILE, "rb");
    if( NULL != fp_r ) {
      int  ret = remove(LS_PCM_RECORD_TEST_FILE);
      if(ret==0)
      {
          DBGMSG("record remove success");
      }else{
          DBGMSG("record remove  filed");
      }
    }
    //打开录音
    DBGMSG("start  record");
    int ret = aureCorder_open(AUD_INDEV_BUILTIN_MIC, 16000);
    DBGMSG("record open status %d", ret);
    //播放录音测试音频
    zb_Create_MusicThread();
    sleep(20);  //
    //获取录音缓存
    audRecorder_read(rcrd, RCRD_CNT);
    //关闭录音
    audRcrder_Stop();
    aureCorder_Close();

    //获取到的音频数据写入到二级制文件里面 wb+
    FILE *pf = fopen(LS_PCM_RECORD_TEST_FILE, "a+");
    fwrite(rcrd, 1, RCRD_CNT, pf);
    fclose(pf);
    delete[]rcrd;
    rec_stop = 1;
    DBGMSG("ls_pcm_read_test,PCM read Finish!\n");
}


int mic_test()
{
    DBGMSG("main,PCM run!\n");
    //设置系统声音
    AudioSystem::setStreamVolumeIndex(AUDIO_STREAM_MUSIC, 6, AUDIO_DEVICE_OUT_SPEAKER);
    zib_audio_record_test();
    DBGMSG("********** start play record ******************");
    usleep(500 * 1000);
    zib_audio_recordplay_test();
    return 0;
}