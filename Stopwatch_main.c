/*
 * mp2.c
 *
 *  Created on: Sep 12, 2024
 *      Author: samaa
 */
#include <avr/io.h>
#include <avr/delay.h>
#include <avr/interrupt.h>

/* Value of compare match */
#define COMPARE_VAL_TIMER1 15625

/* Initial global values */
unsigned char flag_timer1 = 0; /* Timer1 flag to handle timer operations */
unsigned char flag_pause = 0; /* Flag used in pause mode to handle toggling */
unsigned char COUNTUP = 1; /* Differentiate between count-up and count-down */

unsigned int seconds = 0;
unsigned int minutes = 0;
unsigned int hours = 0;

/*------------------------FUNCTION DECLARATIONS---------------------------*/
/* Increment/decrement handling */
void incrementHours();
void incrementMinutes();
void incrementSeconds();
void decrementHours();
void decrementMinutes();
void decrementSeconds();

/* 7-segment and counting handling */
void updateDisplay();
void enableDisplay(char sevenseg);
void SendDigitToDisp(char digit);
void handleCounting();

/*--------------------------------------------------------------------------*/

/*--------------------------INTERRUPT HANDLERS-----------------------------*/
ISR(INT0_vect)
{
    /* Reset the time variables */
    seconds = 0;
    minutes = 0;
    hours = 0;

    /* Reset buzzer as well */
    PORTD &= ~(1<<PD0);
}

/* Pause-handling ISR (INT1) */
ISR(INT1_vect){
	TCCR1B &= ~(1<<CS10) & ~(1<<CS11) & ~(1<<CS12); /* Stop counting using the clock bits */
	flag_pause = 1;
}

/* Resume-handling ISR (INT2) */
ISR(INT2_vect){
	TCCR1B |= (1<<CS10) | (1<<CS12);  /* Resume by setting the clock bits */
	flag_pause = 0;
}

/*--------------------------------------------------------------------------*/

/*-----------------------------INTERRUPT INITIALIZATIONS--------------------*/
/* External INT2 enable and configuration function */
void INT2_Init(void){
	GICR |= (1<<INT2);
	MCUCSR &= ~(1<<ISC2); /* Set to 0 to trigger with the falling edge */
}

/* External INT1 enable and configuration function */
void INT1_Init(void){
	GICR |= (1<<INT1); /* Enable INT1 */
	MCUCR |= (1<<ISC11) | (1<<ISC10); /* Trigger INT1 on rising edge */
}

/* External INT0 enable and configuration function */
void INT0_Init(void)
{
	MCUCR |= (1<<ISC01);   /* Trigger INT0 on rising edge */
	GICR  |= (1<<INT0);    /* Enable INT0 */
}
/*--------------------------------------------------------------------------*/

/*-------------------------------TIMER INIT---------------------------------*/
/* Timer1 CTC Mode Initialization */
void Timer1_Init_CTC_Mode()
{
	TCCR1B = (1<<WGM12);    /* Set CTC Mode and Prescaler of 1024 */
	TCCR1A = (1<<FOC1A) | (1<<FOC1B); /* Non-PWM mode */
	TIMSK |= (1<<OCIE1A); /* Enable Timer1 Compare Interrupt */

	TCNT1 = 0; /* Counter initialized to 0 */

	OCR1A = COMPARE_VAL_TIMER1;  /* Compare value for a 1-second interval, prescaler of 1024, and F_CPU=16Mhz */
}

/* Timer1 Compare Match Interrupt */
ISR(TIMER1_COMPA_vect){
    flag_timer1 = 1; /* Flag for the ISR of Timer1 -> implementation in main loop */
}
/*--------------------------------------------------------------------------*/

/*-------------------------------SETUP FUNCTION-----------------------------*/
void Setup(){
	Timer1_Init_CTC_Mode(); /* Initialize Timer1 in CTC mode */
	INT2_Init(); /* Initialize external interrupt INT2 */
	INT1_Init(); /* Initialize external interrupt INT1 */
	INT0_Init(); /* Initialize external interrupt INT0 */

	TCCR1B |= (1<<CS10) | (1<<CS12); /* Timer immediately starts counting with a prescaler of 1024 */
}
/*--------------------------------------------------------------------------*/

