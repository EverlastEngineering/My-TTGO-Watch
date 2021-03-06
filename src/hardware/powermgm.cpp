#include "config.h"
#include <TTGO.h>
#include <soc/rtc.h>
#include <WiFi.h>

#include "pmu.h"
#include "bma.h"
#include "powermgm.h"
#include "wifictl.h"
#include "timesync.h"
#include "motor.h"
#include "touch.h"

EventGroupHandle_t powermgm_status = NULL;

/*
 *
 */
void powermgm_setup( TTGOClass *ttgo ) {

    powermgm_status = xEventGroupCreate();
    xEventGroupClearBits( powermgm_status, POWERMGM_STANDBY | POWERMGM_PMU_BUTTON | POWERMGM_PMU_BATTERY );

    pmu_setup( ttgo );
    bma_setup( ttgo );
    wifictl_setup();
    timesync_setup( ttgo );
    touch_setup( ttgo );
}

/*
 *
 */
void powermgm_loop( TTGOClass *ttgo ) {

    // event-tripper pmu-button or pmu-battery state change
    if ( powermgm_get_event( POWERMGM_PMU_BUTTON | POWERMGM_PMU_BATTERY | POWERMGM_BMA_WAKEUP ) ) {        
        // if we are in standby, wake up
        if ( powermgm_get_event( POWERMGM_STANDBY ) ) {
            powermgm_clear_event( POWERMGM_STANDBY );
            ttgo->openBL();
            ttgo->displayWakeup();
            ttgo->rtc->syncToSystem();
            ttgo->startLvglTick();
            lv_disp_trig_activity(NULL);
            if ( bma_get_config( BMA_STEPCOUNTER ) )
                ttgo->bma->enableStepCountInterrupt( true );
            motor_vibe( 1 );
        }
        // if we are nor in stand by, go sleep
        else {
            ttgo->bl->adjust( 0 );
            ttgo->displaySleep();
            ttgo->closeBL();
            if ( powermgm_get_event( POWERMGM_WIFI_ACTIVE ) ) wifictl_off();
            while( powermgm_get_event( POWERMGM_WIFI_ACTIVE | POWERMGM_WIFI_CONNECTED | POWERMGM_WIFI_OFF_REQUEST | POWERMGM_WIFI_ON_REQUEST | POWERMGM_WIFI_SCAN ) ) {}
            ttgo->stopLvglTick();
            if ( bma_get_config( BMA_STEPCOUNTER ) )
                ttgo->bma->enableStepCountInterrupt( false );
            powermgm_set_event( POWERMGM_STANDBY );
            setCpuFrequencyMhz( 10 );
            gpio_wakeup_enable ((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable ((gpio_num_t)BMA423_INT1, GPIO_INTR_HIGH_LEVEL);
            esp_sleep_enable_gpio_wakeup ();
            esp_light_sleep_start();
        }
        // clear event
        powermgm_clear_event( POWERMGM_PMU_BUTTON | POWERMGM_PMU_BATTERY | POWERMGM_BMA_WAKEUP );
    }
    pmu_loop( ttgo );
    bma_loop( ttgo );
}

/*
 *
 */
void powermgm_set_event( EventBits_t bits ) {
    xEventGroupSetBits( powermgm_status, bits );
}

/*
 *
 */
void powermgm_clear_event( EventBits_t bits ) {
    xEventGroupClearBits( powermgm_status, bits );
}

/*
 *
 */
EventBits_t powermgm_get_event( EventBits_t bits ) {
    return( xEventGroupGetBits( powermgm_status ) & bits );
}
