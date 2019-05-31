cmake_minimum_required(VERSION 3.4.1)

project(multi_test)



include_directories(includes)
include_directories(../factorytest/includes)



set(HEADER_INCLUDE
        includes/main.h
        includes/zib_wifi_bt.h
        includes/commm_util.h
        ../factorytest/includes/type.h
        )

set(TEL_TEST_SRCES
        tel_test/zb_dailtest_demo.cpp
        tel_test/zib_dial_test.cpp
        )

add_executable(
        multi_test
        ${INCLUDES_CODE}
        ${TEL_TEST_SRCES}
        includes/zib_key.h
        includes/zib_bluetooth.h
        includes/wifi_bt_test.h
        zib_wifi_test.c
        multil_utils.c
        bt_test/zib_bluetooth_test.c
        mic_test/mic_test.cpp
        key_app.cpp
        gsensor_test.c
        main.cpp
        bt_test/zib_bluetooth.c
        gps_test/android_gps.c
        gps_test/gps_test.c
)

find_package(Threads)
target_link_libraries(multi_test ${CMAKE_THREAD_LIBS_INIT})