/*-------------------------------MAIN FUNCTION-----------------------------*/
int main(void){
	/* Flags indicating button presses */
	unsigned char decreaseSecFlag = 1;
	unsigned char increaseSecFlag = 1;
	unsigned char decreaseMinFlag = 1;
	unsigned char increaseMinFlag = 1;
	unsigned char decreaseHourFlag = 1;
	unsigned char increaseHourFlag = 1;
	unsigned char countModeFlag = 1;

	/* Initialize interrupts */
	SREG |= (1<<7); /* Enable global interrupt bit (I-BIT) */

	/* Initialize I/O Ports */
	DDRB = 0x00; /* Set PortB as input (Adjustment PBs, Resume, Toggle) */
	PORTB = 0xFF; /* Internal Pull-up resistor for PortB */

	DDRC |= 0x0F;  /* First 4 pins of PortC as output */
	PORTC &= ~(0x0F); /* Initial value = 0 */

	DDRA |= 0x3F; /* First 6 pins of PortA as output to enable/disable 7-segment display */
	PORTA |= 0x3F;

	DDRD |= (1<<PD0) | (1<<PD4) | (1<<PD5);  /* Output pins of PortD (Buzzer, Red LED, Yellow LED) */

	DDRD &= ~(1<<PD2) & ~(1<<PD3);  /* Input pins of PortD (Reset and pause) */
	PORTD |= (1<<PD2); /* Internal pull-up for PD2 */

	Setup();  /* Setup all interrupts and Timer1 */

	/* Main Application Loop */
	while(1){

	    /* Handle counting through Timer1 flag and counting function */
		if (flag_timer1) {
		    flag_timer1 = 0;
		    handleCounting();
		}

		/* Check if paused, then handle mode toggle */
		if(flag_pause == 1){
			/* Toggle button check */
			if(!(PINB & (1<<PB7))) {
				_delay_ms(30); /* Debounce */
				if(!(PINB & (1<<PB7))) {
					if (countModeFlag){
						COUNTUP = !COUNTUP;
					}
					countModeFlag = 0;
				}
			}
			else {
				countModeFlag = 1;
			}
		}

		/* Handle mode LEDs */
		if (COUNTUP == 1) {
	        PORTD |= (1<<PD4); /* Turn on Red LED for Count-up */
	        PORTD &= ~(1<<PD5);
		}
		else {
	        PORTD |= (1<<PD5); /* Turn on Yellow LED for Count-down */
	        PORTD &= ~(1<<PD4);
		}

		/* Logic for button presses (handled in Count-down mode) */
		if(COUNTUP == 0) {

			/* Decrement seconds */
			if (!(PINB & (1<<PB5))) {
				_delay_ms(30); /* Debounce */
				if (!(PINB & (1<<PB5))) {
					if(decreaseSecFlag) {
						decreaseSecFlag = 0;  /* Reset button flag - done for all */
						decrementSeconds();
					}
				}
			}
			else {
				decreaseSecFlag = 1;
			}

			/* Increment seconds */
			if (!(PINB & (1<<PB6))) {
				_delay_ms(30); /* Debounce */
				if (!(PINB & (1<<PB6))) {
					if(increaseSecFlag) {
						increaseSecFlag = 0;
						incrementSeconds();
					}
				}
			}
			else {
				increaseSecFlag = 1;
			}

			/* Decrement minutes */
			if (!(PINB & (1<<PB3))) {
				_delay_ms(30); /* Debounce */
				if (!(PINB & (1<<PB3))) {
					if(decreaseMinFlag) {
						decreaseMinFlag = 0;
						decrementMinutes();
					}
				}
			}
			else {
				decreaseMinFlag = 1;
			}

			/* Increment minutes */
			if (!(PINB & (1<<PB4))) {
				_delay_ms(30); /* Debounce */
				if (!(PINB & (1<<PB4))) {
					if(increaseMinFlag) {
						increaseMinFlag = 0;
						incrementMinutes();
					}
				}
			}
			else {
				increaseMinFlag = 1;
			}

			/* Decrement hours */
			if (!(PINB & (1<<PB0))) {
				_delay_ms(30); /* Debounce */
				if (!(PINB & (1<<PB0))) {
					if(decreaseHourFlag) {
						decreaseHourFlag = 0;
						decrementHours();
					}
				}
			}
			else {
				decreaseHourFlag = 1;
			}

			/* Increment hours */
			if (!(PINB & (1<<PB1))) {
				_delay_ms(30); /* Debounce */
				if (!(PINB & (1<<PB1))) {
					if(increaseHourFlag) {
						increaseHourFlag = 0;
						incrementHours();
					}
				}
			}
			else {
				increaseHourFlag = 1;
			}
		}

		/* Update the 7-segment display */
		updateDisplay();
	}

	return 0;
}

