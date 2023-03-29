/*
 Name:		portableLowPower_new.ino
 Created:	3/28/2023 9:33:46 AM
 Author:	luigi.santagada
*/
#include <SimModuleActivity.h>
#include <DeviceActivity.h>
#include <AvrMicroRepository.h>
#include <DigitalPort.h>
#include <SimModuleDevice.h>
#include <AvrMicroRepository.h>
#include <SoftwareSerialRepository.h>
#include <InterfaceSerialRepository.h>
#include <SimProgMemRepository.h>

SimModuleDevice simModuleDevice;
DigitalPort listOfPortsForSimModule[1];
SimModuleActivity simModuleActivity;
AvrMicroRepository  avrMicroRepository(19200);
SimProgMemRepository simProgMemRepository;
SoftwareSerialRepository softwareSerialRepository(0, 3, 19200);


// the setup function runs once when you press reset or power the board
void setup() {
	//pinMode(13, OUTPUT);

	//Serial.println(F("RESTART---------------------------------------------"));

	listOfPortsForSimModule[0] = DigitalPort("T", 99);

	listOfPortsForSimModule[0].direction = DigitalPort::PortDirection::output;

	simModuleDevice = SimModuleDevice("Sim01", listOfPortsForSimModule, 1);

	simModuleDevice.init("+39", "3202445649;");

	simModuleActivity = SimModuleActivity(softwareSerialRepository, simProgMemRepository, avrMicroRepository, simModuleDevice);

	simModuleActivity.enableSmsIncoming();

	makePhoneCall();
}

// the loop function runs over and over again until power down or reset
void loop() {
	makePhoneCall();
	//checkIncomingSms();
}

void makePhoneCall()
{
	//const int bl = simModuleActivity._simModuleRepository->get_SS_MAX_RX_BUFF();
	char buffer[1];
	simModuleActivity.makeCall(buffer, 1);

	//Serial.println(buffer);
}
//
//void checkIncomingSms()
//{
//	if (simModuleActivity.getNumberOfSmsReceived() == 0)
//	{
//		//Serial.println(F("no message"));
//
//		return;
//	}
//
//	int bl = simModuleActivity._simModuleRepository->get_SS_MAX_RX_BUFF();
//
//	char response[bl];
//
//	for (int i = 0; i < 10; i++)
//	{
//		simModuleActivity.getSmsByIndex(i);
//
//		if (simModuleActivity.getSmsResponse(response, bl))
//		{
//			//Serial.println(response);
//
//			if (simModuleActivity.isCallerAuthorized(response, "+393202445649"))
//			{
//				//Serial.println(F("caller is authorized"));
//
//				if (simModuleActivity.isSmsOnBuffer(response, 0, 5))
//				{
//					//Serial.println(F("do somethink for 0 command"));
//					makePhoneCall();
//				}
//			}
//			simModuleActivity.deleteSmsByIndex(i);
//		}
//	}
//}
