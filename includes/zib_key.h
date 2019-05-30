//
// Created by wzx on 2019/5/30.
//

#ifndef SAMPLE1_ZIB_KEY_H
#define SAMPLE1_ZIB_KEY_H
#endif //SAMPLE1_ZIB_KEY_H

#include <utils/Log.h>

#define MAX_DEVICES                (16)
#define EVIOCSSUSPENDBLOCK         _IOW('E', 0x91, int)

#define KEY_PRESS_TIME_STEP        (5)
#define KEY_CLICK_GAP              (200000)
#define KEY_LONG_PRESS_TIME        (1000000)

#define KEY_RELEASE    (0)
#define KEY_PRESS      (1)
#define KEY_REPEAT     (2)

#define POLL_TIMEOUT (0)
#define POLL_SUCCESS (1)

#define KEY_ALL_RELEASE      (0)
#define KEY_FIRST_PRESS      (1)
#define KEY_FIRST_RELEASE    (2)
#define KEY_SECOND_PRESS     (3)
#define KEY_SECOND_RELEASE   (4)
#define KEY_TRIPLE_PRESS     (5)
#define KEY_TRIPLE_RELEASE   (6)
#define KEY_CHECK_LONG_KEY_RELEASE      (-1)
#define KEY_CHECK_DOUBLE_KEY_RELEASE    (-2)
#define KEY_CHECK_TRIPLE_KEY_RELEASE    (-3)


/**
 *  0x74 on 功能键
 *  自定义控件
 **/
#define ZIB_KEY_POWER 0x74
#define ZIB_KEY_BACK 0x75
#define ZIB_KEY_OK 0x76


struct key_press_info {
    int code;
    int is_press;
    int state;
    struct timeval start_time;
    struct timeval end_time;
};
//key 按钮状态



int ev_init(void);

void ev_exit(void);

int ev_get(struct input_event *ev, int wait_ms);

static void *input_thread(void *cookie);

int key_test();

int32_t getkey(void);


