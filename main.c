//Main program for Jory Reistad, Carolyn Pugliano Senior Project
//
//We have 1 freq counter, 2 pwm generators, 2 ADCs measuring ~DC voltage
//and some output pins for handling the LCD.
//

#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>

//#include "init.h"
//#include "temp.h"
//#include "freq.h"
//#include "display.h"

#include "lcd.h"

int main() {
//	init();
	lcd_init(LCD_DISP_ON_CURSOR_BLINK);
	int temp = 0;
	int freq = 0;
	while (1) {
//		temp = get_temp();

//		adjust_temp(temp);		

//		freq = get_freq();

		//display(temp, freq);

		
	}
	return 0;
}
