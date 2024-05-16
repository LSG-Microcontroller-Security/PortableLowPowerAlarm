#include "SoftwareSerial.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/pgmspace.h>
#include<avr/io.h>
#include<avr/interrupt.h>

/// <summary>
/// 8mhz speed clock is right
/// </summary>
/// 
/// /// <summary>
/// https://www.gadgetronicx.com/attiny85-sleep-modes-tutorial/
/// https://www.avrfreaks.net/s/topic/a5C3l000000UQxAEAW/t117402
/// </summary>
/// 

unsigned long start_timer = 0;

unsigned long turn_off_timer = 0;

volatile bool is_on_interrupt = false;

//bool volatile is_on_watch_dog = false;
//uint8_t volatile watch_dog_counter = 0;

bool is_on_power_safe = true;
// volatile uint8_t watchDogCounter = 0;
// volatile bool isWatchDogEvent = false;
//  uint8_t voltagePin = A2;
bool is_call_disabled = false;

// uint8_t wd_timer = 2;
// bool wd_isActive = false;

uint8_t sim_boot_pin = 0;

uint8_t sim_module_rx_pin = 1;

uint8_t interrupt_pin = 2;

uint8_t sim_module_tx_pin = 3;

uint8_t debug_tx_pin = 4;

byte adcsra = ADCSRA;

#define ATD "atd"

#define ENABLE_POWER_SAFE "s"

#define SMS_MINUTES_TIME 3UL

// #define ENABLE_CALLS "y"
// #define DISABLE_CALLS "x"

#define DELETE_X_SMS_ELEMENT "AT+CMGD="

#define READ_X_SMS_ELEMENT "AT+CMGR="

//# define WDTO_DELAY WDTO_4S

// #define _DEBUG

void setup(){

#ifdef _DEBUG
	debugOnSerial("r");
#endif // _DEBUG

	pinMode(sim_boot_pin, OUTPUT);

	pinMode(interrupt_pin, INPUT_PULLUP);

	delay(20000);

	set_sms_receiver();

	callPhoneNumber();

	while (millis() < (SMS_MINUTES_TIME * 60UL * 1000UL)){
		startSMSActivity();
	}

	external_interrupt();

	attachInterrupt(0, interrupt_function, FALLING);
}

void interrupt_function(){
	is_on_interrupt = true;
}

void loop()
{
#ifdef _DEBUG
	char _mem[5];
	itoa(freeRam(), _mem, 10);
	debugOnSerial(_mem);
#endif // _DEBUG

	if (!is_on_interrupt) {
	
		turn_sim800c_off();
	
#ifdef _DEBUG
		debugOnSerial("zz.");
#endif // _DEBUG

		adcsra = ADCSRA; // save the ADC Control and Status Register A

		// mySerial.print("adcsra = "); mySerial.println(adcsra,HEX);

		ADCSRA = 0;			 // Turn off ADC

		power_all_disable(); // Power off ADC, Timer 0 and 1, serial interface

		enters_sleep();

		MCUCR &= ~(1 << SE);

		power_all_enable(); // Power everything back on

		ADCSRA = adcsra;	// restore ADCSRA

#ifdef _DEBUG
		debugOnSerial("w.");
#endif // _DEBUG

		turn_sim800c_on();
	}
	is_on_interrupt = false;

	callPhoneNumber();

	delay(15000);
}

void enters_sleep()
{
	MCUCR |= (1 << SM1);      // enabling sleep mode and powerdown sleep mode
	MCUCR |= (1 << SE);     //Enabling sleep enable bit
	__asm__ __volatile__("sleep" "\n\t" ::); //Sleep instruction to put controller to sleep
	//controller stops executing instruction after entering sleep mode  
}

void external_interrupt()
{
	sei();                //enabling global interrupt
	GIMSK |= (1 << PCIE);    //Pin change interrupt enable
	PCMSK |= (1 << PCINT2);   //Pin change interrupt to 2nd pin PB2
}

