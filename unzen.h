/**
* \brief header file for the unzen audio frame work 
* \arthur SeiichiHorie
* \date 6/Apr/2016
*/

#ifndef _unzen_h_
#define _unzen_h_

#include "mbed.h"
/**
 \brief audio framework name space. 
*/
namespace unzen 
{
    /**
      \brief error status type.
    */
    enum error_type {
        no_error,                   ///< No error.
        memory_allocation_error     ///< Fatal. Memory is exhausted.
        };
    
    /**
      \brief adio frame work. Create a object and execute the \ref Framework::start() method.
      
      example :
      \code
#include "unzen.h"          // audio framework include file
#include "umb_adau1361a.h"     // audio codec contoler include file
#include "mbed.h"

#define CODEC_I2C_ADDR 0x38

DigitalOut myled1(LED1);


    // customer signal processing initialization call back.
void init_callback(
            unsigned int block_size     // block size [sample]
            )
{
        // place initialization code here
}


    // customer signal processing call back.
void process_callback(
            float rx_left_buffer[],     // array of the left input samples
            float rx_right_buffer[],    // array of the right input samples
            float tx_left_buffer[],     // place to write the left output samples
            float tx_right_buffer[],    // place to write the right output samples
            unsigned int block_size     // block size [sample]
            )
{
        // Sample processing
    for ( int i=0; i<block_size; i++)   // for all sample
    {
        tx_left_buffer[i] = rx_left_buffer[i];      // copy from input to output
        tx_right_buffer[i] = rx_right_buffer[i];
        
    }
}



int main() 
{    
        // I2C is essential to talk with ADAU1361
    I2C i2c(SDA, SCL);

        // create an audio codec contoler
    shimabara::UMB_ADAU1361A codec(shimabara::Fs_32, &i2c, CODEC_I2C_ADDR ); // Default Fs is 48kHz
//    shimabara::UMB_ADAU1361A codec(shimabara::Fs_441, &i2c, CODEC_I2C_ADDR );
//    shimabara::UMB_ADAU1361A codec(shimabara::Fs_48, &i2c, CODEC_I2C_ADDR );
//    shimabara::UMB_ADAU1361A codec(shimabara::Fs_96, &i2c, CODEC_I2C_ADDR );

       // create an audio framework by singlton pattern
    unzen::Framework audio;
 
         // Set I3C clock to 100kHz
    i2c.frequency( 100000 );


        // Configure the optional block size of signal processing. By default, it is 1[Sample] 
//    audio.set_block_size(16);

    
        // Start the ADAU1361. Audio codec starts to generate the I2C signals 
    codec.start();

        // Start the audio framework on ARM processor.  
    audio.start( init_callback, process_callback);     // path the initializaiton and process call back to framework 


        // periodically changing gain for test
    while(1)     
    {
        for ( int i=-15; i<4; i++ )
        {
            codec.set_hp_output_gain( i, i );
            codec.set_line_output_gain( i, i );
            myled1 = 1;
            wait(0.2);
            myled1 = 0;
            wait(0.2);
        }
    }
}


      \endcode
    */
    class Framework 
    {
    public:
            /**
                \constructor
                \details
                initialize the internal variables and set up all interrrupt / I2S related peripheral. 
                If needed, power up the peripheral, assign the clock and pins. 
                At the end of this constructor, the audio framework is ready to run, but still paused. 
                To start the real processing, call the \ref start method.
                
                Note that this constructor set the block size ( interval count which audio processing
                call back is called )
                as 1. If it is needed to use other value, call \ref set_brock_size() method. 
            */
        Framework(void);
        
            /**
                \brief set the interval interrupt count for each time call back is called. 
                \param block_size An integer parameter > 1. If set to n, for each n interrupts, the audio call back is called. 
                \returns show the error status
                \details
                This method re-allocate the internal buffer. Then, the memory allocation error could occur. To detect the 
                memory allocation error, use \ref get_error() method.
            */
        error_type set_block_size(  unsigned int new_block_size );
        
            /**
                \brief  the real audio signal transfer. Trigger the I2S interrupt and call the call back.
                \param init_cb initializer call back for signal processing. This is invoked only once before processing. Can be NUL
                \param process_cb The call back function
                \details
                Set the call back function, then start the transer on I2S
                
                Before the real data transaction on I2S, this method call init_cb(int) call back. The 
                parameter is block size (>0) which is set by set_block_size() method. If the init_cb is NUL,
                this part is skipped. 
                
                This process_cb function is called for each time the rx buffer is filled. The call back has 5 parameters.
                \li left_rx  Received data from I2S rx port. Left data only. 
                \li right_rx Received data from I2S rx port. Right data only.
                \li left_tx  Buffer to fill the transmission data. This buffer mus be filled by call back. Left data only
                \li right_tx Buffer to fill the transmission data. This buffer mus be filled by call back. Right data only
                \li length   length of above buffers.
                
                The process call back is called for each time interrupt comes n times. Where n is the value which is specified by \ref set_block_size() 
                function. This n is also passed to call back as above length parameter. By default, n is 1.
                
                Note that the call back is called at interrupt context. Not the thread level context.
                That mean, it is better to avoid to call mbed API except the mbed-RTOS API for interrupt handler.
                */
        void start(
                void (* init_cb ) (unsigned int),
                void (* process_cb ) (float[], float[], float[], float[], unsigned int)
                );


