// Library file for initialization of the AVR
// Sets up ADCs, PWM generators, 16-bit timer and output
// pins for the LCD.

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

void init_ADCs(void);
void init_PWM_gens(void);
void init_freq_count();
void init_lcd_pins();

void init(void) {
	init_ADCs();
	init_PWM_gens();
	init_freq_count();
	init_lcd_pins();
}

void init_ADCs(void) {

}

void init_PWM_gens(void) {

}

void init_freq_count(void) {

}

void init_lcd_pins(void) {

}
