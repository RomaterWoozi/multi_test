LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	  $(TOP)/vendor/zib/factorytest/includes \
	  $(TOP)/vendor/zib/common  \
	  $(LOCAL_PATH)/includes  \
	  $(LOCAL_PATH)/tel_test


LOCAL_SRC_FILES:=  \
                  ls_wifi_test.c  \
                  main.cpp   \
                  gsensor_test.c  \
                  tel_test/AppClient.cpp  \
                  tel_test/ls_telephony_util.cpp  \
                  tel_test/zb_dailtest_demo.cpp \
                  tel_test/zib_dial_test.cpp \
                  mic_test/mic_test.cpp \
                  gps_test/android_gps.c \
                  gps_test/gps_test.c  \
                  bt_test/zib_bluetooth.c \
                  bt_test/zib_bluetooth_test.c




LOCAL_MODULE := multi_test

LOCAL_SHARED_LIBRARIES:= libcutils \
                         libbinder \
                         libutils  \
						 libhardware \
						 libzibcommon \
						 libnetutils \
						 libhardware_legacy \
						 libzb_media_util

LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

