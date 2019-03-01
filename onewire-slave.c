#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_def.h"
#include "onewire-slave.h"
#include "stm32f7xx_hal_gpio.h"

// TODO: remove when tests with LEDs are over:
#include "main.h"

// Book of iButton Standards:
// https://pdfserv.maximintegrated.com/en/an/AN937.pdf

// Data structure for storing references to all initialized OneWire instances.
OneWireSlave_HandleTypeDef *OneWireInstances[MAX_ONEWIRE_INSTANCES] = {0};

void OneWireSlave_Init(OneWireSlave_HandleTypeDef *h1ws)
{
    // Init timer for delay
    __HAL_RCC_TIM4_CLK_ENABLE();
    TIM4->PSC = HAL_RCC_GetPCLK1Freq() / 500000 - 1; // 1 tick = 1 microsecond
    TIM4->CR1 = TIM_CR1_CEN;

    // Set initial state
    h1ws->LL_State = ONEWIRE_R_IDLE;

    // Add itself to the global list of active OneWire instances
    for (int i = 0; i < MAX_ONEWIRE_INSTANCES; i++)
    {
        if (!OneWireInstances[i])
        {
            OneWireInstances[i] = h1ws;
            break;
        }
        else if (i == MAX_ONEWIRE_INSTANCES)
        {
            // all instances are used up!
            // TODO: ERROR!
        }
    }
}

void OneWireSlave_DeInit(OneWireSlave_HandleTypeDef *h1ws)
{
    // Remove itself to the global list of active OneWire instances
    for (int i = 0; i < MAX_ONEWIRE_INSTANCES; i++)
    {
        if (OneWireInstances[i] == h1ws)
        {
            OneWireInstances[i] = 0;
            break;
        }
    }
}

//************************************
//       HIGH LEVEL ONEWIRE
//************************************

void OneWire_Send(OneWireSlave_HandleTypeDef *h1ws, __uint8_t *message, __uint16_t message_length)
{
    h1ws->SendDataBuffer = message;
    h1ws->SendDataBuffer_Length = message_length;
    h1ws->SendDataBuffer_Pos = 0;
    h1ws->SendDataBuffer_BitPos = 0x01;
    h1ws->LL_State = ONEWIRE_W_IDLE;
}

void OneWire_SendBit(OneWireSlave_HandleTypeDef *h1ws, __uint8_t bit)
{
    h1ws->Internal_Buffer[0] = (bit) ? 0x80 : 0x00;
    h1ws->SendDataBuffer = h1ws->Internal_Buffer;
    h1ws->SendDataBuffer_Length = 1;
    h1ws->SendDataBuffer_Pos = 0;
    h1ws->SendDataBuffer_BitPos = 0x80;
    h1ws->LL_State = ONEWIRE_W_IDLE;
}

/* NOTE: This function Should not be modified, when the callback is needed,
         the OneWire_Byte_Received_Callback could be implemented in the user file
*/
__weak void OneWire_Byte_Received_Callback(OneWireSlave_HandleTypeDef *h1ws, __uint8_t byte)
{
    /* Prevent unused argument(s) compilation warning */
    (void)h1ws;
    (void)byte;
}

/* NOTE: This function Should not be modified, when the callback is needed,
         the OneWire_Bit_Received_Callback could be implemented in the user file
*/
__weak void OneWire_Bit_Received_Callback(OneWireSlave_HandleTypeDef *h1ws, __uint8_t bit)
{
    /* Prevent unused argument(s) compilation warning */
    (void)h1ws;
    (void)bit;
}

/* NOTE: This function Should not be modified, when the callback is needed,
         the OneWire_Reset_Received_Callback could be implemented in the user file
*/
__weak void OneWire_Reset_Received_Callback(OneWireSlave_HandleTypeDef *h1ws)
{
    /* Prevent unused argument(s) compilation warning */
    (void)h1ws;
}














