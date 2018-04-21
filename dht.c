#define F_CPU 1000000UL //clock frequency
#include <avr/io.h>
#include <util/delay.h>
#include <avr/lcd4bits.h>
#include <avr/dht11.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/interrupt.h>

unsigned int I_RH,D_RH,I_Temp,D_Temp,CheckSum,Page=0,rh=47,TempUp=37,TempLow=10,TempDown=27,ButtonPressed=0,IrrgStpDnLimit=10,InputDuration=100;
//RH for pinc0,rh for set humidity
int Pressed = 0;
int Pressed_Confidence_Level = 0; //Measure button press confidence
int Released_Confidence_Level = 0; //Measure button release confidence
volatile unsigned long milliseconds;

void init_timerModule();//function for timer in atmega32
uint32_t Time1=5,Time3,Time4,Time5,finalTime;
void ShutterOpen();
void ShutterClose();
void Indication();
void GotoSetMode();
ISR(TIMER1_COMPA_vect);
char data[5];
int main(void)
{

	DDRB |= (1<<PINB0 | 1<<PINB1 | 1<<PINB2 | 1<<PINB4 | 1<<PINB5 | 1<<PINB6 | 1<<PINB7);//lcd
	DDRD |= 1<<PIND0;// water indication
	DDRD &=~(1<<PIND1 | 1<<PIND2 | 1<<PIND3 | 1<<PIND4 | 1<<PIND5 | 1<<PIND6 | 1<<PIND7); //input for sensors
	DDRC |= (1<<PINC0 | 1<<PINC1 | 1<<PINC2 | 1<<PINC3 | 1<<PINC4 | 1<<PINC5 | 1<<PINC6);
	PIND  &=~(1<<PIND1 | 1<<PIND2 | 1<<PIND3 | 1<<PIND4 | 1<<PIND5 | 1<<PIND6 | 1<<PIND7);
	PORTC &=~(1<<PINC0 | 1<<PINC1 | 1<<PINC2 | 1<<PINC3 | 1<<PINC4 | 1<<PINC5 | 1<<PINC6);// Indication output
	
	lcd_init(); // LCD initialization
	init_timerModule();//Data for Interrupt function	
	sei(); // Global interrupt function
	while(1)
	{
		if (Page==0)//programmer mode is off (bit_is_clear(PIND,1))
		{
			_delay_ms(2000);
			Request();
			_delay_us(40);  /* send start pulse */
			Response();  /* receive response */
			I_RH=Receive_data();	               /* store first eight bit in I_RH */
			D_RH=Receive_data();	               /* store next eight bit in D_RH */
			I_Temp=Receive_data();                 /* store next eight bit in I_Temp */
			D_Temp=Receive_data();	               /* store next eight bit in D_Temp */
			CheckSum=Receive_data();               /* store next eight bit in CheckSum */
			if ((I_RH + D_RH + I_Temp + D_Temp) != CheckSum)
			{
				lcd_gotoLineOne();
				dis_string("Error");
			}
			else
			{
				lcd_gotoLineOne();
				dis_string("  ROSA FARMING  ");
				_delay_ms(500);
				lcd_gotoLineTwo();
				itoa(I_RH,data,10);
				dis_string("RH: ");
				dis_string(data);
				itoa(D_RH,data,10);
				dis_string("%");
				itoa(I_Temp,data,10);
				dis_string(" T: ");
				dis_string(data);
				itoa(D_Temp,data,10);
				dis_data(0xDF);
				dis_string("C        ");
				itoa(CheckSum,data,10);
				dis_string(data);
				dis_string("   ");	
	         }
			Indication();
			ShutterOpen();
			ShutterClose();
			if (bit_is_set(PIND,5) && ((I_Temp + D_Temp)>=IrrgStpDnLimit) && ((I_RH + D_RH )<=rh)) // water is high and temp is greater than upper limit and humidity is less than user define value
			{
				PORTC |= 1<<PINC3; //On the IRRIGATION indication
				PORTC |= 1<<PINC0; //ON the SPRAY indication
				_delay_ms(10);
			}
			else //Other wise
			{
				PORTC &=~(1<<PINC3); // Of the IRRIGATION indication
				PORTC &=~(1<<PINC0); //OFF the SPRAY indication
				_delay_ms(10);
			}
			if (bit_is_clear(PIND,5)) // water level check condition
			{
				PORTD |= 1<<PIND0; //on water low indicator
			}
			else
			{
				PORTD &=~1<<PIND0;
			}
		}
		else
		{
			GotoSetMode();
		}
	}
}/*
unsigned long millis(void)
{
	return milliseconds;
}*/

