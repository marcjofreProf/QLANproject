/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum Physical Layer
*/
#include "QphysLayerAgent.h"
#include<iostream>
#include<unistd.h> //for usleep
#include <stdio.h>
#include <string.h>
// Time/synchronization management
#include <chrono>

#define LinkNumberMAX 2
#include "./BBBhw/GPIO.h"
#include <stdlib.h>
// Threading
#define WaitTimeAfterMainWhileLoop 1000
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20
#include <thread>
// Semaphore
#include <atomic>
// time points
#define WaitTimeToFutureTimePoint 199000000 // Max 999999999. It is the time barrier to try to achieve synchronization. Considered nanoseconds (it can be changed on the transformatoin used)
//Qubits
#define NumQubitsMemoryBuffer 2048
// MAthemtical calculations
#include <cmath>

using namespace exploringBB; // API to easily use GPIO in c++
/* A Simple GPIO application
* Written by Derek Molloy for the book "Exploring BeagleBone: Tools and
* Techniques for Building with Embedded Linux" by John Wiley & Sons, 2018
* ISBN 9781119533160. Please see the file README.md in the repository root
* directory for copyright and GNU GPLv3 license information.*/
using namespace std;

namespace nsQphysLayerAgent {
QPLA::QPLA() {// Constructor
	PRUGPIO=new GPIO();// Initiates custom PRU code in BBB	
	 //Very slow GPIO BBB not used anymore
	/*// The above pins initializatoins (and also in the destructor will not be needed in the future since it is done with PRU
	inGPIO=new GPIO(this->ReceiveLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
	inGPIO->setDirection(INPUT);
	inGPIO->setEdgeType(NONE);
	inGPIO->streamInOpen();
	outGPIO=new GPIO(this->EmitLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
	outGPIO->setDirection(OUTPUT);
	outGPIO->streamOutOpen();
	outGPIO->streamOutWrite(LOW);//outGPIO.setValue(LOW);*/
	
}

////////////////////////////////////////////////////////
void QPLA::acquire() {
/*while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
*/
// https://stackoverflow.com/questions/61493121/when-can-memory-order-acquire-or-memory-order-release-be-safely-removed-from-com
// https://medium.com/@pauljlucas/advanced-thread-safety-in-c-4cbab821356e
//int oldCount;
bool valueSemaphoreExpected=true;
while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
	if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
	break;
	}
}
}
 
void QPLA::release() {
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}

//////////////////////////////////////////////
int QPLA::InitParametersAgent(){// Client node have some parameters to adjust to the server node
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer
this->valueSemaphore=1; // Make sure it stays a
return 0; //All OK
}

int QPLA::SendParametersAgent(char* ParamsCharArray){// The upper layer gets the information to be send
this->acquire();

strcat(ParamsCharArray,"Phys;");
if (string(this->PayloadSendBuffer)!=string("")){
	strcat(ParamsCharArray,this->PayloadSendBuffer);
}
else{
	strcat(ParamsCharArray,"none_none_");
}
strcat(ParamsCharArray,";");
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer

this->release();

return 0; // All OK

}

int QPLA::SetSendParametersAgent(char* ParamsCharArray){// Node accumulates parameters for the other node

strcat(this->PayloadSendBuffer,ParamsCharArray);

return 0; //All OK
}

int QPLA::ReadParametersAgent(){// Node checks parameters from the other node
if (string(this->PayloadReadBuffer)!=string("") and string(this->PayloadReadBuffer)!=string("none_none_")){
	//cout << "There are new Parameters" << endl;
	this->ProcessNewParameters();
}
memset(this->PayloadReadBuffer, 0, sizeof(this->PayloadReadBuffer));// Reset buffer
return 0; // All OK
}

int QPLA::SetReadParametersAgent(char* ParamsCharArray){// The upper layer sets information to be read
this->acquire();
//cout << "QPLA::ReadParametersAgent: " << ParamsCharArray << endl;
char DiscardBuffer[NumBytesPayloadBuffer]={0};
strcpy(DiscardBuffer,strtok(ParamsCharArray,";"));
strcpy(this->PayloadReadBuffer,strtok(NULL,";"));

this->release();
return 0; // All OK
}
////////////////////////////////////////////////////
int QPLA::countDoubleColons(char* ParamsCharArray) {
  int colonCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ':') {
      colonCount++;
    }
  }

  return colonCount/2;
}

