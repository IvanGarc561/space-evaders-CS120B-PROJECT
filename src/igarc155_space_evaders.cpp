/*        Your Name & E-mail: Ivan Garcia-Mora, igarc155@ucr.edu
*          Discussion Section: 022

 *         Assignment: Custom Lab Project

 *         Exercise Description: As of now the joystick moves a custom character around based on sensitvity on the x axis does not
            move up or down and currently testing how it will keep track of score and lives and when I hit the button it will increase
            score and change character to '|' representing laser on the 16 x 2 LCD
			
 *        

 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.

 *

 *         Demo Link: https://youtu.be/ONtsrxCaEzs

 */
#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega-4.h"
#include "LCD.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_TASKS 3

uint8_t ship_char[8] = { // spaceship design
  0b00000,
  0b00010,
  0b00110,
  0b01110,
  0b11111,
  0b01110,
  0b00110,
  0b00010
};

// Task struct for concurrent synchSMs
typedef struct _task {
    signed char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

const unsigned long TASK0_PERIOD = 100;    // Joystick movement
const unsigned long TASK1_PERIOD = 200;    // LCD display (score/lives)
const unsigned long TASK2_PERIOD = 100;    // Fire button
const unsigned long GCD_PERIOD = 1;

task tasks[NUM_TASKS];

// Shared states
volatile unsigned char score = 0;
volatile unsigned char lives = 3;
volatile uint8_t currentPos = 0;
volatile uint8_t fireActive = 0;
volatile uint8_t fireHeld = 0;

void TimerISR() {
    for (unsigned int i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].elapsedTime == tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}
void PWM_BuzzerInit() {
    DDRD |= (1 << PB0); // Output on OC0A
    TCCR0A |= (1 << COM0A1) | (1 << WGM01) | (1 << WGM00); // Fast PWM, non-inverting
    TCCR0B |= (1 << CS01); // Prescaler 8
    OCR0A = 0; // Initially off
}


long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 1. Joystick Task
enum JoystickMove_States {MOVE_INIT, MOVE_WAIT};
int TickFct_JoystickMove(int state) {
    static uint8_t prevPos = 0;
    unsigned int x = ADC_read(0);
    uint8_t pos = map(x, 0, 1023, 0, 15);
    if (pos > 15) pos = 15;

    currentPos = pos;

    switch (state) {
        case MOVE_INIT:
            state = (pos != prevPos) ? MOVE_WAIT : MOVE_INIT;
            break;
        case MOVE_WAIT:
            state = MOVE_INIT;
            break;
        default:
            state = MOVE_INIT;
            break;
    }

    if (pos != prevPos || fireActive) {
        lcd_goto_xy(1, prevPos);
        lcd_write_character(' ');

        lcd_goto_xy(1, pos);
        lcd_write_character(fireActive ? '|' : 0);

        prevPos = pos;
    }

    return state;
}

// 2. LCD Display Task
enum LCD_States {DISPLAY};
int TickFct_LCD(int state){
    char message[17];
    switch (state) {
        case DISPLAY:
            lcd_goto_xy(0, 0);
            sprintf(message, "Score:%d L:%d", score, lives);
            lcd_write_str(message);
            break;
    }
    return state;
}

// 3. Fire Button Task
enum FireButton_States {FIRE_WAIT, FIRE_SHOOT};
int TickFct_FireButton(int state){
    uint8_t firePressed = (PINC & (1 << PC3));

    switch(state){
        case FIRE_WAIT:
            state = (firePressed) ? FIRE_SHOOT : FIRE_WAIT;
            break;
        case FIRE_SHOOT:
            state = (!firePressed) ? FIRE_WAIT : FIRE_SHOOT;
            break;
        default:
            state = FIRE_WAIT;
            break;
    }

    switch(state){
        case FIRE_WAIT:
            fireActive = 0;
            fireHeld = 0;
            OCR0A = 0;

            break;
        case FIRE_SHOOT:
            fireActive = 1;
            if(!fireHeld){
                score++;
                fireHeld = 1;
                OCR0A = 100; // Enable tone
                TCCR0B |= (1 << CS01);
            }
            break;
    }
    return state;
}

int main(void) {
    DDRC = 0x00; 
    PORTC = 0xFF; // PC3 is fire button
    DDRB = 0xFF; 
    PORTB = 0x00;
    DDRD = 0xFF; 
    PORTD = 0x00;

    ADC_init();
    PWM_BuzzerInit();
    lcd_init();
    lcd_send_command(0x40); // Set CGRAM address to 0 (character 0)

    for (int i = 0; i < 8; i++) {
        lcd_write_character(ship_char[i]); // Sends one row of 5-bit pattern
        _delay_us(40);
    }

    // Task setup
    tasks[0].state = MOVE_INIT;
    tasks[0].period = TASK0_PERIOD;
    tasks[0].elapsedTime = 0;
    tasks[0].TickFct = &TickFct_JoystickMove;

    tasks[1].state = DISPLAY;
    tasks[1].period = TASK1_PERIOD;
    tasks[1].elapsedTime = 0;
    tasks[1].TickFct = &TickFct_LCD;

    tasks[2].state = FIRE_WAIT;
    tasks[2].period = TASK2_PERIOD;
    tasks[2].elapsedTime = 0;
    tasks[2].TickFct = &TickFct_FireButton;

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}
    return 0;
}
