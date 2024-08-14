#include "algorithm"
#include "limits.h"

#include "unzen.h"
#include "unzen_hal.h"

namespace unzen 
{
    Framework::Framework()
    {
            // setup handle for the interrupt handler
        Framework::_fw = this;

            // Clear all buffers        
        _tx_int_buffer[0] = NULL;
        _tx_int_buffer[1] = NULL;
        _rx_int_buffer[0] = NULL;
        _rx_int_buffer[1] = NULL;
        
        _tx_left_buffer = NULL;
        _tx_right_buffer = NULL;
        _rx_left_buffer = NULL;
        _rx_right_buffer = NULL;
 
            // Initialize all buffer
        _buffer_index = 0;
        _sample_index = 0;
        
            // Clear all callbacks
        _pre_interrupt_callback = NULL;
        _post_interrupt_callback = NULL;
        _pre_process_callback = NULL;
        _post_process_callback = NULL;
        
        _process_callback = NULL;

        
            // Initialy block(buffer) size is 1.
        set_block_size( 1 );
        
            // Initialize I2S peripheral
        hal_i2s_setup();

            // Setup the interrupt for the I2S
        NVIC_SetVector(hal_get_i2s_irq_id(), (uint32_t)_i2s_irq_handler);
        set_i2s_irq_priority(hal_get_i2s_irq_priority_level());
        NVIC_EnableIRQ(hal_get_i2s_irq_id());

            // Setup the interrupt for the process
        NVIC_SetVector(hal_get_process_irq_id(), (uint32_t)_process_irq_handler);
        set_process_irq_priority(hal_get_process_irq_priority_level());
        NVIC_EnableIRQ(hal_get_process_irq_id());
        
    }

    error_type Framework::set_block_size(  unsigned int new_block_size )
    {
        
        delete [] _tx_int_buffer[0];
        delete [] _tx_int_buffer[1];
        delete [] _rx_int_buffer[0];
        delete [] _rx_int_buffer[1];
        
        delete [] _tx_left_buffer;
        delete [] _tx_right_buffer;
        delete [] _rx_left_buffer;
        delete [] _rx_right_buffer;
        
        _block_size = new_block_size;

        _tx_int_buffer[0] = new int[ 2 * _block_size ];
        _tx_int_buffer[1] = new int[ 2 * _block_size ];
        _rx_int_buffer[0] = new int[ 2 * _block_size ];
        _rx_int_buffer[1] = new int[ 2 * _block_size ];
        
        _tx_left_buffer = new float[ _block_size ];
        _tx_right_buffer = new float[ _block_size ];
        _rx_left_buffer = new float[ _block_size ];
        _rx_right_buffer = new float[ _block_size ];
 
            // error check
        if ( _rx_int_buffer[0] == NULL |
             _rx_int_buffer[1] == NULL |
             _tx_int_buffer[0] == NULL |
             _tx_int_buffer[1] == NULL |
             _rx_right_buffer == NULL |
             _tx_right_buffer == NULL |
             _rx_left_buffer == NULL |
             _tx_left_buffer == NULL )
        {   // if error, release all 
            delete [] _tx_int_buffer[0];
            delete [] _tx_int_buffer[1];
            delete [] _rx_int_buffer[0];
            delete [] _rx_int_buffer[1];
            
            delete [] _tx_left_buffer;
            delete [] _tx_right_buffer;
            delete [] _rx_left_buffer;
            delete [] _rx_right_buffer;
            
            _tx_int_buffer[0] = NULL;
            _tx_int_buffer[1] = NULL;
            _rx_int_buffer[0] = NULL;
            _rx_int_buffer[1] = NULL;
            
            _tx_left_buffer = NULL;
            _tx_right_buffer = NULL;
            _rx_left_buffer = NULL;
            _rx_right_buffer = NULL;
            
            return memory_allocation_error;
        }

            // clear blocks
        for ( int i=0; i<_block_size*2; i++ )
        {

            _tx_int_buffer[0][i] = 0;
            _tx_int_buffer[1][i] = 0;
            _rx_int_buffer[0][i] = 0;
            _rx_int_buffer[1][i] = 0;
            
        }
            // clear blocks
        for ( int i=0; i<_block_size ; i++ )
        {

            _tx_left_buffer[i] = 0;
            _tx_right_buffer[i] = 0;
            _rx_left_buffer[i] = 0;
            _rx_right_buffer[i] = 0;

        }
         
        return no_error;
    }

