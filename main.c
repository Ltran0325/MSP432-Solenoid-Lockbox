/*******************************************************************************
*                              -- IN PROGRESS--                                *
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
* P5 is OUTPUT to red LED                                                      *
*                                                                              *
* Demo: https://youtu.be/VUVd9RPUBLM                                           *
*******************************************************************************/

// Include header file(s) and define constants
#include "msp.h"
#define N 100                   // debounce loop count
#define d 16                    // digit-bit index to clear display
#define OPEN_KEY 10             // open key is A
#define LOCK_KEY 11             // lock key is B
#define FIVE_SEC 12000            // 5sec delay

// Lockbox password
const int password[] = {1, 2, 3, 4};

// Define digit-bit lookup table
const int digit_array[17] = {
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
0b11111111,  // Blank Display
};

// Define keypad layout
const int keypad_table[4][9] = {
    {d, OPEN_KEY,  3, d, 2, d, d, d,  1},     // 4x9 multiplixer keypad
    {d, LOCK_KEY,  6, d, 5, d, d, d,  4},     // only columns 1, 2, 4, 8 are valid for single keypres
    {d,       12,  9, d, 8, d, d, d,  7},     // A is open key
    {d,       13, 15, d, 0, d, d, d, 14}};    // B is LOCK key

// Define keypad states
typedef enum{
    IDLE,           // scan for keypad input
    PRESS,          // debounce keypad press
    PROCESS,        // accept input key
    RELEASE         // ebounce keypad release
}KeyStates;

// Define lockbox states
typedef enum{
    LOCK,
    SOLENOID,
    DOWN,
    NORMAL,
    PRELOCK
}LockStates;

// Define keypad structure
typedef struct{
    KeyStates state;    // state of keypad FSM
    int x;              // x position of pressed key
    int y;              // y position of pressed key
    int display[4];     // array for keeping the last four pressed numbers
    int display_count;  // display array index
    int pulses;         // debouncing pulses
    int k;              // points to active row of keypad

    // Define lockbox structure
    struct Lockbox{
        LockStates state;   // state of lockbox FSM
        const int *pass_code;   // passcode to lock or open lockbox
        int wait;           // wait counter
        int cnt;            // password attempts
    }lock;

}Keypad;

// Define prototypes
void gpio_init(void);            // initialize GPIO
void keypad_fsm(Keypad *key);    // call keypad FSM as a function
void lockbox_fsm(Keypad *key);   // call lockbox FSM as a function
int  pw_check(int *arr);         // compare user input to password
void wait(int t);                // busy wait

// this flag is 1 if 4 input keys have been pressed
int open_flag = 0;
int lock_flag = 0;
int keypad_freeze = 0;

void main(void)
{
    // Disable watchdog and initialize GPIO
    WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;                // disable watchdog
    gpio_init();                                               // initalize GPIO

    // Initialize instance of keypad structure
    Keypad key = {IDLE, 0, 0, {15,14,0,12}, 0, 0, 0, {LOCK, password, 0, 0} };
    Keypad *ptr = &key;

    while(1){
        keypad_fsm(ptr);
        lockbox_fsm(ptr);
    }

}

