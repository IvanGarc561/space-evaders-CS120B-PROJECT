/* Your Name & E-mail: Ivan Garcia-Mora, igarc155@ucr.edu
*          Discussion Section: 022

 *         Assignment: Custom Lab Project

 *         Exercise Description: As of now the joystick moves a custom character around based on sensitvity on the x axis does not
            move up or down and currently testing how it will keep track of score and lives and when I hit the button it will increase
            score and change character to '|' representing laser on the 16 x 2 LCD. Update Week 9: I have added teh buzzer function however
            I used PB1 instead of PB0 because I couldn't figure it out so I switched LED 1 with Buzzer so now they are complements to each other.
            Also, I have added a start screen when you reset and have started spawning in enemies but I have to get the enemeies a special character
            and laser detection must be done by week 10 and finally last feature left is collision and death but I will get all of this done soon.
			
 *        

 *         I acknowledge all content contained herein, excluding template or example code, is my own original work.

 *

 *         Demo Link: https://youtu.be/aCPlPonD-3A

 */
#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega-4.h"
#include "LCD.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_TASKS 8

uint8_t ship_char[8] = { // spaceship design or you the player
  0b10001,
  0b10101,
  0b11111,
  0b01110,
  0b01110,
  0b01110,
  0b11111,
  0b11011,
};

