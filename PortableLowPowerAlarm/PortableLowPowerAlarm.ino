#include "internal_libraries/SoftwareSerial.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/pgmspace.h>

/// <summary>
/// 8mhz speed clock is right
/// </summary>

unsigned long start_timer = 0;
unsigned long turn_off_timer = 0;
volatile bool is_on_interrupt = false;
bool is_on_power_safe = false;
//volatile uint8_t watchDogCounter = 0;
//volatile bool isWatchDogEvent = false;
// uint8_t voltagePin = A2;
bool is_call_disabled = false;
//uint8_t wd_timer = 2;
//bool wd_isActive = false;

uint8_t sim_boot_pin = 0;
uint8_t sim_module_rx_pin = 1;
uint8_t interrupt_pin = 2;
uint8_t sim_module_tx_pin = 3;
uint8_t debug_tx_pin = 4;

#define ATD "atd"
#define ENABLE_POWER_SAFE "s"
#define ENABLE_CALLS "y"
#define DISABLE_CALLS "x"
#define DELETE_X_SMS_ELEMENT "AT+CMGD="
#define READ_X_SMS_ELEMENT "AT+CMGR="

//#define _DEBUG

void setup()
{
	pinMode(sim_boot_pin, OUTPUT);

	pinMode(interrupt_pin, INPUT_PULLUP);

	//#ifdef _DEBUG
	//	//only use when recompile
	//	delay(60000);
	//#else
	//	delay(5000);
	//#endif

#ifdef _DEBUG
	delay(5000);
	debugOnSerial("r");
#endif

	turn_sim800c_on();

	delay(10000); //Forse inutie..............

	//debugOnSerial("1");

	PCMSK |= bit(PCINT2); // want pin D3 / pin 2

	GIFR |= bit(PCIF); // clear any outstanding interrupts

	GIMSK |= bit(PCIE); // enable pin change interrupts

	set_sms_receiver();

	callPhoneNumber();

	start_timer = millis();

	turn_off_timer = millis();

	attachInterrupt(0, activateSystemInterrupt, FALLING);

	//setup_watchdog(9);

	delay(1000);
}

void loop()
{
//#ifdef _DEBUG
//	debugOnSerial("off.");
//#endif
//
//	turn_sim800c_off();
//
//	delay(10000);
//
//#ifdef _DEBUG
//	debugOnSerial("on.");
//#endif
//
//	turn_sim800c_on();
//
//	delay(10000);
//
//	return;
//#ifdef _DEBUG
//	debugOnSerial("go.");
//#endif

	/*if ((millis() - start_timer) < 120000)
	{
		startSMSActivity();
	}*/

	startSMSActivity();

	if ((millis() - turn_off_timer) > 120000)
	{
		if (!is_on_interrupt)
		{
			if (is_on_power_safe) {

				turn_sim800c_off();
			}
#ifdef _DEBUG
			debugOnSerial("s");
#endif
			enter_sleep();

#ifdef _DEBUG
			debugOnSerial("w");
#endif

			turn_off_timer = millis();

			if (is_on_power_safe) {
				turn_sim800c_on();
				delay(10000);//Forse inutie..............
				set_sms_receiver();
			}
		}
	}

	if (is_on_interrupt && ((millis() - start_timer) > 120000))
	{
//#ifdef _DEBUG
//		debugOnSerial("call");
//#endif

		callPhoneNumber();

		delay(5000);

		/*if (is_on_interrupt != true)
		{*/
		turn_off_timer = millis();

		is_on_interrupt = false;

		/*if (!wd_isActive)
		{
			setup_watchdog(9);
			wd_isActive = true;
		}*/
	}

	// WatchDog Sleep Activity
	//if (isWatchDogEvent && !is_on_power_safe && ((millis() - start_timer) > 120000))
	//{
	//	// blinkLed(1000,1);
	//	//  turn_off_timer = millis();
	//	startSMSActivity();
	//	isWatchDogEvent = false;
	//}
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

#ifdef _DEBUG
	/*mySerial.println(F("AT"));
	delay(100);
	mySerial.println(F("AT+CNETLIGHT=0"));*/
#endif

	delay(1000);

	mySerial.println(F("AT"));
	delay(100);
	mySerial.println(F("AT+CSCLK=2"));

	delay(1000);
}

void startSMSActivity()
{
	getTaggedSmsFromResponse('#');
}


// void watchDogAndSleepActivity()
//{
//	if (isWatchDogEvent)
//	{
// #ifdef _DEBUG
//		debugOnSerial("wdgEv");
// #endif
//		checkBatteryVoltage();
//	}
//	//getTaggedSmsFromResponse('#');
//	//if ((tiltSensorInterrupt == true))
//	//{
//	//	tiltSensorActivity();
//	//}
//	//if (is_on_power_safe) { turnOff(); }else { turnOn(); }
//	isWatchDogEvent = false;
//	enter_sleep();
//	delay(100);
// }



