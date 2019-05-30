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
#include "main.h"
#include "commm_util.h"
#include "zib_key.h"

/**
 *  0x74 on 功能键
 *  自定义控件
 **/
#define ZIB_KEY_POWER 0x74
#define ZIB_KEY_BACK 0x75
#define ZIB_KEY_OK 0x76

struct key_press_info last_key;
static struct pollfd ev_fds[MAX_DEVICES];
static unsigned ev_count = 0;

int ev_init(void) {
    DIR *dir;
    struct dirent *de;
    int fd;

    dir = opendir("/dev/input");
    if (dir != 0) {
        while ((de = readdir(dir))) {
            if (strncmp(de->d_name , "event", 5)) {
                continue;
            }
            fd = openat(dirfd(dir), de->d_name, O_RDONLY);
            if (fd < 0) {
                continue;
            }
            ev_fds[ev_count].fd = fd;
            ev_fds[ev_count].events = POLLIN;
            ioctl(fd, EVIOCSSUSPENDBLOCK, 1);
            ev_count++;
            if (ev_count == MAX_DEVICES) {
                break;
            }
        }
        closedir(dir);
    }
    return 0;
}

void ev_exit(void) {
    while (ev_count > 0) {
        close(ev_fds[--ev_count].fd);
    }
}

/* wait: 0 dont wait; -1 wait forever; >0 wait ms */
int ev_get(struct input_event *ev, int wait_ms) {
    int r;
    unsigned n;

    do {
        r = poll(ev_fds, ev_count, wait_ms);
        if (r > 0) {
            for (n = 0; n < ev_count; n++) {
                if (ev_fds[n].revents & POLLIN) {
                    r = read(ev_fds[n].fd, ev, sizeof(*ev));
                    if (r == sizeof(*ev)) {
                        return 0;
                    }
                }
            }
        }
    } while (wait_ms == -1);
    return -1;
}

long long timeval_diff(struct timeval big,  struct timeval small) {
    return (long long)(big.tv_sec-small.tv_sec)*1000000 + big.tv_usec - small.tv_usec;
}

void single_key_handle(int code, int is_press) {
    printf("single key:%d %s\n", code, is_press ? "press":"release");
    LOGD("single key:%d %s", code, is_press ? "press":"release");
}

void double_key_handle(int code, int is_press) {
    printf("double key:%d %s\n", code, is_press ? "press":"release");
    LOGD("double key:%d %s",  code, is_press ? "press":"release");
}

void triple_key_handle(int code, int is_press) {
    printf("triple key:%d %s\n", code, is_press ? "press":"release");
    LOGD("triple key:%d %s",  code, is_press ? "press":"release");
}

void long_key_handle(int code, int is_press) {
    printf("long key:%d %s\n", code, is_press ? "press":"release");
    LOGD("long key:%d %s",  code, is_press ? "press":"release");
}