//************************************
//            NETWORK LAYER
//    ROM / HIGH LEVEL STATE MACHINE
//************************************

void OneWire_Received_Command(OneWireSlave_HandleTypeDef *h1ws)
{

    // only do ROM actions if this is the first byte after a reset!
    // otherwise it might just be arbitrary data...
    switch (h1ws->ReceiveBuffer)
    {
    case 0xF0: // SEARCH ROM
        // Begin with LSB
        h1ws->ROM_Mask = 0x0000000000000001;
        // immediately write first bit of ROM and its complement to the bus
        h1ws->Internal_Buffer[0] = (h1ws->Init.ROM_Address & h1ws->ROM_Mask) ? (__uint8_t)0x40 : (__uint8_t)0x80;
        h1ws->SendDataBuffer = h1ws->Internal_Buffer;
        h1ws->SendDataBuffer_Length = 1;
        h1ws->SendDataBuffer_Pos = 0;
        h1ws->SendDataBuffer_BitPos = (__uint8_t)0x40;
        h1ws->LL_State = ONEWIRE_W_IDLE;

        h1ws->ROM_State = ONEWIRE_SEARCH_ROM;
        break;
    case 0xEC: // CONDITIONAL SEARCH ROM
        // Begin with LSB
        h1ws->ROM_Mask = 0x0000000000000001;
        // immediately write first bit of ROM and its complement to the bus
        h1ws->Internal_Buffer[0] = (h1ws->Init.ROM_Address & h1ws->ROM_Mask) ? (__uint8_t)0x40 : (__uint8_t)0x80;
        h1ws->SendDataBuffer = h1ws->Internal_Buffer;
        h1ws->SendDataBuffer_Length = 1;
        h1ws->SendDataBuffer_Pos = 0;
        h1ws->SendDataBuffer_BitPos = (__uint8_t)0x40;
        h1ws->LL_State = ONEWIRE_W_IDLE;

        h1ws->ROM_State = ONEWIRE_ALARM_SEARCH;
        break;
    case 0x33: // READ ROM
        // send family code + serial number + CRC of ROM
        // -> we need to reverse the ROM to get this ordering
        h1ws->Internal_Buffer[0] = 0; // do anything here except a declaration (next line) because C does not allow declarations after labels ¯\_(ツ)_/¯
        __uint64_t mask = (__uint64_t)0xFF;
        for(int i=0;i<8;i++) {
            h1ws->Internal_Buffer[i] = ((h1ws->Init.ROM_Address & mask) << (7-i)*8);
            mask = mask << 8;
        }
        OneWire_Send(h1ws, h1ws->Internal_Buffer, 8);
        break;
    case 0x55: // MATCH ROM
        // Begin with LSB
        h1ws->ROM_Mask = 0x0000000000000001;
        h1ws->ROM_State = ONEWIRE_MATCH_ROM;
        break;
    case 0xCC: // SKIP ROM
        break;
    default: // invoke interrupt for handling this command
        OneWire_Byte_Received_Callback(h1ws, h1ws->ReceiveBuffer);
        break;
    }
}

