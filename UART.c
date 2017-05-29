// AC CONTROL/DRIVER BOARD 1
#include "UART.h"
#include "Mainmio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ShowMenu(void);
void ShowConfig();
void u16x_to_str(char *str, unsigned val, unsigned char digits);
void u16_to_str(char *str, unsigned val, unsigned char digits);
void int16_to_str(char *str, int val, unsigned char digits);
int TransmitString(const char* str);
char IntToCharHex(unsigned int i);
void FetchRTData(void);
void StopAllMotorTests(void);

extern void InitPIStruct(void);
extern void EESaveValues(void);
extern void InitializeThrottleAndCurrentVariables(void);
extern void TurnOffADAndPWM();
extern void InitADAndPWM();
extern void InitQEI();

volatile UARTCommand myUARTCommand = {0,0,{0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0},0};

extern volatile int debugMode;
extern volatile int IqRefRef;
extern volatile int IdRefRef;
extern volatile int captureData;
extern volatile int dataCaptureIndex;
extern volatile int currentMaxIterationsBeforeZeroCrossing;

extern volatile int vRef1;
extern volatile int vRef2;
extern volatile int maxRPS_times16;
extern volatile unsigned int faultBits;
extern volatile SavedValuesStruct savedValues;
extern volatile SavedValuesStruct2 savedValues2;
extern unsigned int revCounterMax;
extern volatile unsigned int poscnt;	
extern volatile unsigned int counter10k;
extern volatile unsigned int counter1k;
extern volatile piType myPI;
extern volatile rotorTestType myRotorTest;
extern volatile angleOffsetTestType myAngleOffsetTest;
extern volatile motorSaliencyTestType myMotorSaliencyTest;
extern volatile int bigArray1[];

volatile int readyToDisplayBigArrays = 0;
volatile dataStream myDataStream;

volatile int timeSinceLastCarriageReturn = 0;
volatile char newChar = 0;
volatile int echoNewChar = 0;
volatile dataStream myDataStream;
char intString[] = "xxxxxxxxxx";
					//      0         1         2         3         4
					//      01234567890123456789012345678901234567890	
