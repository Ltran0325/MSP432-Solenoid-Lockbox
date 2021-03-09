/*******************************************************************************
*                MSP432 Keypad Solenoid Lockbox Solution                       *
*                                                                              *
* Author:  Long Tran                                                           *
* Device:  MSP432P401R LaunchPad                                               *
* Program: Display input to keypad                                             *
*                                                                              *
* Important Ports:                                                             *
* P4 is OUTPUT for 7-seg display digit pattern                                 *
* P8 is OUTPUT to control active digits in row                                 *
* P9 is INPUT from keypad column                                               *
*                                                                              *
* P2 is OUTPUT to lock solenoid                                                *
* P5 is OUTPUT to red LED (on during lock mode                                 *
*                                                                              *
* Demo: https://www.youtube.com/watch?v=xvXOEY5Ds3I                            *
*******************************************************************************/

// Include header file(s) and define constants
#include "msp.h"
#include <stdbool.h>
#define N 100                   // debounce loop count
#define BLANK 16                // digit-bit index to clear display
#define OPEN_KEY 10             // open key is 'A' key
#define LOCK_KEY 11             // lock key is 'B' key
#define FIVE_SEC 12000          // 5sec delay

// Define digit-bit lookup table
const uint8_t digit_array[17] = {
0b11000000,  // 0
0b11111001,  // 1
0b10100100,  // 2
0b10110000,  // 3
0b10011001,  // 4
0b10010010,  // 5
0b10000010,  // 6
0b11111000,  // 7
0b10000000,  // 8
0b10010000,  // 9
0b10001000,  // A
0b10000011,  // b
0b11000110,  // C
0b10100001,  // d
0b11000111,  // L
0b11110111,  // _
0b11111111,  // BLANK
};

// Define 4x4 keypad layout
const uint8_t keypad_table[4][9] = {
    {BLANK, OPEN_KEY,  3, BLANK, 2, BLANK, BLANK, BLANK,  1},     // 4x9 multiplixer keypad
    {BLANK, LOCK_KEY,  6, BLANK, 5, BLANK, BLANK, BLANK,  4},     // only columns 1, 2, 4, 8 are valid for single keypres
    {BLANK,       12,  9, BLANK, 8, BLANK, BLANK, BLANK,  7},     // A is open key
    {BLANK,       13, 15, BLANK, 0, BLANK, BLANK, BLANK, 14}};    // B is LOCK key

// Define keypad structure
typedef struct{

    enum {IDLE, PRESS, PROCESS, RELEASE} state; // keypad state variable
    uint8_t x;              // x position of pressed key
    uint8_t y;              // y position of pressed key
    uint8_t display[4];     // array for keeping the last four pressed numbers
    uint8_t display_count;  // display array index
    uint8_t k;              // points to active row of keypad
    uint32_t pulses;        // debouncing pulses
    
} Keypad;

// Define lockbox structure
typedef struct{

    enum {LOCK, SOLENOID, DOWN, NORMAL, PRELOCK} state; // lockbox state variable
    uint32_t wait;               // wait counter
    uint32_t cnt;                // password attempts

} Lockbox;

// Define prototypes
void keypad_fsm(Keypad *key);
void lockbox_fsm(Keypad *key, Lockbox *lock);
bool pw_check(uint8_t *arr);
void set_display(uint8_t *arr, uint8_t digit0, uint8_t digit1, uint8_t digit2, uint8_t digit3);
void set_password(uint8_t *arr);
void gpio_init(void);
void solenoid_off(void);
void solenoid_on(void);
void redLED_off(void);
void redLED_on(void);
void redLED_toggle(void);
void wait(uint32_t t);

// Define flags
bool open_flag = 0;
bool lock_flag = 0;
bool keypad_freeze_flag = 0;
uint8_t lock_pass[4] = {1,2,3,4};   // lock password

