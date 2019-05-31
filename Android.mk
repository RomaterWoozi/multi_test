LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
      hardware/libhardware/include/hardware \
      $(TOP)/frameworks/av/include/media \
	  $(TOP)/vendor/zib/factorytest/includes \
	  system/bluetooth/bluedroid/include \
	  external/bluetooth/bluedroid/btif/include \
      external/bluetooth/bluedroid/gki/ulinux \
      external/bluetooth/bluedroid/stack/include \
      external/bluetooth/bluedroid/stack/btm \
	  $(TOP)/vendor/zib/common  \
	  $(LOCAL_PATH)/includes  \
	  $(LOCAL_PATH)/tel_test


LOCAL_SRC_FILES:=  \
                  zib_wifi_test.c  \
                  main.cpp   \
                  gsensor_test.c  \
                  multil_utils.c \
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
						 libmedia \
						 libhardware \
                         libhardware_legacy \
						 libzb_media_util

LOCAL_MODULE_TAGS := optional
LOCAL_LDLIBS += -lpthread -ldl -llog -lreadline
include $(BUILD_EXECUTABLE)

