// Permission to copy is granted provided that this header remains intact. 
// This software is provided with no warranties.

#ifndef TIMER_H
#define TIMER_H

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>


volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1ms
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerISR(void);

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR2A = 0x00;
  TCCR2B 	= 0x0B;	// bit3 = 1: CTC mode (clear timer on compare)
					// bit2bit1bit0=011: prescaler /64
					// 00001011: 0x0B
					// SO, 16 MHz clock or 16,000,000 /64 = 250,000 ticks/s
					// Thus, TCNT1 register will count at 250,000 ticks/s
          // FOR MICROSECONDS:
          // bit3 = 1: CTC mode (clear timer on compare)
					// bit2bit1bit0=010: prescaler /64
					// 00001010: 0x0A
					// SO, 16 MHz clock or 16,000,000 /8 = 2,000,000 ticks/s
					// Thus, TCNT1 register will count at 2,000,000 ticks/s

	// AVR output compare register OCR1A.
	OCR2A 	= 250;	// Timer interrupt will be generated when TCNT1==OCR1A
					// We want a 1 ms tick. 0.001 s * 250,000 ticks/s = 250
					// So when TCNT1 register equals 250,
					// 1 ms has passed. Thus, we compare to 250.
					// AVR timer interrupt mask register
          // FOR MICROSECONDS:
          // Timer interrupt will be generated when TCNT1==OCR1A
					// We want a 1 us tick. 0.000001 s * 2,000,000 ticks/s = 2
					// So when TCNT1 register equals 2,
					// 1 us has passed. Thus, we compare to 2.
					// AVR timer interrupt mask register

	TIMSK2 	= 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT2 = 0;

	// TimerISR will be called every _avr_timer_cntcurr milliseconds
	_avr_timer_cntcurr = _avr_timer_M;

	//Enable global interrupts
	SREG |= 0x80;	// 0x80: 1000000
}

void TimerOff() {
	TCCR2B 	= 0x00; // bit3bit2bit1bit0=0000: timer off
}



// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER2_COMPA_vect)
{
	// CPU automatically calls when TCNT0 == OCR0 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; 			// Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { 	// results in a more efficient compare
		TimerISR(); 				// Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}

}

int TimerOverflow = 0;

ISR(TIMER1_OVF_vect)
{
	TimerOverflow++;	/* Increment Timer Overflow count */
}


double read_sonar(){
    long count;
    // PORTC = SetBit(PORTC,2,1); 0000_0100 //0x02
    PORTC |= 0x04;
    _delay_us(10);
    // PORTC = SetBit(PORTC,2,0); // 1111_1011
    PORTC &= ~0x04;

    TCNT1 = 0;	/* Clear Timer counter */
		TCCR1B = 0x41;	/* Capture on rising edge, No prescaler*/
		TIFR1 = 1<<ICF1;	/* Clear ICP flag (Input Capture flag) */
		TIFR1 = 1<<TOV1;	/* Clear Timer Overflow flag */

		/*Calculate width of Echo by Input Capture (ICP) */
		while ((TIFR1 & (1 << ICF1)) == 0);/* Wait for rising edge */
		TCNT1 = 0;	/* Clear Timer counter */
		TCCR1B = 0x01;	/* Capture on falling edge, No prescaler */
		TIFR1 = 1<<ICF1;	/* Clear ICP flag (Input Capture flag) */
		TIFR1 = 1<<TOV1;	/* Clear Timer Overflow flag */
		TimerOverflow = 0;  /* Clear Timer overflow count */
		while ((TIFR1 & (1 << ICF1)) == 0);/* Wait for falling edge */
		count = ICR1 + (65535 * TimerOverflow);	/* Take count */
		
		return((double)count / 932.46);
}


#endif // TIMER_H