            /**
                \brief Debug hook for interrupt handler. 
                \param cb A call back which is called at the beggining of I2S interrupt.
                \details
                Parameter cb is call at the begging of the I2S interrupt. This call back can be 
                used to mesure the timing of interrupt by toggling the GPIO pin.
                
                Passing 0 to cb parameter let the framwork ignore the callback.
            */
        void set_pre_interrupt_callback( void (* cb ) (void));
        
            /**
                \brief Debug hook for interrupt handler. 
                \param cb A call back which is called at the end of I2S interrupt.
                \details
                Parameter cb is call at the end of the I2S interrupt. This call back can be 
                used to mesure the timing of interrupt by toggling the GPIO pin.
                
                Passing 0 to cb parameter let the framwork ignore the callback.
            */
        void set_post_interrupt_callback( void (* cb ) (void));
        
            /**
                \brief Debug hook for processing handler. 
                \param cb A call back which is called at the beggining of processing.
                \details
                Parameter cb is call at the beggining of the signal processing. This call back can be 
                used to mesure the load of CPU by toggling the GPIO pin.
                
                Passing 0 to cb parameter let the framwork ignore the callback.
            */
        void set_pre_process_callback( void (* cb ) (void));

            /**
                \brief Debug hook for processing handler. 
                \param cb A call back which is called at the end of processing.
                \details
                Parameter cb is call at the end of the signal processing. This call back can be 
                used to mesure the load of CPU by toggling the GPIO pin.
                
                Passing 0 to cb parameter let the framwork ignore the callback.
            */
        void set_post_process_callback( void (* cb ) (void));
        
            /**
                \brief optional priority control for I2S IRQ. 
                \param pri Priority of IRQ.
                \details
                This is optional control. Usually, user doesn't need to call this method.
                In case the framework has serious irq priority contention with other software, 
                this API help programmer to change the priority of the Unzen framework.
                
                Value must be acceptable to CMSIS NVIC_SetPriority(). For LPC4337, it is range of 0..7. The 0 is highest priority.

                
                By default, the framework set this priority as +2 higher than the lowest.
                
                The priority set by this API must be higher than priority set by \ref set_process_irq_priority
            */
        void set_i2s_irq_priority( unsigned int pri );
        
            /**
                \brief optional priority control for Signal Processing IRQ. 
                \param pri Priority of IRQ.
                \details
                This is optional control. Usually, user doesn't need to call this method.
                In case the framework has serious irq priority contention with other software, 
                this API help programmer to change the priority of the Unzen framework

                Value must be acceptable to CMSIS NVIC_SetPriority(). For LPC4337, it is range of 0..7. The 0 is highest priority.
                
                By default, the framework set this priority as +2 higher than the lowest.

            */
        void set_process_irq_priority( unsigned int pri );

    private:        
        static Framework * _fw;
    private:
        void (* _pre_interrupt_callback )(void);
        void (* _post_interrupt_callback )(void);
        void (* _pre_process_callback )(void);
        void (* _post_process_callback )(void);
        
        void (* _process_callback )( float left_in[], float right_in[], float left_out[], float right_out[], unsigned int length );
        
            // Size of the blocks ( interval of interrupt to call process_callback. 1 means every interrupt. 2 means every 2 interrupt )
        int _block_size;
        
            // Index for indentifying the buffer for interrupt. 0 or 1. 
        int _buffer_index;
        
            // Index for indentifying the buffer for processing. 0 or 1. 
        int _process_index;
        
            // next transfer position in buffer
        int _sample_index;
        
            // buffer for interrupt handler.
            // data format is LRLR... 
        int *_tx_int_buffer[2];
        int *_rx_int_buffer[2];
        
            // buffers for passing 
        float * _tx_left_buffer, * _tx_right_buffer;
        float * _rx_left_buffer, * _rx_right_buffer;
                
            // real processing method.
        void _do_i2s_irq(void);
        void _do_process_irq(void);
        
            // handler for NIVC
        static void _i2s_irq_handler();
        static void _process_irq_handler();        
    };



}

#endif