void activateSystemInterrupt()
{
	is_on_interrupt = true;
}

void switch_sim()
{
	digitalWrite(sim_boot_pin, HIGH);
	delay(500);
	digitalWrite(sim_boot_pin, LOW);
	delay(5000);
	digitalWrite(sim_boot_pin, HIGH);
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

// void clearAllSms()
//{
//	SoftwareSerial mySerial(0, 3);
//	mySerial.begin(19200);
//	delay(3000);
//	mySerial.println(F("AT"));
//	delay(100);
//	mySerial.println(F("AT+CMGD=1,4"));
// }

// bool isSmsValidPhoneNumber(char* phoneNumber)
//{
//	char phone_c = ' ';
//	uint8_t cicle = 0;
//	while (phone_c != '\0')
//	{
//		phone_c = phoneNumber[cicle];
//
//		if (phone_c >= 48 && phone_c <= 57)
//		{
//			return true;
//		}
//		cicle++;
//	}
//	return false;
//
//	//if (phoneNumber.length() == 10)
//	//{
//	//	for (uint8_t i = 0; i < phoneNumber.length(); i++)
//	//	{
//	//		if (phoneNumber[i] < 48 && phoneNumber[i] > 57)
//	//		{
//	//			return false;
//	//		}
//	//	}
//	//	return true;
//	//}
//	//else { return false; }
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

void getTaggedSmsFromResponse(char tag){

	char sms[12] = "";

	exctractSmsTagged(tag, sms);

#ifdef _DEBUG
	debugOnSerial(sms);
#endif

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
				// char s[1]{};
				// s[0] = charToWrite;
				// debugOnSerial(s);
				eeprom_write_byte((uint8_t*)cicle, charToWrite);
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

	if (isSmsCodeFind(sms, DISABLE_CALLS))
	{
		is_call_disabled = true;
	}

	if (isSmsCodeFind(sms, ENABLE_CALLS))
	{

		is_call_disabled = false;

		callPhoneNumber();

		delay(5000);
	}

	if (isSmsCodeFind(sms, ENABLE_POWER_SAFE))
	{

		is_on_power_safe = true;

		callPhoneNumber();

		delay(5000);

#ifdef _DEBUG
		// debugOnSerial("dis.safe");
#endif
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

void enter_sleep(){

	byte adcsra;

	cli();
	
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	
	adcsra = ADCSRA; // save the ADC Control and Status Register A
	// mySerial.print("adcsra = "); mySerial.println(adcsra,HEX);
	
	ADCSRA = 0;			 // Turn off ADC
	
	power_all_disable(); // Power off ADC, Timer 0 and 1, serial interface
	
	sleep_enable();
	
	sei();
	
	sleep_cpu();
	// zzz
	// Wake up
	sleep_disable();
	//
	power_all_enable(); // Power everything back on
	
	ADCSRA = adcsra;	// restore ADCSRA
}

void callPhoneNumber(){
	if (is_call_disabled)
		return;

	char phoneNumber[11]{};

	for (uint8_t i = 0; i < 10; i++)
	{
		phoneNumber[i] = (char)eeprom_read_byte((uint8_t*)i);
	}

	strcat(phoneNumber, "\0");

	callPhoneNumber(phoneNumber);
}

void callPhoneNumber(char* phoneNumber){
#ifdef _DEBUG
	debugOnSerial("call.");
#endif
	SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	char command[30]{};
	// mySerial.println("AT");
	// delay(100);
	// globalString = F("atd");
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

//void setup_watchdog(int ii)
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
//}

//ISR(WDT_vect)
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
//}

int freeRam(){
	extern int __heap_start, * __brkval;
	int v;
	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void debugOnSerial(char* stringa){
	//if (!is_debug_writing_enable) return;
	// use on pin 4 (A2) be careful to remove analog function.
	SoftwareSerial mySerial(99, 4, false);
	mySerial.begin(9600);
	delay(500);
	mySerial.print(F("..."));
	mySerial.println(stringa);
}

void turn_sim800c_on()
{
	SoftwareSerial mySerial(sim_module_rx_pin, sim_module_tx_pin, false);

	mySerial.begin(19200);

	delay(3000);

	bool check = false;

	while (!check) {

		while (mySerial.available() > 0)
		{
			mySerial.read();
		}

		mySerial.println(F("AT"));

		delay(5000);

		if (mySerial.available() > 0)
		{
			if (mySerial.readString().indexOf(F("OK")) != -1){
				
				check = true ;
			}
		}
		else {
			switch_sim();
		}
	}
}

void turn_sim800c_off(){
	switch_sim();
}


