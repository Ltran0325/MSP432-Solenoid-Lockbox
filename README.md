# MSP432-Solenoid-Lockbox

To use keypad input to operate a solenoid lockbox. 
This project is a continuation of MSP432-Keypad:
https://github.com/Ltran0325/MSP432-Keypad/blob/main/main.c 



**Hardware:**

MSP432P401R LaunchPad: https://www.digikey.com/en/products/detail/texas-instruments/MSP-EXP432P401R/5170609

7-segment display: https://www.digikey.com/en/products/detail/lite-on-inc/LTC-4627JR/408223

4x4 matrix keypad: https://www.digikey.com/en/products/detail/adafruit-industries-llc/3844/9561536

red LED (solenoid): https://www.amazon.com/100pcs-Ultra-Bright-Emitting-Diffused/dp/B01GE4WHK6/ref=sr_1_5?dchild=1&keywords=red+led&qid=1614890774&sr=8-5

**Keys:** 

A - Open Key

B - Lock Key

**Demo:** 

https://www.youtube.com/watch?v=A81OljZHvpA

**Criteria:**

- Successfully unlock your lock box (by turning ON the LED connects to P2.5) after you input the correct passcode and pressed the OPEN key .
- Successfully lock your lock box (by turning on the LED at P2.5 & blink the LED at P5.0) after you input the correct passcode again and press the LOCK key
- You input a wrong passcode 5 times (LED does not turn on), and get locked for ~1 minute with '-Ld-' show on your display. 
- Then you input a correct passcode during the locked-    down -period but the box does not open (LED at P2.5 does not turn on).
- The 7-segment display should show the passcode you entered every time, whether is correct or not.
- The 7-segment display should show the corresponding characters '-Ld-' & '-LOC' at lock & lockdown state.

**Lockbox Flowchart:**
![image](https://user-images.githubusercontent.com/62213019/110027542-f5050000-7ce6-11eb-9001-580a07f68d1e.png)