void init_timerModule()
{
	TIMSK |= (1<<OCIE1A);					// Enable the compare match interrupt
	TCCR1B |= (1 << CS10) | (1<<CS11) | (1<<WGM12);	// CTC mode, Clock/64
	OCR1A = 15625-1; // Output Compare Register A					// Load the offset
	ICR1 = 4999;
	return;
}

ISR(TIMER1_COMPA_vect)//Interrupt for toggling multiple function
{
	if(bit_is_clear(PIND,2))
	{
		GotoSetMode();
		Page=1; // PIN0 of PORTC is high
		if (Page>=3) // PIN0 of PORTC is pressed up to 7 times
		{
			Page=0; // Reset the pressed counter of PINC0
		}	
	}
}
void GotoSetMode()
{
 	if(bit_is_clear(PIND,2)) //set mode
	{
		//InputDuration++;
		Pressed_Confidence_Level ++; //Increase Pressed Confidence
		if (Pressed_Confidence_Level >500) //Indicator of good button press
		{
			if (Pressed == 0)
			{
				Page++;
				if (Page==2) //enter into programming mode and set option for humidity
				{	
					lcd_gotoLineOne(); //LCD Crusher on line 1
					dis_string("  PROGRAMMING   ");
					lcd_gotoLineTwo();
					dis_string("                ");
					_delay_ms(2000);
					lcd_gotoLineOne(); //LCD Crusher on line 2
					dis_string("SET IRRIG. POINT");
					_delay_ms(10);
					lcd_gotoLineTwo();
					dis_string("RH: ");
					LCD_DisplayNumber(rh);
					dis_string("%               ");
					rh-1;
				}//end of page 2
				if (Page==3) // reset the counter for programmer mode 
				{
					Page=0;
				}
				Pressed=1;
			}
			Pressed_Confidence_Level = 0;//Zero it so a new pressed condition can be evaluated
			InputDuration=0;
		}
	}
	else
	{
		Released_Confidence_Level ++; //This works just like the pressed
		if (Released_Confidence_Level >500)
		{
			Pressed = 0;
			Released_Confidence_Level = 0;
		}
	}
	if (bit_is_clear(PIND,1)) //  Input values for programmer mode
	{
		Pressed_Confidence_Level ++; //Increase Pressed Confidence
		Released_Confidence_Level = 0; //Reset released button confidence since there is a button press
		if (Pressed_Confidence_Level >500) //Indicator of good button press
		{
			if (Pressed == 0)
			{
				if(InputDuration >= 500)
				{	
					if(Page == 2)
					{
						lcd_gotoLineTwo();
						dis_string("RH: ");
						LCD_DisplayNumber(rh);
						rh++;
						InputDuration++;
						if (rh>=75) // Reset the counter or value of humidity
						{
							rh=0;
						}
					}
				}
				Pressed=1;
				InputDuration = 0;
				Pressed_Confidence_Level = 0;//Zero it so a new pressed condition can be evaluated
			}
		}	
	}
	else
	{
		Released_Confidence_Level ++; //This works just like the pressed
		if (Released_Confidence_Level >500)
		{
			Pressed = 0;
			Released_Confidence_Level = 0;
		}
	}
	return;
}
void ShutterClose()
{
	if((I_Temp + D_Temp)<=TempDown)// Temperature is less than temperature down limit
	{
		if (bit_is_clear(PIND,4))
		{
			PORTC |= 1<<PINC1;//Close the shutter
			//_delay_ms(10);
		}
		else
		{
			PORTC &=~(1<<PINC1);
		//	_delay_ms(10);
		}
	}
	
	return;
}
void ShutterOpen()
{
	if((I_Temp + D_Temp)>=TempUp)// Temperature is less than temperature upper limit
	{
		PORTC &=~1<<PINC4;
		PORTC |= 1<<PINC5;
		if (bit_is_clear(PIND,3))
		{
			PORTC |= 1<<PINC2; //Open the shutter
			//_delay_ms(10);
		}
		else
		{
			PORTC &=~(1<<PINC2);
			//_delay_ms(10);
			
		}
	}
	return;
}
void Indication()
{
	if ((I_Temp + D_Temp)<=TempLow) // Temperature is less than temperature low limit
	{
		PORTC ^= 1<<PINC4;//Toggle COLD temperature indication
		_delay_ms(500);
	}
	if ((I_Temp + D_Temp)>=TempLow && (I_Temp + D_Temp)<=TempUp) //Temperature range is in between temperature low limit and temperature upper limit
	{
		PORTC &=~1<<PINC4; // On the NORMAL temperature indication 
		PORTC &=~(1<<PINC5); // Off the HEAT temperature indication
		_delay_ms(10);
	}
	else // Other wise 
	{
		PORTC &=~1<<PINC5; // Off the HEAT temperature indication
		_delay_ms(10);
	}
	return;
}
		
