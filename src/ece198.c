// Support routines for ECE 198 (University of Waterloo)

// Written by Bernie Roehl, July 2021

#include <stdbool.h>  // for bool datatype

#include "ece198.h"

///////////////////////
// Initializing Pins //
///////////////////////

// Initialize a pin (or pins) to a particular mode, with optional pull-up or pull-down resistors
// and possible alternate function
// (we use this so students don't need to know about structs)

void InitializePin(GPIO_TypeDef *port, uint16_t pins, uint32_t mode, uint32_t pullups, uint8_t alternate)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Pin = pins;
    GPIO_InitStruct.Mode = mode;
    GPIO_InitStruct.Pull = pullups;
    GPIO_InitStruct.Alternate = alternate;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

/////////////////
// Serial Port //
/////////////////

UART_HandleTypeDef UART_Handle;  // the serial port we're using

// initialize the serial port at a particular baud rate (PlatformIO serial monitor defaults to 9600)

HAL_StatusTypeDef SerialSetup(uint32_t baudrate)
{
    __USART2_CLK_ENABLE();        // enable clock to USART2
    __HAL_RCC_GPIOA_CLK_ENABLE(); // serial port is on GPIOA

    GPIO_InitTypeDef GPIO_InitStruct;

    // pin 2 is serial RX
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // pin 3 is serial TX (most settings same as for RX)
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // configure the serial port
    UART_Handle.Instance = USART2;
    UART_Handle.Init.BaudRate = baudrate;
    UART_Handle.Init.WordLength = UART_WORDLENGTH_8B;
    UART_Handle.Init.StopBits = UART_STOPBITS_1;
    UART_Handle.Init.Parity = UART_PARITY_NONE;
    UART_Handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    UART_Handle.Init.Mode = UART_MODE_TX_RX;

    return HAL_UART_Init(&UART_Handle);
}

// wait for a character to arrive from the serial port, then return it

char SerialGetc()
{
    while ((UART_Handle.Instance->SR & USART_SR_RXNE) == 0)
        ;                             // wait for the receiver buffer to be Not Empty
    return UART_Handle.Instance->DR;  // return the incoming character
}

// send a single character out the serial port

void SerialPutc(char c)
{
    while ((UART_Handle.Instance->SR & USART_SR_TXE) == 0)
        ;                          // wait for transmitter buffer to be empty
    UART_Handle.Instance->DR = c;  // send the character
}

// write a string of characters to the serial port

void SerialPuts(char *ptr)
{
    while (*ptr)
        SerialPutc(*ptr++);
}

// get a string of characters (up to maxlen) from the serial port into a buffer,
// collecting them until the user presses the enter key;
// also echoes the typed characters back to the user, and handles backspacing

void SerialGets(char *buff, int maxlen)
{
    int i = 0;
    while (1)
    {
        char c = SerialGetc();
        if (c == '\r') // user pressed Enter key
        {
            buff[i] = '\0';
            SerialPuts("\r\n"); // echo return and newline
            return;
        }
        else if (c == '\b') // user pressed Backspace key
        {
            if (i > 0)
            {
                --i;
                SerialPuts("\b \b"); // overwrite previous character with space
            }
        }
        else if (i < maxlen - 1) // user pressed a regular key
        {
            buff[i++] = c;  // store it in the buffer
            SerialPutc(c);  // echo it
        }
    }
}


////////////////////////////
// Pulse Width Modulation //
////////////////////////////

// set up a specified timer with the given period (whichTimer might be TIM2, for example)

void InitializePWMTimer(TIM_HandleTypeDef *timer, TIM_TypeDef *whichTimer, uint16_t period, uint16_t prescale )
{
    timer->Instance = whichTimer;
    timer->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->Init.Prescaler = prescale;
    timer->Init.Period = period;
    timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->Init.RepetitionCounter = 0;
    HAL_TIM_PWM_Init(timer);
}

