#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>

#define F_CPU 1000000UL
#define TEMP_NUM 98
#define HIGH_CUT 152
#define LOW_CUT 148
#define MAX_COUNT 16

int serial_putchar(char, FILE *);
int serial_getchar(FILE *);
static FILE serial_stream = FDEV_SETUP_STREAM (serial_putchar, serial_getchar, _FDEV_SETUP_RW);

void init_serial(void);
void do_program(void);
void init_temp(void);
void init_pwm(void);
void init_freq(void);
void init_interrupts();
void adjust_pwm(unsigned int, unsigned int);
unsigned int get_temp(unsigned int);
unsigned long get_freq(unsigned int*, unsigned long*);

volatile unsigned int global_Counter;

/* This is the frequency tracker code running on an AVR */
/* Communicates serially to a computer for displaying information */
/* Handles temperature control and freqency measurement */
int main(void) { 
	init_serial();
	init_temp();
	init_pwm();
	init_freq();
	_delay_ms(2000);
	while(1) { 
		do_program();
	}   
	return 0;
}

ISR(TIMER1_OVF_vect) {
	global_Counter++;
}

/* Initializes AVR USART for 1200 baud (assuming 1MHz clock) */
/* 1MHz/(16*(51+1)) = 1202 about 0.2% error                  */
void init_serial(void) {
	UBRR0H=0;
	UBRR0L=12; // 1200 BAUD FOR 1MHZ SYSTEM CLOCK
	UCSR0A= 1<<U2X0;
	UCSR0C= (1<<USBS0)|(3<<UCSZ00) ;  // 8 BIT NO PARITY 2 STOP
	UCSR0B=(1<<RXEN0)|(1<<TXEN0)  ; //ENABLE TX AND RX ALSO 8 BIT
	stdin=&serial_stream;
	stdout=&serial_stream;
}   

/* Program to do */
void do_program(void) {
	unsigned long freq, periods; //Long for holding the value
	unsigned int time, temp1, temp2; //Only have two temp sensors

	fprintf(stdout,"Welcome to the Quartz-Crystal Frequency Tracker \r\n");

	_delay_ms(200);

 	while(1) {
		//fprintf(fp,"Getting temps \r\n");
		temp1 = get_temp(1);
		temp2 = get_temp(2);
		adjust_pwm((temp1+temp2)/2, 0);
		freq = get_freq(&time, &periods);
 		fprintf(stdout,"Freq = %ld, time = %d, count = %ld \r\n", freq, time, periods);
		_delay_ms(500); // For stability
	}
}

void init_temp(void) {
	DDRC = 0;
	PRR = 0x00;

	ADMUX |= (0<<REFS1) |
		(1<<REFS0) | 
		(0<<ADLAR) |
		(0<<MUX2) |
		(0<<MUX1) |
		(0<<MUX0);

	ADCSRA |= (1<<ADEN) |
		(0<<ADSC) |
		(0<<ADATE) |
		(0<<ADIF) |
		(0<<ADIE) |
		(1<<ADPS2) |
		(0<<ADPS1) |
		(1<<ADPS0);

	ADCSRB = 0;
	
	TIMSK1 |= (1 << TOIE1);

	TCCR1B |= (1 << CS12) |
		(1 << CS11) |
		(1 << CS10);
}

void init_pwm(void) {
	DDRD = 0xFF;
	DDRB |= (1 << 3) | (1 << 5) | (1 << 6);

	OCR0B = 0xFA;
	OCR0A = 0xF9;

	OCR2B = 0xFA;
	OCR2A = 0xF9;

	TCCR0A = 0xB3;
	TCCR0B = 0x05;

	TCCR2A = 0xB3;
	TCCR2B = 0x05;
}

unsigned int get_temp(unsigned int channel) {
	unsigned char i = 8;
	unsigned int temp = 0;


	// Selecting ADC channel to look at.
	switch (channel) {
		case 1:
			ADMUX = 0x40;
			break;
		case 2:
			ADMUX = 0x41;
			break;	
	}

	// Start ADC and wait for it to get 8 values
	ADCSRA |= (1<<ADSC);
	while (i--) {
		while ((ADCSRA & (1 << ADSC)) != 0);
		temp += ADC;
		_delay_ms(50);
	}

	// Calculate temp
	temp = temp / 16;

	return temp;
}

void adjust_pwm(unsigned int temp, unsigned int channel) {
	TCCR2B = 0x00;
	TCCR0B = 0x00;	

	unsigned int temp_temp = (2*temp - TEMP_NUM);

	if (temp_temp <= LOW_CUT) {
		OCR2B = 0xFF;
		OCR2A = 0xFF;
		OCR0B = 0xFF;
		OCR0A = 0xFF;
	} else if (temp_temp >= HIGH_CUT) {
		OCR2B = 0x00;
		OCR2A = 0x00;
		OCR0B = 0x00;
		OCR0A = 0x00;
	} else {
		OCR2B = 0x7F;
		OCR2A = 0x7F;
		OCR0B = 0x7F;
		OCR0A = 0x7F;
	}

	TCCR2B = 0x05;
	TCCR0B = 0x05;
}

// Initialize frequency counter
void init_freq(void) {
	DDRB &= 0xFE;
	TCCR1A = 0x00;
	TCCR1B = 0xC1;
	TIMSK1 = 0x01;
}

void init_interrupts(void) {
	sei();
	global_Counter = 0;
}

void turn_off_interrupts(void) {
	cli();
}

// Counting frequency
unsigned long get_freq(unsigned int* period_time, unsigned long* periods) {
	unsigned long freq;
	unsigned int time, count;
	unsigned long period = 0;
	char lock = 0;

	init_interrupts();	
	TIFR1 = 0xFF;
	while((!((TIFR1 >> ICF1) & 0x01))&&(global_Counter < 16));
	// Start at zero count
	TCCR1B = 0xC0; // Timer off
	global_Counter = 0;
	TCNT1 = 0x00;
	TIFR1 = 0xff; // Note: Clears all flags
	TCCR1B = 0xC1; // Timer on

	// Stays here until unlocked
	while (!lock) {
		// Checks for input capture
		if ((TIFR1 >> ICF1) & 0x01) {
			TIFR1 = 0xff;
			count = global_Counter;
			time = ICR1L;
			time = ((ICR1H << 8) & 0xff00) | (time & 0x00ff);
			period++;
			
			// If the timer has past a time (after we have an input capture)
			// Calculate frequency and unlock
			if ((time >= 32000) || (count != 0)) {
				// Note: The divide by 52 is a calibration
				freq = (period * 1000000l)/(count*65536 + time + (period / 52));
				lock = 1;
			}
		}

		// If we've gone too long without an input capture frequency must be zero.
		if (global_Counter >= 16) {
			freq = 0;
			lock = 1;
		}
	}
	
	turn_off_interrupts();
	*period_time = time;
	*periods = period;
	return freq;
}

//simplest possible putchar, waits until UDR is empty and puts character
int serial_putchar(char val, FILE * fp) {
	while((UCSR0A&(1<<UDRE0)) == 0); //wait until empty 
	UDR0 = val;
	return 0;
}

//simplest possible getchar, waits until a char is available and reads it
//note:1) it is a blocking read (will wait forever for a char)
//note:2) if multiple characters come in and are not read, they will be lost
int serial_getchar(FILE * fp) {
	while((UCSR0A&(1<<RXC0)) == 0);  //WAIT FOR CHAR
	return UDR0;
}     


