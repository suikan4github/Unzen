#include "unzen_hal.h"

// #define DEBUGSAI
namespace unzen 
{
        // for timing control.     
    volatile unsigned int dummy;
    
        // Set up I2S peripheral to ready to start.
        // By this HAL, the I2S have to become : 
        // - slave mode
        // - clock must be ready
    void hal_i2s_setup(void)
    {
            //      STM32F746ZG SAI1 Block A :RX : Slave to the external BCLK/WS
            //      STM32F746ZG SAI1 Block B :TX : Sync with Block A. 
            //      See stm32f746xx.h source here : https://developer.mbed.org/teams/Rigado/code/mbed-src-bmd-200/docs/255afbe6270c/stm32f746xx_8h_source.html
            //      This implementation kills SPI6 interrupt
            
                // Setup PLL
                // RCC_PLLCFGR : Has PLLM ( Division Factor M )
                // RCC_CIR : PLL clock interrupt register
                // RCC_CFGR

                // RCC_DKCFGR1 : RCC Dedicated clocks configuration register
                // clear the relevant field
        RCC->DCKCFGR1 &= ~ (
            3 << 20 |       // SAI1 SEL
            31 << 8 );      // PLLSAIDIVQ
                // set the value
        RCC->DCKCFGR1 |= 
            0 << 20 |       // SAI1 SEL   : 0, PLLSAI
            0 << 8 ;        // PLLSAIDIVQ : 0, div by 1
                
                // RCC_PLLSAICFGR : PLLSAI configuration register. mbed set the pre-devider as 8. So, PLL input is 1MHz
                // The VCO have to be more than 100Mhaz. So, set it 192MHz.
        RCC->PLLSAICFGR = 
            4 << 28 |   // PLLSAIR : 4 is dividing by 4 for LCD clock ( 48Mhz )
            4 << 24 |   // PLLSAIQ : 4 is dividing by 4 for SAI clock ( 48Mhz )
            1 << 16 |   // PLLSAIR : 1 is dividing by 4 for USB clock ( 48Mhz )
            192 << 6 ;   // PLLSAIN : 192 is dividing by 192 for vco freq ( 192Mhz )

                // RCC_CR : Starting/Status of PLLSAI
        RCC->CR |= (1 << 28);     // PLL SAI On
                // wait while PLL SAI is not ready
        while ( !( RCC->CR & ( 1<<29 ) ) )
            ;


                // RCC_APB2ENR : APB2 peripherals clock enable register
        RCC->APB2ENR |= ( 1 << 22);     // SAI1 enable
                    
                // RCC_AHB1ENR : AHB1 peripherals clock enable register
        RCC->AHB1ENR |= 1<<4;           // GPIOE enable
                    
                // Control the stability timing. The STM32F746 reference manual requires
                // to wait 2 peripheral cylecs, after enabling its clock. 
                // See chapter 5.2.12
                // Two guarantee the 2 peripheral cyucles, reading status register twice.
        dummy = RCC->CR;
        dummy = RCC->CR;
                    
                // RCC_APB2RSTR : APB2 peripherals reset register
        RCC->APB2RSTR |= ( 1 << 22);     // SAI1 reset
        RCC->APB2RSTR &= ~( 1 << 22);     // SAI1 reset release
        
            
/*
RCC_CR         :03078483
RCC_PLLCFGR    :29406c08
RCC_CFGR       :0000940a
RCC_CIR        :00000000
RCC_APB2RSTR   :00000000
RCC_CR         :03078483
RCC_PLLSAICFGR :24003000
*/
        
            // Setup Global Configuraion Register
        SAI1->GCR = 
                0 << 0 |    // syncin : ingnored because none of the block is external synch from outside of SAI module 
                0 << 4 ;    // syncout : 0 : No sync output for other SAI. 1,Block A(RX) is used for input of otehr block
            
            // Setup SAI Block configuration register.
            // Block A : RX
            // Block B : TX
        SAI1_Block_A->CR1 = 
#ifndef DEBUGSAI          
                0 << 20 |   // MCKDIV   : Meaningless because the block is slave mode.
#else
                2 << 20 |   // MCKDIV   : Master Clock Divider.: Div by 4
#endif                
                1 << 19 |   // NODIV    : Master clock divider is enabled ( perhaps, meaningless in slave mode )
                0 << 17 |   // DMAEN    : 0, DMA disanble, 1: DMA Enable
                0 << 16 |   // SAIXEN   : 0, Disable, 1, Enable. Disable at this moment
                0 << 13 |   // OUTDRIV  : 0, Audio is driven only when SAIXEN is 1. 1, Audio is driven
                0 << 12 |   // MONO     : 0, Stereo. 1, Mono
                0 << 10 |   // SYNCEN   : 0, Async mode. The Async mode referes the outside sync signal in slave mode. 
                1 << 9  |   // CKSTR    : 0, sample by falling edge, 1, sample by rising edge. I2S is sample by rising edge
                0 << 8  |   // LSBFIRST : 0, MSB first. 1, LSB first. I2S is MSB first
                7 << 5 |    // DS       : 7, 32bit. 
                0 << 2 |    // PRTCFG   : 0, Free protocol, 1, SPDIF, 2, AC97. I2S is Free protocol
#ifndef DEBUGSAI  
                3 << 0 ;    // MODE     : 0, master tx. 1, master rx. 2, slave tx. 3, slave rx
#else
                1 << 0 ;    // MODE     : 0, master tx. 1, master rx. 2, slave tx. 3, slave rx
#endif                
            // configuration register 2
        SAI1_Block_A->CR2 = 
                0 << 14 |   // COMP     : 0, No companding
                0 << 13 |   // CPL      : Ignoered when no companding
                0 << 7 |    // MUTECNT  : 0, ignored when no muting
                0 << 6 |    // MUTEVAL  : 0, meaningless except SPDIF mode
                0 << 5 |    // MUTE     : 0, No mute. 1, mute
                0 << 4 |    // TRIS     : 0, Drive all slot. 1, Drive only active slot. Meaningless for I2S and RX
                1 << 3 |    // FFLUSH   : 0, No FIFO Flush. 1, FIFO Flush
                1 << 0;     // FTH      : 0, FIFO empty. 1, 1/4 FIFO. 2, 1/2 FIFO. 3, 3/4 FIFO. 4, FIFO full
                
            // Frame configuration register
        SAI1_Block_A->FRCR =
                1 << 18 |   // FSOFF    : 0, FS is asserted on the first bit. 1, FS is asserted before the first bit.
                0 << 17 |   // FSPOL    : 0, Active low. 1, active high. I2S in left first operation is actilve low FS.
                1 << 16 |   // FSDEF    : 0, FS is start frame signal. 1, FS has also channel side info. I2S have to set 1
                31 << 8 |   // FSALL    : Frame sync active level lenght. Seems to be meaningless in slave mode. Set as to be sure.
                63 << 0 ;   // FRL      : Frame length. Seems to be meaning less in slave mode. Set as to be sure. 
                
            // Slot register
        SAI1_Block_A->SLOTR = 
                0xFFFF << 16 |   // SLOTEN   : bit mask to specify the active slot. In I2S, 2 slts are active.
                1 << 8 |    // NBSLOT   : Number of slots - 1 ( Ref manual seems to be wrong )
                2 << 6 |    // SLOTSZ   : 0, same with data size. 1, 16bit. 2, 32bit
                0 << 0 ;    // FBOFF    : The manual is not clear. Perhaps, 0 is OK.
                
            // interrupt mask. Only FIFO interrupt is allowed.
        SAI1_Block_A->IMR = 
                0 << 6 |    // LFSDETIE : Late frame synchronization detection interrupt enable
                0 << 5 |    // AFSDETIE : Anticipated frame synchronization detection interrupt enable. AC97 only
                0 << 4 |    // CNDYIE   : CODEC nott ready interrupt. AC97 only
                1 << 3 |    // FREQIE   : FIFO Interrupt Request. Enable for RX
                0 << 2 |    // WCKCFGIE : Wrong clock configuration interrupt enable. 
                0 << 1 |    // MUTEDETIE: Mute detection interrupt enable. 
                0 << 0;     // OVRUDRIE : Overrun/underrun interrupt enable          
            
            // Clear flag register
        SAI1_Block_A->CLRFR = 0xFFFFFFFF;       // clear all flags
        
            // Setup SAI Block configuration register.
            // Block A : RX
            // Block B : TX
        SAI1_Block_B->CR1 = 
                0 << 20 |   // MCKDIV   : Meaningless because the block is slave mode.
                0 << 19 |   // NODIV    : Master clock divider is enabled ( perhaps, meaningless in slave mode )
                0 << 17 |   // DMAEN    : 0, DMA disanble, 1: DMA Enable
                0 << 16 |   // SAIXEN   : 0, Disable, 1, Enable. Disable at this moment
                0 << 13 |   // OUTDRIV  : 0, Audio is driven only when SAIXEN is 1. 1, Audio is driven
                0 << 12 |   // MONO     : 0, Stereo. 1, Mono
                1 << 10 |   // SYNCEN   : 1, sync with internal audio block. 
                1 << 9  |   // CKSTR    : 0, sample by falling edge, 1, sample by rising edge. I2S is sample by rising edge
                0 << 8  |   // LSBFIRST : 0, MSB first. 1, LSB first. I2S is MSB first
                7 << 5 |    // DS       : 7, 32bit. 
                0 << 2 |    // PRTCFG   : 0, Free protocol, 1, SPDIF, 2, AC97. I2S is Free protocol
                2 << 0 ;    // MODE     : 0, master tx. 1, master rx. 2, slave tx. 3, slave rx
                
            // configuration register 2
        SAI1_Block_B->CR2 = 
                0 << 14 |   // COMP     : 0, No companding
                0 << 13 |   // CPL      : Ignoered when no companding
                0 << 7 |    // MUTECNT  : 0, ignored when no muting
                0 << 6 |    // MUTEVAL  : 0, meaningless except SPDIF mode
                0 << 5 |    // MUTE     : 0, No mute. 1, mute
                0 << 4 |    // TRIS     : 0, Drive all slot. 1, Drive only active slot. Meaningless for I2S and RX
                1 << 3 |    // FFLUSH   : 0, No FIFO Flush. 1, FIFO Flush
                1 << 0;     // FTH      : 0, FIFO empty. 1, 1/4 FIFO. 2, 1/2 FIFO. 3, 3/4 FIFO. 4, FIFO full
                
            // Frame configuration register
        SAI1_Block_B->FRCR =
                1 << 18 |   // FSOFF    : 0, FS is asserted on the first bit. 1, FS is asserted before the first bit.
                0 << 17 |   // FSPOL    : 0, Active low. 1, active high. I2S in left first operation is actilve low FS.
                1 << 16 |   // FSDEF    : 0, FS is start frame signal. 1, FS has also channel side info. I2S have to set 1
                31 << 8 |   // FSALL    : Frame sync active level lenght. Seems to be meaningless in slave mode. Set as to be sure.
                63 << 0 ;   // FRL      : Frame length. Seems to be meaning less in slave mode. Set as to be sure. 
                
            // Slot register
        SAI1_Block_B->SLOTR = 
                0xFFFF << 16 |   // SLOTEN   : bit mask to specify the active slot. In I2S, 2 slts are active.
                1 << 8 |    // NBSLOT   : Number of slots - 1 ( Ref manual seems to be wrong )
                2 << 6 |    // SLOTSZ   : 0, same with data size. 1, 16bit. 2, 32bit
                0 << 0 ;    // FBOFF    : The manual is not clear. Perhaps, 0 is OK.
                
            // interrupt mask : TX doesn't trigger interrupt
        SAI1_Block_B->IMR = 
                0 << 6 |    // LFSDETIE : Late frame synchronization detection interrupt enable
                0 << 5 |    // AFSDETIE : Anticipated frame synchronization detection interrupt enable. AC97 only
                0 << 4 |    // CNDYIE   : CODEC nott ready interrupt. AC97 only
                0 << 3 |    // FREQIE   : FIFO Interrupt Request. Enable for RX
                0 << 2 |    // WCKCFGIE : Wrong clock configuration interrupt enable. 
                0 << 1 |    // MUTEDETIE: Mute detection interrupt enable. 
                0 << 0;     // OVRUDRIE : Overrun/underrun interrupt enable          
            
            // Clear flag register
        SAI1_Block_B->CLRFR = 0xFFFFFFFF;       // clear all flags


            //  Fill up tx FIO by 3 stereo samples.
        hal_put_i2s_tx_data( 0 ); // left
        hal_put_i2s_tx_data( 0 ); // right
        hal_put_i2s_tx_data( 0 ); // left
        hal_put_i2s_tx_data( 0 ); // right
        hal_put_i2s_tx_data( 0 ); // left
        hal_put_i2s_tx_data( 0 ); // right

    }
    
