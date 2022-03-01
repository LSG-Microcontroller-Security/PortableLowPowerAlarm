#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/power.h>


unsigned long timer = 0;
uint8_t interruptPin = 2;
//uint8_t ledPin = 0;
uint8_t transistorPin = 1;
volatile bool sy = false;
bool isOnPowerSafe = true;
bool isWatchDogCicle = false;
uint8_t watchDogCounter = 0;
bool isWatchDogEvent = false;
uint8_t voltagePin = A2;


void setup()
{
	SoftwareSerial mySerial(0, 3);
	delay(1000);
	mySerial.begin(19200);
	//pinMode(ledPin, OUTPUT);
	pinMode(transistorPin, OUTPUT);
	pinMode(interruptPin, INPUT_PULLUP);
	turnOn();
	delay(20000);
	//}
	//mySerial.println("restart");
	timer = millis();
	PCMSK |= bit(PCINT2);  // want pin D3 / pin 2
	GIFR |= bit(PCIF);    // clear any outstanding interrupts
	GIMSK |= bit(PCIE);    // enable pin change interrupts
	setup_watchdog(9); // approximately 4 seconds sleep
	attachInterrupt(0, activateSystemInterrupt, FALLING);

	setSMSIncoming();

	deleteBuffer();

	String phoneNumber = "";

	for (uint8_t i = 0; i < 10; i++)
	{
		phoneNumber.concat((char)eeprom_read_byte((uint8_t*)i));
	}
	
	callPhoneNumber(phoneNumber);
	delay(5000);
	setSleepMode();
	analogReference(DEFAULT);
	
}

void setSleepMode()
{
	SoftwareSerial mySerial(0, 3);
	mySerial.println(F("AT+CSCLK=2"));
}

void setSMSIncoming()
{
	SoftwareSerial mySerial(0, 3);
	nullCommand();
	mySerial.println(F("AT+CPMS=\"SM\""));

	mySerial.println(F("AT+CMGF=1"));

	mySerial.println(F("AT+CMGD=1,4"));
}

void deleteBuffer() {
	SoftwareSerial mySerial(0, 3);
	delay(1000);
	mySerial.readString();
}

void activateSystemInterrupt()
{
	sy = true;
}

void turnOn()
{
	digitalWrite(transistorPin, HIGH);
}

void turnOff()
{
	digitalWrite(transistorPin, LOW);
}

void loop()
{
	if (millis() < 60000)
	{
		sms();
	}
	if ((sy == true) && (millis() > 60000))
	{
		/*blinkLed(500,1);*/
		if (digitalRead(transistorPin) != HIGH)
		{
			turnOn();
			delay(20000);
		}
		timer = millis();
		sy = false;

		nullCommand();
		delay(1000);

		deleteBuffer();
		
		String phoneNumber = "";

		for (uint8_t i = 0; i < 10; i++)
		{
			phoneNumber.concat((char)eeprom_read_byte((uint8_t*)i));
		}

		callPhoneNumber(phoneNumber);

		delay(5000);
	}
	if (millis() - timer > 60000)
	{
		if (isWatchDogEvent)
		{
			float measure = 0;

			measure = (5.1 / 1024) * analogRead(voltagePin);
			//mySerial.print("meausere = "); mySerial.println(measure);
			if (measure < 3.25)
			{
				if (digitalRead(transistorPin) != HIGH)
				{
					turnOn();
					delay(20000);
				}

				nullCommand();
				delay(1000);
				deleteBuffer();

				String phoneNumber = "";

				for (uint8_t i = 0; i < 10; i++)
				{
					phoneNumber.concat((char)eeprom_read_byte((uint8_t*)i));
				}

				callPhoneNumber(phoneNumber);
				delay(15000);
				turnOff();
			}
			isWatchDogEvent = false;
		}
		if (isOnPowerSafe)
		{
			turnOff();
		}
		isWatchDogCicle = false;
		enter_sleep();
		delay(100);
	}
}

void sms()
{
	SoftwareSerial mySerial(0, 3);
	String response = "";
	/*if (mySerial.available() > 0)
	{
		while (mySerial.available() > 0) {
			response.concat((char)mySerial.read());
		}
	}
	response = "";*/
	nullCommand();
	mySerial.println(F("AT+CMGR=1"));
	delay(2000);
	if (mySerial.available() > 0)
	{
		while (mySerial.available() > 0) {
			response.concat((char)mySerial.read());
			response.concat((char)mySerial.read());
		}
		delay(500);
		int index = response.lastIndexOf(F("#"));
		if (index != -1)
		{
			/*blinkLed(1000,1);*/
			String phoneNumber = response.substring(index + 1, index + 11);
			for (uint8_t i = 0; i < 10; i++)
			{
				eeprom_write_byte((uint8_t*)i, phoneNumber[i]);
				//mySerial.println(phoneNumber[i]);
			}
			nullCommand();
			callPhoneNumber(phoneNumber);
		}
		index = response.lastIndexOf(F("&"));
		if (index != -1)
		{
			/*blinkLed(1000, 1);*/
			isOnPowerSafe = false;
			//mySerial.println(F("entrato!"));
		}
		/*index = response.lastIndexOf(F("%"));
		if (index != -1)
		{
			blinkLed(1000, 1);
			isOnPowerSafe = false;
		}*/

	}
}

//void blinkLed(uint8_t speed, uint8_t cicle)
//{
//	return;
//	for (uint8_t i = 0; i <= cicle; i++)
//	{
//		digitalWrite(0, HIGH);
//		delay(speed);
//		digitalWrite(0, LOW);
//		delay(speed);
//	}
//}

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
	// …zzz
	// Wake up
	sleep_disable();
	//
	power_all_enable(); // Power everything back on
	ADCSRA = adcsra;               //restore ADCSRA
}

void callPhoneNumber(String phoneNumber)
{
	SoftwareSerial mySerial(0, 3);
	delay(1000);
	mySerial.print(F("atd")); mySerial.print(phoneNumber); mySerial.println(F(";"));
}

//	cli();
//	sleep_enable();
//	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//	//adcsra = ADCSRA;               //save the ADC Control and Status Register A
//	mySerial.print("adcsra before = "); mySerial.println(ADCSRA, HEX);
//	//ADCSRA = 0;                    //disable ADC
//	sei();
//	sleep_cpu();                   //go to sleep
//	sleep_disable();               //wake up here
//	//ADCSRA = adcsra;               //restore ADCSRA
//	mySerial.print("adcsra after = "); mySerial.println(ADCSRA, HEX);
//}

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
	isWatchDogCicle = true;
	if (watchDogCounter == 35)
	{
		isWatchDogEvent = true;
		watchDogCounter = 0;
	}
	else {
		watchDogCounter++;
	}
}

void nullCommand()
{
	SoftwareSerial mySerial(0, 3);
	mySerial.println(F("AT"));
}