void Process_Received_Bit(OneWireSlave_HandleTypeDef *h1ws, __uint8_t bit)
{
    switch (h1ws->ROM_State)
    {
    case ONEWIRE_READING_COMMAND: // Read commands (first byte after reset)
        // store bit in receive buffer
        h1ws->ReceiveBuffer |= (h1ws->ReceiveBuffer_BitPos & ((bit) ? (__uint8_t)0xFF : (__uint8_t)0x00));

        // advance buffer to next bit
        h1ws->ReceiveBuffer_BitPos = h1ws->ReceiveBuffer_BitPos << 1; // LSB byte order!
        if (!h1ws->ReceiveBuffer_BitPos)                              // buffer is full
        {
            h1ws->ROM_State = ONEWIRE_READING_BITS;
            OneWire_Received_Command(h1ws);
            h1ws->ReceiveBuffer = 0;
            h1ws->ReceiveBuffer_BitPos = (__uint8_t)0x01; // data is sent LSB first in 1-wire
        }
        break;
    case ONEWIRE_READING_BITS: // Read payload data
        // store bit in receive buffer
        h1ws->ReceiveBuffer |= (h1ws->ReceiveBuffer_BitPos & ((bit) ? (__uint8_t)0xFF : (__uint8_t)0x00));

        // advance buffer to next bit
        h1ws->ReceiveBuffer_BitPos = h1ws->ReceiveBuffer_BitPos << 1; // LSB byte order!
        if (!h1ws->ReceiveBuffer_BitPos)                              // buffer is full
        {
            h1ws->ROM_State = ONEWIRE_READING_BITS;
            OneWire_Byte_Received_Callback(h1ws, h1ws->ReceiveBuffer);
            h1ws->ReceiveBuffer = 0;
            h1ws->ReceiveBuffer_BitPos = (__uint8_t)0x01; // data is sent LSB first in 1-wire
        }
        break;
    case ONEWIRE_MATCH_ROM:
        if ((bit && (h1ws->Init.ROM_Address & h1ws->ROM_Mask)) || (!bit && !(h1ws->Init.ROM_Address & h1ws->ROM_Mask))) // bit and ROM bit do match
        {
            h1ws->ROM_Mask = h1ws->ROM_Mask << 1;
            if (!h1ws->ROM_Mask) // whole ROM has been compared
            {
                // Listen for next byte
                h1ws->ROM_State = ONEWIRE_READING_BITS;
            }
        }
        else
        {
            h1ws->ROM_State = ONEWIRE_WAIT; // means: match failed -> slave should shut up until next reset
        }
        break;
    case ONEWIRE_ALARM_SEARCH: // we assume we are never alarmed because we don't make errors during commands :P
    case ONEWIRE_SEARCH_ROM:
        if ((bit && (h1ws->Init.ROM_Address & h1ws->ROM_Mask)) || (!bit && !(h1ws->Init.ROM_Address & h1ws->ROM_Mask))) // bits do match
        {
            h1ws->ROM_Mask = h1ws->ROM_Mask << 1;
            if (!h1ws->ROM_Mask) // whole ROM has been compared
            {
                // Listen for next byte
                h1ws->ROM_State = ONEWIRE_READING_BITS;
            } else {
                // write next LSB bit of ROM and its complement to bus
                h1ws->Internal_Buffer[0] = (h1ws->Init.ROM_Address & h1ws->ROM_Mask) ? (__uint8_t)0x40 : (__uint8_t)0x80;
                h1ws->SendDataBuffer = h1ws->Internal_Buffer;
                h1ws->SendDataBuffer_Length = 1;
                h1ws->SendDataBuffer_Pos = 0;
                h1ws->SendDataBuffer_BitPos = (__uint8_t)0x40;
                h1ws->LL_State = ONEWIRE_W_IDLE;
            }
        }
        else
        {
            h1ws->ROM_State = ONEWIRE_WAIT; // means: match failed -> slave should shut up until next reset
        }
        break;
    case ONEWIRE_WAIT: // wait until next reset
        h1ws->ROM_State = ONEWIRE_WAIT;
        break;
    }

    OneWire_Bit_Received_Callback(h1ws, bit);
}

void OneWire_Process_Reset_Signal(OneWireSlave_HandleTypeDef *h1ws)
{
    h1ws->ROM_State = ONEWIRE_READING_COMMAND;

    // reset everything
    h1ws->ReceiveBuffer = 0;
    h1ws->ReceiveBuffer_BitPos = (__uint8_t)0x01;
    h1ws->SendDataBuffer_Pos = 0;
    h1ws->SendDataBuffer_BitPos = (__uint8_t)0x01;
    h1ws->SendDataBuffer_Length = 0;

    // invoke reset callback
    OneWire_Reset_Received_Callback(h1ws);
}

