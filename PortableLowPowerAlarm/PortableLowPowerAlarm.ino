#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

/// <summary>
/// 8mhz speed clock is right
/// </summary>

unsigned long startTimer = 0 ;
unsigned long turnOffTimer = 0;
uint8_t interruptPin = 2;
uint8_t transistorPin = 1;
volatile bool isOnTiltSensorInterrupt = false;
bool isOnPowerSafe = false;
uint8_t watchDogCounter = 0;
bool isWatchDogEvent = false;
//uint8_t voltagePin = A2;
bool isCallDisabled = false;
uint8_t wd_timer = 2;
bool wd_isActive = false;

#define ATD "atd"

void setup()
{
	pinMode(transistorPin, OUTPUT);
	
	pinMode(interruptPin, INPUT_PULLUP);

	turnOn();

	PCMSK |= bit(PCINT2);  // want pin D3 / pin 2

	GIFR |= bit(PCIF);    // clear any outstanding interrupts

	GIMSK |= bit(PCIE);    // enable pin change interrupts

	setSmsReceiver();

	//analogReference(DEFAULT);

	callPhoneNumber();

	startTimer = millis();

	turnOffTimer = millis();

	//wdt_disable();

	//setup_watchdog(9); // approximately 4 seconds sleep

	attachInterrupt(0, activateSystemInterrupt, FALLING);

	setup_watchdog(9);

	delay(1000);

	//detachInterrupt(0);
}

void loop() {

	if ((millis() - startTimer) < 120000) {
		startSMSActivity();
	}

	if ((millis() - turnOffTimer) > 120000) {
		if (!isOnTiltSensorInterrupt) {
			if (isOnPowerSafe) {
				turnOff();
			}
			wd_isActive = false;
			wdt_disable();
			enter_sleep();
		}
	}
	
	if (isOnTiltSensorInterrupt && ((millis() - startTimer) > 120000)) {
		tiltSensorInterruptActivity();
		turnOffTimer = millis();
		if (!wd_isActive)
		{
			setup_watchdog(9);
			wd_isActive = true;
		}
	}

	//WatchDog Sleep Activity
	if (isWatchDogEvent && !isOnPowerSafe && ((millis() - startTimer) > 120000)) {
		//turnOffTimer = millis();
		startSMSActivity();
		isWatchDogEvent = false;
	}
}

SoftwareSerial SoftwareDynamicSerial(uint8_t rx, uint8_t tx, long speed, bool inverse_logic = false)
{
	SoftwareSerial mySerial(rx, tx, inverse_logic);
	mySerial.begin(speed);
	return mySerial;
}

void startSMSActivity() {
	getTaggedSmsFromResponse('#');
}

void tiltSensorInterruptActivity()
{
#ifdef _DEBUG
	debugOnSerial("intr");
#endif

	if (isOnPowerSafe) {
		turnOn();
	}

	isOnTiltSensorInterrupt = false;

	callPhoneNumber();

	delay(5000);
}

//void watchDogAndSleepActivity()
//{
//	if (isWatchDogEvent)
//	{
//#ifdef _DEBUG
//		debugOnSerial("wdgEv");
//#endif
//		checkBatteryVoltage();
//	}
//	//getTaggedSmsFromResponse('#');
//	//if ((tiltSensorInterrupt == true))
//	//{
//	//	tiltSensorActivity();
//	//}
//	//if (isOnPowerSafe) { turnOff(); }else { turnOn(); }
//	isWatchDogEvent = false;
//	enter_sleep();
//	delay(100);
//}

void setSmsReceiver()
{
	SoftwareSerial mySerial = SoftwareDynamicSerial(0, 3, 19200);

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

	mySerial.println(F("AT"));
	delay(100);
	mySerial.println(F("AT+CNETLIGHT=0"));

	delay(1000);

	mySerial.println(F("AT"));
	delay(100);
	mySerial.println(F("AT+CSCLK=2"));

	delay(1000);

}

void debugOnSerial(char* stringa)
{
	//return;
	//use on pin 4 (A2) be careful to remove analog function.
	SoftwareSerial mySerial = SoftwareDynamicSerial(99, 4, 9600);
	mySerial.print(F("...")); mySerial.println(stringa);
}

void activateSystemInterrupt()
{
	isOnTiltSensorInterrupt = true;
}

void turnOn()
{
	digitalWrite(transistorPin, HIGH);
	delay(20000);
}

void turnOff()
{
	digitalWrite(transistorPin, LOW);
}

//void checkBatteryVoltage()
//{
//	float measure = 0;
//
//	for (int i = 0; i < 50; i++)
//	{
//		measure = measure + ((5.10f / 1024.00f) * analogRead(voltagePin));
//	}
//
//	measure = measure / 50;
//
//	if (measure < 3.35f)
//	{
//		if (digitalRead(transistorPin) != HIGH)
//		{
//			turnOn();
//			delay(20000);
//		}
//
//		for (int i = 0; i < 5; i++)
//		{
//			callPhoneNumber();
//		}
//
//		delay(15000);
//
//		turnOff();
//	}
//	isWatchDogEvent = false;
//}