// set up a particular channel (e.g. TIM_CHANNEL_1) on the given timer instance

void InitializePWMChannel(TIM_HandleTypeDef *timer, uint32_t channel)
{
    TIM_OC_InitTypeDef outputChannelInit;
    outputChannelInit.OCMode = TIM_OCMODE_PWM1;
    outputChannelInit.OCPolarity = TIM_OCPOLARITY_HIGH;
    outputChannelInit.OCFastMode = TIM_OCFAST_ENABLE;
    outputChannelInit.Pulse = timer->Init.Period;
    HAL_TIM_PWM_ConfigChannel(timer, &outputChannelInit, channel);
    HAL_TIM_PWM_Start(timer, channel);
}

// set the number of ticks in a cycle (i.e. <= the timer's period) for which the output should be high

void SetPWMDutyCycle(TIM_HandleTypeDef *timer, uint32_t channel, uint32_t value)
{
    switch (channel)
    {
    case TIM_CHANNEL_1:
        timer->Instance->CCR1 = value;
        break;
    case TIM_CHANNEL_2:
        timer->Instance->CCR2 = value;
        break;
    case TIM_CHANNEL_3:
        timer->Instance->CCR3 = value;
        break;
    case TIM_CHANNEL_4:
        timer->Instance->CCR4 = value;
        break;
    }
}

/////////////////////
// Keypad Scanning //
/////////////////////

struct { GPIO_TypeDef *port; uint32_t pin; }
rows[] = {
    { GPIOC, GPIO_PIN_7 },
    { GPIOA, GPIO_PIN_9 },
    { GPIOA, GPIO_PIN_8 },
    { GPIOB, GPIO_PIN_10 }
},
cols[] = {
    { GPIOB, GPIO_PIN_4 },
    { GPIOB, GPIO_PIN_5 },
    { GPIOB, GPIO_PIN_3 },
    { GPIOA, GPIO_PIN_10 }
};