// Task struct for concurrent synchSMs
typedef struct _task {
    signed char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

const unsigned long JOYSTICK_PERIOD = 100;    // Joystick movement
const unsigned long LCD_PERIOD = 200;    // LCD display
const unsigned long FIRE_PERIOD = 100;    // Fire button
const unsigned long LED_PERIOD = 300; // LEDS display
const unsigned long RESET_PERIOD = 100; // Resets entire game
const unsigned long ASTEROID_PERIOD = 1000; // Spawn enemies
const unsigned long LASER_PERIOD = 100;
const unsigned long GCD_PERIOD = 100;

task tasks[NUM_TASKS];

volatile unsigned char score = 0;
volatile unsigned char lives = 3;
volatile uint8_t currentPos = 0;
volatile unsigned char fireActive = 0;
volatile unsigned char fired = 0;
volatile unsigned char resetTrigger = 0;
volatile unsigned char buzzerOn = 0;
volatile unsigned char buzzerTicks = 0;
volatile uint8_t asteroidX = 15;
volatile uint8_t asteroidY = 0;
volatile unsigned char asteroidActive = 1;
volatile unsigned char gameStarted = 0;
volatile unsigned char gameEnded = 0;
volatile uint8_t laserX = 0;
volatile uint8_t laserY = 2; 
volatile unsigned char laserActive = 0;
volatile unsigned char fireToStart = 1;

void PWM_BuzzerInit() { // Tried using PB0 but does not work so PB1 only works
    DDRB |= (1 << PB1); // PB1 = OC1A
    TCCR1A = (1 << COM1A0); 
    TCCR1B = (1 << WGM12) | (1 << CS11);
    OCR1A = 255; 
}


void TimerISR() {
    for (unsigned int i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].elapsedTime == tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 1. Joystick Task
enum JoystickMove_States {MOVE_INIT, MOVE_WAIT};
int TickFct_JoystickMove(int state) {
    static uint8_t prevPos = 0;
    unsigned int x = ADC_read(0);
    uint8_t pos = map(x, 0, 1023, 0, 16);
    if (pos >= 15){
        pos = 15; // based on lcd position
    }

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

    if (!gameEnded && (pos != prevPos || fireActive)) {
        // testing gameStarted = 1;
        lcd_goto_xy(1, prevPos);
        lcd_write_character(' ');
        lcd_goto_xy(1, pos);
        lcd_write_character(fireActive ? '|' : 0);

        prevPos = pos;
        if(fireActive && currentPos == asteroidX && gameStarted && !gameEnded){
            score++;
            OCR1A = 300; // test tomorrow
            lcd_goto_xy(0, currentPos);
            lcd_write_character(' '); // erases enemy
        }
    }

    return state;
}

// 2. LCD Display Task
enum LCD_States {DISPLAY};
int TickFct_LCD(int state){
    char message[17];
    switch (state) {
        case DISPLAY:
            if(!gameStarted || gameEnded){
            lcd_goto_xy(0, 0);
            sprintf(message, "Score:%d L:%d", score, lives);
            lcd_write_str(message);
            }
            else if (gameStarted){
                lcd_goto_xy(0, 0);
                lcd_write_str("                ");
            }
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
            if (firePressed) {
                state = FIRE_SHOOT;
            } else {
                state = FIRE_WAIT;
            }
            break;
        case FIRE_SHOOT:
            if (!firePressed) {
                if (!gameStarted && fireToStart) {
                    gameStarted = 1;
                    fireToStart = 0;
                }
                state = FIRE_WAIT;
            } else {
                state = FIRE_SHOOT;
            }
            break;

        default:
            state = FIRE_WAIT;
            break;
    }

    switch(state){
        case FIRE_WAIT:
            fireActive = 0;
            fired = 0;
            break;
        case FIRE_SHOOT:
            fireActive = 1;
            if (!fired && !gameEnded) {
                fired = 1;
                fireActive = 1;

                // Start laser
                laserX = currentPos;
                laserY = 1;
                laserActive = 1;

                buzzerOn = 1;
                OCR1A = 100;
                buzzerTicks = 1;
            }
            break;
    }
    return state;
}

// 4. LED Display Task
enum LED_States {LED_INIT, LED_BLINK};
int TickFct_LED(int state) {
    static uint8_t blinkState = 0;

    switch (state) {
        case LED_INIT:
            state = LED_BLINK;
            break;
        case LED_BLINK:
            state = LED_BLINK;
            break;
        default:
            state = LED_INIT;
            break;
    }

    switch (lives) {
        case 3:
            PORTB = SetBit(PORTB, 0, 1); 
            PORTB = SetBit(PORTB, 2, 1); 
            PORTB = SetBit(PORTB, 3, 1);
            break;
        case 2:
            PORTB = SetBit(PORTB, 0, 1); 
            PORTB = SetBit(PORTB, 2, 1); 
            PORTB = SetBit(PORTB, 3, 0); 
            break;
        case 1:
            blinkState = !blinkState;
            PORTB = SetBit(PORTB, 0, blinkState); 
            PORTB = SetBit(PORTB, 2, 0);          
            PORTB = SetBit(PORTB, 3, 0);          
            break;
        default: //dead 
            blinkState = !blinkState;
            PORTB = SetBit(PORTB, 0, blinkState);
            PORTB = SetBit(PORTB, 2, !blinkState);
            PORTB = SetBit(PORTB, 3, blinkState);
            gameEnded = 1;
            break;
    }

    return state;
}

// 5. Reset Button Task
enum Reset_States {RESET_WAIT, RESET_PRESS};
int TickFct_ResetButton(int state) {
    uint8_t resetPressed = (PINC & (1 << PC4)); 

    switch (state) {
        case RESET_WAIT:
            state = resetPressed ? RESET_PRESS : RESET_WAIT;
            break;
        case RESET_PRESS:
            resetTrigger = 1;
            state = resetPressed ? RESET_PRESS : RESET_WAIT;
            break;
        default:
            state = RESET_WAIT;
            break;
    }

    switch (state) {
        case RESET_WAIT:
            break;
        case RESET_PRESS:
            buzzerOn = 1;
            OCR1A = 255;
            buzzerTicks = 1;
            score = 0;
            lives = 3;
            fireActive = 0;
            fired = 0;
            lcd_clear();
            gameStarted = 0;
            gameEnded = 0; 
            fireToStart = 1;
            resetTrigger= 0;
            break;
    }

    return state;
}
// 6. Buzzer Task
enum Buzzer_States {BUZZER_WAIT, BUZZER_ON};
int TickFct_Buzzer(int state) {
    switch (state) {
        case BUZZER_WAIT:
            state = (buzzerOn) ? BUZZER_ON : BUZZER_WAIT;
            break;
        case BUZZER_ON:
            state = (buzzerTicks == 0) ? BUZZER_WAIT : BUZZER_ON;
            break;
        default:
            state = BUZZER_WAIT;
            break;
    }

    switch (state) {
        case BUZZER_WAIT:
            OCR1A = 0;
            break;
        case BUZZER_ON:
            if (buzzerTicks > 0) {
                buzzerTicks--;
            } else {
                buzzerOn = 0;
            }
            break;
    }

    return state;
}

enum Asteroid_States {ASTEROID_START, ASTEROID_WAIT, ASTEROID_FALL};
int TickFct_Asteroid(int state) {
    static unsigned char AsteroidTick = 0;
    static unsigned char Asteroidtimer = 5;
    switch (state) {
        case ASTEROID_START:
            if (!gameStarted || gameEnded) {
                return state; // Don't spawn anything yet
            }

            asteroidX = (rand() % 16);
            asteroidY = 0;
            asteroidActive = 1;
            state = ASTEROID_WAIT;
            break;

        case ASTEROID_WAIT:
            if (gameEnded){
                return ASTEROID_START;
            } 

            if(AsteroidTick < Asteroidtimer ){
                state = ASTEROID_WAIT;
            } else{
                state = ASTEROID_FALL;
                AsteroidTick = 0;

                if(score >= 5){
                    Asteroidtimer = 3;
                } else if (score >= 10){
                    Asteroidtimer = 2;
                }
            }
            break;
        case ASTEROID_FALL:
            // Check collision with player
            if ((asteroidY == 1 && asteroidX == currentPos) || (asteroidY > 2)) {
                lives--;
                buzzerOn = 1;
                OCR1A = 200;
                buzzerTicks = 2;

                // Clear asteroid
                lcd_goto_xy(asteroidY, asteroidX);
                lcd_write_character(' ');

                asteroidActive = 0;

                if (lives == 0) {
                    gameEnded = 1;
                    lcd_clear();
                    lcd_goto_xy(0, 3);
                    lcd_write_str("GAME OVER");
                }

                state = ASTEROID_START;
            } else{
                state = ASTEROID_FALL;
            }
            break;

        default:
            state = ASTEROID_START;
            break;
    }

    switch(state){
        case ASTEROID_START:
            if (!gameStarted || gameEnded || !asteroidActive){
                return state;
            }
        
            break;
        case ASTEROID_WAIT:
            // Clear previous position
            lcd_goto_xy(asteroidY, asteroidX);
            lcd_write_character('*'); // CUSTOM LATER
            AsteroidTick++;
            break;
        case ASTEROID_FALL:
            // clear
            lcd_goto_xy(asteroidY, asteroidX);
            lcd_write_character(' ');

            // Move down
            asteroidY++;

            // Creates new character
            lcd_goto_xy(asteroidY, asteroidX);
            lcd_write_character('*'); // CUSTOM LATER
            break;
    }

    return state;
}

enum Laser_States {LASER_WAIT, LASER_MOVE};
int TickFct_Laser(int state) {
    switch (state) {
        case LASER_WAIT:
            if(laserActive && gameStarted && !gameEnded){
                state = LASER_MOVE;
            } else{
                state = LASER_WAIT;
            }
            break;

        case LASER_MOVE:
            if ((!laserActive || gameEnded || !gameEnded || laserY == 0)) {
                laserActive = 0;
                state = LASER_WAIT;
            } else{
                state = LASER_WAIT;
            }
            break;

        default:
            state = LASER_WAIT;
            break;
    }
    switch(state){
        case LASER_WAIT:
            laserActive = 0;
            break;
        case LASER_MOVE:
            // Clear previous position
            lcd_goto_xy(laserY, laserX);
            lcd_write_character(' ');

            laserY--;

            // Draw new laser
            lcd_goto_xy(laserY, laserX);
            lcd_write_character('|');

            // Checks for hit
            if (laserY == asteroidY && laserX == asteroidX) {
                score++;
                laserActive = 0;

                // Removes asteroid
                lcd_goto_xy(asteroidY, asteroidX);
                lcd_write_character(' ');
                asteroidActive = 0;

                OCR1A = 250; // hit tone
                buzzerTicks = 2;
            }
            break;
    }

    return state;
}



int main(void) {
    DDRC = 0x00; 
    PORTC = 0xFF;
    DDRB = 0xFF; 
    PORTB = 0x00;
    DDRD = 0xFF; 
    PORTD = 0x00;

    ADC_init();
    lcd_init();
    PWM_BuzzerInit();
    lcd_send_command(0x40);
    for (int i = 0; i < 8; i++) {
        lcd_write_character(ship_char[i]);
    }
    _delay_ms(2);

    // Task setup
    tasks[0].state = MOVE_INIT;
    tasks[0].period = JOYSTICK_PERIOD;
    tasks[0].elapsedTime = 0;
    tasks[0].TickFct = &TickFct_JoystickMove;

    tasks[1].state = DISPLAY;
    tasks[1].period = LCD_PERIOD;
    tasks[1].elapsedTime = 0;
    tasks[1].TickFct = &TickFct_LCD;

    tasks[2].state = FIRE_WAIT;
    tasks[2].period = FIRE_PERIOD;
    tasks[2].elapsedTime = 0;
    tasks[2].TickFct = &TickFct_FireButton;

    tasks[3].state = LED_INIT;
    tasks[3].period = LED_PERIOD;
    tasks[3].elapsedTime = 0;
    tasks[3].TickFct = &TickFct_LED;

    tasks[4].state = RESET_WAIT;
    tasks[4].period = RESET_PERIOD;
    tasks[4].elapsedTime = 0;
    tasks[4].TickFct = &TickFct_ResetButton;

    tasks[5].state = BUZZER_WAIT;
    tasks[5].period = GCD_PERIOD;
    tasks[5].elapsedTime = 0;
    tasks[5].TickFct = &TickFct_Buzzer;

    tasks[6].state = ASTEROID_START;
    tasks[6].period = ASTEROID_PERIOD;
    tasks[6].elapsedTime = 0;
    tasks[6].TickFct = &TickFct_Asteroid;

    tasks[7].state = LASER_WAIT;
    tasks[7].period = LASER_PERIOD;
    tasks[7].elapsedTime = 0;
    tasks[7].TickFct = &TickFct_Laser;

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}
    return 0;
}