void input_keys_analyse(int type, struct input_event *ev) {
    long long time_diff_temp;
    struct timeval now_time = {0, 0};
    while (gettimeofday(&last_key.end_time,  (struct timezone *)0) < 0) {;}
    time_diff_temp = timeval_diff(last_key.end_time, last_key.start_time);

    //同步key数据
    if(type)
    {
        if(last_key.code==0)
        {
            LOGD("2 event value  %d,event type %d event code %d  type %d, last_key.state  %d ,last_key.code %d ",ev->value,ev->type,ev->code,type,last_key.state,last_key.code);
            last_key.code=ev->code;
            last_key.state=KEY_FIRST_PRESS;
        }
    }
    switch(last_key.state) {
        case KEY_ALL_RELEASE: /* wait press1 to state1 */
            if(POLL_TIMEOUT == type || KEY_REPEAT == ev->value) {
                /* nothing need todo */
            }else{
                if(KEY_PRESS == ev->value) {
                    last_key.state = KEY_FIRST_PRESS;
                    last_key.code = ev->code;
                    while (gettimeofday(&last_key.start_time, (struct timezone *)0) < 0) {;}
                }else{
                    printf("[%d]state:%d\n", __LINE__, last_key.state);
                    LOGD("[%d]state:%d", __LINE__, last_key.state);
                }
            }
            break;
        case KEY_FIRST_PRESS: /* prssed, wait release1 to state2 */
            if(POLL_TIMEOUT == type || KEY_REPEAT == ev->value) {
                if(time_diff_temp >= KEY_LONG_PRESS_TIME) {
                    last_key.state = KEY_CHECK_LONG_KEY_RELEASE;
                    long_key_handle(last_key.code, KEY_PRESS);
                }
            }else{
                if(KEY_PRESS == ev->value) {
                    if(last_key.code != ev->code) {
                            printf("new key pressed,last_key:%d,new_key:%d\n", last_key.code, ev->code);
                            LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = 0;
                    }
                }else{
                    if(last_key.code == ev->code) {
                        if(time_diff_temp >= KEY_CLICK_GAP) {
                            last_key.state = KEY_ALL_RELEASE;
                            single_key_handle(last_key.code, KEY_RELEASE);
                        }else{
                            /* check for double key */
                            while (gettimeofday(&last_key.start_time, (struct timezone *)0) < 0) {;}
                            last_key.state = KEY_FIRST_RELEASE;
                        }
                    }else{
                        /* new key released */
                        printf("new key released, last_key:%d,new_key:%d\n", last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = KEY_ALL_RELEASE;
                    }
                }
            }
            break;
        case KEY_FIRST_RELEASE: /* released ,wait press2 to state3 */
            if(POLL_TIMEOUT == type || KEY_REPEAT == ev->value) {
                if(time_diff_temp >= KEY_CLICK_GAP) {
                    last_key.state = KEY_ALL_RELEASE;
                    single_key_handle(last_key.code, 0);
                }
            }else{
                if(KEY_RELEASE == ev->value) {
                    if(last_key.code != ev->code){
                        printf("new key release,last_key:%d,new_key:%d\n",last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = 0;
                    }
                }else{
                    if(last_key.code == ev->code) {
                        /* check if 3st key will be press */
                        while (gettimeofday(&last_key.start_time, (struct timezone *)0) < 0) {;}
                        last_key.state = KEY_SECOND_PRESS;
                    }else{
                        printf("new key preassed, last_key:%d,new_key:%d\n",last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = KEY_ALL_RELEASE;
                    }
                }
            }
            break;
        case KEY_SECOND_PRESS: /* pressed, wait release2 to state4 */
            if(POLL_TIMEOUT == type || KEY_REPEAT == ev->value) {
                if(time_diff_temp >= KEY_CLICK_GAP) {
                    last_key.state = KEY_CHECK_DOUBLE_KEY_RELEASE;
                    double_key_handle(last_key.code, KEY_PRESS);
                }
            }else{
                if(KEY_PRESS == ev->value) {
                    if(last_key.code != ev->code) {
                        printf("new key pressed,last_key:%d,new_key:%d\n", last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = KEY_ALL_RELEASE;
                    }
                }else{
                    if(last_key.code == ev->code) {
                        while (gettimeofday(&last_key.start_time, (struct timezone *)0) < 0) {;}
                        last_key.state = KEY_SECOND_RELEASE;
                    }else{
                        printf("new key released, last_key:%d,new_key:%d\n", last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = KEY_ALL_RELEASE;
                    }
                }
            }
            break;
        case KEY_SECOND_RELEASE: /* released, check 3st key */
            if(POLL_TIMEOUT == type || KEY_REPEAT  == ev->value) {
                if(time_diff_temp >= KEY_CLICK_GAP) {
                    last_key.state = 0;
                    double_key_handle(last_key.code, 0);
                }
            }else{
                if(KEY_RELEASE == ev->value) {
                    if(last_key.code != ev->code) {
                        printf("new key release,last_key:%d,new_key:%d\n", last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = KEY_ALL_RELEASE;
                    }
                }else{
                    if(last_key.code == ev->code) {
                        triple_key_handle(last_key.code, 1);
                        last_key.state = KEY_CHECK_TRIPLE_KEY_RELEASE;
                    }else{
                        printf("new key released, last_key:%d,new_key:%d\n", last_key.code, ev->code);
                        LOGD("new key released, last_key:%d,new_key:%d", last_key.code, ev->code);
                        last_key.state = KEY_ALL_RELEASE;
                    }
                }
            }
            break;
        case KEY_CHECK_LONG_KEY_RELEASE:
            if(POLL_SUCCESS == type && ev->code == last_key.code && KEY_RELEASE == ev->value) {
                long_key_handle(last_key.code, KEY_RELEASE);
                last_key.state = KEY_ALL_RELEASE;
            }
            break;
        case KEY_CHECK_DOUBLE_KEY_RELEASE:
            if(POLL_SUCCESS == type && ev->code == last_key.code && KEY_RELEASE == ev->value) {
                double_key_handle(last_key.code, KEY_RELEASE);
                last_key.state = KEY_ALL_RELEASE;
            }
            break;
        case KEY_CHECK_TRIPLE_KEY_RELEASE:
            if(POLL_SUCCESS == type && ev->code == last_key.code && KEY_RELEASE == ev->value) {
                triple_key_handle(last_key.code, KEY_RELEASE);
                last_key.state = KEY_ALL_RELEASE;
            }
            break;
        default:
            last_key.state = KEY_ALL_RELEASE;
            break;
    }
}

void *input_thread(void *cookie) {
    int fd = -1;
    for(;;) {
        struct input_event ev;
        do {
            fd = ev_get(&ev,KEY_PRESS_TIME_STEP);
            if(-1 == fd) {
                input_keys_analyse(0, &ev);
            }
            else if(EV_KEY == ev.type) {
                //不处理功能键之外的event事件
                if(ev.code==ZIB_KEY_POWER&& ev.code==ZIB_KEY_BACK&& ev.code==ZIB_KEY_OK){
                    input_keys_analyse(1, &ev);
                }
            }
        }while(ev.type != EV_KEY || ev.code > KEY_MAX);
    }
    return NULL;
}

int key_test()
{
    pthread_t t_input;
    int ret;
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
    last_key.state = KEY_ALL_RELEASE;
    pthread_join(t_input, NULL);
    return 0;
}