void InitializeKeypad() {
    // rows are outputs, columns are inputs and are pulled low so they don't "float"
    for (int i = 0; i < 4; ++i) {
        InitializePin(rows[i].port, rows[i].pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
        InitializePin(cols[i].port, cols[i].pin, GPIO_MODE_INPUT, GPIO_PULLDOWN, 0);
     }
}

int ReadKeypad() {
    // scan a 4x4 key matrix by applying a voltage to each row in succession and seeing which column is active
    // (should work with a 4x3 matrix, since last column will just return zero)
    for (int row = 0; row < 4; ++row) {
        // enable the pin for (only) this row
        for (int i = 0; i < 4; ++i)
            HAL_GPIO_WritePin(rows[i].port, rows[i].pin, i == row);  // all low except the row we care about
        for (int col = 0; col < 4; ++col)  // check all the column pins to see if any are high
            if (HAL_GPIO_ReadPin(cols[col].port, cols[col].pin))
                return row*4+col;
    }
    return -1;  // none of the keys were pressed
}

///////////////////////
// 7-Segment Display //
///////////////////////

struct { GPIO_TypeDef *port; uint32_t pin; }
segments[] = {
    { GPIOC, GPIO_PIN_3 },  // A
    { GPIOC, GPIO_PIN_2 },  // B
    { GPIOC, GPIO_PIN_10 },  // C
    { GPIOC, GPIO_PIN_12 },  // D
    { GPIOA, GPIO_PIN_13 },  // E
    { GPIOA, GPIO_PIN_14 },  // F
    { GPIOC, GPIO_PIN_13 },  // G
    { GPIOB, GPIO_PIN_7 },  // H (also called DP)
};
/*
struct {GPIO_TypeDef *port; uint32_t pin;}
segments4Digit[] = {
    {GPIOC, GPIO_PIN_9 }, //A
    {GPIOC, GPIO_PIN_6 }, //B
    {GPIOC, GPIO_PIN_8}, //C
    {GPIOB, GPIO_PIN_3 }, //D
    {GPIOB, GPIO_PIN_5 }, //E
    {GPIOB, GPIO_PIN_8 }, //F
    {GPIOB, GPIO_PIN_10}, //G
    {GPIOA, GPIO_PIN_9 }, //DP
    {GPIOA, GPIO_PIN_10 }, //D1
    {GPIOB, GPIO_PIN_0 }, //D2
    {GPIOC, GPIO_PIN_1 }, //D3
    {GPIOC, GPIO_PIN_0 }, //D4
};*/
struct {GPIO_TypeDef *port; uint32_t pin;}
segments4Digit[] = {
    {GPIOA, GPIO_PIN_12 }, //A
    {GPIOC, GPIO_PIN_9 }, //B
    {GPIOB, GPIO_PIN_1}, //C
    {GPIOB, GPIO_PIN_14 }, //D
    {GPIOB, GPIO_PIN_13 }, //E
    {GPIOC, GPIO_PIN_5 }, //F
    {GPIOB, GPIO_PIN_2}, //G
    {GPIOB, GPIO_PIN_15 }, //DP
    {GPIOA, GPIO_PIN_11 }, //D1
    {GPIOC, GPIO_PIN_6 }, //D2
    {GPIOC, GPIO_PIN_8 }, //D3
    {GPIOB, GPIO_PIN_12 }, //D4
};
// for each digit, we have a byte (uint8_t) which stores which segments are on and off
// (bits are ABCDEFGH, right to left, so the low-order bit is segment A)
uint8_t digitmap[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07, 0x7F, 0x67 };

void Initialize7Segment() {
    for (int i = 0; i < 8; ++i)
        InitializePin(segments[i].port, segments[i].pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
}

void Initialize4Digit7Segment() {
    for(int i = 0; i < 12; ++i)
        InitializePin(segments4Digit[i].port, segments4Digit[i].pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
}

void Display7Segment(int digit) {
    int value = 0;  // by default, don't turn on any segments
    if (digit >= 0 && digit <= 9)  // see if it's a valid digit
        value = digitmap[digit];   // convert digit to a byte which specifies which segments are on
    //value = ~value;   // uncomment this line for common-anode displays
    // go through the segments, turning them on or off depending on the corresponding bit
    for (int i = 0; i < 8; ++i)
        HAL_GPIO_WritePin(segments[i].port, segments[i].pin, (value >> i) & 0x01);  // move bit into bottom position and isolate it
}


// This code is the working function
/*
void Display4Digit7Segment(int digit) {
    int value = 0;
    //int i = 0;
    if (digit >= 0 && digit <= 9) {
       // HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 0);
       // HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 0);

        value = digitmap[digit]; 
        for(int i = 0; i < 4; ++i){
            HAL_GPIO_WritePin(segments4Digit[i+8].port, segments4Digit[i+8].pin, 0);
            value = ~value;
            for (int j = 0; j < 8; ++j){
                HAL_GPIO_WritePin(segments4Digit[j].port, segments4Digit[j].pin, (value >> j) & 0x01);
            }
        }
    }
}
*/

void Display4Digit7Segment(int digit) {
    int value = 0;
    HAL_Delay(100);
    //int i = 0;
    if (digit >= 0 && digit <= 9) {
       
        HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 1);
        HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 1);
        
        
        value = digitmap[digit];             
        for(int i = 0; i < 4; ++i){                
            HAL_GPIO_WritePin(segments4Digit[i+8].port, segments4Digit[i+8].pin, 0);                
            value = ~value;
            for (int j = 0; j < 8; ++j){
                HAL_GPIO_WritePin(segments4Digit[j].port, segments4Digit[j].pin, (value >> j) & 0x01);
            }                
        }
    }
}


void Display4DigitInt7Segment(int num) {
    if (num >= 0 && num <= 9999){
       
        int unit = 0;
        int ten = 0;
        int hun = 0;
        int thou = 0;
       
        unit = num%10;
        num = num/10;
        ten = num%10;
        num = num/10;
        hun = num%10;
        num = num/10;
        thou = num;
     /*
        char buff[100];
        sprintf()

        
        SerialPuts("5");
        SerialPuts("6");
        SerialPuts("7");
        SerialPuts("8");
*/
        int unitvalue = 0;
        int tenvalue = 0;
        int hunvalue = 0;
        int thouvalue =0;
        unitvalue = digitmap[unit];
        tenvalue =digitmap[ten];
        hunvalue = digitmap[hun];
        thouvalue = digitmap[thou];


        // for(int i = 0; i < 4; i++){
        //     HAL_GPIO_WritePin(segments4Digit[i+8].port, segments4Digit[i+8].pin, 0);
        // }
        //     HAL_Delay(1000);
           
            HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 0);
            HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 1);
            for(int i = 0; i < 8; ++i){ 
                HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (unitvalue >> i) & 0x01);
            } 
            HAL_Delay(1);

            HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 0);
            HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 1);
            for(int i = 0; i < 8; ++i){ 
                 HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (tenvalue >> i) & 0x01);    
            }     
            HAL_Delay(1);

            HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 0);
            HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 1);
            for(int i = 0; i < 8; ++i){ 
                HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (hunvalue >> i) & 0x01);   
            }    
            HAL_Delay(1);

            HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 0);
            for(int i = 0; i < 8; ++i){ 
                HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (thouvalue >> i) & 0x01);   
            }       
            HAL_Delay(1);
