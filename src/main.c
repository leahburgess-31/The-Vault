/**
 * @brief ECE198 The Vault Project 
 * @author Kelly Mak:
    * @passcode setup 
    * @led lights 
    * @LCd display 
    * @timer 
* @author Leah Burgeess:
    * @ servo motor 
    * @4 digit 7 segment display
    * @joystick 
    * @buzzer
 * 
 */

#include <stdbool.h> // booleans, i.e. true and false
#include <stdio.h>   // sprintf() function
#include <stdlib.h>  // srand() and random() functions

#include "ece198.h" // import ece198 library
#include "LiquidCrystal.h" // import LiquidCrystal Library


int main(void)
{
    HAL_Init(); // initialize the Hardware Abstraction Layer

    // Peripherals (including GPIOs) are disabled by default to save power, so we
    // use the Reset and Clock Control registers to enable the GPIO peripherals that we're using.

    __HAL_RCC_GPIOA_CLK_ENABLE(); // enable port A (for the on-board LED, for example)
    __HAL_RCC_GPIOB_CLK_ENABLE(); // enable port B (for the rotary encoder inputs, for example)
    __HAL_RCC_GPIOC_CLK_ENABLE(); // enable port C (for the on-board blue pushbutton, for example)

    // passcode setup 
    int passcode [4] = {0,0,0,0};  
    // get tick for randomizer 
    bool randInitialized = false;
    if (!randInitialized){
        srand(HAL_GetTick());
    }
    // get random number for each digit 
    for (int i = 0; i < 4; i++){
        passcode[i] = rand()%10;
    }
    // get the 4 digits from array 
    int result = passcode[0]+passcode[1] *10+passcode[2]*100+passcode[3]*1000;

// conversion from decimal to binary 
    char b1[20];
    itoa(passcode[0], b1, 2);
    char b2 [20];
    itoa(passcode[1], b2, 2);
    char b3 [20];
    itoa(passcode[2], b3, 2);
    char b4 [20];
    itoa(passcode[3], b4, 2);
 
 // conversion from binary to character for lcd display 
    char pass[20];
    char pass1[20];
    char pass2[20];
    char pass3[20];
    sprintf(pass, "%d", passcode[0]);
    sprintf(pass1, "%d", passcode[1]);
    sprintf(pass2, "%d", passcode[2]);
    sprintf(pass3, "%d", passcode[3]);

    // Initializing the pinouts for the components
    Initialize4Digit7Segment(); //4 digits 7 segment display
    InitializeJoystick(); // joystick 
    InitializeBuzzer(); //buzzer 
    InitializePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1,GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0); // led lightbulb (green, red)
    LiquidCrystal(GPIOB, GPIO_PIN_3, GPIO_PIN_0,GPIO_PIN_4, GPIO_PIN_10,GPIO_PIN_6,GPIO_PIN_9,GPIO_PIN_0); // Lcd display

    // servo motor 
    uint16_t period = 9999, prescale = 15; // period and prescale

    __TIM2_CLK_ENABLE();  // enable timer 2
    TIM_HandleTypeDef pwmTimerInstance;  // this variable stores an instance of the timer
    InitializePWMTimer(&pwmTimerInstance, TIM2, period, prescale);   // initialize the timer instance
    InitializePWMChannel(&pwmTimerInstance, TIM_CHANNEL_2);          // initialize one channel (can use others for motors, etc)
    InitializePin(GPIOB, GPIO_PIN_3, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_AF1_TIM2); // connect the motor to the timer output

    // lcd display show words 
    print("Convert to Decimal:"); // first row 
    setCursor(0,1); // second row 
    print(b1);
    print(" ");
    print(b2);
    print(" ");
    print(b3);
    print(" ");
    print(b4);
    
    // scrolling through the boxes in the lcd display
    for (int positionCounter = 0; positionCounter < 13; positionCounter++) {
        // scroll one position left:
        scrollDisplayLeft();
        // wait a bit:
        HAL_Delay(350);
    }
    // scroll 29 positions (string length + display length) to the right

    // to move it offscreen right:

    for (int positionCounter = 0; positionCounter < 29; positionCounter++) {
        // scroll one position right:
        scrollDisplayRight();
        // wait a bit:
        HAL_Delay(350);
    }

    // scroll 16 positions (display length + string length) to the left

    // to move it back to center:

    for (int positionCounter = 0; positionCounter < 16; positionCounter++) {
        // scroll one position left:
        scrollDisplayLeft();
        // wait a bit:
        HAL_Delay(350);
    }
    
    // declaring variables 
    bool prvUp = 0; 
    bool prvRight = 0;
    bool prvButton = 0;
    int deltaUp = 0; // if the joystick is pushed up 
    int deltaRight = 0; // if joystick is pushed to the right 
    int deltaButton = 0; // if the button is pressed
    int count = 0; // counter
    int pos = 0;  // position
    int arr[4] = {0,0,0,0}; // 4 digit 7 segment array 
    int time = 180000; // timer for 3 minutes

    while (HAL_GetTick() < time){ // while the timer has not reached to 3 minutes

    // joystick input 
        deltaUp = InputNumberup(&prvUp);
        deltaRight = InputNumberright(&prvRight);
        deltaButton = buttonPushed(&prvButton);
        
        if (deltaUp != 0) {
            arr[pos] += 1;
            arr[pos] = arr[pos]%10; 
        }

        if (deltaRight != 0) {
            pos += 1;
            pos = pos%4;
        } 
        // get ccount from input and show on 7 segment display
        count = arr[0]+arr[1]*10+arr[2]*100+arr[3]*1000;
        Display4DigitInt7Segment(count); 

        // if the input passcode is correct
        if (count == result){
            // green led lightbulb will turn on 
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, true);
            HAL_Delay(250);  // 250 milliseconds == 1/4 second

        // buzzer is turn on 
            playTone(3,50);
            playTone(2,100);

        // servo motor is on 
        for (uint32_t i = 0; i < period; ++i){
        SetPWMDutyCycle(&pwmTimerInstance, TIM_CHANNEL_2, i);
        HAL_Delay(1);
        } 
        // terminate the function
        break;
    }
}
    // if the timer is over
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, true); // red light is on 
    HAL_Delay(250); 
    clear(); // clear board
    print("resetting");// print on first row in lcd display
    main(); // restart the code 
    return 0;
}

// This function is called by the HAL once every millisecond
void SysTick_Handler(void)
{
    HAL_IncTick(); // tell HAL that a new tick has happened
    // we can do other things in here too if we need to, but be careful
}