/*------------------------FUNCTION DEFINITIONS---------------------------*/
/* 7-segment display update */
void updateDisplay(){
	int sec_tens = seconds/10;
	int sec_ones = seconds % 10;
	int minutes_tens = minutes/10;
	int minutes_ones = minutes % 10;
	int hours_tens = hours/10;
	int hours_ones = hours % 10;

    /* Turn on each 7-segment display one by one */
	enableDisplay(6);
	SendDigitToDisp(sec_ones);
	_delay_ms(2);

	enableDisplay(5);
	SendDigitToDisp(sec_tens);
	_delay_ms(2);

	enableDisplay(4);
	SendDigitToDisp(minutes_ones);
	_delay_ms(2);

	enableDisplay(3);
	SendDigitToDisp(minutes_tens);
	_delay_ms(2);

	enableDisplay(2);
	SendDigitToDisp(hours_ones);
	_delay_ms(2);

	enableDisplay(1);
	SendDigitToDisp(hours_tens);
	_delay_ms(2);
}

/* Enable each 7-segment display */
void enableDisplay(char sevenseg){

	PORTA &= ~(0x3F);  /* Disable all displays first */
	switch(sevenseg) {
		case 1:
			PORTA = (1<<PA0);
			break;
		case 2:
			PORTA = (1<<PA1);
			break;
		case 3:
			PORTA = (1<<PA2);
			break;
		case 4:
			PORTA = (1<<PA3);
			break;
		case 5:
			PORTA = (1<<PA4);
			break;
		case 6:
			PORTA = (1<<PA5);
			break;
	}
}

/* Send digit to the 7-segment display */
void SendDigitToDisp(char digit){
	PORTC = digit & 0x0F; /* Send the 4 LSB to the decoder */
}

/* Function to handle counting logic (count-up or countdown) */
void handleCounting(){
    if (COUNTUP) {  /* Count-up mode */
        seconds++;
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
        }
        if (minutes >= 60) {
            minutes = 0;
            hours++;
        }
        if (hours >= 24) {
            hours = 0;
        }
    }
    else {  /* Countdown mode */
        if (hours == 0 && minutes == 0 && seconds == 0) {
            /* Trigger buzzer when countdown reaches zero */
            PORTD |= (1<<PD0);  /* Activate buzzer */
            TCCR1B &= ~(1<<CS10) & ~(1<<CS11) & ~(1<<CS12);  /* Stop the timer */
        } else {
            /* Countdown logic */
            if (seconds > 0) {
                seconds--;
            } else {
                seconds = 59;
                if (minutes > 0) {
                    minutes--;
                } else {
                    minutes = 59;
                    if (hours > 0) {
                        hours--;
                    }
                }
            }
        }
    }
}

/* Increment hours, minutes, and seconds */
void incrementHours(){
	if(hours < 23)
		hours++;
	else
		hours = 0;
	updateDisplay();
}

void incrementMinutes(){
	if(minutes < 59)
		minutes++;
	else {
		minutes = 0;
		incrementHours();
	}
	updateDisplay();
}

void incrementSeconds(){
	if(seconds < 59)
		seconds++;
	else {
		seconds = 0;
		incrementMinutes();
	}
	updateDisplay();
}

/* Decrement hours, minutes, and seconds */
void decrementHours(){
	if(hours > 0)
		hours--;
	updateDisplay();
}

void decrementMinutes(){
	if(minutes > 0)
		minutes--;
	updateDisplay();
}

void decrementSeconds(){
	if(seconds > 0)
		seconds--;
	updateDisplay();
}
