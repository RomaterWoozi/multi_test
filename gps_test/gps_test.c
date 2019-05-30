





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/Log.h>
#include <hardware_legacy/power.h>
#include "zib_gps.h"
#include "commm_util.h"
#include "main.h"

// temporary storage for GPS callbacks
struct s_gps_test {
    int sSVNum;
    int last_sSVNum;
    char sSvSnr[40];  //GPS信号也是有强有弱，GPS的信号强弱用SNR数值表示，从0到99，数值越小，信号越弱，数值越大，信号越强
    int sTimeout;
    time_t gOpenTime;
    unsigned int gps_is_enable;
    factory_gps_result_type_t gps_result_type;
};

struct s_gps_test gps_test = {
        .sSVNum =0,
        .last_sSVNum=0,
        .sSvSnr={0},
        .sTimeout =0,
        .gps_is_enable=0,
};


static GpsSvStatus sGpsSvStatus;
static const char *sNmeaString;
static int sNmeaStringLength;

#define WAKE_LOCK_NAME  "GPS"


static void location_callback(GpsLocation *location) {
    int flags = 0;
    double latitude = 0;
    double longitude = 0;
    double altitude = 0;
    float speed = 0;
    float bearing = 0;
    float accuracy = 0;
    long timestamp = 0;
    if (location != NULL) {
        flags = location->flags;
        latitude = location->latitude;
        longitude = location->longitude;
        altitude = location->altitude;
        speed = location->speed;
        bearing = location->bearing;
        accuracy = location->accuracy;
        timestamp = location->timestamp;
    }

    ALOGD("flags=%d latitude=%lf longitude=%lf altitude=%lf", flags, latitude, longitude, altitude);
    ALOGD("speed=%f bearing=%f, accuracy=%f ", speed, bearing, accuracy);
    ALOGD("timestamp=%ld ", timestamp);

    printf("flags=%d latitude=%lf longitude=%lf altitude=%lf \n", flags, latitude, longitude, altitude);
    printf("speed=%f bearing=%f, accuracy=%f \n", speed, bearing, accuracy);
    printf("timestamp=%ld \n", timestamp);
}

/*
 	GPS status unknown. 
#define GPS_STATUS_NONE             0
 	GPS has begun navigating. 
#define GPS_STATUS_SESSION_BEGIN    1
    GPS has stopped navigating. 
#define GPS_STATUS_SESSION_END      2
   GPS has powered on but is not navigating. 
#define GPS_STATUS_ENGINE_ON        3
   GPS is powered off. 
#define GPS_STATUS_ENGINE_OFF       4

*/
#define GPS_TEST_TIME_OUT         (100) // s120


// GPS
static void status_callback(GpsStatus *status) {
    ALOGD("status->status=%d ", status->status);
    printf("status->status=%d \n", status->status);
}

static void sv_status_callback(GpsSvStatus *sv_status) {
    printf("sv_status_callback().. \n");
    memcpy(&sGpsSvStatus, sv_status, sizeof(sGpsSvStatus));
}

static void nmea_callback(GpsUtcTime timestamp, const char *nmea, int length) {
    // The Java code will call back to read these values
    // We do this to avoid creating unnecessary String objects
    sNmeaString = nmea;
    sNmeaStringLength = length;
    ALOGD("sNmeaString=%s sNmeaStringLength=%d", sNmeaString, sNmeaStringLength);
    printf("sNmeaString=%s sNmeaStringLength=%d \n", sNmeaString, sNmeaStringLength);
    printf("timestamp=%lld \n", timestamp);
}

static void set_capabilities_callback(uint32_t capabilities) {
    printf("set_capabilities_callback: %du\n", capabilities);
    ALOGD("set_capabilities_callback: %du\n", capabilities);
}

static void acquire_wakelock_callback() {
    printf("acquire_wakelock_callback() \n");
    acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKE_LOCK_NAME);
}

static void release_wakelock_callback() {
    printf("release_wakelock_callback() \n");
    release_wake_lock(WAKE_LOCK_NAME);
}

static void request_utc_time_callback() {
    printf("request_utc_time_callback() \n");
}

static pthread_t create_thread_callback(const char *name, void (*start)(void *), void *arg) {
    pthread_t thread_id = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread_id, &attr, (void *) start, arg);
    pthread_setname_np(thread_id, name);
    return thread_id;
}

GpsCallbacks sGpsCallbacks = {
        sizeof(GpsCallbacks),
        location_callback,
        status_callback,
        sv_status_callback,
        nmea_callback,
        set_capabilities_callback,
        acquire_wakelock_callback,
        release_wakelock_callback,
        create_thread_callback,
        request_utc_time_callback,
};

