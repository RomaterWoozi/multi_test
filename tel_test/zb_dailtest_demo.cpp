/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/poll.h>
#include <time.h>
#include <linux/input.h>
#include <pthread.h>
#include <zib_common_debug.h>
#include "zib_dial_test.h"
//#include "zib_key.h"
#include "../includes/main.h"
#include "zib_mediautils.h"
#include "../key_app.cpp"

int tel_test() {
    pthread_t t_input,call_listen;
    int ret;
    
    //add  voice prompts
            LOGD("add  voice prompts");
    printf("start key listen\n");
            LOGD("start key listen");
    ev_init();
    if(0 == ev_count) {
        printf("open input file failed:%d\n", ev_count);
                LOGD("open input file failed:%d", ev_count);
        return -1;
    }
    ret = pthread_create(&t_input, NULL, input_thread, NULL);
    if(ret) {
        printf("thread input_thread create failed\n");
                LOGD("thread input_thread create failed");
        ev_exit();
        return -1;
    }
            LOGD("main 2");
    ret =pthread_create(&call_listen,NULL,zb_dial_test1,NULL);
    last_key.state = KEY_ALL_RELEASE;
    pthread_join(t_input, NULL);

            LOGD("add  call_listen");

            LOGD("main end");
    return 0;
}