// Call lockbox FSM as a function
void lockbox_fsm(Keypad *key){

    // Switch keypad lockbox debouncing state
    switch ((key->lock).state){

        // Wait for user to input 4 digits, then compare with password
        case LOCK:

            if((key->lock).wait == 0){
                key->display[0] = 15;  // "_LOC"
                key->display[1] = 14;
                key->display[2] = 0;
                key->display[3] = 12;
                (key->lock).wait++;
            }
            if(open_flag){
                open_flag = 0;
                (key->lock).wait = 0;
                key->display_count = 0;
                if(pw_check(key->display)){         // if password success
                    (key->lock).cnt = 0;
                    (key->lock).state = SOLENOID;
                    break;
                }else{                              // if password wrong, increment cntr
                    (key->lock).cnt += 1;
                    if((key->lock).cnt >= 5){       // if wrong 5 times, Lockdown
                        (key->lock).cnt = 0;
                        (key->lock).state = DOWN;
                    }
                }
            }break;

        case SOLENOID:

            // Energize solenoid
            if((key->lock).wait == 0){P2->OUT &= ~BIT5;}

            // Return to normal after 5s
            if((key->lock).wait > FIVE_SEC){
               (key->lock).wait = 0;
               P2->OUT |= BIT5;                // turn off solenoid
               (key->lock).state = NORMAL;
               break;
            }else{
               (key->lock).wait++;
            }break;

        case DOWN:
            if((key->lock).wait == 0){
                key->display[0] = 15;  // "_Ld_"
                key->display[1] = 14;
                key->display[2] = 13;
                key->display[3] = 15;
                keypad_freeze = 1;
            }
            if((key->lock).wait > FIVE_SEC){
                (key->lock).wait = 0;
                (key->lock).state = LOCK;
                keypad_freeze = 0;
                key->display[0] = 0;  // "_LOC"
                key->display[1] = 0;
                key->display[2] = 0;
                key->display[3] = 0;
                break;
            }else{
                (key->lock).wait++;
            }break;

       case NORMAL:
            P5->OUT ^= BIT0;        // Toggle normal LED
            if((key->lock).wait == 0){
                key->display[0] = 0;  // "0000"
                key->display[1] = 0;
                key->display[2] = 0;
                key->display[3] = 0;
            }
            if(open_flag){
                open_flag = 0;
                (key->lock).state = SOLENOID;
            }
            if(lock_flag){
                lock_flag = 0;
                (key->lock).wait = 0;
                key->display_count = 0;
                if(pw_check(key->display)){         // if password success
                  //(key->lock).state = PRELOCK;
                    (key->lock).state = LOCK;
                }
            }
            (key->lock).wait++;
            break;

     /*case PRELOCK:

           if( !(key->display_count)){
               (key->lock).wait = 0;
               (key->lock).state = NORMAL;
           }
           (key->lock).wait++;
           if((key->lock).wait == FIVE_SEC){
               (key->lock).wait = 0;
               (key->lock).state = LOCK;
           }break;
    */
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
    if(!keypad_freeze){

        temp = (P9->IN) & 0x0F;             // scan input at row-k
        if(temp > 0 ){                      // if key pressed detected
            key->x = temp;                  // acknowledge input x position
            key->y = key->k;                // acknowledge input y position
        }
    }
    // Increment index-k
    (key->k)++;
    if (key->k >= 4){key->k = 0;}

    if(keypad_freeze){return;}

    // Busy wait to remove digit flickering

    // Switch keypad debouncing state
    switch (key->state){

        // Wait for input
        case IDLE:
            if(key->x != 0){
                key->state = PRESS;
                key->pulses = 0;
            }break;

        // Accept input if N pulses of HIGH detected
        case PRESS:
            P4->OUT = 0xFF;                     // blank display
            P8->OUT = 0xFF & ~(BIT5 >> key->y);  // switch to row where input was prev found
            temp = (P9->IN) & 0x0F;             // read column input
            if(temp == key->x){key->pulses++;}    // increment pulses if last input is same as new input
            else{
                key->pulses = 0;
                key->state = IDLE;            // input success
            }
            if(key->pulses > N){
                key->pulses = 0;
                key->state = PROCESS;            // input success
            }break;

        // Update display array with accepted input
        case PROCESS:

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
            if(key->display_count > 3){
               key->display_count = 0;
            }
            break;

        // Accept release if N pulses of LOW detected
        case RELEASE:
            P4->OUT = 0xFF;                          // blank display
            P8->OUT = 0xFF & ~(BIT5 >> key->y);      // switch to row where input was prev found
            temp = (P9->IN) & 0x0F;                  // read keypad column input
            if(temp == 0){key->pulses++;}
            else{
                key->pulses = 0;
                key->state = RELEASE;
            }
            if(key->pulses > N){
                key->pulses = 0;
                key->state = IDLE;                    // release successful
            }break;

    }
}

// Initialize GPIO
void gpio_init(void){
    P2->DIR |= BIT5;          // P2.5 is solenoid output
    P4->DIR  = 0xFF;          // P4 is 7-Seg LED output
    P5->DIR |= BIT0;          // P5.0 is lock state LED
    P8->DIR  = 0xFF;          // P8 is display output
    P9->DIR  = 0x00;          // P9 is keypad input

    P2->OUT = 0x20;
}

// Busy wait
void wait(int t){
    while(t >= 0){t--;}
}

// Compare user input to password
int pw_check(int *arr){
    int i = 0;
    while(i < 4){
        if(*(arr+i) != password[i]){return 0;}
        i++;
    }
    return 1;
}
