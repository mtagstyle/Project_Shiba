/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
* @defgroup nrf_qdec_example main.c
* @{
* @ingroup nrf_qdec_example
* @brief QDEC example application main file.
*
* This is an example quadrature decoder application.
* The example requires that QDEC A,B inputs are connected with QENC A,B outputs and
* QDEC LED output is connected with QDEC LED input.
*
* Example uses software quadrature encoder simulator QENC.
* Quadrature encoder simulator uses one channel of GPIOTE module.
* The state of the encoder changes on the inactive edge of the sampling clock generated by LED output.
*
* In a infinite loop QENC produces variable number of positive and negative pulses
* synchronously with bursts of clock impulses generated by QDEC at LED output.
* The pulses are counted by QDEC operating in a REPORT mode.
* Pulses counted by QDEC are compared with pulses generated by QENC.
* The tests stops if there is a difference between number of pulses counted and generated.
*
*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrf_drv_qdec.h"
#include "nrf_error.h"
#include "app_error.h"
#include "qenc_sim.h"

static volatile bool m_report_ready_flag = false;
static volatile bool m_first_report_flag = true;
static volatile uint32_t m_accdblread;
static volatile int32_t m_accread;

static void qdec_event_handler(nrf_drv_qdec_event_t event)
{
    if (event.type == NRF_QDEC_EVENT_REPORTRDY)
    {
        m_accdblread = event.data.report.accdbl;
        m_accread = event.data.report.acc;
        m_report_ready_flag = true;
        nrf_drv_qdec_disable();
    }
}

void check_report(int32_t expected)
{
    // only first run is specific...
    if ((expected > 0) && m_first_report_flag)
    {
       expected = expected-1;
    }
    else if ((expected < 0) && m_first_report_flag)
    {
       expected = expected+1;
    }
    APP_ERROR_CHECK_BOOL( m_accdblread == 0);
    APP_ERROR_CHECK_BOOL( m_accread == expected );
    m_first_report_flag = false;  // clear silently after first run

}

int main(void)
{
    uint32_t err_code;
    bool forever = true;
    uint32_t  number_of_pulses;
    uint32_t  min_number_of_pulses = 2;
    uint32_t  max_number_of_pulses = 0;
    int32_t   pulses;
    int32_t   sign = 1;
    
    err_code = nrf_drv_qdec_init(NULL, qdec_event_handler);
    APP_ERROR_CHECK(err_code);
    
    max_number_of_pulses = nrf_qdec_reportper_to_value(QDEC_CONFIG_REPORTPER);

    // initialize quadrature encoder simulator
    qenc_init((nrf_qdec_ledpol_t)nrf_qdec_ledpol_get());

    while (forever)
    {
      // change a number and sign of pulses produced by simulator in a loop
      for (number_of_pulses=min_number_of_pulses; number_of_pulses<= max_number_of_pulses; number_of_pulses++ )
      {
        pulses = sign*number_of_pulses;      // pulses have sign
        qenc_pulse_count_set(pulses);        // set pulses to be produced by encoder
        nrf_drv_qdec_enable();               // start burst sampling clock, clock will be stopped by REPORTRDY event
        while (! m_report_ready_flag)                         // wait for a report
        {
          __WFE();
        }
        m_report_ready_flag = false;
        check_report(pulses);                 // check if pulse count is as expected, assert otherwise
      }
      min_number_of_pulses = 1;               // only first run is specific, for 1 there would be no call back...
      sign = -sign;                           // change sign of pulses in a loop
    }
    return -1;                                // this should never happen
}

/** @} */