    void Framework::start(
                    void (* init_cb ) (unsigned int),
                    void (* process_cb ) (float[], float[], float[], float[], unsigned int)
                    )
    {
            // if needed, call the initializer
        if ( init_cb )
            init_cb( _block_size );
            
            // register the signal processing callback
        _process_callback = process_cb;
        
            // synchronize with Word select signal, to process RX/TX as atomic timing.
        hal_i2s_pin_config_and_wait_ws();
        hal_i2s_start();
    }

    void Framework::set_i2s_irq_priority( unsigned int pri )
    {
        NVIC_SetPriority(hal_get_process_irq_id(), pri);  // must be higher than PendSV of mbed-RTOS
    }
    
    void Framework::set_process_irq_priority( unsigned int pri )
    {
        NVIC_SetPriority(hal_get_i2s_irq_id(), pri);      // must be higher than process IRQ
    }

    void Framework::set_pre_interrupt_callback( void (* cb ) (void))
    { 
        _pre_interrupt_callback = cb;
    }
    
    void Framework::set_post_interrupt_callback( void (* cb ) (void))
    { 
        _post_interrupt_callback = cb;
    }
    
    void Framework::set_pre_process_callback( void (* cb ) (void))
    { 
        _pre_process_callback = cb;
    }
    
    void Framework::set_post_process_callback( void (* cb ) (void))
    { 
        _post_process_callback = cb;
    }

    void Framework::_do_i2s_irq(void)
    {
            // if needed, call pre-interrupt call back
        if ( _pre_interrupt_callback )
            _pre_interrupt_callback();
            
            // irq is handled only when the buffer is correctly allocated    
        if (_tx_left_buffer)
        {
            int sample;
            
                // check how many data have to be transmimted per interrupt. 
            for ( int i=0; i<hal_data_per_sample(); i++ )
            {
                    // copy received data to buffer
                hal_get_i2s_rx_data( sample );
                _rx_int_buffer[_buffer_index][_sample_index] = sample;
                
                    // copy buffer data to transmit register
                sample = _tx_int_buffer[_buffer_index][_sample_index];
                hal_put_i2s_tx_data( sample );
                
                    // increment index
                _sample_index ++;
            }
            
                // Implementation of the double buffer algorithm.
                // if buffer transfer is complete, swap the buffer
            if (_sample_index >= _block_size * 2)
            {
                    // index for the signal processing
                _process_index = _buffer_index;

                    // swap buffer
                if ( _buffer_index == 0 )
                    _buffer_index = 1;
                else
                    _buffer_index = 0;

                    // rewind sample index
                _sample_index = 0;

                    // Trigger interrupt for signal processing
                NVIC->STIR = hal_get_process_irq_id();       
            }
        }

            // if needed, call post-interrupt call back
        if ( _post_interrupt_callback )
            _post_interrupt_callback();
            
    }

    void Framework::_do_process_irq(void)
    {
            // If needed, call the pre-process hook
        if ( _pre_process_callback )
            _pre_process_callback();
            
            // Only when the process_call back is registered.
        if ( _process_callback )
        {
            int j = 0;
                
                // Format conversion.
                // -- premuted from LRLRLR... to LLL.., RRR...
                // -- convert from fixed point to floating point
                // -- scale down as range of [-1, 1)
            for ( int i=0; i<_block_size; i++ )
            {
                _rx_left_buffer[i]  = _rx_int_buffer[_process_index][j++]/ -(float)INT_MIN;
                _rx_right_buffer[i] = _rx_int_buffer[_process_index][j++]/ -(float)INT_MIN;
            }
                
            _process_callback
                    (
                        _rx_left_buffer,
                        _rx_right_buffer,
                        _tx_left_buffer,
                        _tx_right_buffer,
                        _block_size
                    );
                
                // Format conversion.
                // -- premuted from LLL.., RRR... to LRLRLR...
                // -- convert from floating point to fixed point
                // -- scale up from range of [-1, 1)
            j = 0;
            for ( int i=0; i<_block_size; i++ )
            {
                _tx_int_buffer[_process_index][j++] = _tx_left_buffer[i]  * -(float)INT_MIN ;
                _tx_int_buffer[_process_index][j++] = _tx_right_buffer[i] * -(float)INT_MIN ;
            }
    
        }

            // if needed, call post-process callback
        if ( _post_process_callback )
            _post_process_callback();
    }
    
    void Framework::_process_irq_handler()
    {
        Framework::_fw->_do_process_irq();
    }
    
    void Framework::_i2s_irq_handler()
    {
        Framework::_fw->_do_i2s_irq();
    }
     
     
    Framework * Framework::_fw; 
 
}
    