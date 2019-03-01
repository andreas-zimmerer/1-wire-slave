#ifndef __ONE_WIRE_SLAVE_H__
#define __ONE_WIRE_SLAVE_H__

#ifdef __cplusplus
extern "C"
{
#endif

//--------------------
// GLOBAL CONFIG
//--------------------
#define MAX_ONEWIRE_INSTANCES 1 // Maximum number of OneWire instances handled by this lib. Less is of course a little faster and requires less memory.

    /*
     * Internal Eum: you probably don't need to touch this. Ever.
     * It represents the states of the slave on the Link Layer.
     * References:
     *  - https://www.maximintegrated.com/en/app-notes/index.mvp/id/126
     */
    typedef enum
    {
        ONEWIRE_R_IDLE,             // Default. Slave is listening for new signals from the master
        ONEWIRE_MASTER_SENDS_DATA,  // The master is just sending data and we need to listen if it's a '0', a '1' or a 'RESET'
        ONEWIRE_W_IDLE,             // Default state when slave wants to send data. We need to wait until the master requests new data.
        ONEWIRE_WRITING,            // We are just about to send data
        ONEWIRE_RESET,              // The master just sent a 'RESET' signal!
        ONEWIRE_SENDING_PRESENCE,   // We are about to send a 'PRESENCE' signal as a reply to the 'RESET'
    } OneWire_LowLevel_State;

    /*
     * Internal Eum: you probably don't need to touch this. Ever.
     * It represents the states of the slave on the Network Layer.
     * References:
     *  - in-depth description of SEARCH ROM algorithm: https://www.maximintegrated.com/en/app-notes/index.mvp/id/187
     *  - iButton standard: https://pdfserv.maximintegrated.com/en/an/AN937.pdf
     */
    typedef enum
    {
        ONEWIRE_READING_BITS,       // We are just happily reading random bits from the master
        ONEWIRE_READING_COMMAND,    // We are reading bits - but as soon as we have one byte we will try to interpret it as a certain ROM command.
        ONEWIRE_MATCH_ROM,          // After the master initiated the MATCH ROM procedure, we need to react accordingly -> we need to shut up and compare the ROM sent by the master
        ONEWIRE_SEARCH_ROM,         // After the master initiated the SEARCH ROM procedure, we need to react accordingly -> we need to send our ROM (quite complex algorithm)
        ONEWIRE_ALARM_SEARCH,       // Right now, this behavior is implemented exactly as SEARCH ROM because being 'alarmed' is not supported by this lib (but can be added quite easily)
        ONEWIRE_WAIT,               // When MATCH ROM or SEARCH ROM did not succeed _for us_ then we need to stay quiet until the next 'RESET' signal.
    } OneWire_ROM_State;

    /*
     * This is important for using this library.
     * These are the expected values in Get_Pin_State() and OneWire_Interrupt_Callback() that you need to implement if you use another microcontroller.
     */
    typedef enum 
    {
        PIN_LOW = 0,                // LOW -> 1-wire bus is currently low ('active')
        PIN_HIGH,                   // HIGHT -> 1-wire bus is currently high ('idle')
    } OneWire_Pin_State;

    /*
     * Fields required for correct initilization of the OneWire slave interface!
     */
    typedef struct
    {
        __uint64_t ROM_Address; // The ROM address of this device [the library doesn't care if this is meaningful; but the master might look at the family code or other data]
        __uint32_t Pin;         // This pin will be used for asking the state (HIGH or LOW) of the 1-wire bus. [If it's just one pin: PullUp, with interrupt on falling and raising edge]. You can also connect two pins to the bus (e.g. one wire sending/output and one for receiving/interrupts)
    } OneWireSlave_InitTypeDef;

    /*
     * Internal working struct. You *can* write to it during runtime if you really need to modify
     * the fields. But really - think about it first if you really need to...
     */
    typedef struct __OneWireSlave_HandleTypeDef
    {
        OneWireSlave_InitTypeDef Init;
        OneWire_LowLevel_State LL_State;
        OneWire_ROM_State ROM_State;
        __uint8_t Internal_Buffer[8];
        __uint64_t ROM_Mask;
        __uint8_t *SendDataBuffer;
        __uint16_t SendDataBuffer_Length;
        __uint16_t SendDataBuffer_Pos;
        __uint8_t SendDataBuffer_BitPos;
        __uint8_t ReceiveBuffer;
        __uint8_t ReceiveBuffer_BitPos;
    } OneWireSlave_HandleTypeDef;

    /*
     * Initializes the OneWire interface. Make sure to pass meaningful data in the "Init" field
     * of OneWireSlave_HandleTypeDef (all other important fields are set by this function).
     * For a description on the values required look at @OneWireSlave_InitTypeDef.
     */
    void OneWireSlave_Init(OneWireSlave_HandleTypeDef *h1ws);

    /*
     * Deinitializes the OneWire interface.
     */
    void OneWireSlave_DeInit(OneWireSlave_HandleTypeDef *h1ws);

    /*
     * Implement this method to receive bytes from the master. Certain default actions regarding
     * the ROM are handled by this library. If the command code is not known, this method will be
     * invoked.
     * Here you can act accordingly. If you do nothing, the slave will stay in reception mode.
     * If you call @OneWire_Send instead, the slave will respond to the master.
     */
    void OneWire_Byte_Received_Callback(OneWireSlave_HandleTypeDef *source, __uint8_t byte);

    /*
     * Same as above, but for every single bit. You usually do not need to overwrite/handle this
     * except there is a certain bit-based protocol (outside of usual ROM commands) that you need
     * to handle.
     */
    void OneWire_Bit_Received_Callback(OneWireSlave_HandleTypeDef *source, __uint8_t bit);

    /*
     * Callback for 'RESET' signal. The library will handle all internal stuff.
     * But if you need to do something on a 'RESET' signal outside of this library, just
     * overwrite/implement it.
     */
    void OneWire_Reset_Received_Callback(OneWireSlave_HandleTypeDef *source);

    /*
     * Writes the message to be sent in the output buffer and changes the OneWire mode to "sending".
     * This means that the slave will respond with the message from the buffer when the master asks
     * for it.
     * The master can still cancel the sending of the message by sending a reset signal. Therefore,
     * this method does not ensure successful sending of the message. But only does this by
     * "best effort".
     */
    void OneWire_Send(OneWireSlave_HandleTypeDef *h1ws, __uint8_t *message, __uint16_t message_length);

    /*
     * Same as above, but just for sending one single bit.
     * Again, you usually don't need this, except there is a procedure/message that requires single bits.
     */
    void OneWire_SendBit(OneWireSlave_HandleTypeDef *h1ws, __uint8_t bit);


    /******************************
     * The following functions are platform specific and need to be implemented for every processor.
     * They are very low level and don't do much, but this library depends on functionality provided
     * by these functions.
     * In short, the following functions need to be implemented by the user of this library because
     * they are processor-specific:
     * 
     *  - void Send_Signal(__uint32_t Pin, __uint32_t duration_in_us)
     *  - void Start_Time_Meassurement(void)
     *  - __uint32_t Get_Elapsed_Time_In_Microseconds(void)
     *  - OneWire_Pin_State Get_Pin_State(__uint32_t Pin)
     * 
     * Furthermore, the following functions needs to be called by the user of this library when
     * there is an interrupts for a falling or raising edge on the 1-wire pin (e.g. the user
     * needs to implement the processor-specific interrupt routine):
     * 
     *  - void OneWire_Interrupt_Callback(OneWireSlave_HandleTypeDef *h1ws, OneWire_Pin_State pin_state)
     * 
     ******************************/

    // This function needs to be called when there is a falling OR a raising edge on the 1-wire pin.
    // Note that it should also be called on interrupts observed by our own signals.
    void OneWire_Interrupt_Callback(OneWireSlave_HandleTypeDef *h1ws, OneWire_Pin_State pin_state);

    // This function should pull our 1-wire pin low for a given duration in microseconds.
    // Whether this function is implemented synchronously or asynchronously is up to you,
    // but note that the rest of this library is implemented async.
    void Send_Signal(__uint32_t Pin, __uint32_t duration_in_us);

    // Starts meassuring of time.
    // When this message is called, the time is assumed to be 0.
    // Therefore, a call to Get_Elapsed_Time_In_Microseconds() meassures the time since the last call
    // to this function.
    void Start_Time_Meassurement(void);

    // Returns a timestamp in microseconds since the last call to Start_Time_Meassurement().
    __uint32_t Get_Elapsed_Time_In_Microseconds(void);

    // Returns the state of the given pin.
    OneWire_Pin_State Get_Pin_State(__uint32_t Pin);


#ifdef __cplusplus
}
#endif

#endif /* __ONE_WIRE_SLAVE_H__ */