char showConfigString[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";


void InitUART2() {
	U2BRG = 15; //For 14.7MHz, 115200bps=7 38.4kbps==23.  For 29.5MHz, 115200bps == 15.
	U2MODE = 0;  // initialize to 0.
	U2MODEbits.PDSEL = 0b00; // 8 N 
	U2MODEbits.STSEL = 0; // 1 stop bit.

	IEC1bits.U2RXIE = 1;  // enable receive interrupts.
	IPC6bits.U2RXIP = 5;	// INTERRUPT priority of 2.
//bit 7-6 URXISEL<1:0>: Receive Interrupt Mode Selection bit
//11 =Interrupt flag bit is set when Receive Buffer is full (i.e., has 4 data characters)
//10 =Interrupt flag bit is set when Receive Buffer is 3/4 full (i.e., has 3 data characters)
//0x =Interrupt flag bit is set when a character is received
	U2STAbits.URXISEL = 0b00;  // 0b11 later..

	U2MODEbits.UARTEN = 1; // enable the uart
	asm("nop");
	U2STAbits.UTXEN = 1; // Enable transmissions
	Nop();Nop();Nop();Nop();
	Nop();Nop();Nop();Nop();
	U2STAbits.OERR = 0; // ClearReceiveBuffer();
}

void __attribute__((__interrupt__, auto_psv)) _U2RXInterrupt(void) {
	IFS1bits.U2RXIF = 0;  // clear the interrupt.
	echoNewChar = 1;
	newChar = U2RXREG;		// get the character that caused the interrupt.


	if (myUARTCommand.complete == 1) {	// just ignore everything until the command is processed.
		return;
	}

	if (newChar == 0x0d) {	// carriage return.
//		if (counter10k - timeSinceLastCarriageReturn < 2000) return;
//		timeSinceLastCarriageReturn = counter10k;
		myUARTCommand.complete = 1;
		myUARTCommand.string[myUARTCommand.i] = 0;  // instead of placing a carriage return, place a 0 to null terminate the string.
		return;
	}
	if (myUARTCommand.i >= MAX_COMMAND_LENGTH) {  // the command was too long.  It's just garbage anyway, so start over.
		//myUARTCommand.complete = 0;  // It can't make it here unless myUARTCommand.complete == 0 anyway.
		myUARTCommand.i = 0;	// just clear the array, and start over.
		myUARTCommand.string[0] = 0;
//		myUARTCommand.number = 0;  // This is done in "ProcessCommand", so you don't need to do it here.
		return;
	}
	myUARTCommand.string[myUARTCommand.i] = newChar; // save the character that caused the interrupt!
	myUARTCommand.i++;
}

// process the command, and reset UARTCommandPtr back to zero.
// myUARTCommand is of the form XXXXXXXXX YYYYY<enter>
void ProcessCommand(void) 
{
	static int i = 0;
	if (echoNewChar) 
    {
//		StopAllMotorTests();		// also, stop the motor tests.
		while (echoNewChar) 
        {
			if (U2STAbits.UTXBF == 0) // TransmitReady();
            { 
				U2TXREG = newChar; 	// SendCharacter(newChar);
				if (newChar == 0x0d) 
                {
					while (1) 
                    { 
						if (U2STAbits.UTXBF == 0) // TransmitReady();
                        { 
							U2TXREG = 0x0a; 	// SendCharacter(line feed);
							break;
						}
					}
				}
				echoNewChar = 0;
			}
		}
	}
	else 
    {
		if (myUARTCommand.complete != 1) // if the command isn't yet complete, don't try to process it!  Maybe someone is only half-way done with their command.  Ex:  "sav".  Process "sav"?  No!  wait until they type "save<cr>"
        {	
			return;
		}
		myUARTCommand.number = 0;	
		for (i = 0; myUARTCommand.string[i] != 0; i++) 
        {
			if (myUARTCommand.string[i] == ' ') 
            {
				myUARTCommand.number = atoi((char *)&myUARTCommand.string[i+1]);
				myUARTCommand.string[i] = 0;  // null terminate the text portion.			
				break;
			}
		}
		if (!strcmp((const char *)&myUARTCommand.string[0], "save")) 
        {
			TurnOffADAndPWM();
			EESaveValues();
			InitADAndPWM();
		}		
		else if (!strcmp((const char *)&myUARTCommand.string[0], "motor-type")) 
        {
			if (myUARTCommand.number > 0 && myUARTCommand.number < 5) 
            {
				TurnOffADAndPWM();
				savedValues.motorType = myUARTCommand.number;
				InitADAndPWM();
				InitQEI();
			}
		}
		// Let's say you typed the command "kp 1035".  The following would have happened:
		// myUARTCommand.string[] would contain only the text portion of the command, and is terminated with a 0.  string[] = {'p',0,?,?,?,?,?,?,?,?,?,?,?,...}
		// Also, myUARTCommand.number = the number argument after the command. So, number = 1035.
		else if (!strcmp((const char *)&myUARTCommand.string[0], "kp")) 
        {
			if (myUARTCommand.number <= 32767u && myUARTCommand.number > 0) 
            {
				savedValues.Kp = (int)(myUARTCommand.number); 
				InitPIStruct();
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "ki"))
        { 
			if (myUARTCommand.number <= 32767u && myUARTCommand.number > 0) 
            {
				savedValues.Ki = (int)(myUARTCommand.number); 
				InitPIStruct();
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "angle-offset"))
        { 
			if (myUARTCommand.number <= 511 && myUARTCommand.number >= 0) 
            {
				savedValues2.angleOffset = (unsigned int)(myUARTCommand.number); 	// this one is the extra for displaying on the screen.
				myAngleOffsetTest.currentAngleOffset = savedValues2.angleOffset;  	// this is the working copy.
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "saliency")){ 
			if (myUARTCommand.number <= 1024 && myUARTCommand.number >= 0) {  // 
				savedValues2.KArrayIndex = (unsigned int)(myUARTCommand.number); 	// this one is the extra for displaying on the screen.
				myMotorSaliencyTest.KArrayIndex = savedValues2.KArrayIndex;  	// this is the working copy.
			}
		}		
		else if (!strcmp((const char *)&myUARTCommand.string[0], "current-sensor-amps-per-volt")) {  // 
			if (myUARTCommand.number <= 480 && myUARTCommand.number > 0) {
				savedValues.currentSensorAmpsPerVolt = (int)(myUARTCommand.number); 
				InitializeThrottleAndCurrentVariables();
			}	
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-regen-position")) { 
			if (myUARTCommand.number <= 1023u && myUARTCommand.number > 0) {
				savedValues.maxRegenPosition = (int)(myUARTCommand.number); 
			}	
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "min-regen-position")) { 
			if (myUARTCommand.number <= 1023u && myUARTCommand.number > 0) {
				savedValues.minRegenPosition = (int)(myUARTCommand.number); 
			}	
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "min-throttle-position")) { 
			if (myUARTCommand.number <= 1023u && myUARTCommand.number > 0) {
				savedValues.minThrottlePosition = (int)(myUARTCommand.number); 
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-throttle-position")) { 
			if (myUARTCommand.number <= 1023u && myUARTCommand.number > 0) {
				savedValues.maxThrottlePosition = (int)(myUARTCommand.number); 
			}	
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "fault-throttle-position")) { 
			if (myUARTCommand.number <= 1023u && myUARTCommand.number > 0) {
				savedValues.throttleFaultPosition = (int)(myUARTCommand.number); 
			}	
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-battery-amps")) { 
			if (myUARTCommand.number <= 9999 && myUARTCommand.number > 0) {
				savedValues.maxBatteryAmps = (int)(myUARTCommand.number); 
				InitializeThrottleAndCurrentVariables();
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-battery-amps-regen")) { 
			if (myUARTCommand.number <= 9999 && myUARTCommand.number > 0) {
				savedValues.maxBatteryAmpsRegen = (int)(myUARTCommand.number); 
				InitializeThrottleAndCurrentVariables();
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-motor-amps")) { 
			if (myUARTCommand.number <= 999 && myUARTCommand.number > 0) {
				savedValues.maxMotorAmps = (int)(myUARTCommand.number); 
				InitializeThrottleAndCurrentVariables();
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-motor-amps-regen")) { 
			if (myUARTCommand.number <= 999 && myUARTCommand.number > 0) {
				savedValues.maxMotorAmpsRegen = (int)(myUARTCommand.number); 
				InitializeThrottleAndCurrentVariables();
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "precharge-time")) { 
			if (myUARTCommand.number <= 9999 && myUARTCommand.number > 0) {
				savedValues.prechargeTime = (int)(myUARTCommand.number); 
			}
		}
		// NOW WE ARE ON SavedValues2...
		else if (!strcmp((const char *)&myUARTCommand.string[0], "rotor-time-constant")) { 
			if (myUARTCommand.number <= ROTOR_TIME_CONSTANT_ARRAY_SIZE+5 && myUARTCommand.number >= 5) {
				myRotorTest.timeConstantIndex = savedValues2.rotorTimeConstantIndex = (int)(myUARTCommand.number-5);
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "pole-pairs")) {
			if (myUARTCommand.number <= 999 && myUARTCommand.number >= 1) {
				savedValues2.numberOfPolePairs = (int)(myUARTCommand.number); 
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "max-rpm")) { 
			if (myUARTCommand.number <= 32767 && myUARTCommand.number > 0) {
				savedValues2.maxRPM = (int)(myUARTCommand.number); 
				maxRPS_times16 = (((long)savedValues2.maxRPM) << 2) / 15;  // 4/15 to convert to rps_times16
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "throttle-type")) { // 0 means hall effect throttle, or maxOHms to 0 Ohms. 1 means 0 Ohms to maxOhms throttle 
			if (myUARTCommand.number <= 1) {
				savedValues2.throttleType = (int)(myUARTCommand.number); 
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "encoder-ticks")) {
			if (myUARTCommand.number <= 5000u && myUARTCommand.number >= 16) {
				savedValues2.encoderTicks = (int)(myUARTCommand.number); 
				revCounterMax = (unsigned)(160000L / (4*savedValues2.encoderTicks));  // 4* because I'm doing 4 times resolution for the encoder. 160,000 because revolutions per 16 seconds is computed as:  16*10,000*poscnt * rev/(maxPosCnt*revcounter*(16sec)
				// revCounterMax may only be of use for the induction motor.
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "pi-ratio")) {
			if (myUARTCommand.number < 1000 && myUARTCommand.number >= 50) {
				myPI.ratioKpKi = (int)(myUARTCommand.number); 
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "run-pi-test")) {
			currentMaxIterationsBeforeZeroCrossing = 20;
			myPI.testRunning = 1;
			myPI.testFinished = 0;
			myPI.zeroCrossingIndex = -1;
			myPI.Kp = myPI.ratioKpKi;
			myPI.Ki = 1;
//			myPI.Kp = savedValues.Kp;
//			myPI.Ki = savedValues.Ki;
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "run-pi-test2")) {
			myPI.testRunning2 = 1;
			myPI.zeroCrossingIndex = -1;
			
//			myPI.Kp = myPI.ratioKpKi;
//			myPI.Ki = 1;
			myPI.Kp = savedValues.Kp;
			myPI.Ki = savedValues.Ki;
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "run-rotor-test")) {
			if (savedValues.motorType == 1) {		
				myRotorTest.startTime = counter10k;
				myRotorTest.timeConstantIndex = 0;	// always start at zero, and then it will increment up to around 145, giving each rotorTimeConstant candidate 5 seconds to spin the motor the best it can.
				myRotorTest.testRunning = 1;
				myRotorTest.testFinished = 0;
				myRotorTest.maxTestSpeed = 0;
				myRotorTest.bestTimeConstantIndex = 0;
			}
			else {
				TransmitString("Your motor type is currently set to permanent magnet.  This test is for an AC induction motor!\r\n");
				TransmitString("To change your motor to AC induction, the command is 'motor-type 1'\r\n");
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "run-angle-offset-test")) {
			if (savedValues.motorType >= 2) {
				if (myUARTCommand.number < 512) {			// angleOffset is normalized to something in [0,511]
					myAngleOffsetTest.startTime = counter10k;
					myAngleOffsetTest.currentAngleOffset = myUARTCommand.number;	// it will increment up to 511, giving each angle candidate some time.
					myAngleOffsetTest.testRunning = 1;
					myAngleOffsetTest.testFinished = 0;
					myAngleOffsetTest.testFailed = 1;
//					myAngleOffsetTest.maxTestSpeed = 0;
//					myAngleOffsetTest.bestAngleOffset = 0;
				}
			}
			else {
				TransmitString("Your motor type is AC induction.  This test is for a permanent maget AC motor!\r\n");
				TransmitString("To change your motor to permanent maget, the command is 'motor-type 2'\r\n");
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "run-saliency-test")) {
			if (savedValues.motorType >= 2) {
				if (myUARTCommand.number < 1024) {  // What percent of the currentRadius should Id be (but negative)?
					myMotorSaliencyTest.startTime = myMotorSaliencyTest.elapsedTime = counter10k;
					myMotorSaliencyTest.KArrayIndex = myUARTCommand.number;	
					myMotorSaliencyTest.testRunning = 1;
					myMotorSaliencyTest.testFinished = 0;
					myMotorSaliencyTest.testFailed = 1;
//					myMotorSaliencyTest.maxTestSpeed = 0;
//					myMotorSaliencyTest.bestKArrayIndex = 0;
				}
			}
			else {
				TransmitString("Your motor type is AC induction.  This test is for a permanent maget AC motor!\r\n");
				TransmitString("To change your motor to permanent maget, the command is 'motor-type 2' or 'motor-type 3'\r\n");
			}
		}


		else if ((!strcmp((const char *)&myUARTCommand.string[0], "config")) || (!strcmp((const char *)&myUARTCommand.string[0], "settings"))) {
			ShowConfig();
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "data-stream-period")) {  // in milliseconds
			if (myUARTCommand.number > 0) {
				myDataStream.period = myUARTCommand.number;
				myDataStream.showStreamOnce = 0;
		// bit 15 set: display myDataStream.timer
		// Bit 14 set: display myDataStream.Id_times10
		// bit 13 set: display myDataStream.Iq_times10
		// Bit 12 set: display myDataStream.IdRef_times10
		// bit 11 set: display myDataStream.IqRef_times10
		// Bit 10 set: display myDataStream.Vd
		// bit 9 set: display myDataStream.Vq
		// Bit 8 set: display myDataStream.Ia_times10
		// bit 7 set: display myDataStream.Ib_times10
		// bit 6 set: display myDataStream.Ic_times10
		// Bit 5 set: display myDataStream.Va
		// bit 4 set: display myDataStream.Vb
		// bit 3 set: display myDataStream.Vc
		// bit 2 set: display myDataStream.percentOfVoltageDiskBeingUsed
		// bit 1 set: display myDataStream.batteryAmps_times10
		// bit 0 set: future use
		//	int dataToDisplaySet2;
		// Bit 15 set: display myDataStream.rawThrottle
		// bit 14 set: display myDataStream.throttle
		// Bit 13 set: display myDataStream.temperature
		// bit 12 set: display myDataStream.slipSpeedRPM
		// Bit 11 set: display myDataStream.electricalSpeedRPM
		// bit 10 set: display myDataStream.mechanicalSpeedRPM
		// bit 9 set:  display poscnt, which is a saved copy of the encoder ticks.  It's just a way to debug the encoder, to make sure it's working.
		// Bit 8-0 set: future use. 
	
				if (savedValues2.dataToDisplaySet1 & 32768) {
					TransmitString("time,");
				}
				if (savedValues2.dataToDisplaySet1 & 16384) {
					TransmitString("Id,");
				}
				if (savedValues2.dataToDisplaySet1 & 8192) {
					TransmitString("Iq,");
				}
				if (savedValues2.dataToDisplaySet1 & 4096) {
					TransmitString("IdRef,");
				}
				if (savedValues2.dataToDisplaySet1 & 2048) {
					TransmitString("IqRef,");
				}
				if (savedValues2.dataToDisplaySet1 & 256) {
					TransmitString("Ia,");
				}
				if (savedValues2.dataToDisplaySet1 & 128) {
					TransmitString("Ib,");
				}
				if (savedValues2.dataToDisplaySet1 & 64) {
					TransmitString("Ic,");
				}
				if (savedValues2.dataToDisplaySet1 & 4) {
					TransmitString("percentVolts,");
				}
				if (savedValues2.dataToDisplaySet1 & 2) {
					TransmitString("batteryAmps,");
				}
				if (savedValues2.dataToDisplaySet2 & 32768) {
					TransmitString("rawThrottle,");
				}
				if (savedValues2.dataToDisplaySet2 & 16384) {
					TransmitString("throttle,");
				}
				if (savedValues2.dataToDisplaySet2 & 8192) {
					TransmitString("temperaure,");
				}
				if (savedValues2.dataToDisplaySet2 & 4096) {
					TransmitString("slipSpeed,");
				}
				if (savedValues2.dataToDisplaySet2 & 2048) {
					TransmitString("electricalSpeed,");
				}
				if (savedValues2.dataToDisplaySet2 & 1024) {
					TransmitString("mechanicalSpeed,");
				}
				if (savedValues2.dataToDisplaySet2 & 512) {
					TransmitString("poscnt,");
				}
				TransmitString("\r\n");
				myDataStream.startTime = counter1k;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "data")) {  // show the datastream one time.
			myDataStream.period = 1;
			myDataStream.showStreamOnce = 1;
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-time")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 32768;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~32768;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-id")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 16384;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~16384;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-iq")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 8192;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~8192;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-idref")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 4096;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~4096;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-iqref")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 2048;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~2048;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-ia")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 256;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~256;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-ib")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 128;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~128;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-ic")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 64;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~64;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-percent-volts")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 4;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~4;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-battery-amps")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet1 |= 2;
			}
			else {
				savedValues2.dataToDisplaySet1 &= ~2;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-raw-throttle")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 32768;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~32768;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-throttle")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 16384;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~16384;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-temperature")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 8192;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~8192;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-slip-speed")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 4096;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~4096;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-electrical-speed")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 2048;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~2048;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-mechanical-speed")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 1024;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~1024;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "stream-poscnt")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				savedValues2.dataToDisplaySet2 |= 512;
			}
			else {
				savedValues2.dataToDisplaySet2 &= ~512;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "debug")) {  // in milliseconds
			if (myUARTCommand.number == 1) {
				debugMode = 1;
			}
			else {
				debugMode = 0;
			}
		}
		else if (!strcmp((const char *)&myUARTCommand.string[0], "off")) {
			if (myRotorTest.testRunning) {  // Stop the rotor test if it was running, and just keep the best value of the rotor time constant that you had found up to this point.
				savedValues2.rotorTimeConstantIndex = myRotorTest.bestTimeConstantIndex;
				myRotorTest.timeConstantIndex = savedValues2.rotorTimeConstantIndex;

				myRotorTest.testRunning = 0;
				myRotorTest.testFinished = 1;
//				currentRadiusRefRef = 0;
			}
			else if (myPI.testRunning) { // Stop the PI test if it was running.
				currentMaxIterationsBeforeZeroCrossing = 20;
				InitPIStruct();
				myPI.testFailed = 1;
				myPI.testFinished = 1;
//				currentRadiusRefRef = 0;
			}
			if (myAngleOffsetTest.testRunning) {  // 
				myAngleOffsetTest.testRunning = 0;
				myAngleOffsetTest.testFinished = 1;
			}
			myDataStream.period = 0;  // Stop the data stream if it was running.  I already do this any time a key is hit, so this is redundant.
			// if the PI test is running, terminate it.
			// if the rotor test is running, stop and use the current best rotorTimeConstant that has been found so far.
			// if the angle offset test is running, stop and use the current best angle offset that has been found so far.

			ShowMenu();
		}

		else if (!strcmp((const char*)&myUARTCommand.string[0], "swap-ab")) {
			savedValues2.swapAB = (myUARTCommand.number & 1);
			QEICONbits.SWPAB = savedValues2.swapAB;
		}
		else if (!strcmp((const char*)&myUARTCommand.string[0], "2")) {
			if (myAngleOffsetTest.currentAngleOffset < 511-5) {
				myAngleOffsetTest.currentAngleOffset+=5;
				savedValues2.angleOffset = myAngleOffsetTest.currentAngleOffset;  	// this is the working copy.
			}
		}
		else if (!strcmp((const char*)&myUARTCommand.string[0], "1")) {
			if (myAngleOffsetTest.currentAngleOffset >= 5) {
				myAngleOffsetTest.currentAngleOffset-=5;
				savedValues2.angleOffset = myAngleOffsetTest.currentAngleOffset;  	// this is the working copy.
			}
		}