int QPLA::countDoubleUnderscores(char* ParamsCharArray) {
  int underscoreCount = 0;
  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == '_') {
      underscoreCount++;
    }
  }
  return underscoreCount/2;
}

int QPLA::ProcessNewParameters(){
char ParamsCharArray[NumBytesPayloadBuffer]={0};
char HeaderCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
char ValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
char TokenValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};

strcpy(ParamsCharArray,this->PayloadReadBuffer);

int NumDoubleUnderscores = this->countDoubleUnderscores(ParamsCharArray);
//cout << "NumDoubleUnderscores: " << NumDoubleUnderscores << endl;

for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
	if (iHeaders==0){
		strcpy(HeaderCharArray[iHeaders],strtok(ParamsCharArray,"_"));		
	}
	else{
		strcpy(HeaderCharArray[iHeaders],strtok(NULL,"_"));
	}
	strcpy(ValuesCharArray[iHeaders],strtok(NULL,"_"));
}
//cout << "Phys: Processing Parameters" << endl;
//cout << "this->PayloadReadBuffer: " << this->PayloadReadBuffer << endl;
for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
//cout << "HeaderCharArray[iHeaders]: " << HeaderCharArray[iHeaders] << endl;
// Missing to develop if there are different values
/*
if (string(HeaderCharArray[iHeaders])==string("EmitLinkNumberArray[0]")){
	this->EmitLinkNumberArray[0]=(int)atoi(ValuesCharArray[iHeaders]);	
	outGPIO=new GPIO(this->EmitLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
	outGPIO->setDirection(OUTPUT);
	outGPIO->streamOutOpen();
	outGPIO->streamOutWrite(LOW);//outGPIO.setValue(LOW);
}
else if (string(HeaderCharArray[iHeaders])==string("ReceiveLinkNumberArray[0]")){
	this->ReceiveLinkNumberArray[0]=(int)atoi(ValuesCharArray[iHeaders]);
	inGPIO=new GPIO(this->ReceiveLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
	inGPIO->setDirection(INPUT);
	inGPIO->setEdgeType(NONE);
	inGPIO->streamInOpen();
}
*/
if (string(HeaderCharArray[iHeaders])==string("QuBitsPerSecondVelocity[0]")){this->QuBitsPerSecondVelocity[0]=(float)atoi(ValuesCharArray[iHeaders]);}
else if (string(HeaderCharArray[iHeaders])==string("OtherClientNodeFutureTimePoint")){// Also helps to wait here for the thread
	//cout << "OtherClientNodeFutureTimePoint: " << (unsigned int)atoi(ValuesCharArray[iHeaders]) << endl;
	std::chrono::nanoseconds duration_back((unsigned long long int)strtoull(ValuesCharArray[iHeaders],NULL,10));
	this->OtherClientNodeFutureTimePoint=Clock::time_point(duration_back);
	
	// Debugging
	//TimePoint TimePointClockNow=Clock::now();
	//auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	//unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds) 
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	//auto duration_since_epoch=this->OtherClientNodeFutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	//unsigned int time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epoch).count(); // Convert 
	//cout << "time_as_count: " << time_as_count << endl;
}
/*
else if (string(HeaderCharArray[iHeaders])==string("ClearOtherClientNodeFutureTimePoint")){//CLear this node OtherClientNodeFutureTimePoints to avoid having a non-zero value eventhough the other node has finished transmitting and this one for some reason could no execute it
//if (this->threadEmitQuBitRefAux.joinable()){
	
	//cout << "Check block release Process New Parameters" << endl;	
	//if (this->threadEmitQuBitRefAux.joinable()){
	//this->release();
	//	this->threadEmitQuBitRefAux.join();
	//this->acquire();
	//}
	char NewMessageParamsCharArray[NumBytesPayloadBuffer] = {0};
	strcpy(NewMessageParamsCharArray,"JoinOtherClientNodeThread_0_"); // Initiates the ParamsCharArray, so use strcpy
this->SetSendParametersAgent(NewMessageParamsCharArray);// Send parameter to the other node
	this->RunThreadEmitQuBitFlag=true;
	// Reset the ClientNodeFutureTimePoint
	this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
}
else if (string(HeaderCharArray[iHeaders])==string("JoinOtherClientNodeThread")){
	//cout << "JoinOtherClientNodeThread" << endl;
	this->RunThreadReceiveQuBitFlag=true;
	//if (this->threadReceiveQuBitRefAux.joinable()){
	//this->release();
	//this->threadReceiveQuBitRefAux.join();
	//this->acquire();
	//}
}
*/
else{// discard
}
}