void set_sms_receiver()
{
	SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	mySerial.println(F("AT"));

	delay(100);

	mySerial.println(F("AT+CPMS=\"SM\""));

	delay(1000);

	mySerial.println(F("AT"));

	delay(100);
	mySerial.println(F("AT+CMGF=1"));

	delay(1000);

	mySerial.println(F("AT"));
	delay(100);
	mySerial.println(F("AT+CMGD=1,4"));

	delay(1000);

	// mySerial.println(F("AT"));
	// delay(100);
	// mySerial.println(F("AT+CNETLIGHT=0"));

	delay(1000);

	mySerial.println(F("AT"));
	delay(100);
	mySerial.println(F("AT+CSCLK=2"));

	delay(1000);
}

void startSMSActivity(){
	getTaggedSmsFromResponse('#');
}

void switch_sim()
{
	digitalWrite(sim_boot_pin, HIGH);
	delay(5000);
	digitalWrite(sim_boot_pin, LOW);
	delay(10000);
}

bool exctractSmsTagged(char tag, char* sms)
{
	bool returnValue = false;

	SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	// clear buffer
	while (mySerial.available() > 0)
	{
		mySerial.read();
	}
	delay(1000);

	for (uint8_t index = 1; index < 4; index++)
	{
		char number[2] = { index + 48, '\0' };

		char command[10] = READ_X_SMS_ELEMENT;

		strcat(command, number);

		mySerial.println(F("AT"));

		delay(100);

		mySerial.println(command);

		delay(3000);

		if (mySerial.available() > 0)
		{
			if (mySerial.readStringUntil(tag).length() > 0)
			{
				// blinkLed(1000);
				if (mySerial.available() > 0)
				{
					mySerial.readStringUntil(tag).toCharArray(sms, 20, 0);

					char command[10] = DELETE_X_SMS_ELEMENT;

					strcat(command, number);

					mySerial.println(command);

					returnValue = true;
				}
			}
		}
	}

	return returnValue;
}

//void setup_watchdog(int delay_time)
//{
//	byte bb;
//	if (delay_time > 9) delay_time = 9;
//	bb = delay_time & 7;
//	if (delay_time > 7) bb |= (1 << 5);
//	bb |= (1 << WDCE);
//
//	MCUSR &= ~(1 << WDRF);
//	// start timed sequence
//	WDTCR |= (1 << WDCE) | (1 << WDE);
//	// set new watchdog timeout value
//	WDTCR = bb;
//	WDTCR |= _BV(WDIE);
//}
//
//ISR(WDT_vect){
//	if (watch_dog_counter == 1)
//	{
//		is_on_watch_dog = true;
//		watch_dog_counter = 0;
//	}
//	else
//	{
//		watch_dog_counter++;
//	}
//}

// void clearAllSms()
//{
//	SoftwareSerial mySerial(0, 3);
//	mySerial.begin(19200);
//	delay(3000);
//	mySerial.println(F("AT"));
//	delay(100);
//	mySerial.println(F("AT+CMGD=1,4"));
// }

// bool isSmsOnPowerSafeOff(char* sms)
//{
//	if (strcmp(sms, "y") == 0)
//	{
//		return  true;
//	}
//	return false;
// }

// bool isSmsOnPowerSafeOn(char* sms)
//{
//	if (strcmp(sms, "s") == 0)
//	{
//		return  true;
//	}
//	return false;
// }

bool isSmsCodeFind(char* sms, char code[1])
{
	if (strcmp(sms, code) == 0)
	{
		return true;
	}
	return false;
}