//		else if (!strcmp((const char*)&myUARTCommand.string[0], "4")) {
//		}
//		else if (!strcmp((const char*)&myUARTCommand.string[0], "3")) {
//		}
		else if (!strcmp((const char*)&myUARTCommand.string[0], "c")) {
			dataCaptureIndex = 0;
			captureData = 1;
		}

		else if (!strcmp((const char *)&myUARTCommand.string[0], "?")) {  // show the valid list of commands
			TransmitString("List of valid commands:\r\n");
			TransmitString("save\r\n");
			TransmitString("motor-type xxx (rangle 1-4)\r\n");
			TransmitString("kp xxx (range 0-32767)\r\n");
			TransmitString("ki xxx (range 0-32767)\r\n");
			TransmitString("current-sensor-amps-per-volt xxx (range 0-480)\r\n");
			TransmitString("max-regen-position xxx (range 0-1023)\r\n");
			TransmitString("min-regen-position xxx (range 0-1023)\r\n");
			TransmitString("min-throttle-position xxx (range 0-1023)\r\n");
			TransmitString("max-throttle-position xxx (range 0-1023)\r\n");
			TransmitString("fault-throttle-position xxx (range 0-1023)\r\n");
			TransmitString("max-battery-amps xxx (range 0-999)\r\n");
			TransmitString("max-battery-amps-regen xxx (range 0-999)\r\n");
			TransmitString("max-motor-amps xxx (range 0-999)\r\n");
			TransmitString("max-motor-amps-regen xxx (range 0-999)\r\n");
			TransmitString("precharge-time xxx (in tenths of a sec. range 0-9999)\r\n");
			TransmitString("rotor-time-constant xxx (in millisec. range 0-150)\r\n");
			TransmitString("pole-pairs xxx (range 0-999)\r\n");
			TransmitString("max-rpm xxx (range 0-32767)\r\n");
			TransmitString("throttle-type xxx (range 0-1)\r\n");
			TransmitString("encoder-ticks xxx (range 64-5000)\r\n");
			TransmitString("pi-ratio xxx (range 50-1000.  pi-ratio = Kp/Ki)\r\n");
			TransmitString("angle-offset xxx (range 0-511)\r\n");
			TransmitString("saliency xxx (range 0-1023)\r\n");
			TransmitString("run-pi-test\r\n");
			TransmitString("run-rotor-test\r\n");
			TransmitString("run-angle-offset-test\r\n");
			TransmitString("run-saliency-test\r\n");			
			TransmitString("config\r\n");
			TransmitString("data-stream-period xxx (range 0-32767)\r\n");
			TransmitString("data\r\n");
			TransmitString("stream-time xxx (range 0-1)\r\n");
			TransmitString("stream-id xxx (range 0-1)\r\n");
			TransmitString("stream-iq xxx (range 0-1)\r\n");
			TransmitString("stream-idref xxx (range 0-1)\r\n");
			TransmitString("stream-iqref xxx (range 0-1)\r\n");
			TransmitString("stream-ia xxx (range 0-1)\r\n");
			TransmitString("stream-ib xxx (range 0-1)\r\n");
			TransmitString("stream-ic xxx (range 0-1)\r\n");
			TransmitString("stream-percent-volts xxx (range 0-1)\r\n");
			TransmitString("stream-battery-amps xxx (range 0-1)\r\n");
			TransmitString("stream-raw-throttle xxx (range 0-1)\r\n");
			TransmitString("stream-throttle xxx (range 0-1)\r\n");
			TransmitString("stream-temperature xxx (range 0-1)\r\n");
			TransmitString("stream-slip-speed xxx (range 0-1)\r\n");
			TransmitString("stream-electrical-speed xxx (range 0-1)\r\n");
			TransmitString("stream-mechanical-speed xxx (range 0-1)\r\n");
			TransmitString("stream-poscnt xxx (range 0-1)\r\n");
			TransmitString("off (this stops the data stream)\r\n");
			TransmitString("swap-ab xxx (range 0-1)\r\n"); 
		}
		else {
			TransmitString("Invalid command.  Type '?' to see a valid list of commands.\r\n");
		}
	
		myUARTCommand.string[0] = 0; 	// clear the string.
		myUARTCommand.i = 0;
		myUARTCommand.number = 0;
		myUARTCommand.complete = 0;  // You processed that command.  Dump it!  Do this last.  The ISR will only run through if the command is NOT yet complete (in other words, if complete == 0). 
	}
}

