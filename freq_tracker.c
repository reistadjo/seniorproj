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
unsigned long get_freq(void);

volatile unsigned int global_Counter = 0;

/* this is the high_low server running on serial on an avr */
/* it sets up the serial communication for 1200 baud */
/* note that the random number generator will always generate the same sequence */
int main(void) { 
	init_serial();
	init_temp();
	init_pwm();
	init_freq();
	//init_interrupts();
	_delay_ms(2000);
	while(1) { 
		do_program();
	}   
	return 0;
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

/* Server code... */
void do_program(void) {
	FILE *fp, *fpr;

	unsigned long freq;
	unsigned int temp1, temp2, temp3, temp4;

	fp=stdout;
 
	fprintf(fp,"Welcome to the Quartz-Crystal Frequency Tracker \r\n");

	_delay_ms(200);

 	while(1) {
		//fprintf(fp,"Getting temps \r\n");
		temp1 = get_temp(1);
		temp2 = get_temp(1);
		temp3 = get_temp(2);
		temp4 = get_temp(3);
		adjust_pwm(temp2, 0);
		freq = get_freq();
 		fprintf(fp,"Freq = %d, Temp = %d,%d \r\n", freq, temp3, temp4);
		_delay_ms(200);
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
	DDRB |= (1 << 3);

	OCR2B = 0xFA;
	OCR2A = 0xF9;

	TCCR2A = 0xB3;
	TCCR2B = 0x05;
}

unsigned int get_temp(unsigned int channel) {
	unsigned char i = 8;
	unsigned int temp = 0;

/*
	switch (channel) {
		case 0:
			ADMUX = 0x40;
			break;
		case 1:
			ADMUX = 0x41;
			break;
		case 2:
			ADMUX = 0x42;
			break;
		case 3:
			ADMUX = 0x43;
			break;	
	}
*/
	ADMUX = 0x40;
	ADCSRA |= (1<<ADSC);
	while (i--) {
		while ((ADCSRA & (1 << ADSC)) != 0);
		temp += ADC;
		_delay_ms(50);
	}

	temp = temp / 16;

	return temp;
}

void adjust_pwm(unsigned int temp, unsigned int channel) {
	TCCR2B = 0x00;	

	unsigned int temp_temp = (2*temp - TEMP_NUM);

	if (temp_temp <= LOW_CUT) {
		OCR2B = 0xFF;
		OCR2A = 0xFF;
	} else if (temp_temp >= HIGH_CUT) {
		OCR2B = 0x00;
		OCR2A = 0x00;
	} else {
		OCR2A = 0x7F;
		OCR2B = 0x7F;
	}

	TCCR2B = 0x05;
}

void init_freq(void) {
	DDRB &= 0xFE;
	TCCR1A = 0x00;
	TCCR1B = 0xC1;
}

void init_interrupts(void) {
	
}

unsigned long get_freq(void) {
	unsigned long freq;
	unsigned long period = 0;
	unsigned long initial_count = 0;
	char lock = 0;

	TCCR1B = 0xC0;
	global_Counter = 0;
	TCNT1 = 0x00;
	TIFR1 = 0xff;
	TCCR1B = 0xC1;

	while(!((TIFR1 >> ICF1) & 0x01));
	TIFR1 = 0xFF;
	initial_count = ICR1L;
	initial_count = (ICR1H << 8) | initial_count;

	while (!lock) {
		if ((TIFR1 >> ICF1) & 0x01) {
			TIFR1 = 0xff;
			freq = ICR1L;
			freq = (ICR1H << 8) | freq;
			period++;
			if (freq > 32000 || global_Counter != 0) {
				freq = (period * 1000000l)/(global_Counter*65536 + (freq-initial_count));
				lock = 1;
			}
		}
		if (global_Counter > 16) {
			freq = 0;
			lock = 1;
		}
	}

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