void getTaggedSmsFromResponse(char tag)
{

	char sms[12] = "";

	exctractSmsTagged(tag, sms);

#ifdef _DEBUG
	debugOnSerial(sms);
#endif // _DEBUG

	char phone_c = ' ';

	phone_c = sms[0];

	// check if sms is a configuration phone number
	if (phone_c >= 48 && phone_c <= 57)
	{
		char charToWrite = ' ';

		uint8_t cicle = 0;

		while (charToWrite != '\0' && cicle < 20)
		{
			charToWrite = sms[cicle];
			if (charToWrite != '\0')
			{
				 //char s[1]{};
				 //s[0] = charToWrite;
				 //debugOnSerial(s);
				eeprom_write_byte((uint8_t*)cicle + 100, charToWrite);
				cicle++;
			}
		}
		callPhoneNumber();
	}

	//	if (isSmsOnPowerSafeOff(sms))
	//	{
	//		is_on_power_safe = false;
	//		callPhoneNumber();
	// #ifdef _DEBUG
	//		debugOnSerial("deactivate safeMode");
	// #endif
	//	}

	// if (isSmsCodeFind(sms, DISABLE_CALLS))
	// {
	// 	is_call_disabled = true;
	// }
	//
	// if (isSmsCodeFind(sms, ENABLE_CALLS))
	// {
	//
	// 	is_call_disabled = false;
	//
	// 	callPhoneNumber();
	//
	// 	delay(5000);
	// }
	//
	if (isSmsCodeFind(sms, ENABLE_POWER_SAFE))
	{

		is_on_power_safe = true;

		callPhoneNumber();

		delay(5000);
	}

	// String response = "";
	// SoftwareSerial mySerial(0, 3);
	// mySerial.begin(19200);
	// delay(1000);
	// mySerial.readString();
	// delay(1000);
	// mySerial.println("AT+CMGR=1");
	// delay(2000);
	// if (mySerial.available() > 0)
	//{
	//	response = mySerial.readStringUntil('#');
	//	if (response.length() > 0)
	//	{
	//		if (mySerial.available() > 0)
	//		{
	//			String phoneNumber = mySerial.readString();
	//
	//			bool verify = true;

	//			for (uint8_t i = 0; i < 10; i++)
	//			{
	//				if (phoneNumber[i] < 48 && phoneNumber[i] > 57)
	//				{
	//					verify = false;
	//				}
	//			}
	//			/*mySerial.println(freeRam());*/
	//			//mySerial.println(phoneNumber);
	//			//if (verify)
	//			//{
	//			//	mySerial.println("AT+CHUP");
	//			//	delay(5000);
	//			//	check = 1;
	//			//	for (uint8_t i = 0; i < 10; i++)
	//			//	{
	//			//			//eeprom_write_byte((uint8_t*)i, response[i]);
	//			//	}

	//			//	String command = F("atd");
	//			//	command.concat(phoneNumber);
	//			//	command.concat(';');
	//			//	mySerial.println(command);
	//			//	delay(5000);
	//			//}
	//		}
	//	}
	//}
	// return;

	// if (mySerial.available() > 0)
	//{
	//	if (mySerial.readString().indexOf('#') != -1)
	//	{
	//		while (mySerial.available() > 0) {
	//			response.concat((char)mySerial.read());
	//		}
	//		//mySerial.print("x"); mySerial.print(response); mySerial.println("x");
	//	}

	//	////int index = response.lastIndexOf(F("#"));
	//	///*if (index != -1 && check == 0)*/
	//	//if (check == 0)
	//	//{
	//	//	/*String phoneNumber = response.substring(index + 1, index + 11);*/
	//	//	//mySerial.print("phoneNumber :"); mySerial.println(phoneNumber);
	//	//	bool verify = true;

	//	//	for (uint8_t i = 0; i < 10; i++)
	//	//	{
	//	//		if (response[i] < 48 && response[i] > 57)
	//	//		{
	//	//			verify = false;
	//	//		}
	//	//	}

	//	//	if (verify)
	//	//	{
	//	//		mySerial.println("AT+CHUP");
	//	//		delay(5000);
	//	//		check = 1;

	//	//		for (uint8_t i = 0; i < 10; i++)
	//	//		{
	//	//			if (response[i] >= 48 && response[i] <= 57)
	//	//			{
	//	//				//eeprom_write_byte((uint8_t*)i, response[i]);
	//	//			}
	//	//		}

	//	//		String command = F("atd");
	//	//		command.concat(response);
	//	//		command.concat(';');
	//	//		mySerial.println(command);
	//	//		delay(5000);
	//	//	}
	//	//}
	//	int index = response.lastIndexOf(F("&"));

	//	if (index != -1)
	//	{
	//		is_on_power_safe = false;
	//	}
	//}
}

