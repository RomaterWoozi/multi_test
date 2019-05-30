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
#include <cstdio>

int main(int argc, char *argv[]) {

//    FILE *file;

    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s [-g] gsensor test  \n  [-w] wifi test \n [-bt] bluethood\n [-m]mic test\n [-k] key test\n [-gps_test] gps_test test \n ",
                argv[0]);
        return 1;
    }
    /* parse command line arguments */
    argv += 2;
    int ret = -1;
    while (*argv) {
        if (strcmp(*argv, "-g")) {
            argv++;
            ret = zib_gsensor_test();
            printf("gsensor status ret=%d  \n ",ret);
            DBGMSG("gsensor status ret=%d  \n ",ret);
            if (ret) {
                zib_audio_play_by_name(ZIB_FACTORY_GSENSOR_PASS);
            } else {
                zib_audio_play_by_name(ZIB_FACTORY_GSENSOR_FAIL);
            }

        } else if (strcmp(*argv, "-w")) {
            argv++;
            ret= wifi_test();
            printf("wifi test result ret=%d  \n ",ret);
            DBGMSG("wifi test result ret=%d  \n ",ret);
            if(ret){
                zib_audio_play_by_name(ZIB_FACTORY_WIFI_PASS);
            }else{
                zib_audio_play_by_name(ZIB_FACTORY_WIFI_FAIL);
            }

        } else if (strcmp(*argv, "-k")) {
            argv++;
//               ret=key_test.key_test();
        } else if (strcmp(*argv, "-b")) {
            argv++;

        } else if (strcmp(*argv, "-gps")) {
            argv++;
            ret = test_gps_start();


        }else if(strcmp(*argv,"-t")){
            argv++;
           ret= tel_test();
            printf("telephone test result ret=%d  \n ",ret);
            DBGMSG("telephone test result ret=%d  \n ",ret);
        }
    }
}