return 0; // All OK
}
////////////////////////////////////////////////////

int QPLA::InitAgentProcess(){
	// Then, regularly check for next job/action without blocking		  	
	// Not used void* params;
	// Not used this->threadRef=std::thread(&QTLAH::AgentProcessStaticEntryPoint,params);
	this->threadRef=std::thread(&QPLA::AgentProcessRequestsPetitions,this);
	  //if (ret) {
	    // Handle the error
	  //} 
	return 0; //All OK
}


int QPLA::SimulateEmitQuBit(){
this->acquire();
if (this->RunThreadSimulateReceiveQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateEmitQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateEmitQuBitRefAux=std::thread(&QPLA::ThreadSimulateEmitQuBit,this);
threadSimulateEmitQuBitRefAux.detach();
}
else{
cout << "Not possible to launch ThreadSimulateEmitQuBit" << endl;
}
this->release();

return 0; // return 0 is for no error
}

int QPLA::ThreadSimulateEmitQuBit(){
cout << "Simulate Emiting Qubits" << endl;
//struct timespec requestHalfPeriod,requestQuarterPeriod,requestWhileWait;
//requestHalfPeriod.tv_sec=0;
//requestQuarterPeriod.tv_sec=0;
//requestWhileWait.tv_sec=0;
//requestHalfPeriod.tv_nsec = (long)QuBitsNanoSecHalfPeriodInt[0];
//requestQuarterPeriod.tv_nsec = (long)QuBitsNanoSecQuarterPeriodInt[0];
struct timespec requestWhileWait;

int MaxWhileRound=1000;
// Wait to receive the FutureTimePoint from client node
this->acquire();
while(this->OtherClientNodeFutureTimePoint==std::chrono::time_point<Clock>() && MaxWhileRound>0){
	this->release();
	MaxWhileRound--;	
	usleep((int)(0.1*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//Maybe some sleep to reduce CPU consumption	
	this->acquire();
	};
if (MaxWhileRound<=0){
this->OtherClientNodeFutureTimePoint=Clock::now();
cout << "QPLA:ThreadEmitQuBit could not obtain in time the TimePoint from the other node" << endl;
}// Provide a TimePoint to avoid blocking issues
TimePoint FutureTimePoint=this->OtherClientNodeFutureTimePoint;
this->release();
//cout << "MaxWhileRound: " << MaxWhileRound << endl;
MaxWhileRound=100;
/////////////////////////////////////////////
// Checks
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds)
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;

auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;
unsigned long long int TimePointsDiff_time_as_count=0;
long long int CheckTimePointsDiff_time_as_count=0;
CheckTimePointsDiff_time_as_count=(long long int)(TimeNow_time_as_count-TimePointFuture_time_as_count);
cout << "CheckTimePointsDiff_time_as_count: " << CheckTimePointsDiff_time_as_count << endl;
///////////////////////////////////
/*
while(TimeNow_time_as_count<TimePointFuture_time_as_count && MaxWhileRound>0){
	MaxWhileRound--;	
	TimePointClockNow=Clock::now();
	duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
        if (TimeNow_time_as_count>=TimePointFuture_time_as_count){TimePointsDiff_time_as_count=0;}
        else{TimePointsDiff_time_as_count=TimePointFuture_time_as_count-TimeNow_time_as_count;}
        if (TimePointsDiff_time_as_count>(unsigned long long int)WaitTimeToFutureTimePoint){TimePointsDiff_time_as_count=(unsigned long long int)WaitTimeToFutureTimePoint;}//conditions to not get extremely large sleeps
        requestWhileWait.tv_nsec=(long)(TimePointsDiff_time_as_count);
	if (TimePointsDiff_time_as_count>0){clock_nanosleep(CLOCK_REALTIME,0,&requestWhileWait,NULL);}// usleep(TimePointsDiff_time_as_count);//Maybe some sleep to reduce CPU consumption
        //cout << "TimePointsDiff_time_as_count: " << TimePointsDiff_time_as_count << endl;
        TimePointClockNow=Clock::now();
	duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 	
	};
*/
this->acquire();// So that there are no segmentatoin faults by grabbing the CLOCK REALTIME and also this has maximum 
requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
this->RunThreadSimulateEmitQuBitFlag=true;//enable again that this thread can again be called
clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);// Synch barrier

// After passing the TimePoint barrier, in terms of synchronizaton to the action in synch, it is desired to have the minimum indispensable number of lines of code (each line of code adds time jitter)

//cout << "MaxWhileRound: " << MaxWhileRound << endl;

//this->acquire();
 //exploringBB::GPIO outGPIO=exploringBB::GPIO(this->EmitLinkNumberArray[0]); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.
 
 cout << "Start Emiting Qubits" << endl;// For less time jitter this line should be commented
 PRUGPIO->SendTriggerSignals(); // It is long enough emitting sufficient qubits for the receiver to get the minimum amount of multiples of 1024
 
 /* Very slow GPIO BBB not used anymore
 // Basic Output - Generate a pulse of 1 second period
 ////clock_nanosleep(CLOCK_REALTIME,0,&requestQuarterPeriod,NULL);
 //TimePointFuture_time_as_count+=(long)QuBitsNanoSecQuarterPeriodInt[0];
 //requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
//requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
//clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);
 for (int iIterWrite=0;iIterWrite<NumQubitsMemoryBuffer;iIterWrite++){
	 outGPIO->streamOutWrite(HIGH);//outGPIO.setValue(HIGH);
	 //clock_nanosleep(CLOCK_REALTIME,0,&requestHalfPeriod,NULL);
	 TimePointFuture_time_as_count+=(long)QuBitsNanoSecHalfPeriodInt[0];
	 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
	requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);
	 outGPIO->streamOutWrite(LOW);//outGPIO.setValue(LOW);
	 //clock_nanosleep(CLOCK_REALTIME,0,&requestHalfPeriod,NULL);
	 TimePointFuture_time_as_count+=(long)QuBitsNanoSecHalfPeriodInt[0];
	 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
	requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);
 }
 */
 // Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
 this->release();
 
 //cout << "Qubit emitted" << endl;
cout << "End Emiting Qubits" << endl;

 return 0; // return 0 is for no error
}