//void enter_sleep()
//{
//
//	//byte adcsra;
//
//	cli();
//
//	GIMSK &= ~(1 << PCIE);
//	delay(100);
//
//	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//
//	//adcsra = ADCSRA; // save the ADC Control and Status Register A
//	// mySerial.print("adcsra = "); mySerial.println(adcsra,HEX);
//
//	//ADCSRA = 0; // Turn off ADC
//
//	//power_all_disable(); // Power off ADC, Timer 0 and 1, serial interface
//
//	sleep_enable();
//
//	sei();
//	GIFR |= bit(PCIF); // clear any outstanding interrupts
//	GIMSK |= bit(PCIE); // enable pin change interrupts
//	delay(100);
//
//	sleep_cpu();
//	// zzz
//	// Wake up
//	cli();
//	GIMSK &= ~(1 << PCIE);
//	delay(100);
//
//	sleep_disable();
//	//
//	//power_all_enable(); // Power everything back on
//
//	//ADCSRA = adcsra; // restore ADCSRA
//
//}

void callPhoneNumber()
{
	if (is_call_disabled)
		return;

	char phoneNumber[11]{};

	for (uint8_t i = 0; i < 10; i++)
	{
		phoneNumber[i] = (char)eeprom_read_byte((uint8_t*)i + 100);
	}

	strcat(phoneNumber, "\0");

	callPhoneNumber(phoneNumber);
}

void callPhoneNumber(char* phoneNumber)
{
#ifdef _DEBUG
	debugOnSerial("call.");
#endif // _DEBUG


	SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	char command[30]{};

	strcat(command, ATD);

	strcat(command, phoneNumber);

	strcat(command, ";");

	strcat(command, "\0");

	mySerial.println(F("AT"));

	delay(1000);

	mySerial.println(command);

	delay(10000);

	/*mySerial.println(F("AT"));
	delay(1000);
	mySerial.println(F("AT+CNETLIGHT=0"));*/
}

// void setup_watchdog(int ii)
//{
//
//	byte bb;
//	int ww;
//	if (ii > 9)
//		ii = 9;
//	bb = ii & 7;
//	if (ii > 7)
//		bb |= (1 << 5);
//	bb |= (1 << WDCE);
//	ww = bb;
//
//	MCUSR &= ~(1 << WDRF);
//	// start timed sequence
//	WDTCR |= (1 << WDCE) | (1 << WDE);
//	// set new watchdog timeout value
//	WDTCR = bb;
//	WDTCR |= _BV(WDIE);
// }

// ISR(WDT_vect)
//{
//	if (watchDogCounter == wd_timer)
//	{
//		isWatchDogEvent = true;
//		watchDogCounter = 0;
//	}
//	else
//	{
//		watchDogCounter++;
//	}
// }

int freeRam()
{
	extern int __heap_start, * __brkval;
	int v;
	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void debugOnSerial(char* stringa)
{
	SoftwareSerial mySerial(99, 4, false);
	mySerial.begin(9600);
	delay(500);
	mySerial.print(F("..."));
	mySerial.println(stringa);
}

void turn_sim800c_on()
{
	digitalWrite(sim_boot_pin, HIGH);
	delay(5000);
	digitalWrite(sim_boot_pin, LOW);
	delay(10000);

	/*SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	bool check = false;

	while (!check)
	{

		while (mySerial.available() > 0)
		{
			mySerial.read();
		}

		mySerial.println(F("AT"));

		delay(5000);

		if (mySerial.available() > 0)
		{
			if (mySerial.readString().indexOf(F("OK")) != -1)
			{
				check = true;
			}
		}
		else
		{
			switch_sim();
		}
	}*/
}

void turn_sim800c_off()
{

	SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	int i = 0;

	for (i; i < 2; i++)
	{

		mySerial.println(F("AT"));

		delay(5000);

		while (mySerial.available() > 0)
		{
			switch_sim();

			delay(5000);

			while (mySerial.available() > 0)
			{
				mySerial.read();
			}
			mySerial.flush();

			mySerial.println(F("AT"));

			delay(5000);
		}
	}
	digitalWrite(sim_boot_pin, HIGH);
}