static void xtra_download_request_callback(void) {
    ALOGD("xtra_download_request_callback()");
    printf("xtra_download_request_callback() \n");
}

GpsXtraCallbacks sGpsXtraCallbacks = {
        xtra_download_request_callback,
        create_thread_callback,
};

static void zib_agps_status_callback(AGpsStatus *agps_status) {
    uint32_t ipaddr;
    // ipaddr field was not included in original AGpsStatus
    if (agps_status->size >= sizeof(AGpsStatus))
        ipaddr = agps_status->ipaddr;
    else
        ipaddr = 0xFFFFFFFF;

    ALOGD("agps_status->type=%d agps_status->status=%d,ipaddr=0x%x", agps_status->type, agps_status->status, ipaddr);
    printf("agps_status->type=%d agps_status->status=%d,ipaddr=0x%x \n", agps_status->type, agps_status->status,
           ipaddr);
}

AGpsCallbacks sAGpsCallbacks = {
        zib_agps_status_callback,
        create_thread_callback,
};

static void zib_gps_ni_notify_callback(GpsNiNotification *notification) {
    ALOGD("gps_ni_notify_callback\n");

    ALOGD("notification->notification_id=%d \n", notification->notification_id);
    ALOGD("notification->ni_type=%d \n", notification->ni_type);
    ALOGD("notification->notify_flags=%d \n", notification->notify_flags);
    ALOGD("notification->timeout=%d \n", notification->timeout);
    ALOGD("notification->default_response=%d \n", notification->default_response);
    ALOGD("notification->requestor_id=%s \n", notification->requestor_id);
    ALOGD("notification->text=%s \n", notification->text);
    ALOGD("notification->requestor_id_encoding=%d \n", notification->requestor_id_encoding);
    ALOGD("notification->text_encoding=%d \n", notification->text_encoding);
    ALOGD("notification->text_encoding=%s \n", notification->extras);


    printf("notification->notification_id=%d \n", notification->notification_id);
    printf("notification->ni_type=%d \n", notification->ni_type);
    printf("notification->notify_flags=%d \n", notification->notify_flags);
    printf("notification->timeout=%d \n", notification->timeout);
    printf("notification->default_response=%d \n", notification->default_response);
    printf("notification->requestor_id=%s \n", notification->requestor_id);
    printf("notification->text=%s \n", notification->text);
    printf("notification->requestor_id_encoding=%d \n", notification->requestor_id_encoding);
    printf("notification->text_encoding=%d \n", notification->text_encoding);
    printf("notification->text_encoding=%s \n", notification->extras);
}

GpsNiCallbacks sGpsNiCallbacks = {
        zib_gps_ni_notify_callback,
        create_thread_callback,
};

static void agps_request_set_id(uint32_t flags) {
    ALOGD("%s() flags=%d \n", __FUNCTION__, flags);
    printf("%s() flags=%d \n", __FUNCTION__, flags);
}

static void agps_request_ref_location(uint32_t flags) {
    ALOGD("%s() flags=%d \n", __FUNCTION__, flags);
    printf("%s() flags=%d \n", __FUNCTION__, flags);
}

AGpsRilCallbacks sAGpsRilCallbacks = {
        agps_request_set_id,
        agps_request_ref_location,
        create_thread_callback,
};

static void zib_gps_geofence_transition_callback(int32_t geofence_id, GpsLocation *location,
                                                 int32_t transition, GpsUtcTime utc_timestamp) {
    int flags = 0;
    double latitude = 0;
    double longitude = 0;
    double altitude = 0;
    float speed = 0;
    float bearing = 0;
    float accuracy = 0;
    long timestamp = 0;
    if (location != NULL) {
        flags = location->flags;
        latitude = location->latitude;
        longitude = location->longitude;
        altitude = location->altitude;
        speed = location->speed;
        bearing = location->bearing;
        accuracy = location->accuracy;
        timestamp = location->timestamp;
    }

    ALOGD("%s() geofence_id =%d ", __FUNCTION__, geofence_id);
    ALOGD("flags=%d latitude=%lf longitude=%lf altitude=%lf", flags, latitude, longitude, altitude);
    ALOGD("speed=%f bearing=%f, accuracy=%f ", speed, bearing, accuracy);
    ALOGD("timestamp=%ld ", timestamp);
    ALOGD("utc_timestamp=%lld ", utc_timestamp);
    ALOGD("transition=%d ", transition);

    printf("%s() geofence_id =%d \n", __FUNCTION__, geofence_id);
    printf("flags=%d latitude=%lf longitude=%lf altitude=%lf \n", flags, latitude, longitude, altitude);
    printf("speed=%f bearing=%f, accuracy=%f  \n", speed, bearing, accuracy);
    printf("timestamp=%ld \n", timestamp);
    printf("utc_timestamp=%lld \n", utc_timestamp);
    printf("transition=%d \n", transition);

};