int QPLA::SimulateReceiveQuBit(){
this->acquire();
//cout << "this->RunThreadSimulateReceiveQuBitFlag: " << this->RunThreadSimulateReceiveQuBitFlag << endl;
//cout << "this->RunThreadSimulateEmitQuBitFlag: " << this->RunThreadSimulateEmitQuBitFlag << endl;
//cout << "this->RunThreadAcquireSimulateNumStoredQubitsNode: " << this->RunThreadAcquireSimulateNumStoredQubitsNode << endl;

if (this->RunThreadSimulateReceiveQuBitFlag and this->RunThreadAcquireSimulateNumStoredQubitsNode){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateReceiveQuBitFlag=false;//disable that this thread can again be called
this->RunThreadAcquireSimulateNumStoredQubitsNode=false;
std::thread threadSimulateReceiveQuBitRefAux=std::thread(&QPLA::ThreadSimulateReceiveQubit,this);
threadSimulateReceiveQuBitRefAux.detach();
}
else{
cout << "Not possible to launch ThreadSimulateReceiveQubit" << endl;
}
this->release();
return 0; // return 0 is for no error
}

int QPLA::ThreadSimulateReceiveQubit(){
cout << "Simulate Receiving Qubits" << endl;

//struct timespec requestHalfPeriod,requestQuarterPeriod,requestPeriod,requestWhileWait;
//requestHalfPeriod.tv_sec=0;
//requestQuarterPeriod.tv_sec=0;
//requestPeriod.tv_sec=0;
//requestWhileWait.tv_sec=0;
//requestHalfPeriod.tv_nsec = (long)QuBitsNanoSecHalfPeriodInt[0];
//requestQuarterPeriod.tv_nsec = (long)QuBitsNanoSecQuarterPeriodInt[0];
//requestPeriod.tv_nsec = (long)QuBitsNanoSecPeriodInt[0];
struct timespec requestWhileWait;
// Client sets a future TimePoint for measurement and communicates it to the server (the one sending the qubits)
// Somehow, here it is assumed that the two system clocks are quite snchronized (maybe with the Precise Time Protocol)
/*
// Debugging
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
//
*/
TimePoint FutureTimePoint = Clock::now()+std::chrono::nanoseconds(WaitTimeToFutureTimePoint);// Set a time point in the future
auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Convert 
//cout << "time_as_count: " << time_as_count << endl;
// Mount the Parameters message for the other node
char ParamsCharArray[NumBytesPayloadBuffer] = {0};
strcpy(ParamsCharArray,"OtherClientNodeFutureTimePoint_"); // Initiates the ParamsCharArray, so use strcpy

char charNum[NumBytesPayloadBuffer] = {0}; 
sprintf(charNum, "%llu", TimePointFuture_time_as_count);//%llu: unsigned long long int
strcat(ParamsCharArray,charNum);

strcat(ParamsCharArray,"_"); // Final _
//cout << "ParamsCharArray: " << ParamsCharArray << endl;
this->acquire();
this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other node
PRUGPIO->ClearStoredQuBits();
TimeTaggs[NumQubitsMemoryBuffer]={0}; // Clear the array
ChannelTags[NumQubitsMemoryBuffer]={0}; // Clear the array
this->release();
usleep((int)(100*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give some time to be able to send the above message
/*
unsigned long long int TimePointsDiff_time_as_count=0;
int MaxWhileRound=100;
while(Clock::now()<FutureTimePoint && MaxWhileRound>0){
	MaxWhileRound--;
	TimePointClockNow=Clock::now();
	duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	        
        if (TimeNow_time_as_count>=TimePointFuture_time_as_count){TimePointsDiff_time_as_count=0;}
        else{TimePointsDiff_time_as_count=TimePointFuture_time_as_count-TimeNow_time_as_count;}
        if (TimePointsDiff_time_as_count>(unsigned long long int)WaitTimeToFutureTimePoint){TimePointsDiff_time_as_count=(unsigned long long int)WaitTimeToFutureTimePoint;}//conditions to not get extremely large sleeps
        //cout << "TimePointsDiff_time_as_count: " << TimePointsDiff_time_as_count << endl;
        requestWhileWait.tv_nsec=(long)(TimePointsDiff_time_as_count);
	if (TimePointsDiff_time_as_count>0){clock_nanosleep(CLOCK_REALTIME,0,&requestWhileWait,NULL);}//usleep(TimePointsDiff_time_as_count);}//clock_nanosleep(CLOCK_REALTIME,0,&requestWhileWait,NULL);}//usleep(TimePointsDiff_time_as_count);//Maybe some sleep to reduce CPU consumption	
};
*/
this->acquire();// So that there are no segmentatoin faults by grabbing the CLOCK REALTIME and also this has maximum priority
requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL); // Synch barrier

// After passing the TimePoint barrier, in terms of synchronizaton to the action in synch, it is desired to have the minimum indispensable number of lines of code (each line of code adds time jitter)
//cout << "MaxWhileRound: " << MaxWhileRound << endl;

cout << "Start Receiving Qubits" << endl;// This line should be commented to reduce the time jitter

// Start measuring
 //exploringBB::GPIO inGPIO=exploringBB::GPIO(this->ReceiveLinkNumberArray[0]); // Receiving GPIO. Of course gnd have to be connected accordingly.
 
 PRUGPIO->ReadTimeStamps();// Multiple reads can be done in multiples of 1024 qubit timetags
 // Basic Input 
 /* Very slow GPIO BBB not used anymore
 ////clock_nanosleep(CLOCK_REALTIME,0,&requestQuarterPeriod,NULL);
 TimePointFuture_time_as_count+=(long)QuBitsNanoSecQuarterPeriodInt[0];
 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;iIterRead++){	 
	 SimulateQuBitValueArray[iIterRead]=inGPIO->streamInRead();//getValue();
	 //clock_nanosleep(CLOCK_REALTIME,0,&requestPeriod,NULL);	
	 TimePointFuture_time_as_count+=(long)QuBitsNanoSecPeriodInt[0];
	 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
	requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL); 
 }
 */
 
 this->release();
 cout << "End Receiving Qubits" << endl;
 
 
 /*
 // Basic input
 // Count received QuBits
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;iIterRead++){// Count how many qubits 
 	if (SimulateQuBitValueArray[iIterRead]==1){
 		SimulateNumStoredQubitsNodeAux++;
 	} 	
 }
 */

this->acquire();
this->SimulateNumStoredQubitsNode[0]=PRUGPIO->RetrieveNumStoredQuBits(TimeTaggs,ChannelTags);
//cout << "The value of the input is: "<< inGPIO.getValue() << endl;
// Tell the other node to clear the TimePoint (this avoids having a time point in the other node after having finished this one (because it was not ocnsumed)
//ParamsCharArray[NumBytesPayloadBuffer] = {0};
//strcpy(ParamsCharArray,"ClearOtherClientNodeFutureTimePoint_0_"); // Initiates the ParamsCharArray, so use strcpy
//this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other node
this->RunThreadSimulateReceiveQuBitFlag=true;//enable again that this thread can again be called
this->release();

return 0; // return 0 is for no error
}

int QPLA::GetSimulateNumStoredQubitsNode(float* TimeTaggsDetAnalytics){
this->acquire();
while(this->RunThreadSimulateReceiveQuBitFlag==false){this->release();usleep((int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Compute interesting analystics on the Timetaggs and deteciton so that not all data has to be transfered thorugh sockets
// Param 0: Num detections channel 1
// Param 1: Num detections channel 2
// Param 2: Num detections channel 3
// Param 3: Num detections channel 4
// Param 4: Multidetection events
// Param 5: Mean time difference between tags
// Param 6: std time difference between tags
for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
if (ChannelTags[i]&0x0001==1){
TimeTaggsDetAnalytics[0]=(float)TimeTaggsDetAnalytics[0]+1.0;
}
if ((ChannelTags[i]>>1)&0x0001==1){
TimeTaggsDetAnalytics[1]=(float)TimeTaggsDetAnalytics[1]+1.0;
}
if ((ChannelTags[i]>>2)&0x0001==1){
TimeTaggsDetAnalytics[2]=(float)TimeTaggsDetAnalytics[2]+1.0;
}
if ((ChannelTags[i]>>3)&0x0001==1){
TimeTaggsDetAnalytics[3]=(float)TimeTaggsDetAnalytics[3]+1.0;
}
if (((float)(ChannelTags[i]&0x0001)+(float)((ChannelTags[i]>>1)&0x0001)+(float)((ChannelTags[i]>>2)&0x0001)+(float)((ChannelTags[i]>>3)&0x0001))>1.0){
TimeTaggsDetAnalytics[4]=(float)TimeTaggsDetAnalytics[4]+1.0;
}
if (i>0){
TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(float)(TimeTaggs[i]-TimeTaggs[i-1]);
}
}
if ((SimulateNumStoredQubitsNodeAux-1)>0){TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]/((float)(SimulateNumStoredQubitsNodeAux-1));}

for (int i=1;i<SimulateNumStoredQubitsNodeAux;i++){
TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+pow((float)(TimeTaggs[i]-TimeTaggs[i-1])-TimeTaggsDetAnalytics[5],2);
}
if ((SimulateNumStoredQubitsNodeAux-1)>0){TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]/((float)(SimulateNumStoredQubitsNodeAux-1)));}

cout << "TimeTaggsDetAnalytics[0]: " << TimeTaggsDetAnalytics[0] << endl;
cout << "TimeTaggsDetAnalytics[1]: " << TimeTaggsDetAnalytics[1] << endl;
cout << "TimeTaggsDetAnalytics[2]: " << TimeTaggsDetAnalytics[2] << endl;
cout << "TimeTaggsDetAnalytics[3]: " << TimeTaggsDetAnalytics[3] << endl;
cout << "TimeTaggsDetAnalytics[4]: " << TimeTaggsDetAnalytics[4] << endl;
cout << "TimeTaggsDetAnalytics[5]: " << TimeTaggsDetAnalytics[5] << endl;
cout << "TimeTaggsDetAnalytics[6]: " << TimeTaggsDetAnalytics[6] << endl;

this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
this->release();

return SimulateNumStoredQubitsNodeAux;
}