/*
            HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (unitvalue >> i) & 0x01);
           // HAL_Delay(1000);
            HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 0);

            HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (tenvalue >> i) & 0x01);
           // HAL_Delay(1000);
            HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 0);

            HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (hunvalue >> i) & 0x01);
           // HAL_Delay(1000);
            HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 0);

            HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 1);
            HAL_GPIO_WritePin(segments4Digit[i].port, segments4Digit[i].pin, (thouvalue >> i) & 0x01);
           // HAL_Delay(1000);
            HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 0);

          //  HAL_GPIO_WritePin(segments4Digit[ten].port, segments4Digit[ten].pin, (tenvalue >> ten) & 0x01);
           // HAL_GPIO_WritePin(segments4Digit[hun].port, segments4Digit[hun].pin, (hunvalue >> hun) & 0x01);
          //  HAL_GPIO_WritePin(segments4Digit[thou].port, segments4Digit[thou].pin, (thouvalue >> thou) & 0x01);
           */
           
            
    
        
            


       // HAL_GPIO_WritePin(segments4Digit[unit].port, segments4Digit[unit].pin, (unitvalue >> unit) & 0x01);

    }







}
/*
void Display4Digit7Segment(int digit) {
    int value = 0;
    HAL_GPIO_WritePin(segments4Digit[8].port, segments4Digit[8].pin, 1);
    HAL_GPIO_WritePin(segments4Digit[9].port, segments4Digit[9].pin, 0);
    HAL_GPIO_WritePin(segments4Digit[10].port, segments4Digit[10].pin, 0);
    HAL_GPIO_WritePin(segments4Digit[11].port, segments4Digit[11].pin, 0);

    if (digit >= 0 && digit <= 9) {
        value = digitmap[digit]; 
            value = value;
            //for (int j = 0; j < 8; ++j){
            HAL_GPIO_WritePin(segments4Digit[3].port, segments4Digit[3].pin, (value >> 3) & 0x01);
       // }
    }
}
*/
struct {GPIO_TypeDef *port; uint32_t pin;}
joyPins[] = {
    {GPIOA, GPIO_PIN_9}, //x direction
    {GPIOC, GPIO_PIN_7}, //y direction
    {GPIOA, GPIO_PIN_7}, //clickr switch
};