static void zib_gps_geofence_status_callback(int32_t status, GpsLocation *location) {
    int flags = 0;
    double latitude = 0;
    double longitude = 0;
    double altitude = 0;
    float speed = 0;
    float bearing = 0;
    float accuracy = 0;
    long timestamp = 0;
    if (location != NULL) {
        flags = location->flags;
        latitude = location->latitude;
        longitude = location->longitude;
        altitude = location->altitude;
        speed = location->speed;
        bearing = location->bearing;
        accuracy = location->accuracy;
        timestamp = location->timestamp;
    }
    ALOGD("%s() status=%d", __FUNCTION__, status);
    ALOGD("flags=%d latitude=%lf longitude=%lf altitude=%lf", flags, latitude, longitude, altitude);
    ALOGD("speed=%f bearing=%f, accuracy=%f ", speed, bearing, accuracy);
    ALOGD("timestamp=%ld ", timestamp);


    printf("%s() status=%d \n", __FUNCTION__, status);
    printf("flags=%d latitude=%lf longitude=%lf altitude=%lf \n", flags, latitude, longitude, altitude);
    printf("speed=%f bearing=%f, accuracy=%f \n", speed, bearing, accuracy);
    printf("timestamp=%ld \n", timestamp);
};

static void zib_gps_geofence_add_callback(int32_t geofence_id, int32_t status) {
    if (status != GPS_GEOFENCE_OPERATION_SUCCESS) {
        DBGMSG("Error in geofence_add_callback: %d\n", status);
    }

    ALOGD("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
    printf("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
};

static void zib_gps_geofence_remove_callback(int32_t geofence_id, int32_t status) {
    if (status != GPS_GEOFENCE_OPERATION_SUCCESS) {
        DBGMSG("Error in geofence_remove_callback: %d\n", status);
    }

    ALOGD("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
    printf("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
};

static void zib_gps_geofence_resume_callback(int32_t geofence_id, int32_t status) {

    if (status != GPS_GEOFENCE_OPERATION_SUCCESS) {
        DBGMSG("Error in geofence_resume_callback: %d\n", status);
    }

    ALOGD("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
    printf("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
};

static void zib_gps_geofence_pause_callback(int32_t geofence_id, int32_t status) {
    if (status != GPS_GEOFENCE_OPERATION_SUCCESS) {
        DBGMSG("Error in geofence_pause_callback: %d\n", status);
    }

    ALOGD("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
    printf("%s geofence_id =%d status=%d \n", __FUNCTION__, geofence_id, status);
};

GpsGeofenceCallbacks sGpsGeofenceCallbacks = {
        zib_gps_geofence_transition_callback,
        zib_gps_geofence_status_callback,
        zib_gps_geofence_add_callback,
        zib_gps_geofence_remove_callback,
        zib_gps_geofence_pause_callback,
        zib_gps_geofence_resume_callback,
        create_thread_callback,
};


static int gps_read_nmea(char *nmea, int buffer_size) {
    int length = sNmeaStringLength;

    if (length > buffer_size)
        length = buffer_size;
    memcpy(nmea, sNmeaString, length);

    return length;
}

static void *gps_processthread_show(void *arg) {
    int i;
    int snr_count = 0;

    ALOGD("gps_processthread_show");
    time_t gOpenTime,now_time;
    gOpenTime = time(NULL);
    int sTimeout = GPS_TEST_TIME_OUT;
    sGpsSvStatus.num_svs = 0;
    printf("%s %d \n", __func__, __LINE__);

    while (1) {
        //卫星数量大于4颗
        now_time = time(NULL);
        if (sGpsSvStatus.num_svs > 4) {
            ALOGD("sGpsSvStatus.num_svs =%d \n", sGpsSvStatus.num_svs);
            for (i = 0; i < sGpsSvStatus.num_svs; i++) {
                ALOGD("prn =%d \n", sGpsSvStatus.sv_list[i].prn);
                ALOGD("snr =%f \n", sGpsSvStatus.sv_list[i].snr);
                ALOGD("elevation =%f \n", sGpsSvStatus.sv_list[i].elevation);
                ALOGD("azimuth =%f \n", sGpsSvStatus.sv_list[i].azimuth);

                printf("prn =%d \n", sGpsSvStatus.sv_list[i].prn);
                printf("snr =%f \n", sGpsSvStatus.sv_list[i].snr);
                printf("elevation =%f \n", sGpsSvStatus.sv_list[i].elevation);
                printf("azimuth =%f \n", sGpsSvStatus.sv_list[i].azimuth);
                //设置SNR值，只有当获取到的卫星中有一个SNR值大于35就说明GPS测试成功
                if (sGpsSvStatus.sv_list[i].snr >= 35) {
                    snr_count++;
                }
            }
            if (snr_count > 0) {
                set_gps_result(GPS_TEST_SUCCESS);
                DBGMSG(" GPS Test PASS Catch the Star");
                sleep(4);
                break;
            }

        }


        if (now_time-gOpenTime>GPS_TEST_TIME_OUT) {
            // Timeout to catch the star, gps_test test failed
            set_gps_result(GPS_TEST_FAIL);
            DBGMSG("mmitest GPS Test FAILED");
            sleep(1);
            break;
        } else {
            set_gps_result(GPS_TEST_TESTING);
            DBGMSG("mmitest GPS Testing ......");
            sleep(1);
            sTimeout--;
            gps_test.sTimeout--;
        }

    }

    FUN_EXIT
    return NULL;
}


void gps_close(void) {
    ALOGD("gps_close");
    int err;
    err = android_location_GpsLocationProvider_is_supported();
    if (!err) {
        return;
    }

    android_location_GpsLocationProvider_stop();
    android_location_GpsLocationProvider_cleanup();

    printf(" gps_close .........\n");
    exit(0);
}

void set_gps_result(factory_gps_result_type_t result) {
    gps_test.gps_result_type = result;
}


/*
	return :
	    0:  GPS测试失败
	    1:  GPS测试成功
*/
int test_gps_start()
{
    int err = 0;
    pthread_t gps_t;

    ALOGD("start gpsTest");
    //remove("data/gps_outdirfile.txt");
    //system("rm /data/gps_test/log/*");
    signal(SIGINT, gps_close);
    gps_test.gOpenTime = time(NULL);

    android_location_GpsLocationProvider_class_init_native();

    err = android_location_GpsLocationProvider_is_supported();
    if (!err) {
        DBGMSG(" not support gps_test\n");
        return -1;
    }

    ALOGD("start gpsTest %d\n", err);

    err = android_location_GpsLocationProvider_init(
            &sGpsCallbacks,  /* GpsCallbacks */
            &sGpsXtraCallbacks, /* GpsXtraCallbacks */
            &sAGpsCallbacks,  /* AGpsCallbacks */
            &sGpsNiCallbacks, /* GpsNiCallbacks */
            &sAGpsRilCallbacks, /* AGpsRilCallbacks */
            &sGpsGeofenceCallbacks /* GpsGeofenceCallbacks */
    );

    if (!err) {
        DBGMSG(" gps_test init fail err =%d\n", err);
        printf(" gps_test init fail err =%d\n", err);
        return 0;
    }
    printf("%s %d \n", __func__, __LINE__);
    err = android_location_GpsLocationProvider_start();
    if (!err) {
        DBGMSG(" gps_test start fail err =%d\n", err);
        printf(" gps_test start fail err =%d\n", err);
        return -1;
    }
    pthread_create(&gps_t, NULL, (void *) gps_processthread_show, NULL);
    pthread_join(gps_t, NULL);
    DBGMSG("gps_processthread_show end");
    gps_close();
    return 0;
}


int create_dir(const char *sPathName) {
    char DirName[256];
    strcpy(DirName, sPathName);
    int i, len = strlen(DirName);
    if (DirName[len - 1] != '/')
        strcat(DirName, "/");
    len = strlen(DirName);
    for (i = 1; i < len; i++) {
        if (DirName[i] == '/') {
            DirName[i] = 0;
            if (access(DirName, NULL) != 0) {
                if (mkdir(DirName, 0755) == -1) {
                    perror("mkdir   error");
                    return -1;
                }
            }
            DirName[i] = '/';
        }
    }

    return 0;
}

int gps_Open(void) {
    struct gps_device_t *device;
    struct hw_module_t *module;
    int err, ret;

    if (gps_test.gps_is_enable == 1) {
                LOGD("Gps is already enabled");
        return 0;
    }

    remove("data/gps_outdirfile.txt");
    if (access("/data/gps_test/log/", 0) == -1) {
        int status = create_dir("/data/gps_test/log/");
        if (status==0) {
            DBGMSG("create data/gps_test/log success!");
        } else {
            DBGMSG("create data/gps_test/log failed!");
        }
    }
    system("rm /data/gps_test/log/*");

    android_location_GpsLocationProvider_class_init_native();

    err = android_location_GpsLocationProvider_is_supported();
    if (!err) {
        DBGMSG(" not support gps_test\n");
        return -1;
    }

    ALOGD("start gpsTest %d\n", err);

    gps_test.gps_is_enable = 1;
    DBGMSG("mmitest gOpenTime=%ld", gps_test.gOpenTime);
    gps_test.sSVNum = 0;
    return 0;
}