void StopAllMotorTests() {
	myDataStream.period = 0;  	// stop the data stream during this test.
	myPI.testRunning = 0;	 	// stop the PI test if it's running
	myPI.testFailed = 1;	
	myPI.testFinished = 0;
	myRotorTest.testRunning = 0;	// stop the rotor time constant search if there was one.
	myRotorTest.testFinished = 0;
	myAngleOffsetTest.testRunning = 0;	// stop the permanent magnet angle offset search if there was one.
	myAngleOffsetTest.testFinished = 0;	
	myMotorSaliencyTest.testRunning = 0;	// stop the permanent magnet angle offset search if there was one.
	myMotorSaliencyTest.testFinished = 0;	
}

void ShowConfig() {
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// motor-type=x\r\n
		strcpy(showConfigString,"motor-type=x\r\n");
		u16_to_str(&showConfigString[11], savedValues.motorType, 1);	
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// kp=xxxxx ki=xxxxx\r\n
		strcpy(showConfigString,"kp=xxxxx ki=xxxxx\r\n");
		u16_to_str(&showConfigString[3], savedValues.Kp, 5);	
		u16_to_str(&showConfigString[12], savedValues.Ki, 5);
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// current-sensor-amps-per-volt=xxxx\r\n;
		strcpy(showConfigString,"current-sensor-amps-per-volt=xxxx\r\n");
		u16_to_str(&showConfigString[29], savedValues.currentSensorAmpsPerVolt, 4);
		TransmitString(showConfigString);
	// 0         1         2         3         4         5         6         7         8
	// 01234567890123456789012345678901234567890123456789012345678901234567890123456789012345
	// max-regen-position=xxxx\r\n
	// min-regen-position=xxxx\r\n 
		strcpy(showConfigString,"max-regen-position=xxxx\r\n");
		u16_to_str(&showConfigString[19], savedValues.maxRegenPosition, 4);
		TransmitString(showConfigString);
		strcpy(showConfigString,"min-regen-position=xxxx\r\n");
		u16_to_str(&showConfigString[19], savedValues.minRegenPosition, 4);
		TransmitString(showConfigString);
	// 0         1         2         3         4         5         6         7         8
	// 01234567890123456789012345678901234567890123456789012345678901234567890123456789012345
	// min-throttle-position=xxxx\r\n
	// max-throttle-position=xxxx\r\n 
	// fault-throttle-position=xxxx\r\n
		strcpy(showConfigString,"min-throttle-position=xxxx\r\n");
		u16_to_str(&showConfigString[22], savedValues.minThrottlePosition, 4);
		TransmitString(showConfigString);

		strcpy(showConfigString,"max-throttle-position=xxxx\r\n");
		u16_to_str(&showConfigString[22], savedValues.maxThrottlePosition, 4);
		TransmitString(showConfigString);

		strcpy(showConfigString,"fault-throttle-position=xxxx\r\n");
		u16_to_str(&showConfigString[24], savedValues.throttleFaultPosition, 4);
		TransmitString(showConfigString);
	// 0         1         2         3         4         5         6         7         8
	// 01234567890123456789012345678901234567890123456789012345678901234567890123456789012345
	// max-battery-amps=xxxx amps\r\n
	// max-battery-amps-regen=xxxx amps\r\n
		strcpy(showConfigString,"max-battery-amps=xxxz amps\r\n");
		u16_to_str(&showConfigString[17], savedValues.maxBatteryAmps, 4);
		TransmitString(showConfigString);

		strcpy(showConfigString,"max-battery-amps-regen=xzxx amps\r\n");
		u16_to_str(&showConfigString[23], savedValues.maxBatteryAmpsRegen, 4);
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// max-motor-amps=xxx amps\r\n
	// max-motor-amps-regen=xxx amps\r\n
		strcpy(showConfigString,"max-motor-amps=xxx amps\r\n");
		u16_to_str(&showConfigString[15], savedValues.maxMotorAmps, 3);
		TransmitString(showConfigString);

		strcpy(showConfigString,"max-motor-amps-regen=xxx amps\r\n");
		u16_to_str(&showConfigString[21], savedValues.maxMotorAmpsRegen, 3);
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// precharge-time=xxxx tenths of a sec\r\n
		strcpy(showConfigString,"precharge-time=xxxx tenths of a sec\r\n");
		u16_to_str(&showConfigString[15], savedValues.prechargeTime, 4);
		TransmitString(showConfigString);

		if (savedValues.motorType == 1) {
			// **NOW WE ARE IN SavedValues2**
			// 0         1         2         3         4         5
			// 012345678901234567890123456789012345678901234567890123456789
			// rotor-time-constant=xxx ms\r\n
			//
			strcpy(showConfigString,"rotor-time-constant=xxx ms\r\n");
			u16_to_str(&showConfigString[20], savedValues2.rotorTimeConstantIndex+5, 3);  // for display purposes, add 5 so it's millisec.
			TransmitString(showConfigString);
		}
		else {
			// **NOW WE ARE IN SavedValues2**
			// 0         1         2         3         4         5
			// 012345678901234567890123456789012345678901234567890123456789
			// angle-offset=xxx\r\n
			//
			strcpy(showConfigString,"angle-offset=xxx\r\n");
			u16_to_str(&showConfigString[13], savedValues2.angleOffset, 3);  // for display purposes, add 5 so it's millisec.
			TransmitString(showConfigString);			
			// 0         1         2         3         4         5
			// 012345678901234567890123456789012345678901234567890123456789
			// saliency=xxxx\r\n
			//
			strcpy(showConfigString,"saliency=xxxx\r\n");
			u16_to_str(&showConfigString[9], savedValues2.KArrayIndex, 4);  // for display purposes, add 5 so it's millisec.
			TransmitString(showConfigString);			
		}

	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// pole-pairs=xxx\r\n
	//
		strcpy(showConfigString,"pole-pairs=xxx\r\n");
		u16_to_str(&showConfigString[11], savedValues2.numberOfPolePairs, 3);  
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// max-rpm=xxxxx rev/min\r\n
	//
		strcpy(showConfigString,"max-rpm=xxxxx rev/min\r\n");
		u16_to_str(&showConfigString[8], savedValues2.maxRPM, 5);  
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// throttle-type=x\r\n
	//
		strcpy(showConfigString,"throttle-type=x\r\n");
		u16_to_str(&showConfigString[14], savedValues2.throttleType, 1);  
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// encoder-ticks=xxxx ticks/rev\r\n
	//
		strcpy(showConfigString,"encoder-ticks=xxxx ticks/rev\r\n");
		u16_to_str(&showConfigString[14], savedValues2.encoderTicks, 4);  
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// pi-ratio=xxx\r\n
	//
		strcpy(showConfigString,"pi-ratio=xxx\r\n");
		u16_to_str(&showConfigString[9], myPI.ratioKpKi, 3);  // ADCBUF1 is the raw throttle.
		TransmitString(showConfigString);
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// raw-throttle=xxxx\r\n
	//
		strcpy(showConfigString,"raw-throttle=xxxx\r\n");
		u16_to_str(&showConfigString[13], ADCBUF1, 4);  // ADCBUF1 is the raw throttle.
		TransmitString(showConfigString);
}

