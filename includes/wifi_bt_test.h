//
// Created by wzx on 2019/5/29.
//

#ifndef SAMPLE1_WIFI_BT_TEST_H
#define SAMPLE1_WIFI_BT_TEST_H
#endif //SAMPLE1_WIFI_BT_TEST_H




int sensor_stop();
int get_sensor_name(const char * name );
int getSensorId(const char *sensorname);
int find_input_dev(int mode, const char *event_name);
int enable_sensor();
int sensor_enable(const char *sensorname);
int sensor_disable(const char *sensorname);
int sensor_stop();
int sensor_load();