        // Pin configuration and sync with WS signal
    void hal_i2s_pin_config_and_wait_ws(void)
    {
            // See DM00166116.pdf ST32F746ZG Datasheet Rev 4 Table 12
            // See https://developer.mbed.org/platforms/ST-Nucleo-F746ZG/
            // See stm32f746xx.h source here : https://developer.mbed.org/teams/Rigado/code/mbed-src-bmd-200/docs/255afbe6270c/stm32f746xx_8h_source.html
            
            // PE3  SAI1_SD_B   (AF6) : DAC
            // PE4  SAI1_FS_A   (AF6) : WS
            // PE5  SAI1_SCK_A  (AF6) : CLK
            // PE6  SAI1_SD_A   (AF6) : ADC
            
            // Clear the mode field of PE3-6
        GPIOE->MODER &=
            ~(  3 << 6 |    // PE3
                3 << 8 |    // PE4
                3 << 10 |   // PE5
                3 << 12 );  // PE6 

            // Set the pin mode
        GPIOE->MODER |=
                2 << 6 |    // PE3 is Alternate Function
                2 << 8 |    // PE4 is Alternate Function
                2 << 10 |   // PE5 is Alternate Function
                2 << 12;    // PE6 is Alternate Function


            // Clear the OTYPE field of PE3-6  ( Clear is push-pull )
        GPIOE->OTYPER &=
            ~(  1 << 3 |    // PE3
                1 << 4 |    // PE4
                1 << 5 |    // PE5
                1 << 6 );   // PE6 


            // Clear the OSPEEDR field of PE3-6
        GPIOE->OSPEEDR &=
            ~(  3 << 6 |    // PE3
                3 << 8 |    // PE4
                3 << 10 |   // PE5
                3 << 12 );  // PE6 

            // Set the OSPEEDR
        GPIOE->OSPEEDR |=
                1 << 6 |    // PE3 DAC is medium speed
                0 << 8 |    // PE4 is input
                0 << 10 |   // PE5 is input
                0 << 12;    // PE6 is input


            // Clear the PUPDR field of PE3-6 ( Clear is no pull-up/no pull-down )
        GPIOE->PUPDR &=
            ~(  3 << 6 |    // PE3
                3 << 8 |    // PE4
                3 << 10 |   // PE5
                3 << 12 );  // PE6 


            // Clear the Alternate funciton of PE3-6 
        GPIOE->AFR[0] &=
            ~(  0xF << 12 | // PE3
                0xF << 16 | // PE4
                0xF << 20 | // PE5
                0xF << 24 );// PE6 

            // Set the Alternate function of PE3-6 
        GPIOE->AFR[0] |=
                6 << 12 |   // PE3  Alternalte Funciton 6
                6 << 16 |   // PE4  Alternalte Funciton 6
                6 << 20 |   // PE5  Alternalte Funciton 6
                6 << 24 ;   // PE6  Alternalte Funciton 6

        // Now, we set all the pin. We don't need to wait the WS, 
        // Because SAI has TX/RX sync.
    }

    
        // Start I2S transfer. Interrupt starts  
    void hal_i2s_start(void)
    {
            // Setup SAI Block configuration register.
            // Block A : RX
            // Block B : TX
            
            // Block B is sync to Block A. So, Block B first.
        SAI1_Block_B->CR1 |= 
            1 << 16 ;   // SAIXEN   : 0, Disable, 1, Enable. Disable at this moment
            // Now, Block A and B start together
        SAI1_Block_A->CR1 |= 
            1 << 16 ;   // SAIXEN   : 0, Disable, 1, Enable. Disable at this moment

    }
 
