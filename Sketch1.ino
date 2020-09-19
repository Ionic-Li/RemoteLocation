/*
 Name:		ReceiverTest.ino
 Created:	2020/7/21 10:06:47
 Author:	lyj01
*/
#include <TimerOne.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include "Wireless.h"

//fsm states
enum States
{
	WAITING_STATE,//waiting for the wake up message
	WAKEUP_STATE//send data to the receiver
};

//PIN definations
const uint8_t GPS_POWER_PIN = 8;
const uint8_t LED_PIN = 11;

//constants
const unsigned long SECOND = 1000000;

//Obj definations
TinyGPSPlus GPS;
SoftwareSerial GPSSerial(2, 12);
//rx = 6, tx = 7, aux = 3, m0 = 4, m1 = 5, target address is 0x07, target channel is 0x09, intialize as power saving mode
Wireless Receiver(6, 7, 3, 4, 5, 7, 9, POWSAVING_MODE);

//variable defination
String Rec;
double Location[2] = { 100, 0 };
double TempLoc[2] = { 0, 0 };
uint8_t TempCount = 0;
States FsmState = WAITING_STATE;//Fsm state
unsigned long LEDBegin = 0;//count for how long the led has been on
volatile uint8_t Counter = 0;//count for the time that no confirm message has been received
volatile uint8_t CountSec = 0;//count for the time in a loop of 60 seconds

//function declaration
void interrupt();
void setState(States NewState);//set fsm state
void led();//light the led

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(9600);
	GPSSerial.begin(9600);

	//turn off the gps module
	pinMode(GPS_POWER_PIN, OUTPUT);
	digitalWrite(GPS_POWER_PIN, LOW);

	//LED pin
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, LOW);

	Timer1.initialize();
}

// the loop function runs over and over again until power down or reset
void loop() {
	MsgType RecMsgType = Receiver.getMsg(Rec);
	if (RecMsgType != NONE)
	{
		Serial.print(RecMsgType);
	}
	switch (FsmState)
	{
	case WAITING_STATE:
		if (RecMsgType == WAKEUP_MSG)
		{
			setState(WAKEUP_STATE);//set fsm state to wake up state
		}
		else if (RecMsgType == LIGHTUP_MSG)
		{
			LEDBegin = millis();
		}
		break;
	case WAKEUP_STATE:
		if (RecMsgType == WAKEUP_MSG || RecMsgType == LIGHTUP_MSG)
		{
			Counter = 0;//if confirm of led message has been received, set the counter to 0
			//if receive message to light up the led for 10 seconds
			if (RecMsgType == LIGHTUP_MSG)
			{
				LEDBegin = millis();
			}
			//if Location is available
			if (Location[0] != 100)
			{
				while (!Receiver.available());//wait for the Receiver to be available and send a gps message
				Receiver.sendMsg(GPS_MSG, (char*)Location, 2 * sizeof(double));
			}
		}
		if (Counter >= 120)//if no confirm message has been received for over 2 minute, turn to waiting state
		{
			setState(WAITING_STATE);
		}
		if (CountSec >= 60)
		{
			CountSec = 0;
		}
		//update gps data in a period of 10 seconds
		if (CountSec % 5 == 0)
		{
			//if no valid location
			do
			{
				GPSSerial.listen();
				//get GPS data
				while (GPSSerial.available())
				{
					GPS.encode(GPSSerial.read());
				}
				if (GPS.location.isUpdated())
				{
					TempCount++;
					TempLoc[0] += GPS.location.lat();
					TempLoc[1] += GPS.location.lng();

					//update location
					if (TempCount >= 20 || GPS.speed.knots() > 0)
					{
						TempLoc[0] /= TempCount;
						TempLoc[1] /= TempCount;
						Location[0] = TempLoc[0];
						Location[1] = TempLoc[1];
						TempCount = 0;
						TempLoc[0] = 0;
						TempLoc[1] = 0;
					}
					/*Location[0] = GPS.location.lat();
					Location[1] = GPS.location.lng();*/
				}
			} while (Location[0] == 100);
			CountSec++;
		}
		break;
	}
	led();
}

void interrupt()
{
	Counter++;
	CountSec++;
}

void setState(States NewState)
{
	//if state hasn't been changed, do nothing
	if (NewState == FsmState)
	{
		return;
	}
	switch (NewState)
	{
	case WAITING_STATE:
		digitalWrite(GPS_POWER_PIN, LOW);//switch off the gps module
		Receiver.setMode(POWSAVING_MODE);//set the Receiver to power saving mode
		Timer1.detachInterrupt();
		break;
	case WAKEUP_STATE:
		Counter = 0;
		CountSec = 0;
		Location[0] = 100;
		TempCount = 0;
		TempLoc[0] = 0, TempLoc[1] = 0;
		digitalWrite(GPS_POWER_PIN, HIGH);//turn on the gps module
		Receiver.setMode(WAKEUP_MODE);//change the mode of the Receiver to send message
		Timer1.restart();
		Timer1.attachInterrupt(interrupt);//attach interrupt to send message
		break;
	}
	FsmState = NewState;
}

void led()
{
	//if led hasn't begin, do nothing
	if (LEDBegin == 0)
	{
		return;
	}
	//get how long LED has been on
	unsigned long Inter = millis() - LEDBegin;
	if (Inter / 1000 >= 20)//if the LED has been on for over 20 seconds turn it off
	{
		digitalWrite(LED_PIN, LOW);
		LEDBegin = 0;
	}
	else
	{
		int Brightness = 255 - Inter % 511;
		analogWrite(LED_PIN, abs(Brightness));
	}
}