bool exctractSmsTagged(char tag, char* sms)
{
	bool returnValue = false;

	SoftwareSerial mySerial = SoftwareDynamicSerial(0, 3, 19200);

	while (mySerial.available() > 0) {
		mySerial.read();
	}
	delay(1000);


	for (uint8_t index = 1; index < 4; index++)
	{
		char number[2] = { index + 48 ,'\0' };

		char command[10] = "AT+CMGR=";

		strcat(command, number);

		mySerial.println(F("AT"));

		delay(100);

		mySerial.println(command);

		delay(3000);

		if (mySerial.available() > 0)
		{
			if (mySerial.readStringUntil(tag).length() > 0)
			{
				if (mySerial.available() > 0)
				{
					mySerial.readStringUntil(tag).toCharArray(sms, 20, 0);

					char command[10] = "AT+CMGD=";

					strcat(command, number);

					mySerial.println(command);

					returnValue = true;
				}
			}

		}
	}

	return returnValue;
}

//void clearAllSms()
//{
//	SoftwareSerial mySerial(0, 3);
//	mySerial.begin(19200);
//	delay(3000);
//	mySerial.println(F("AT"));
//	delay(100);
//	mySerial.println(F("AT+CMGD=1,4"));
//}

//bool isSmsValidPhoneNumber(char* phoneNumber)
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
//}

//bool isSmsOnPowerSafeOff(char* sms)
//{
//	if (strcmp(sms, "y") == 0)
//	{
//		return  true;
//	}
//	return false;
//}

//bool isSmsOnPowerSafeOn(char* sms)
//{
//	if (strcmp(sms, "s") == 0)
//	{
//		return  true;
//	}
//	return false;
//}

bool isSmsCodeFind(char* sms,char code[1])
{
	if (strcmp(sms, code) == 0)
	{
		return  true;
	}
	return false;
}

void getTaggedSmsFromResponse(char tag) {

	char sms[12] = "";

	exctractSmsTagged(tag, sms);

#ifdef _DEBUG
	debugOnSerial(sms);
#endif

	char phone_c = ' ';

	phone_c = sms[0];

	//check if sms is a configuration phone number
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
				eeprom_write_byte((uint8_t*)cicle, charToWrite);
				cicle++;
			}
		}
		callPhoneNumber(sms);
	}

	//	if (isSmsOnPowerSafeOff(sms))
	//	{
	//		isOnPowerSafe = false;
	//		callPhoneNumber();
	//#ifdef _DEBUG
	//		debugOnSerial("deactivate safeMode");
	//#endif
	//	}

	if (isSmsCodeFind(sms, "x")) {
		isCallDisabled = true;
	}

	if (isSmsCodeFind(sms, "y")) {

		isCallDisabled = false;

		callPhoneNumber();

		delay(5000);
	}

	if (isSmsCodeFind(sms,"s")){

		isOnPowerSafe = true;

		callPhoneNumber();
		
		delay(5000);

#ifdef _DEBUG
		//debugOnSerial("dis.safe");
#endif
	}

	//String response = "";
	//SoftwareSerial mySerial(0, 3);
	//mySerial.begin(19200);
	//delay(1000);
	//mySerial.readString();
	//delay(1000);
	//mySerial.println("AT+CMGR=1");
	//delay(2000);
	//if (mySerial.available() > 0)
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
	//return;

	//if (mySerial.available() > 0)
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
	//		isOnPowerSafe = false;
	//	}
	//}
}

void enter_sleep()
{
	byte adcsra;
	cli();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	adcsra = ADCSRA;               //save the ADC Control and Status Register A
	//mySerial.print("adcsra = "); mySerial.println(adcsra,HEX);
	ADCSRA = 0; // Turn off ADC
	power_all_disable(); // Power off ADC, Timer 0 and 1, serial interface
	sleep_enable();
	sei();
	sleep_cpu();
	// �zzz
	// Wake up
	sleep_disable();
	//
	power_all_enable(); // Power everything back on
	ADCSRA = adcsra;               //restore ADCSRA
}

void callPhoneNumber()
{
	if (isCallDisabled) return;

	char phoneNumber[11]{};

	for (uint8_t i = 0; i < 10; i++)
	{
		phoneNumber[i] = (char)eeprom_read_byte((uint8_t*)i);
	}

	strcat(phoneNumber, "\0");

	callPhoneNumber(phoneNumber); \
}

void callPhoneNumber(char* phoneNumber)
{
	SoftwareSerial mySerial = SoftwareDynamicSerial(0, 3, 19200);
	char command[30]{};
	//mySerial.println("AT");
	//delay(100);
	//globalString = F("atd");
	strcat(command, ATD);
	strcat(command, phoneNumber);
	strcat(command, ";");
	strcat(command, "\0");
	mySerial.println(F("AT"));
	delay(100);
	mySerial.println(command);
	delay(7000);

}

void setup_watchdog(int ii) {

	byte bb;
	int ww;
	if (ii > 9) ii = 9;
	bb = ii & 7;
	if (ii > 7) bb |= (1 << 5);
	bb |= (1 << WDCE);
	ww = bb;

	MCUSR &= ~(1 << WDRF);
	// start timed sequence
	WDTCR |= (1 << WDCE) | (1 << WDE);
	// set new watchdog timeout value
	WDTCR = bb;
	WDTCR |= _BV(WDIE);
}

ISR(WDT_vect) {
	if (watchDogCounter == wd_timer) {
		isWatchDogEvent = true;
		watchDogCounter = 0;
	}
	else {
		watchDogCounter++;
	}
}

int freeRam() {
	extern int __heap_start, * __brkval;
	int v;
	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void blinkLedDebug(int time)
{
	digitalWrite(1, HIGH);
	delay(time);
	digitalWrite(1, LOW);
	delay(time);
}