QPLA::~QPLA() {
// destructor
/* Very slow GPIO BBB not used anymore
outGPIO->streamOutClose();
inGPIO->streamInClose();
*/
delete PRUGPIO; // Destructor for the PRUGPIO instance
this->threadRef.join();// Terminate the process thread
}

int QPLA::NegotiateInitialParamsNode(){
try{
 this->acquire();
if (string(this->SCmode[0])==string("client")){
	 char ParamsCharArray[NumBytesPayloadBuffer]="QuBitsPerSecondVelocity[0]_1000_";// Set initialization value for the other node
	 this->SetSendParametersAgent(ParamsCharArray);// Set initialization values for the other node
}
else{//server
// Expect to receive some information
}

this->release();
} // try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
   }

return 0;// All OK
}

void QPLA::AgentProcessRequestsPetitions(){// Check next thing to do

 this->NegotiateInitialParamsNode();

 bool isValidWhileLoop = true;
 while(isValidWhileLoop){
 try{
   try {
   	this->acquire();// Wait semaphore until it can proceed
    	this->ReadParametersAgent(); // Reads messages from above layer
        this->release(); // Release the semaphore 
        usleep((int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
    }
    catch (const std::exception& e) {
	// Handle the exception
    	cout << "Exception: " << e.what() << endl;
    }
    } // upper try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
    }
    } // while
}

} /* namespace nsQphysLayerAgent */