void main(void)
{
    // Disable watchdog and initialize GPIO
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;
    gpio_init();

    // Initialize structure instances
    Keypad key = {IDLE, 0, 0, {15,14,0,12}, 0, 0, 0 };
    Lockbox lock = {LOCK, 0, 0};

    while(1)
    {
        keypad_fsm(&key);
        lockbox_fsm(&key, &lock);
    }

}

// Call keypad FSM as a function
void keypad_fsm(Keypad *key){

    static int temp;

    // Display digit-k
    P4->OUT = 0xFF;                                  // blank display
    P8->OUT = 0xFF & ~(BIT5 >> key->k);              // enable k-th display
    P4->OUT = digit_array[key->display[key->k]];     // display k-th char in array
    wait(100);

    // Scan for keypad input in row-k if keypad is not frozen
    if(!keypad_freeze_flag)
    {
        temp = (P9->IN) & 0x0F;             // scan input at row-k
        if(temp > 0 )                       // if key press detected,
        {
            key->x = temp;                  // acknowledge input x position
            key->y = key->k;                // acknowledge input y position
        }
    }

    // Increment index-k
    (key->k)++;
    if (key->k >= 4){key->k = 0;}

    // Return if keypad is frozen
    if(keypad_freeze_flag){return;}

    // Switch keypad debouncing state
    switch (key->state){

        // Wait for input
        case IDLE:
        {
            if(key->x != 0){
                key->state = PRESS;
                key->pulses = 0;
            }break;
        }

        // Accept input if N pulses of HIGH detected
        case PRESS:
        {
            // Scan for input where input was previously found
            P4->OUT = 0xFF;                      // blank display
            P8->OUT = 0xFF & ~(BIT5 >> key->y);  // switch to row where input was prev found
            temp = (P9->IN) & 0x0F;              // read column input
            if(temp == key->x){
                key->pulses++;                   // increment pulse if same input
            }else{
                key->pulses = 0;
                key->state = IDLE;               // input fail
            }

            // Process input if N pulses
            if(key->pulses > N){
                key->pulses = 0;
                key->state = PROCESS;            // input success
            }break;
        }

        // Update display array with accepted input
        case PROCESS:
        {
            // set open key flag if open key pressed
            if(key->y == 0 && key->x == 1){
                open_flag = 1;
                key->state = RELEASE;
                break;
            }

            // set lock key flag if lock key pressed
            if(key->y == 1 && key->x == 1){
                lock_flag = 1;
                key->state = RELEASE;
                break;
            }

            // update display if numerical key pressed
            key->display[key->display_count] = keypad_table[key->y][key->x];
            key->display_count++;
            key->state = RELEASE;
            if(key->display_count > 3){key->display_count = 0;}

            break;
        }

        // Accept release if N pulses of LOW detected
        case RELEASE:
        {
            // Scan for input where input was previously found
            P4->OUT = 0xFF;                          // blank display
            P8->OUT = 0xFF & ~(BIT5 >> key->y);      // switch to row where input was prev found
            temp = (P9->IN) & 0x0F;                  // read keypad column input
            if(temp == 0){
                key->pulses++;                       // increment pulse if no input
            }else{
                key->pulses = 0;
                key->state = RELEASE;                // release fail
            }
            if(key->pulses > N){
                key->pulses = 0;
                key->state = IDLE;                    // release successful
            }break;
        }

    }
}

