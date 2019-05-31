#include "zib_bluetooth.h"

extern int main_done;

/*******************************************************************************
 ** Console commands
 *******************************************************************************/
int zb_bluetooth_test()
{

    config_permissions();
    ZIB_DBG("****Bluetooth Test app starting****");
    if (HAL_load() < 0 ) {
        perror("HAL failed to initialize, exit\n");
        unlink(PID_FILE);
        exit(0);
    }
    setup_test_env();
    /* Automatically perform the init */
    bdt_init();
    gatt_init();

//    fflush(stdout);
    do_enable(NULL);
    sleep(2);
//    fflush(stdout);
    do_start_discovery();
    while (1){
        usleep(100*1000);
        if(get_device_find_status()){
            ZIB_DBG("bluetooth has found device");
            break;
        }
        if(get_discovery_state_changed_status()==BT_DISCOVERY_STOPPED){
            ZIB_DBG("found devices  %s",get_device_find_status()?" success":"failed");
            break;
        }
    }
    HAL_unload();

    ZIB_DBG("****Bluetooth Test app terminating****");

    return get_device_find_status();
}