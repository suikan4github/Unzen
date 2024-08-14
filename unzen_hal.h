#ifndef _UNZEN_HAL_H_
#define _UNZEN_HAL_H_

#include "mbed.h"

namespace unzen 
{
        // Set up I2S peripheral to ready to start.
        // By this HAL, the I2S have to become : 
        // - slave mode
        // - clock must be ready
    void hal_i2s_setup(void);
    
        // configure the pins of I2S and then, wait for WS. 
        // This waiting is important to avoid the delay between TX and RX.
        // The HAL API will wait for the WS changes from left to right then return.
        // The procesure is : 
        // 1. configure WS pin as GPIO
        // 2. wait the WS rising edge
        // 3. configure all pins as I2S
    void hal_i2s_pin_config_and_wait_ws(void);

    
        // Start I2S transfer. Interrupt starts  
    void hal_i2s_start(void);

        // returns the IRQ ID for I2S RX interrupt 
    IRQn_Type hal_get_i2s_irq_id(void);
    
        // returns the IRQ ID for process IRQ. Typically, this is allocated to the reserved IRQ.
    IRQn_Type hal_get_process_irq_id(void);
    
        // The returned value must be compatible with CMSIS NVIC_SetPriority() API. That mean, it is integer like 0, 1, 2...
    unsigned int hal_get_i2s_irq_priority_level(void);

        // The returned value must be compatible with CMSIS NVIC_SetPriority() API. That mean, it is integer like 0, 1, 2...
    unsigned int hal_get_process_irq_priority_level(void);

 
        // reutun the intenger value which tells how much data have to be transfered for each
        // interrupt. For example, if the stereo 32bit data ( total 64 bit ) have to be sent, 
        // have to return 2. 
    unsigned int hal_data_per_sample(void);

        // get data from I2S RX peripheral. Where sample is one audio data. Stereo data is constructed by 2 samples.   
    void hal_get_i2s_rx_data( int & sample);
    
        // put data into I2S TX peripheral. Where sample is one audio data. Stereo data is constructed by 2 samples.   
    void hal_put_i2s_tx_data( int sample );
}


#endif