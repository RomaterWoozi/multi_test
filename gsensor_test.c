//
// Created by wzx on 2019/3/25.
//

// enable or disable local debug


#include <sys/types.h>
#include <hardware/sensors.h>
#include <time.h>
#include <pthread.h>
#include <type.h>
#include "zib_asensor_test.h"
#include "commm_util.h"
#include "main.h"

#define PROJECT_ZIB_GSENSOR_TEST_H
#define SPRD_GSENSOR_DEV "/dev/mc3xxx"
#define MC34XX_IOC_MAGIC     77 //0x1D
#define MC34XX_ACC_IOCTL_SET_DELAY  _IOW(MC34XX_IOC_MAGIC,0,int)
#define MC34XX_ACC_IOCTL_GET_DELAY  _IOR(MC34XX_IOC_MAGIC,1,int)
#define MC34XX_ACC_IOCTL_SET_ENABLE  _IOW(MC34XX_IOC_MAGIC,2,int)
#define MC34XX_ACC_IOCTL_GET_ENABLE  _IOR(MC34XX_IOC_MAGIC,3,int)
#define MC34XX_ACC_IOCTL_CALIBRATION _IOW(MC34XX_IOC_MAGIC,4,int)
#define SPRD_GSENSOR_STEP 0

static int cur_row = 2;
static int x_row, y_row, z_row;
static time_t begin_time,over_time;
static int sensor_id;
static sensors_event_t data;
static int asensor_result=0;
static int x_pass, y_pass, z_pass;
static float x_value, y_value, z_value;

static int asensor_check(float datas)
{
#ifndef HAL_VERSION_1_3
    int ret = -1;
    int divisor = 1000;

    int start_1 = SPRD_ASENSOR_1G-SPRD_ASENSOR_OFFSET;
    int end_1 = SPRD_ASENSOR_1G+SPRD_ASENSOR_OFFSET;
    int start_2 = -SPRD_ASENSOR_1G-SPRD_ASENSOR_OFFSET;
    int end_2 = -SPRD_ASENSOR_1G+SPRD_ASENSOR_OFFSET;

    if(datas > 10000)
        datas = datas/divisor;
    if( ((start_1<datas)&&(datas<end_1))||
        ((start_2<datas)&&(datas<end_2)) ){
        ret = 0;
    }

    return ret;

#else
    int ret = -1;
	float start_1 = SPRD_ASENSOR_1G-SPRD_ASENSOR_OFFSET;
	float start_2 = -SPRD_ASENSOR_1G+SPRD_ASENSOR_OFFSET;

	if( ((start_1<datas) || (start_2>datas)) ){
		ret = 0;
	}

	return ret;

#endif
}

static void *asensor_thread()
{
    int i,n;
    int col = 6;
    char onetime = 1;
    int flush = 0;
    time_t start_time,now_time;
    int type_num = 0;
    static const size_t numEvents = 16;
    sensors_event_t buffer[numEvents];

    x_pass = y_pass = z_pass = 0;
    begin_time=time(NULL);

    type_num = list[sensor_id].type;
            LOGD("activate sensor: %s success!!! type_num = %d.", list[sensor_id].name, type_num);
    start_time=time(NULL);
    do {
        now_time=time(NULL);
                LOGD("mmitest here while!   \n");
        n = device->poll(device, buffer, numEvents);
                LOGD("mmitest here afterpoll\n n = %d",n);
        if (n < 0) {
                    LOGD("mmitest poll() failed\n");
            break;
        }
        for (i = 0; i < n; i++) {
            data = buffer[i];
//#if 0
//			if (data.version != sizeof(sensors_event_t)) {
                    LOGD("mmitestsensor incorrect event version (version=%d, expected=%d)!!",data.version, sizeof(sensors_event_t));
//				break;
//			}
//#endif
            if (type_num == data.type){
                        LOGD("mmitest value=<%5.1f,%5.1f,%5.1f>\n",data.data[0], data.data[1], data.data[2]);
                if(onetime&&data.data[0]&&data.data[1]&&data.data[2]){
                    onetime=0;
                    x_value = data.data[0];
                    y_value = data.data[1];
                    z_value = data.data[2];
                }
                if(x_pass == 0 && asensor_check(data.data[0]) == 0 && x_value != data.data[0] && flush != 0) {
                    x_pass = 1;
                            LOGD("x_pass");
                }

                if(y_pass == 0 && asensor_check(data.data[1]) == 0 && y_value != data.data[1] && flush != 0) {
                    y_pass = 1;
                            LOGD("y_pass");
                }

                if(z_pass == 0 && asensor_check(data.data[2]) == 0 && z_value != data.data[2] && flush != 0) {
                    z_pass = 1;
                            LOGD("z_pass");                }
                usleep(2*1000);
            }else if (SENSOR_TYPE_META_DATA == data.type) {
                flush++;
                        LOGD("data flush completed!");
            }
        }

        if((now_time-start_time)>=ASENSOR_TIMEOUT) break;

    } while (!(x_pass&y_pass&z_pass));

    if(x_pass&y_pass&z_pass)
        asensor_result = RL_PASS;
    else
        asensor_result = RL_FAIL;
    return NULL;
}


int test_asensor_start(void)
{
    pthread_t thread;
    const char *ptr = "Accelerometer";
            LOGD("gsensor start senloaded =  %d",senloaded);
    if(senloaded < 0)
    {
        senloaded = sensor_load();
    }


    if(senloaded < 0){
        sleep(1);
        goto end;
    }else{
        //enable gsensor
        sensor_id = sensor_enable(ptr);
                LOGD("test msensor ID is %d~", sensor_id);
        if(sensor_id < 0){

            goto end;
        }
        pthread_create(&thread, NULL, (void*)asensor_thread, NULL);
        pthread_join(thread, NULL); /* wait "handle key" thread exit. */
        if(RL_PASS == asensor_thread){
                    LOGD("test g_sensor RL_PASS");
        }
        else if(RL_FAIL == asensor_thread){
                    LOGD("test g_sensor RL_FAIL");
        }else{

        }

        sensor_disable(ptr);
    }

    end:
//    save_result(CASE_TEST_GYRSOR,gsensor_result);
    over_time=time(NULL);
            LOGD("mmitest casetime gsensor is %ld s",(over_time-begin_time));
    usleep(500 * 1000);
    return asensor_result;
}


int sprd_gsensor_xyz(int *x, int *y, int *z)
{
    int nwr, ret, fd, err=0;
    char value[20];
    uint32_t ulenable = 0;
    fd = open(SPRD_GSENSOR_DEV, O_RDONLY);
    if(fd < 0)
    {
        printf("open %s fail\r\n", SPRD_GSENSOR_STEP);
        return errno;
    }

    ulenable = 1;
    err = ioctl(fd, MC34XX_ACC_IOCTL_SET_ENABLE, &ulenable);

    ret = read(fd, value, nwr);

    ulenable = 0;
    err = ioctl(fd, MC34XX_ACC_IOCTL_SET_ENABLE, &ulenable);

    close(fd);

    *x = value[0];
    *y = value[1];
    *z = value[2];
    printf("x:%d y:%d z:%d", value[0], value[1], value[2]);

    return ret;
}

int zib_gsensor_test()
{
    int x,y,z;
    int ret=sprd_gsensor_xyz(&x,&y,&z);
    if(ret){
        DBGMSG("get data success x = %, y  =%,  z=%",x,y,z);
    } else{
        DBGMSG("get data fail x = %, y  =%,  z=%",x,y,z);
    }
    return 1;
}