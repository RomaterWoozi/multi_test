//
// Created by wzx on 2019/5/29.
//

#include "includes/main.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include "zib_gps.h"
#include "zib_mediautils.h"
#include "main.h"
#include "zib_dial_test.h"
#include "zib_gsensor_test.h"
#include "zib_key.h"
#include "zib_bluetooth.h"
#include <media/AudioSystem.h>
#include <system/audio.h>
using namespace android;

int main(int argc, char *argv[]) {

//    FILE *file;

    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s \n [-g] gsensor test  \n [-w] wifi test \n [-bt] bluethood\n [-m]mic test\n [-k] key test\n [-gps_test] gps_test test \n ",
                argv[0]);
        return 1;
    }
    /* parse command line arguments */
    printf("step1  \n");

    int ret = -1;
    while (++argv, --argc) {
        if (**argv == '-' || **argv == '/') {
            char *p;
            printf("step2 argv %s \n", *argv);
            if (strcmp(*argv, "g") == 0) {
                zib_audio_play_by_name(ZIB_FACTORY_GSENSOR_START);
                ret = zib_gsensor_test();
                printf("gsensor status test_ret=%d  \n ", ret);
                DBGMSG("gsensor status test_ret=%d  \n ", ret);
                if (ret) {
                    zib_audio_play_by_name(ZIB_FACTORY_GSENSOR_PASS);
                } else {
                    zib_audio_play_by_name(ZIB_FACTORY_GSENSOR_FAIL);
                }
                break;
            } else if (strcmp(*argv, "w") == 0) {

                ret = wifi_test();
                printf("wifi test result test_ret=%d  \n ", ret);
                DBGMSG("wifi test result test_ret=%d  \n ", ret);
                if (ret) {
                    zib_audio_play_by_name(ZIB_FACTORY_WIFI_PASS);
                } else {
                    zib_audio_play_by_name(ZIB_FACTORY_WIFI_FAIL);
                }
                break;
            } else if (strcmp(*argv, "k") == 0) {

//               test_ret=key_test.key_test();
                break;
            } else if (strcmp(*argv, "-bt")==0) {
                ret = zb_bluetooth_test();
                if (ret) {
                    zib_audio_play_by_name(ZIB_FACTORY_BT_PASS);
                } else {
                    zib_audio_play_by_name(ZIB_FACTORY_BT_FAIL);
                }
                break;
            } else if (strcmp(*argv, "-gps")==0) {
                ret = test_gps_start();
                if (ret == 0) {
                    zib_audio_play_by_name(ZIB_FACTORY_GPS_SIGNAL_PASS);
                } else {
                    zib_audio_play_by_name(ZIB_FACTORY_GPS_SIGNAL_FAIL);
                }
                break;
            } else if (strcmp(*argv, "-t")==0) {
                ret = tel_test();
                printf("telephone test result test_ret=%d  \n ", ret);
                DBGMSG("telephone test result test_ret=%d  \n ", ret)
                break;
            }else if(strcmp(*argv,"-s")==0){
                printf("step3");
                if(argc>=2){
                    argv++;
                    int sound=15;
                    if (*argv) sound= atoi(*argv);
                    AudioSystem::setStreamVolumeIndex(AUDIO_STREAM_MUSIC, random()*15, AUDIO_DEVICE_OUT_SPEAKER);
                }
                break;
            }
        }

    }
}