void InitializeJoystick() {
    for (int i = 0; i < 3; ++i){
        InitializePin(joyPins[i].port, joyPins[i].pin, GPIO_MODE_INPUT, GPIO_PULLDOWN, 0);
    }
    //printf("ok it worked maybe");
}



 bool InputNumberright(bool *prvRight){
     bool right = 0;

    if (-(HAL_GPIO_ReadPin(joyPins[1].port,joyPins[1].pin)-1) == 1){
        right = 1;
    } else {   
        right = 0;
    }


    if(right != *prvRight && right ==1){
        *prvRight = right;
        return 1; 
    }else{
        *prvRight = right;
        return 0; 
    }              

 }




bool InputNumberup(bool *prvup){
    bool up = 0;

    if ((-(HAL_GPIO_ReadPin(joyPins[0].port,joyPins[0].pin)-1) == 1)){
        up = 1;
    } else {
        up = 0;
    }

    if(up != *prvup && up ==1){
        *prvup = up;
        return 1; 
    }else{
        *prvup = up;
        return 0; 
    }              
}

bool buttonPushed(bool *prvButton){
    bool button = 0;

    if (HAL_GPIO_ReadPin(joyPins[2].port,joyPins[2].pin) == 1){
        button = 1;
    } else {
        button = 0;
    }

    if (button != *prvButton && button == 1){
        *prvButton = button;
        return 1;
    } else {
        *prvButton = button;
        return 0;
    }
}

void InitializeBuzzer() {
    InitializePin(GPIOB, GPIO_PIN_8, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 1);

}

void playTone(int pitch, int length) {
     for (int i = 0; i < length; i++){
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, 1);
            HAL_Delay(pitch);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, 0);
            HAL_Delay(pitch);
        }
}

void InitializeServo(){
    InitializePin(GPIOA, GPIO_PIN_10, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 1);
}


////////////////////
// Rotary Encoder //
////////////////////

// read a rotary encoder (handles the quadrature encoding)
//(uses a previousClk boolean variable provided by the caller)

int ReadEncoder(GPIO_TypeDef *clkport, int clkpin, GPIO_TypeDef *dtport, int dtpin, bool *previousClk)
{
    bool clk = HAL_GPIO_ReadPin(clkport, clkpin);
    bool dt = HAL_GPIO_ReadPin(dtport, dtpin);
    int result = 0;  // default to zero if encoder hasn't moved
    if (clk != *previousClk)           // if the clk signal has changed since last time we were called...
        result = dt != clk ? 1 : -1;   // set the result to the direction (-1 if clk == dt, 1 if they differ)
    *previousClk = clk;                // store for next time
    return result;
}



/////////
// ADC //
/////////

// Written by Rodolfo Pellizzoni, September 2021

void InitializeADC(ADC_HandleTypeDef* adc, ADC_TypeDef* whichAdc)  // whichADC might be ADC1, for example
{
    adc->Instance = whichAdc;
    adc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    adc->Init.Resolution = ADC_RESOLUTION_12B;
    adc->Init.ScanConvMode = DISABLE;
    adc->Init.ContinuousConvMode = DISABLE;
    adc->Init.DiscontinuousConvMode = DISABLE;
    adc->Init.NbrOfDiscConversion = 1;
    adc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->Init.NbrOfConversion = 1;
    adc->Init.DMAContinuousRequests = DISABLE;
    adc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(adc);
}

// read from the specified ADC channel

uint16_t ReadADC(ADC_HandleTypeDef* adc, uint32_t channel)  // channel might be ADC_CHANNEL_1 for example
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    HAL_ADC_ConfigChannel(adc, &sConfig);
    HAL_ADC_Start(adc);
    HAL_ADC_PollForConversion(adc, HAL_MAX_DELAY);
    uint16_t res = HAL_ADC_GetValue(adc);
    HAL_ADC_Stop(adc);
    return res;





}