    IRQn_Type hal_get_i2s_irq_id(void)
    {
        return SAI1_IRQn;
    }
    
    
    IRQn_Type hal_get_process_irq_id(void)
    {
        return SPI6_IRQn;   // STM32F746 SPI6 is killed. This interrupt is assigned for signal processing in Unzen
    }
    
    
         // The returned value must be compatible with CMSIS NVIC_SetPriority() API. That mean, it is integer like 0, 1, 2...
    unsigned int hal_get_i2s_irq_priority_level(void)
    {
           // STM32F746 has 4 bits priority field. So, heighest is 0, lowest is 15.
           // But CMSIS NVIC_SetPriority() inverse the priority of the interupt ( F**k! )
           // Then, 15 is heighest, 0 is lowerest in CMSIS.
           // setting 12 as i2s irq priority allows, some other interrupts are higher 
           // and some others are lower than i2s irq priority.
        return 12;
    }


        // The returned value must be compatible with CMSIS NVIC_SetPriority() API. That mean, it is integer like 0, 1, 2...
    unsigned int hal_get_process_irq_priority_level(void)
    {
           // STM32F746 has 4 bits priority field. So, heighest is 0, lowest is 15.
           // But CMSIS NVIC_SetPriority() inverse the priority of the interupt ( S**t! )
           // Then 15 is heighest, 0 is lowerest in CMSIS.
           // setting 4 as process priority allows, some other interrupts are higher 
           // and some other interrupts are lower then process priority.
        return 4;   
    }
 
        // STM32F746 transferes 2 wordｓ ( left and right ) for each interrupt.
    unsigned int hal_data_per_sample(void)
    {
        return 2;
    }

        // return true when the sample parameter is ready to read.
        // return false when the sample is not ready to read.
    void hal_get_i2s_rx_data( int & sample)
    {
            // RX is SAI1_BlockA. See the comment on top of this file
        sample = SAI1_Block_A->DR;
    }
    
        // put a sample to I2S TX data regisger
    void hal_put_i2s_tx_data( int sample )
    {
            // TX is SAI1_Block_B. See the comment on top of this file
        SAI1_Block_B->DR = sample;
    }
}