// Returns the next bit to be sent
inline __uint8_t Get_Current_Bit_To_Send(OneWireSlave_HandleTypeDef *h1ws)
{
    __uint8_t next_bit = h1ws->SendDataBuffer[h1ws->SendDataBuffer_Pos] & h1ws->SendDataBuffer_BitPos;

    // Advance to next bit

    return next_bit;
}

// Returns true, if there are still bits that need to be sent.
inline __uint8_t Advance_To_Next_Bit_In_Buffer(OneWireSlave_HandleTypeDef *h1ws)
{
    h1ws->SendDataBuffer_BitPos = h1ws->SendDataBuffer_BitPos << 1; // LSB byte order!
    if (!h1ws->SendDataBuffer_BitPos)                               // we need to go to the next byte
    {
        if (h1ws->SendDataBuffer_Pos + 1 < h1ws->SendDataBuffer_Length)
        {
            h1ws->SendDataBuffer_Pos++;
            h1ws->SendDataBuffer_BitPos = (__uint8_t)0x01; // dats is sent LSB first in 1-wire
        }
        else
        {
            return 0; // done sending
        }
    }
    return 1; // still more bits in the buffer
}












//************************************
//          LINK LAYER
//    PROTOCOL STATE MACHINE
//************************************

// Returns true, if there are more bits to be sent.
inline void Send_Next_Bit(OneWireSlave_HandleTypeDef *h1ws)
{
    if (Get_Current_Bit_To_Send(h1ws))
    { // Send a "1"
        // we don't have to do anything here -> 1 is implicit
    }
    else
    { // Send a "0"
        Send_Signal(h1ws->Init.Pin, 46);
    }
}

/*
 * @params h1ws: handle for the active OneWire interface.
 * @params pin_state: indicates, whether the lin is low (=set) or high (=reset)
 *                    after an interrupt has been received.
 */
void Process_Communation_Protocol(OneWireSlave_HandleTypeDef *h1ws, OneWire_Pin_State pin_state)
{
    switch (h1ws->LL_State)
    {
    case ONEWIRE_R_IDLE:
        if (pin_state == PIN_LOW) // Master initiates communication
        {
            // save timestamp of message initiation
            Start_Time_Meassurement();
            h1ws->LL_State = ONEWIRE_MASTER_SENDS_DATA;
        }
        else
        {
            // error
        }
        break;
    case ONEWIRE_MASTER_SENDS_DATA:
        if (pin_state == PIN_HIGH) // Master finished transmitting signal
        {
            __uint32_t time_elapsed = Get_Elapsed_Time_In_Microseconds();
            
            if (time_elapsed <= 100) // Master sent a bit
            {
                __uint8_t bit = 0;
                if (time_elapsed < 20)
                {
                    bit = 1; // = master sent "1"
                }
                else
                {
                    bit = 0; // = master sent "0"
                }
                h1ws->LL_State = ONEWIRE_R_IDLE;
                Process_Received_Bit(h1ws, bit);
                break;
            }
            // else: master sent a RESET signal -> go to next state immediatelly

            // compoare with timestamp. The signal was either a "1", a "0" or a "RESET"
            // make sure to call according callbacks
            // if it was a RESET signal, fall through to the next case
        }
        else
        {
            // error
        }
    case ONEWIRE_RESET:
        goto_reset_state:
        if (pin_state == PIN_HIGH) // Reset signal by master is over. We now need to send our presence signal.
        {
            // send presence signal so master knows there are devices
            Send_Signal(h1ws->Init.Pin, 100);

            OneWire_Process_Reset_Signal(h1ws);

            h1ws->LL_State = ONEWIRE_SENDING_PRESENCE;
        }
        else
        {
            // error
        }
        break;
    case ONEWIRE_SENDING_PRESENCE:
        if (pin_state == PIN_LOW) // our own presence signal started
        {
            h1ws->LL_State = ONEWIRE_SENDING_PRESENCE;
        }
        else // our own presence signal is over
        {
            h1ws->LL_State = ONEWIRE_R_IDLE;
        }
        break;
    case ONEWIRE_W_IDLE:
        if (pin_state == PIN_LOW) // Master requests data
        {
            Start_Time_Meassurement();
            Send_Next_Bit(h1ws);
            h1ws->LL_State = ONEWIRE_WRITING;
        }
        else
        {
            // error
        }
        break;
    case ONEWIRE_WRITING:
        if (pin_state == PIN_HIGH) {
            __uint32_t time_elapsed = Get_Elapsed_Time_In_Microseconds();
            
            if (time_elapsed > 300)
            { // we trapped into a reset signal
                goto goto_reset_state;
            }

            if (Advance_To_Next_Bit_In_Buffer(h1ws))
            {
                h1ws->LL_State = ONEWIRE_W_IDLE;
            }
            else
            {
                h1ws->LL_State = ONEWIRE_R_IDLE;
            }
        }
        break;
    }
}

