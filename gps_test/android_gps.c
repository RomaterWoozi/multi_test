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



#define LOG_NDEBUG 0

#include <hardware/hardware.h>
#include <utils/Log.h>
#include <string.h>
#include <pthread.h>
#include "zib_gps.h"
#include "commm_util.h"

static const GpsInterface* sGpsInterface = NULL;
static const GpsXtraInterface* sGpsXtraInterface = NULL;
static const AGpsInterface* sAGpsInterface = NULL;
static const GpsNiInterface* sGpsNiInterface = NULL;
static const GpsDebugInterface* sGpsDebugInterface = NULL;
static const AGpsRilInterface* sAGpsRilInterface = NULL;
static const GpsGeofencingInterface* sGpsGeofencingInterface = NULL;


void android_location_GpsLocationProvider_class_init_native(void)
{
	ALOGD("gpslocationprovider class init");
    hw_module_t* module;
	struct gps_device_t* gps_device;
	int err;

    err = hw_get_module(GPS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
    if (err == 0) {
        err = module->methods->open(module, GPS_HARDWARE_MODULE_ID, (hw_device_t**)&gps_device);
        if (err == 0) {
            sGpsInterface = gps_device->get_gps_interface(gps_device);
        }
    }
	ALOGD("gpslocationprovider hw_get_module %d",err);
    if (sGpsInterface) {
        sGpsXtraInterface =
            (const GpsXtraInterface*)sGpsInterface->get_extension(GPS_XTRA_INTERFACE);
        sAGpsInterface =
            (const AGpsInterface*)sGpsInterface->get_extension(AGPS_INTERFACE);
        sGpsNiInterface =
            (const GpsNiInterface*)sGpsInterface->get_extension(GPS_NI_INTERFACE);
        sGpsDebugInterface =
            (const GpsDebugInterface*)sGpsInterface->get_extension(GPS_DEBUG_INTERFACE);
        sAGpsRilInterface =
            (const AGpsRilInterface*)sGpsInterface->get_extension(AGPS_RIL_INTERFACE);
        sGpsGeofencingInterface =
            (const GpsGeofencingInterface*)sGpsInterface->get_extension(GPS_GEOFENCING_INTERFACE);
			
	ALOGD("gpslocationprovider sGpsInterface ");
    }
}

int android_location_GpsLocationProvider_is_supported(void)
{
    return (sGpsInterface != NULL);
}

int android_location_GpsLocationProvider_init(
	GpsCallbacks* Gpscallbacks,
	GpsXtraCallbacks* GpsXtracallbacks,
	AGpsCallbacks* AGpscallbacks,
	GpsNiCallbacks *GpsNicallbacks,
	AGpsRilCallbacks* AGpsRilcallbacks,
	GpsGeofenceCallbacks* GpsGeofencecallbacks
	)
{
	ALOGD("gpsLocationprovider init");
    // fail if the main interface fails to initialize
    if (!sGpsInterface || sGpsInterface->init(Gpscallbacks) != 0)
        return 0;

    // if XTRA initialization fails we will disable it by sGpsXtraInterface to NULL,
    // but continue to allow the rest of the GPS interface to work.
    if (sGpsXtraInterface && sGpsXtraInterface->init(GpsXtracallbacks) != 0)
        sGpsXtraInterface = NULL;
    if (sAGpsInterface&&AGpscallbacks)
        sAGpsInterface->init(AGpscallbacks);
    if (sGpsNiInterface&&GpsNicallbacks)
        sGpsNiInterface->init(GpsNicallbacks);
    if (sAGpsRilInterface&&AGpsRilcallbacks)
        sAGpsRilInterface->init(AGpsRilcallbacks);
    if (sGpsGeofencingInterface&&GpsGeofencecallbacks)
        sGpsGeofencingInterface->init(GpsGeofencecallbacks);

    return 1;
}

void android_location_GpsLocationProvider_cleanup(void)
{
    if (sGpsInterface)
        sGpsInterface->cleanup();
}

int android_location_GpsLocationProvider_set_position_mode(
        int mode, int recurrence, int min_interval, int preferred_accuracy, int preferred_time)
{
    if (sGpsInterface)
        return (sGpsInterface->set_position_mode(mode, recurrence, min_interval, preferred_accuracy,
                preferred_time) == 0);
    else
        return 0;
}

int android_location_GpsLocationProvider_start(void)
{
    if (sGpsInterface)
        return (sGpsInterface->start() == 0);
    else
        return 0;
}

int android_location_GpsLocationProvider_stop(void)
{
    if (sGpsInterface)
        return (sGpsInterface->stop() == 0);
    else
        return 0;
}

void android_location_GpsLocationProvider_delete_aiding_data(int flags)
{
    if (sGpsInterface) {
        ALOGD("delete aiding data %x\n", flags);
        sGpsInterface->delete_aiding_data(flags);
    }
}


void android_location_GpsLocationProvider_agps_set_reference_location_cellid(
		int type, int mcc, int mnc, int lac, int cid)
{
    AGpsRefLocation location;

	ALOGD("pgslocation cellid");

    if (!sAGpsRilInterface) {
        ALOGE("no AGPS RIL interface in agps_set_reference_location_cellid");
        return;
    }

    switch(type) {
        case AGPS_REF_LOCATION_TYPE_GSM_CELLID:
        case AGPS_REF_LOCATION_TYPE_UMTS_CELLID:
            location.type = type;
            location.u.cellID.mcc = mcc;
            location.u.cellID.mnc = mnc;
            location.u.cellID.lac = lac;
            location.u.cellID.cid = cid;
            break;
        default:
            ALOGE("Neither a GSM nor a UMTS cellid (%s:%d).",__FUNCTION__,__LINE__);
            return;
            break;
    }
	
    sAGpsRilInterface->set_ref_location(&location, sizeof(location));
}

void android_location_GpsLocationProvider_agps_send_ni_message(unsigned char *ni_msg, int size)
{
    size_t sz;

    if (!sAGpsRilInterface) {
        ALOGE("no AGPS RIL interface in send_ni_message");
        return;
    }
	
    if (size < 0)
        return;
	
    sz = (size_t)size;
	
    sAGpsRilInterface->ni_message(ni_msg,sz);
}

void android_location_GpsLocationProvider_agps_set_id(int type,const char *setid)
{
    if (!sAGpsRilInterface) {
        ALOGE("no AGPS RIL interface in agps_set_id");
        return;
    }
	
    sAGpsRilInterface->set_set_id(type, setid);
}


void android_location_GpsLocationProvider_inject_time(long time, long timeReference, int uncertainty)
{
    if (sGpsInterface)
        sGpsInterface->inject_time(time, timeReference, uncertainty);
}

void android_location_GpsLocationProvider_inject_location(
        double latitude, double longitude, float accuracy)
{
    if (sGpsInterface)
        sGpsInterface->inject_location(latitude, longitude, accuracy);
}

int android_location_GpsLocationProvider_supports_xtra(void)
{
    return (sGpsXtraInterface != NULL);
}

void android_location_GpsLocationProvider_inject_xtra_data(char* bytes, int length)
{
    if (!sGpsXtraInterface) {
        ALOGE("no XTRA interface in inject_xtra_data");
        return;
    }

    sGpsXtraInterface->inject_xtra_data(bytes, length);
}

void android_location_GpsLocationProvider_agps_data_conn_open(const char *apnStr)
{
    if (!sAGpsInterface) {
        ALOGE("no AGPS interface in agps_data_conn_open");
        return;
    }
    if (apnStr == NULL) {
       ALOGE("apnStr  in NULL ");
        return;
    }
    sAGpsInterface->data_conn_open(apnStr);
}

void android_location_GpsLocationProvider_agps_data_conn_closed(void)
{
    if (!sAGpsInterface) {
        ALOGE("no AGPS interface in agps_data_conn_closed");
        return;
    }
    sAGpsInterface->data_conn_closed();
}

void android_location_GpsLocationProvider_agps_data_conn_failed(void)
{
    if (!sAGpsInterface) {
        ALOGE("no AGPS interface in agps_data_conn_failed");
        return;
    }
    sAGpsInterface->data_conn_failed();
}

void android_location_GpsLocationProvider_set_agps_server(int type, const char *c_hostname, int port)
{
    if (!sAGpsInterface) {
        ALOGE("no AGPS interface in set_agps_server");
        return;
    }
	
    sAGpsInterface->set_server(type, c_hostname, port);
}

void android_location_GpsLocationProvider_send_ni_response(int notifId, int response)
{
    if (!sGpsNiInterface) {
        ALOGE("no NI interface in send_ni_response");
        return;
    }
	
    sGpsNiInterface->respond(notifId, response);
}

char *android_location_GpsLocationProvider_get_internal_state(void)
{
    char *result = NULL;
	size_t length = 0;
	size_t maxLength = 2047;
    char buffer[maxLength+1];
	
    if (sGpsDebugInterface) {
        length = sGpsDebugInterface->get_internal_state(buffer, maxLength);
        if (length > maxLength) length = maxLength;
        buffer[length] = 0;
        memcpy(result,buffer,length);
    }
    return result;
}

void android_location_GpsLocationProvider_update_network_state(
        int connected, int type, int roaming, int available, const char *extraInfo, const char * apn)
{

    if (sAGpsRilInterface && sAGpsRilInterface->update_network_state) {
        if (extraInfo) {
            sAGpsRilInterface->update_network_state(connected, type, roaming, extraInfo);
        } else {
            sAGpsRilInterface->update_network_state(connected, type, roaming, NULL);
        }

        // update_network_availability starttest was not included in original AGpsRilInterface
        if (sAGpsRilInterface->size >= sizeof(AGpsRilInterface)
                && sAGpsRilInterface->update_network_availability) {
            sAGpsRilInterface->update_network_availability(available, apn);
        }
    }
}

int android_location_GpsLocationProvider_is_geofence_supported(void) 
{
    if (sGpsGeofencingInterface != NULL) {
        return 1;
    }
    return 0;
}

int android_location_GpsLocationProvider_add_geofence(
        int geofence_id, double latitude, double longitude, double radius,
        int last_transition, int monitor_transition, int notification_responsiveness,int unknown_timer) 
 {
        
    if (sGpsGeofencingInterface != NULL) {
        sGpsGeofencingInterface->add_geofence_area(geofence_id, latitude, longitude,
                radius, last_transition, monitor_transition, notification_responsiveness,
                unknown_timer);
        return 1;
    } else {
        ALOGE("Geofence interface not available");
    }
    return 0;
}

int android_location_GpsLocationProvider_remove_geofence(int geofence_id)
{
    if (sGpsGeofencingInterface != NULL) {
        sGpsGeofencingInterface->remove_geofence_area(geofence_id);
        return 1;
    } else {
        ALOGE("Geofence interface not available");
    }
    return 0;
}

int android_location_GpsLocationProvider_pause_geofence(int geofence_id) 
{
    if (sGpsGeofencingInterface != NULL) {
        sGpsGeofencingInterface->pause_geofence(geofence_id);
        return 1;
    } else {
        ALOGE("Geofence interface not available");
    }
    return 0;
}

char android_location_GpsLocationProvider_resume_geofence(int geofence_id, int monitor_transition) 
{
    if (sGpsGeofencingInterface != NULL) {
        sGpsGeofencingInterface->resume_geofence(geofence_id, monitor_transition);
        return 1;
    } else {
        ALOGE("Geofence interface not available");
    }
    return 0;
}