/*
// For testing
using namespace nsQphysLayerAgent;

int main(int argc, char const * argv[]){
 // Basically used for testing, since the declarations of the other functions will be used by other Agents as primitives 
 // argv and argc are how command line arguments are passed to main() in C and C++.

 // argc will be the number of strings pointed to by argv. This will (in practice) be 1 plus the number of arguments, as virtually all implementations will prepend the name of the program to the array.

 // The variables are named argc (argument count) and argv (argument vector) by convention, but they can be given any valid identifier: int main(int num_args, char** arg_strings) is equally valid.

 // They can also be omitted entirely, yielding int main(), if you do not intend to process command line arguments.
 
 //printf( "argc:     %d\n", argc );
 //printf( "argv[0]:  %s\n", argv[0] );
 
 //if ( argc == 1 ) {
 // printf( "No arguments were passed.\n" );
 //}
 //else{
 // printf( "Arguments:\n" );
 // for (int i = 1; i < argc; ++i ) {
 //  printf( "  %d. %s\n", i, argv[i] );
 // }
 //}
 
 int objectEmitLinkNumberArray[2], objectReceiveLinkNumberArray[2];
 QPLA objectQPLA(2, objectEmitLinkNumberArray, objectReceiveLinkNumberArray); 
 if (argc>1){objectQPLA.emitQuBit();} // ./QphysLayerAgent 1
 else{objectQPLA.receiveQuBit();}   // ./QphysLayerAgent

 return 0;
}
*/