// This function is called when there was an interrupt on the
// corresponding GPIO pin (raising or falling edge).
void OneWire_Interrupt_Callback(OneWireSlave_HandleTypeDef *h1ws, OneWire_Pin_State pin_state)
{
    // invoke protocol state machine
    Process_Communation_Protocol(h1ws, pin_state);
}











//************************************
//          PHYSICAL LAYER
//    LOW LEVEL INTERRUPT HANDLING
//  NEEDS TO BE IMPLEMENTED BY USER
//************************************

void Send_Signal(__uint32_t Pin, __uint32_t duration_in_us)
{
    // in our case we connected two pins to the 1-wire bus:
    // One which receives the interrupts and one for sending signals
    // Because we also have just one 1-wire slave running on this device,
    // we can simply ignore the pin given as a parameter and use our default
    // output pin.

    HAL_GPIO_WritePin(OneWireOutput_GPIO_Port, OneWireOutput_Pin, GPIO_PIN_RESET);

    // keep bus low for specified time
    Start_Time_Meassurement();
    __uint32_t targetTick = Get_Elapsed_Time_In_Microseconds() + duration_in_us;
    while (Get_Elapsed_Time_In_Microseconds() <= targetTick)
        ;

    HAL_GPIO_WritePin(OneWireOutput_GPIO_Port, OneWireOutput_Pin, GPIO_PIN_SET);
}

// Interrupt handler for GPIO pins invoked by the processor.
// This function works for pins connected to the GPIOB
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // disable interrupts
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

    GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOB, GPIO_Pin);

    // Cool, we received an interrupt at one of our pins.
    // Now we need to check if this pin is associated to one
    // of our OneWire instances.
    // If so, this instance can handle the callback.
    for (int i = 0; i < MAX_ONEWIRE_INSTANCES; i++)
    {
        if ((__uint16_t)OneWireInstances[i]->Init.Pin == GPIO_Pin)
        {
            OneWire_Interrupt_Callback(OneWireInstances[i], (pin_state)? PIN_HIGH : PIN_LOW);
            break;
        }
    }

    // enable interrupts again
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Start_Time_Meassurement(void) {
    TIM4->CNT = 0;
}

__uint32_t Get_Elapsed_Time_In_Microseconds(void) {
    return TIM4->CNT;
}

OneWire_Pin_State Get_Pin_State(__uint32_t Pin) {
    __uint16_t HAL_GPIOx = (Pin >> 16);
    __uint16_t HAL_Pin = Pin & 0x0000FFFF;

    if(HAL_GPIO_ReadPin(HAL_GPIOx, HAL_Pin)) {
        return PIN_HIGH;
    } else {
        return PIN_LOW;
    }
}