// Call lockbox FSM as a function
void lockbox_fsm(Keypad *key, Lockbox *lock){

    // Switch keypad lockbox debouncing state
    switch (lock->state){

        // Wait for user to input 4 digits, then compare with password using open key. Red LED on.
        case LOCK:
        {
            // Entering LOCK state:
            if(lock->wait == 0){
            redLED_on();
            set_display(key->display, 15, 14, 0, 12); // display "_LOC"
            lock->wait++;
            }

            // Test input if open key pressed
            if(open_flag){
                open_flag = 0;
                lock->wait = 0;
                if(pw_check(key->display)){
                    lock->cnt = 0;
                    lock->state = SOLENOID;     // password success
                    redLED_off();
                    break;
                }else{                          // if password wrong, increment cnt
                    lock->cnt += 1;
                    if(lock->cnt >= 5){
                        lock->cnt = 0;
                        lock->state = DOWN;     // if wrong 5 times, enter lockdown state
                        redLED_off();
                    }
                }
            }break;
        }

        // Energize solenoid to open door (5 sec).
        case SOLENOID:
        {
            // Entering SOLENOID state:
                solenoid_on();
                lock->wait++;

            // Return to NORMAL state after 5s
            if(lock->wait > FIVE_SEC){
                lock->wait = 0;
                lock->state = NORMAL;
                solenoid_off();
            }break;

        }

        // Freeze keypad for 1 minute, then return to LOCK state
        case DOWN:
        {
            // Entering DOWN state:
            set_display(key->display, 15, 14, 13, 15); // Display "_Ld_"
            keypad_freeze_flag = 1;                    // freeze keypad (no input)
            lock->wait++;

            // Return to LOCK state after 1 minute
            if(lock->wait > FIVE_SEC*3){
                lock->wait = 0;
                lock->state = LOCK;
                keypad_freeze_flag = 0;
            }break;

        }


        case NORMAL:
        {
            // Entering NORMAL state:
            if(lock->wait == 0){
                set_display(key->display, 0, 0, 0, 0); // "0000"
                lock->wait++;
            }

            // Re-energize lock solenoid if open key is pressed
            if(open_flag){
                open_flag = 0;
                lock->wait = 0;
                lock->state = SOLENOID;
            }

            // Go to PRELOCK state and set new user password
            if(lock_flag){
                lock_flag = 0;
                lock->wait = 0;
                set_password(key->display);
                lock->state = PRELOCK;
            }break;

        }

        // 5 second period before LOCK state. Blink red LED. Allow user option to return to normal state with locking.
        case PRELOCK:
        {
            if(lock->wait%1000 == 0){     //  Blink LED every 1000 loops
                redLED_toggle();
            }
            lock->wait++;

            if(key->display_count){       // if input detected, return to NORMAL state
                lock->wait = 0;
                key->display_count = 0;
                lock->state = NORMAL;
                redLED_off();
            }

            if(lock->wait == FIVE_SEC){   // if no input detected in 5s, lock the safe
                lock->wait = 0;
                lock->state = LOCK;
            }break;

        }
    }
}

// Compare user input to password
bool pw_check(uint8_t *arr ){
    int i = 0;
    while(i < 4){
        if(arr[i] != lock_pass[i]){return 0;}
        i++;
    }
    return 1;
}

void set_password(uint8_t *arr){
    lock_pass[0] = *(arr);
    lock_pass[1] = *(arr+1);
    lock_pass[2] = *(arr+2);
    lock_pass[3] = *(arr+3);
}

// Set display array
void set_display(uint8_t *arr, uint8_t digit0, uint8_t digit1, uint8_t digit2, uint8_t digit3){
    *(arr)    = digit0;
    *(arr+1)  = digit1;
    *(arr+2)  = digit2;
    *(arr+3)  = digit3;
}

void gpio_init(void){
    P2->DIR |= BIT5;          // P2.5 is solenoid output
    P4->DIR  = 0xFF;          // P4 is 7-Seg LED output
    P5->DIR |= BIT0;          // P5.0 is lock state LED
    P8->DIR  = 0xFF;          // P8 is display output
    P9->DIR  = 0x00;          // P9 is keypad input

    redLED_off();
    solenoid_off();
}

void redLED_off(void){
    P5->OUT |= BIT0;
}

void redLED_on(void){
    P5->OUT &= ~BIT0;
}

void redLED_toggle(void){
    P5->OUT ^= BIT0;
}
void solenoid_on(void){
    P2->OUT &= ~BIT5;
}

void solenoid_off(void){
    P2->OUT |= BIT5;
}

void wait(uint32_t t){
    while(t != 0){t--;}     // Busy wait
}