// Input is an integer from 0 to 15.  Output is a character in '0', '1', '2', ..., '9', 'a','b','c','d','e','f'
char IntToCharHex(unsigned int i) {
	if (i <= 9) {
		return ((unsigned char)(i + 48));
	}
	else {
		return ((unsigned char)(i + 55));
	}
}

void ShowMenu(void)
{
	TransmitString("AC controller firmware, ver. 1.0\r\n");
}

// convert val to string (inside body of string) with specified number of digits
// do NOT terminate string
void u16_to_str(char *str, unsigned val, unsigned char digits)
{
	str = str + (digits - 1); // go from right to left.
	while (digits > 0) { // 
		*str = (unsigned char)(val % 10) + '0';
		val = val / 10;
		str--;
		digits--;
	}
}

// convert val to string (inside body of string) with specified number of digits (not counting the + or - sign).
// do NOT terminate string
// Ex:  -2345 should have length 4. It will be printed as -2345
// 2345 should also have length 4.  It will be printed as +2345.
// So, the first symbol is either '-' or '+'. 
void int16_to_str(char *str, int val, unsigned char digits)
{	
	if (val < 0) {
		str[0] = '-';
		val = -val;
	}
	else {
		str[0] = '+';
	}
	str = str + digits; // go from right to left.
	while (digits > 0) { // 
		*str = (unsigned char)(val % 10) + '0';
		val = val / 10;
		str--;
		digits--;
	}
}

// convert val to hex string (inside body of string) with specified number of digits
// do NOT terminate string
void u16x_to_str(char *str, unsigned val, unsigned char digits)
{
	unsigned char nibble;
	
	str = str + (digits - 1);
	while (digits > 0) {
		nibble = val & 0x000f;
		if (nibble >= 10) nibble = (nibble - 10) + 'A';
		else nibble = nibble + '0';
		*str = nibble;
		val = val >> 4;
		str--;
		digits--;
	}
}

int TransmitString(const char* str) {  // For echoing onto the display
	unsigned int i = 0;
	unsigned int now = 0;
	
//	now = TMR5;	// timer 4 runs at 59KHz.  Timer5 is the high word of the 32 bit timer.  So, it updates about 1 time per second.
	while (1) {
		if (str[i] == 0) {
			return 1;
		}
		if (U2STAbits.UTXBF == 0) { // TransmitReady();
			U2TXREG = str[i]; 	// SendCharacter(str[i]);
			i++;
		}
//		if (TMR5 - now > 5000) { 	// 5 seconds
//			faultBits |= UART_FAULT;
//			return 0;
//		}
//		#ifndef DEBUG 
//			ClrWdt();
//		#endif
	}
}

void StreamData() {
	static volatile int tenths = 0;
	static volatile int temp;
	//	unsigned int dataToDisplaySet1;
	// 0b0000 0000 0000 0000
	// bit 15 set: display myDataStream.timer
	// Bit 14 set: display myDataStream.Id_times10
	// bit 13 set: display myDataStream.Iq_times10
	// Bit 12 set: display myDataStream.IdRef_times10
	// bit 11 set: display myDataStream.IqRef_times10
	// Bit 10 set: display myDataStream.Vd
	// bit 9 set: display myDataStream.Vq
	// Bit 8 set: display myDataStream.Ia_times10
	// bit 7 set: display myDataStream.Ib_times10
	// bit 6 set: display myDataStream.Ic_times10
	// Bit 5 set: display myDataStream.Va
	// bit 4 set: display myDataStream.Vb
	// bit 3 set: display myDataStream.Vc
	// bit 2 set: display myDataStream.percentOfVoltageDiskBeingUsed
	// bit 1 set: display myDataStream.batteryAmps_times10
	// bit 0 set: future use
	//	unsigned int dataToDisplaySet2;
	// Bit 15 set: display myDataStream.rawThrottle
	// bit 14 set: display myDataStream.throttle
	// Bit 13 set: display myDataStream.temperature
	// bit 12 set: display myDataStream.slipSpeedRPM
	// Bit 11 set: display myDataStream.electricalSpeedRPM
	// bit 10 set: display myDataStream.mechanicalSpeedRPM
	// bit 9 set: display poscnt
	// Bit 8-0 set: future use. 

	if (savedValues2.dataToDisplaySet1 & 32768) {
		u16_to_str((char *)&intString[0], myDataStream.timer, 5); // intString[] = "00345".  Now, add a comma and null terminate it.
		intString[5] = ',';
		intString[6] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet1 & 16384) {
		temp = abs(myDataStream.Id_times10);
		tenths = temp % 10;
		myDataStream.Id_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.Id_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet1 & 8192) {
		temp = abs(myDataStream.Iq_times10);
		tenths = temp % 10;
		myDataStream.Iq_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.Iq_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet1 & 4096) {
		temp = abs(myDataStream.IdRef_times10);
		tenths = temp % 10;
		myDataStream.IdRef_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.IdRef_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet1 & 2048) {
		temp = abs(myDataStream.IqRef_times10);
		tenths = temp % 10;
		myDataStream.IqRef_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.IqRef_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
//	if (savedValues2.dataToDisplaySet1 & 1024) {
//		int16_to_str((char *)&intString[0], myDataStream.Vd, 3);	 // ex: intString[] = "+087"
//		intString[4] = ','; // it is on 4 rather than 3 because there is a + or - as the first character of the string, since it's an int.
//		intString[5] = 0; // null terminate it.					
//		TransmitString((char *)&intString[0]);
//	}
//	if (savedValues2.dataToDisplaySet1 & 512) {
//		int16_to_str((char *)&intString[0], myDataStream.Vq, 3);	 // ex: intString[] = "+087"
//		intString[4] = ','; // it is on 4 rather than 3 because there is a + or - as the first character of the string, since it's an int.
//		intString[5] = 0; // null terminate it.					
//		TransmitString((char *)&intString[0]);
//	}
	if (savedValues2.dataToDisplaySet1 & 256) {
		temp = abs(myDataStream.Ia_times10);
		tenths = temp % 10;
		myDataStream.Ia_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.Ia_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet1 & 128) {
		temp = abs(myDataStream.Ib_times10);
		tenths = temp % 10;
		myDataStream.Ib_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.Ib_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet1 & 64) {
		temp = abs(myDataStream.Ic_times10);
		tenths = temp % 10;
		myDataStream.Ic_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.Ic_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
//	if (savedValues2.dataToDisplaySet1 & 32) {
//		int16_to_str((char *)&intString[0], myDataStream.Va, 3);	 // ex: intString[] = "+087"
//		intString[4] = ','; // it is on 4 rather than 3 because there is a + or - as the first character of the string, since it's an int.
//		intString[5] = 0; // null terminate it.					
//		TransmitString((char *)&intString[0]);
//	}
//	if (savedValues2.dataToDisplaySet1 & 16) {
//		int16_to_str((char *)&intString[0], myDataStream.Vb, 3);	 // ex: intString[] = "+087"
//		intString[4] = ','; // it is on 4 rather than 3 because there is a + or - as the first character of the string, since it's an int.
//		intString[5] = 0; // null terminate it.					
//		TransmitString((char *)&intString[0]);
//	}
//	if (savedValues2.dataToDisplaySet1 & 8) {
//		int16_to_str((char *)&intString[0], myDataStream.Vc, 3);	 // ex: intString[] = "+087"
//		intString[4] = ','; // it is on 4 rather than 3 because there is a + or - as the first character of the string, since it's an int.
//		intString[5] = 0; // null terminate it.					
//		TransmitString((char *)&intString[0]);
//	}
	if (savedValues2.dataToDisplaySet1 & 4) {
		u16_to_str((char *)&intString[0], myDataStream.percentOfVoltageDiskBeingUsed, 3); // intString[] = "075".  Now, add a comma and null terminate it.
		intString[3] = ',';
		intString[4] = 0;	
		TransmitString((char *)&intString[0]);

	}
	if (savedValues2.dataToDisplaySet1 & 2) {
		temp = abs(myDataStream.batteryAmps_times10);
		tenths = temp % 10;
		myDataStream.batteryAmps_times10 /= 10;
		int16_to_str((char *)&intString[0], myDataStream.batteryAmps_times10, 3);	 // ex: intString[] = "+087"
		intString[4] = '.';
		intString[5] = (char)(tenths + 48);
		intString[6] = ',';
		intString[7] = 0; // null terminate it.					
		TransmitString((char *)&intString[0]);
	}
	//if (savedValues2.dataToDisplaySet1 & 1) {
	//}
	// Bit 15 set: display myDataStream.rawThrottle
	// bit 14 set: display myDataStream.throttle
	// Bit 13 set: display myDataStream.temperature
	// bit 12 set: display myDataStream.slipSpeedRPM
	// Bit 11 set: display myDataStream.electricalSpeedRPM
	// bit 10 set: display myDataStream.mechanicalSpeedRPM
	// bit 9 set: display poscnt
	// Bit 8-0 set: future use.

	if (savedValues2.dataToDisplaySet2 & 32768) {
		u16_to_str((char *)&intString[0], myDataStream.rawThrottle, 4); // intString[] = "0345".  Now, add a comma and null terminate it.
		intString[4] = ',';
		intString[5] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet2 & 16384) {
		int16_to_str((char *)&intString[0], myDataStream.throttle, 4); // intString[] = "+0345".  Now, add a comma and null terminate it.
		intString[5] = ',';
		intString[6] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet2 & 8192) {
		u16_to_str((char *)&intString[0], myDataStream.temperature, 3); // intString[] = "38".  Now, add a comma and null terminate it.
		intString[3] = ',';
		intString[4] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet2 & 4096) {
		int16_to_str((char *)&intString[0], myDataStream.slipSpeedRPM, 4); // intString[] = "+0345".  Now, add a comma and null terminate it.
		intString[5] = ',';
		intString[6] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet2 & 2048) {
		int16_to_str((char *)&intString[0], myDataStream.electricalSpeedRPM, 5); // intString[] = "+03457".  Now, add a comma and null terminate it.
		intString[6] = ',';
		intString[7] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet2 & 1024) {
		int16_to_str((char *)&intString[0], myDataStream.mechanicalSpeedRPM, 5); // intString[] = "+03457".  Now, add a comma and null terminate it.
		intString[6] = ',';
		intString[7] = 0;	
		TransmitString((char *)&intString[0]);
	}
	if (savedValues2.dataToDisplaySet2 & 512) {
		u16_to_str((char *)&intString[0], POSCNT, 5); // intString[] = "03457".  Now, add a comma and null terminate it.
		intString[5] = ',';
		intString[6] = 0;	
		TransmitString((char *)&intString[0]);
	}

	TransmitString("\r\n");
}

void TransmitBigArrays() {
	int i = 0;
	if (readyToDisplayBigArrays) {
		readyToDisplayBigArrays = 0;
		for (i = 0; i < 254; i++) {
			int16_to_str((char *)&intString[0], bigArray1[i], 4);  // intString[] = "+0345".  Now, add a comma and null terminate it.
			intString[5] = ',';
			intString[6] = 0;	
			TransmitString((char *)&intString[0]);
			ClrWdt();
		}
		TransmitString("\r\n\r\n");
	}
}

