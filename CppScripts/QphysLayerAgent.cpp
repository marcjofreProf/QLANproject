/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum Physical Layer
*/
#include "QphysLayerAgent.h"
#include<iostream>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<bitset>
// Time/synchronization management
#include <chrono>

#define LinkNumberMAX 2
#include "./BBBhw/GPIO.h"
#include <stdlib.h>
// Threading
#define WaitTimeAfterMainWhileLoop 10000000 // nanoseconds
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20
#define IPcharArrayLengthMAX 15
#define NumHostConnection 5
// Threads
#include <thread>
// Semaphore
#include <atomic>
// time points
#define WaitTimeToFutureTimePoint 399000000 // Max 999999999. It is the time barrier to try to achieve synchronization. Considered nanoseconds (it can be changed on the transformatoin used)
#define UTCoffsetBarrierErrorThreshold 400000000 //37000000000 // Some BBB when synch with linuxPTP have an error on the UTC offset with respect TAI. Remove this sistemic offset and announce it!
// Mathematical calculations
#include <cmath>

//using namespace exploringBB; // API to easily use GPIO in c++. No because it will confuse variables (like semaphore acquire)
/* A Simple GPIO application
* Written by Derek Molloy for the book "Exploring BeagleBone: Tools and
* Techniques for Building with Embedded Linux" by John Wiley & Sons, 2018
* ISBN 9781119533160. Please see the file README.md in the repository root
* directory for copyright and GNU GPLv3 license information.*/
using namespace std;

namespace nsQphysLayerAgent {
QPLA::QPLA() {// Constructor
	PRUGPIO.InitAgentProcess();// Initialize thread of the agent below
	//PRUGPIO=new GPIO();// Initiates custom PRU code in BBB
	//PRUGPIO->InitAgentProcess(); // Launch the periodic synchronization. Done in the class itself
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
	// Synchronized "slotted" emission
	
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

bool QPLA::isPotentialIpAddressStructure(const char* ipAddress) {
  int dots = 0;
  int digitCount = 0;

  for (int i = 0; ipAddress[i] != '\0'; ++i) {
    if (isdigit(ipAddress[i])) {
      digitCount++;
    } else if (ipAddress[i] == '.') {
      if (digitCount == 0 || digitCount > 3) {
        return false; // Invalid structure
      }
      dots++;
      digitCount = 0;
    } else {
      return false; // Invalid character
    }
  }

  return dots == 3 && digitCount > 0 && digitCount <= 3; // Check for 4 octets
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
	std::chrono::nanoseconds duration_back(static_cast<unsigned long long int>(strtoull(ValuesCharArray[iHeaders],NULL,10)));
	this->OtherClientNodeFutureTimePoint=Clock::time_point(duration_back);
	//cout << "OtherClientNodeFutureTimePoint: " << static_cast<unsigned long long int>(strtoull(ValuesCharArray[iHeaders],NULL,10)) << endl;
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

struct timespec QPLA::SetFutureTimePointOtherNode(){// It is responsability of the host to distribute this time point to the other host's nodes
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
// Somehow, here it is assumed that the two system clocks are quite synchronized (maybe with the Precise Time Protocol)
/*
// Debugging
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
//
*/
this->FutureTimePoint = Clock::now()+std::chrono::nanoseconds(WaitTimeToFutureTimePoint);// Set a time point in the future
auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Add some margin so that busywait can be implemented for faster response // Convert 
//cout << "time_as_count: " << time_as_count << endl;

// Tell to the other nodes
char ParamsCharArray[NumBytesPayloadBuffer] = {0};
for (int iIterIPaddr=0;iIterIPaddr<NumHostConnection;iIterIPaddr++){// Iterate over the different nodes to tell
	if (isPotentialIpAddressStructure(IPaddresses[iIterIPaddr])){
		// Mount the Parameters message for the other node
		if (iIterIPaddr==0){strcpy(ParamsCharArray,"IPdest_");} // Initiates the ParamsCharArray, so use strcpy
		else{strcat(ParamsCharArray,"IPdest_");} // Continues the ParamsCharArray, so use strcat
		strcat(ParamsCharArray,IPaddresses[iIterIPaddr]);// Indicate the address to send the Future time Point
		strcat(ParamsCharArray,"_");// Add underscore separator
		strcat(ParamsCharArray,"OtherClientNodeFutureTimePoint_"); // Continues the ParamsCharArray, so use strcat
		char charNum[NumBytesPayloadBuffer] = {0}; 
		sprintf(charNum, "%llu", TimePointFuture_time_as_count);//%llu: unsigned long long int
		strcat(ParamsCharArray,charNum);
		strcat(ParamsCharArray,"_"); // Final _
	}
} // end for to the different addresses to send the params information
//cout << "QPLA::ParamsCharArray: " << ParamsCharArray << endl;
this->acquire();
this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other nodes
this->release();
this->RelativeNanoSleepWait((unsigned int)(100*(unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));// Give some time to be able to send the above message
////////////////////////////////////////////
TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-this->TimeClockMarging; // Add some margin so that busywait can be implemented for faster response
requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
//////////////////////////
return requestWhileWait;
}

struct timespec QPLA::GetFutureTimePointOtherNode(){
struct timespec requestWhileWait;
int MaxWhileRound=1000;
// Wait to receive the FutureTimePoint from other node
this->acquire();
while(this->OtherClientNodeFutureTimePoint==std::chrono::time_point<Clock>() && MaxWhileRound>0){
	this->release();
	MaxWhileRound--;	
	this->RelativeNanoSleepWait((unsigned int)(0.1*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//Maybe some sleep to reduce CPU consumption	
	this->acquire();
	};
if (MaxWhileRound<=0){
this->OtherClientNodeFutureTimePoint=Clock::now();
cout << "QPLA could not obtain in time the TimePoint from the other node" << endl;
}// Provide a TimePoint to avoid blocking issues
this->FutureTimePoint=this->OtherClientNodeFutureTimePoint;
// Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
this->release();
//cout << "MaxWhileRound: " << MaxWhileRound << endl;
/////////////////////////////////////////////
// Checks
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds)
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;

auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-this->TimeClockMarging; // Add some margin so that busywait can be implemented for faster response // // Convert duration to desired time unit (e.g., milliseconds,microseconds)
//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;
unsigned long long int TimePointsDiff_time_as_count=0;
long long int CheckTimePointsDiff_time_as_count=0;
CheckTimePointsDiff_time_as_count=(long long int)(TimeNow_time_as_count-TimePointFuture_time_as_count);
cout << "CheckTimePointsDiff_time_as_count: " << CheckTimePointsDiff_time_as_count << endl;
if (CheckTimePointsDiff_time_as_count<=(-UTCoffsetBarrierErrorThreshold)){
	cout << "UTC TAI offset wrongly resolved!" << endl;
	TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-UTCoffsetBarrierErrorThreshold-this->TimeClockMarging; // Add some margin so that busy wait can be implemented for faster response // // Convert duration to desired time unit (e.g., milliseconds,microseconds)
	CheckTimePointsDiff_time_as_count=(long long int)(TimeNow_time_as_count-TimePointFuture_time_as_count);
	this->FutureTimePoint=this->FutureTimePoint-std::chrono::nanoseconds(UTCoffsetBarrierErrorThreshold);
	cout << "New CheckTimePointsDiff_time_as_count: " << CheckTimePointsDiff_time_as_count << endl;
}

requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
///////////////////////////////////
return requestWhileWait;
}


int QPLA::SimulateEmitQuBit(char* ModeActivePassiveAux,const char (&IPaddressesAux)[NumHostConnection][IPcharArrayLengthMAX],int numReqQuBitsAux,int* FineSynchAdjValAux){
this->acquire();
strcpy(this->ModeActivePassive,ModeActivePassiveAux);
for (int iIterIPaddr=0;iIterIPaddr<NumHostConnection;iIterIPaddr++){strcpy(this->IPaddresses[iIterIPaddr],IPaddressesAux[iIterIPaddr]);}
this->numReqQuBits=numReqQuBitsAux;
this->FineSynchAdjVal[0]=FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]=FineSynchAdjValAux[1];// synch trig frequency
if (this->RunThreadSimulateEmitQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateEmitQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateEmitQuBitRefAux=std::thread(&QPLA::ThreadSimulateEmitQuBit,this);
threadSimulateEmitQuBitRefAux.join();//threadSimulateEmitQuBitRefAux.detach();
}
else{
cout << "Not possible to launch ThreadSimulateEmitQuBit" << endl;
}
this->release();

return 0; // return 0 is for no error
}

int QPLA::ThreadSimulateEmitQuBit(){
cout << "Emiting Qubits" << endl;
struct timespec requestWhileWait;
if (string(this->ModeActivePassive)==string("Active")){
	requestWhileWait=this->SetFutureTimePointOtherNode();
}
else{
	requestWhileWait = this->GetFutureTimePointOtherNode();
}
this->acquire();// So that there are no segmentatoin faults by grabbing the CLOCK REALTIME and also this has maximum priority
clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL);// Synch barrier
//while (Clock::now() < this->FutureTimePoint);// Busy wait.

// After passing the TimePoint barrier, in terms of synchronizaton to the action in synch, it is desired to have the minimum indispensable number of lines of code (each line of code adds time jitter)

 //exploringBB::GPIO outGPIO=exploringBB::GPIO(this->EmitLinkNumberArray[0]); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.
 
 //cout << "Start Emiting Qubits" << endl;// For less time jitter this line should be commented
 
 PRUGPIO.SendTriggerSignals(this->FineSynchAdjVal);//PRUGPIO->SendTriggerSignals(); // It is long enough emitting sufficient qubits for the receiver to get the minimum amount of multiples of 2048
 
 /* Very slow GPIO BBB not used anymore
 // Basic Output - Generate a pulse of 1 second period
 ////clock_nanosleep(CLOCK_TAI,0,&requestQuarterPeriod,NULL);
 //TimePointFuture_time_as_count+=(long)QuBitsNanoSecQuarterPeriodInt[0];
 //requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
//requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
//clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL);
 for (int iIterWrite=0;iIterWrite<NumQubitsMemoryBuffer;iIterWrite++){
	 outGPIO->streamOutWrite(HIGH);//outGPIO.setValue(HIGH);
	 //clock_nanosleep(CLOCK_TAI,0,&requestHalfPeriod,NULL);
	 TimePointFuture_time_as_count+=(long)QuBitsNanoSecHalfPeriodInt[0];
	 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
	requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
	clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL);
	 outGPIO->streamOutWrite(LOW);//outGPIO.setValue(LOW);
	 //clock_nanosleep(CLOCK_TAI,0,&requestHalfPeriod,NULL);
	 TimePointFuture_time_as_count+=(long)QuBitsNanoSecHalfPeriodInt[0];
	 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
	requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
	clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL);
 }
 */
this->RunThreadSimulateEmitQuBitFlag=true;//enable again that this thread can again be called. It is okey since it entered a block semaphore part and then no other sempahored part will run until this one finishes. At the same time, returning to true at this point allows the read to not go out of scope and losing this flag parameter
 this->release();
 
 //cout << "Qubit emitted" << endl;
cout << "End Emiting Qubits" << endl;

 return 0; // return 0 is for no error
}

int QPLA::SimulateReceiveQuBit(char* ModeActivePassiveAux,const char (&IPaddressesAux)[NumHostConnection][IPcharArrayLengthMAX],int numReqQuBitsAux){
this->acquire();
strcpy(this->ModeActivePassive,ModeActivePassiveAux);
for (int iIterIPaddr=0;iIterIPaddr<NumHostConnection;iIterIPaddr++){strcpy(this->IPaddresses[iIterIPaddr],IPaddressesAux[iIterIPaddr]);}
this->numReqQuBits=numReqQuBitsAux;
if (this->RunThreadSimulateReceiveQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateReceiveQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateReceiveQuBitRefAux=std::thread(&QPLA::ThreadSimulateReceiveQubit,this);
threadSimulateReceiveQuBitRefAux.join();//threadSimulateReceiveQuBitRefAux.detach();
}
else{
cout << "Not possible to launch ThreadSimulateReceiveQubit" << endl;
}
this->release();
return 0; // return 0 is for no error
}

int QPLA::ThreadSimulateReceiveQubit(){
cout << "Receiving Qubits" << endl;
this->acquire();
TimeTaggs[NumQubitsMemoryBuffer]={0}; // Clear the array. Actually only the first items is set to 0.
ChannelTags[NumQubitsMemoryBuffer]={0}; // Clear the array. Actually only the first items is set to 0.
PRUGPIO.ClearStoredQuBits();//PRUGPIO->ClearStoredQuBits();
this->release();
int iIterRuns;
int DetRunsCount = NumQubitsMemoryBuffer/NumQuBitsPerRun;
//cout << "DetRunsCount: " << DetRunsCount << endl;
struct timespec requestWhileWait;
if (string(this->ModeActivePassive)==string("Active")){
	requestWhileWait=this->SetFutureTimePointOtherNode();
}
else{
	requestWhileWait = this->GetFutureTimePointOtherNode();
}
this->acquire();
// So that there are no segmentation faults by grabbing the CLOCK REALTIME and also this has maximum priority
clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL); // Synch barrier
//while (Clock::now() < this->FutureTimePoint);// Busy wait 

// After passing the TimePoint barrier, in terms of synchronizaton to the action in synch, it is desired to have the minimum indispensable number of lines of code (each line of code adds time jitter)

//cout << "Start Receiving Qubits" << endl;// This line should be commented to reduce the time jitter

// Start measuring
 //exploringBB::GPIO inGPIO=exploringBB::GPIO(this->ReceiveLinkNumberArray[0]); // Receiving GPIO. Of course gnd have to be connected accordingly.
 
 for (iIterRuns=0;iIterRuns<DetRunsCount;iIterRuns++){
	PRUGPIO.ReadTimeStamps();//PRUGPIO->ReadTimeStamps();// Multiple reads can be done in multiples of 2048 qubit timetags
 }
 // Basic Input 
 /* Very slow GPIO BBB not used anymore
 ////clock_nanosleep(CLOCK_TAI,0,&requestQuarterPeriod,NULL);
 TimePointFuture_time_as_count+=(long)QuBitsNanoSecQuarterPeriodInt[0];
 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL);
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;iIterRead++){	 
	 SimulateQuBitValueArray[iIterRead]=inGPIO->streamInRead();//getValue();
	 //clock_nanosleep(CLOCK_TAI,0,&requestPeriod,NULL);	
	 TimePointFuture_time_as_count+=(long)QuBitsNanoSecPeriodInt[0];
	 requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
	requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
	clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL); 
 }
 */
 /*
 // Basic input
 // Count received QuBits
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;iIterRead++){// Count how many qubits 
 	if (SimulateQuBitValueArray[iIterRead]==1){
 		SimulateNumStoredQubitsNodeAux++;
 	} 	
 }
 */

this->SimulateNumStoredQubitsNode[0]=PRUGPIO.RetrieveNumStoredQuBits(TimeTaggs,ChannelTags);//PRUGPIO->RetrieveNumStoredQuBits(TimeTaggs,ChannelTags);
this->RunThreadSimulateReceiveQuBitFlag=true;//enable again that this thread can again be called
this->release();
cout << "End Receiving Qubits" << endl;
return 0; // return 0 is for no error
}

int QPLA::GetSimulateNumStoredQubitsNode(double* TimeTaggsDetAnalytics){
this->acquire();
while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
this->RunThreadAcquireSimulateNumStoredQubitsNode=false;
int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];
TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;
// General computations - might be overriden by the below activated particular analysis
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Compute interesting analytics on the TIMETAGGS and detection so that not all data has to be transfered through sockets
// It has to have double precision so that statistics are useful
// Param 0: Num detections channel 1
// Param 1: Num detections channel 2
// Param 2: Num detections channel 3
// Param 3: Num detections channel 4
// Param 4: Multidetection events
// Param 5: Mean time difference between tags
// Param 6: std time difference between tags
// Param 7: time value first count tags

// Check that we now exceed the QuBits buffer size
if (SimulateNumStoredQubitsNodeAux>NumQubitsMemoryBuffer){SimulateNumStoredQubitsNodeAux=NumQubitsMemoryBuffer;}

if (SimulateNumStoredQubitsNodeAux>0){
for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
//cout << "TimeTaggs[i]: "<< TimeTaggs[i] << endl;
//cout << "ChannelTags[i]: "<< std::bitset<8>(ChannelTags[i]) << endl;
if (ChannelTags[i]&0x0001==1 or (ChannelTags[i]>>4)&0x0001==1){TimeTaggsDetAnalytics[0]++;}
if ((ChannelTags[i]>>1)&0x0001==1 or (ChannelTags[i]>>5)&0x0001==1){TimeTaggsDetAnalytics[1]++;}
if ((ChannelTags[i]>>2)&0x0001==1 or (ChannelTags[i]>>6)&0x0001==1){TimeTaggsDetAnalytics[2]++;}
if ((ChannelTags[i]>>3)&0x0001==1 or (ChannelTags[i]>>7)&0x0001==1){TimeTaggsDetAnalytics[3]++;}

if (((ChannelTags[i]&0x0001)+((ChannelTags[i]>>1)&0x0001)+((ChannelTags[i]>>2)&0x0001)+((ChannelTags[i]>>3)&0x0001)+((ChannelTags[i]>>4)&0x0001)+((ChannelTags[i]>>5)&0x0001)+((ChannelTags[i]>>6)&0x0001)+((ChannelTags[i]>>7)&0x0001))>1){
TimeTaggsDetAnalytics[4]=(double)TimeTaggsDetAnalytics[4]+1.0;
}
if (i>0){//Compute the mean value
TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*((double)(TimeTaggs[i]-TimeTaggs[i-1]));

//// Debugging
//if ((TimeTaggs[i]-TimeTaggs[i-1])>100){
//cout << "TimeTaggs[i]: " << TimeTaggs[i] << endl;
//cout << "TimeTaggs[i-1]: " << TimeTaggs[i-1] << endl;
//}

}
}

for (int i=1;i<SimulateNumStoredQubitsNodeAux;i++){
if (i>0){TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*pow((double)(TimeTaggs[i]-TimeTaggs[i-1])-TimeTaggsDetAnalytics[5],2.0);}
}
TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]);
TimeTaggsDetAnalytics[7]=(double)(TimeTaggs[0]);// Timetag of the first capture
}
else{
TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;
}
//cout << "TimeTaggsDetAnalytics[0]: " << TimeTaggsDetAnalytics[0] << endl;
//cout << "TimeTaggsDetAnalytics[1]: " << TimeTaggsDetAnalytics[1] << endl;
//cout << "TimeTaggsDetAnalytics[2]: " << TimeTaggsDetAnalytics[2] << endl;
//cout << "TimeTaggsDetAnalytics[3]: " << TimeTaggsDetAnalytics[3] << endl;
//cout << "TimeTaggsDetAnalytics[4]: " << TimeTaggsDetAnalytics[4] << endl;
//cout << "TimeTaggsDetAnalytics[5]: " << TimeTaggsDetAnalytics[5] << endl;
//cout << "TimeTaggsDetAnalytics[6]: " << TimeTaggsDetAnalytics[6] << endl;
//cout << "TimeTaggsDetAnalytics[7]: " << TimeTaggsDetAnalytics[7] << endl;
// Particular analysis
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Part to analyze if there is absolute synch between clocks with channel 1 and and histogram periodic signals of 4 steps (ch1, ch2, ch3, ch4).
// Accordingly a complete sycle has 8 counts (2 counts for each step)
// Accordingly, the mean wrapped count difference is stored in TimeTaggsDetAnalytics[5]
// Accordingly, the std wrapped count difference is stored in TimeTaggsDetAnalytics[6]
cout << "TIMETAGGING ANALYSIS of QphysLayerAgent.h" << endl;
cout << "When not using hist analysis, it can be changed back to 2048 NUMRECORDS in PRUassTaggDetScript.p, GPIO.h and top of QphysLayerAgent.h" << endl;
cout << "It has to be used PRUassTrigSigScriptHist4Sig in PRU1" << endl;
cout << "It has to have connected only ch1 timetagger" << endl;
cout << "Attention TimeTaggsDetAnalytics[5] stores the mean wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[6] stores the std wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[7] stores the syntethically corrected first timetagg" << endl;
cout << "In GPIO it can be increased NumberRepetitionsSignal when deactivating this hist. analysis" << endl;
if (SimulateNumStoredQubitsNodeAux>0){
unsigned long long int HistPeriodicityAux=4096; // Periodicity in number of PRU counts
unsigned long long int TimeTaggs0Aux=TimeTaggs[0];
unsigned long long int TimeTaggsLastAux=TimeTaggs[SimulateNumStoredQubitsNodeAux-1];
//for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){//// To have synchronisms in between inter captures. For long range synch testing with histogram, this could be commented
//TimeTaggs[i]=TimeTaggs[i]-TimeTaggs0Aux+OldLastTimeTagg+HistPeriodicityAux;
//}

TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;

TimeTaggsDetAnalytics[7]=(double)(TimeTaggs[0]);

TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
for (int i=0;i<(SimulateNumStoredQubitsNodeAux-1);i++){
if (i==0){cout << "TimeTaggs[1]-TimeTaggs[0]: " << TimeTaggs[1]-TimeTaggs[0] << endl;}
else if(i==(SimulateNumStoredQubitsNodeAux-2)){cout << "TimeTaggs[i+1]-TimeTaggs[i]: " << TimeTaggs[i+1]-TimeTaggs[i] << endl;}

TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*(((double)((HistPeriodicityAux/2+TimeTaggs[i+1]-TimeTaggs[i])%(HistPeriodicityAux)))-(double)(HistPeriodicityAux/2));
}

for (int i=0;i<(SimulateNumStoredQubitsNodeAux-1);i++){
TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*pow(((double)((HistPeriodicityAux/2+TimeTaggs[i+1]-TimeTaggs[i])%(HistPeriodicityAux)))-(double)(HistPeriodicityAux/2)-TimeTaggsDetAnalytics[5],2.0);
}
TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]);

//OldLastTimeTagg=TimeTaggsLastAux;// Update value

//cout << "Offset corrected TimeTaggs[0]: " << TimeTaggs[0] << endl;
//cout << "Offset corrected TimeTaggs[1]: " << TimeTaggs[1] << endl;
//cout << "Offset corrected TimeTaggs[2]: " << TimeTaggs[2] << endl;
//cout << "Offset corrected TimeTaggs[3]: " << TimeTaggs[3] << endl;

}
else{
TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;
}
////////////////////////////////
// Particular analysis
/*
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Compute interesting analytics on the COINCIDENCE Timetaggs and detection so that not all data has to be transfered through sockets
// It has to have double precision so that statistics are useful
// Param 0: Num detections channel 1
// Param 1: Num detections channel 2
// Param 2: Num detections channel 3
// Param 3: Num detections channel 4
// Param 4: Multidetection events
// Param 5: Mean time difference between coincidences tags
// Param 6: std time difference between coincidences tags
// Param 7: time value first coincidence tags
TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;
if (SimulateNumStoredQubitsNodeAux>0){
int iIterCoincidence=0;
unsigned long long int TimeCoincidenceTaggs[NumQubitsMemoryBuffer]={0}; // Coincidence Timetaggs of the detections

for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
//cout << "TimeTaggs[i]: "<< TimeTaggs[i] << endl;
//cout << "ChannelTags[i]: "<< std::bitset<8>(ChannelTags[i]) << endl;
if (ChannelTags[i]&0x0001==1 and (ChannelTags[i]>>4)&0x0001==1){TimeTaggsDetAnalytics[0]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}
if ((ChannelTags[i]>>1)&0x0001==1 and (ChannelTags[i]>>5)&0x0001==1){TimeTaggsDetAnalytics[1]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}
if ((ChannelTags[i]>>2)&0x0001==1 and (ChannelTags[i]>>6)&0x0001==1){TimeTaggsDetAnalytics[2]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}
if ((ChannelTags[i]>>3)&0x0001==1 and (ChannelTags[i]>>7)&0x0001==1){TimeTaggsDetAnalytics[3]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}

if (((ChannelTags[i]&0x0001)+((ChannelTags[i]>>1)&0x0001)+((ChannelTags[i]>>2)&0x0001)+((ChannelTags[i]>>3)&0x0001)+((ChannelTags[i]>>4)&0x0001)+((ChannelTags[i]>>5)&0x0001)+((ChannelTags[i]>>6)&0x0001)+((ChannelTags[i]>>7)&0x0001))>1){
TimeTaggsDetAnalytics[4]=(double)TimeTaggsDetAnalytics[4]+1.0;
}


}// end of for
int NumCoincidences=iIterCoincidence;

for (int iIterCoincidence=1;iIterCoincidence<NumCoincidences;iIterCoincidence++){
if (iIterCoincidence>0){//Compute the mean value for coincidences
TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(1.0/((double)NumCoincidences-1.0))*((double)(TimeCoincidenceTaggs[iIterCoincidence]-TimeCoincidenceTaggs[iIterCoincidence-1]));

//// Debugging
//if ((TimeCoincidenceTaggs[iIterCoincidence]-TimeCoincidenceTaggs[iIterCoincidence-1])>100){
//cout << "TimeCoincidenceTaggs[iIterCoincidence]: " << TimeCoincidenceTaggs[iIterCoincidence] << endl;
//cout << "TimeCoincidenceTaggs[iIterCoincidence-1]: " << TimeCoincidenceTaggs[iIterCoincidence-1] << endl;
//}

}
}

for (int iIterCoincidence=1;iIterCoincidence<NumCoincidences;iIterCoincidence++){
if (iIterCoincidence>0){TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+(1.0/((double)NumCoincidences-1.0))*pow((double)(TimeCoincidenceTaggs[iIterCoincidence]-TimeCoincidenceTaggs[iIterCoincidence-1])-TimeTaggsDetAnalytics[5],2.0);}
}
TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]);
TimeTaggsDetAnalytics[7]=(double)(TimeCoincidenceTaggs[0]);// Timetag of the first coincidence capture

//cout << "TimeTaggsDetAnalytics[0]: " << TimeTaggsDetAnalytics[0] << endl;
//cout << "TimeTaggsDetAnalytics[1]: " << TimeTaggsDetAnalytics[1] << endl;
//cout << "TimeTaggsDetAnalytics[2]: " << TimeTaggsDetAnalytics[2] << endl;
//cout << "TimeTaggsDetAnalytics[3]: " << TimeTaggsDetAnalytics[3] << endl;
//cout << "TimeTaggsDetAnalytics[4]: " << TimeTaggsDetAnalytics[4] << endl;
//cout << "TimeTaggsDetAnalytics[5]: " << TimeTaggsDetAnalytics[5] << endl;
//cout << "TimeTaggsDetAnalytics[6]: " << TimeTaggsDetAnalytics[6] << endl;
//cout << "TimeTaggsDetAnalytics[7]: " << TimeTaggsDetAnalytics[7] << endl;

/// Part to analyze if there is absolute synch between clocks with channel 1 and and histogram periodic signals of 4 steps (ch1, ch2, ch3, ch4).
// Accordingly a complete sycle has 8 counts (2 counts for each step)
// Accordingly, the mean wrapped count difference is stored in TimeTaggsDetAnalytics[5]
// Accordingly, the std wrapped count difference is stored in TimeTaggsDetAnalytics[6]
cout << "COINCIDENCE ANALYSIS of QphysLayerAgent.h" << endl;
cout << "When not using hist analysis, it can be changed back to 2048 NUMRECORDS in PRUassTaggDetScript.p, GPIO.h and top of QphysLayerAgent.h" << endl;
cout << "It has to be used PRUassTrigSigScriptHist4Sig in PRU1" << endl;
cout << "It has to have connected only ch1 timetagger" << endl;
cout << "Attention TimeTaggsDetAnalytics[5] stores the mean wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[6] stores the std wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[7] stores the syntethically corrected first timetagg" << endl;
cout << "In GPIO it can be increased NumberRepetitionsSignal when deactivating this hist. analysis" << endl;
unsigned long long int HistPeriodicityAux=4096; // Periodicity in number of PRU counts
unsigned long long int TimeTaggs0Aux=TimeTaggs[0];
unsigned long long int TimeTaggsLastAux=TimeTaggs[SimulateNumStoredQubitsNodeAux-1];
//for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){//// To have synchronisms in between inter captures. For long range synch testing with histogram, this could be commented
//TimeTaggs[i]=TimeTaggs[i]-TimeTaggs0Aux+OldLastTimeTagg+HistPeriodicityAux;
//}

TimeTaggsDetAnalytics[7]=(double)(TimeCoincidenceTaggs[0]);

TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
for (int iIterCoincidence=0;iIterCoincidence<(NumCoincidences-1);iIterCoincidence++){
if (iIterCoincidence==0){cout << "TimeCoincidenceTaggs[1]-TimeCoincidenceTaggs[0]: " << TimeCoincidenceTaggs[1]-TimeCoincidenceTaggs[0] << endl;}
else if(iIterCoincidence==(NumCoincidences-2)){cout << "TimeTaggs[iIterCoincidence+1]-TimeTaggs[iIterCoincidence]: " << TimeCoincidenceTaggs[iIterCoincidence+1]-TimeCoincidenceTaggs[iIterCoincidence] << endl;}

TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(1.0/((double)NumCoincidences-1.0))*(((double)((HistPeriodicityAux/2+TimeCoincidenceTaggs[iIterCoincidence+1]-TimeCoincidenceTaggs[iIterCoincidence])%(HistPeriodicityAux)))-(double)(HistPeriodicityAux/2));
}

for (int i=0;i<(SimulateNumStoredQubitsNodeAux-1);i++){
TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+(1.0/((double)NumCoincidences-1.0))*pow(((double)((HistPeriodicityAux/2+TimeCoincidenceTaggs[iIterCoincidence+1]-TimeCoincidenceTaggs[iIterCoincidence])%(HistPeriodicityAux)))-(double)(HistPeriodicityAux/2)-TimeTaggsDetAnalytics[5],2.0);
}
TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]);

//OldLastTimeTagg=TimeTaggsLastAux;// Update value

//cout << "Offset corrected TimeCoincidenceTaggs[0]: " << TimeCoincidenceTaggs[0] << endl;
//cout << "Offset corrected TimeCoincidenceTaggs[1]: " << TimeCoincidenceTaggs[1] << endl;
//cout << "Offset corrected TimeCoincidenceTaggs[2]: " << TimeCoincidenceTaggs[2] << endl;
//cout << "Offset corrected TimeCoincidenceTaggs[3]: " << TimeCoincidenceTaggs[3] << endl;

}
else{
TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;
}
////////////////////////////////
*/

this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
this->release();

return SimulateNumStoredQubitsNodeAux;
}

int QPLA::RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep){
struct timespec ts;
ts.tv_sec=(int)(TimeNanoSecondsSleep/((long)1000000000));
ts.tv_nsec=(long)(TimeNanoSecondsSleep%(long)1000000000);
clock_nanosleep(CLOCK_TAI, 0, &ts, NULL); //

return 0; // All ok
}

QPLA::~QPLA() {
// destructor
/* Very slow GPIO BBB not used anymore
outGPIO->streamOutClose();
inGPIO->streamInClose();
*/
//delete PRUGPIO; // Destructor for the PRUGPIO instance
this->threadRef.join();// Terminate the process thread
this->PRUGPIO.~GPIO(); // Destruct the instance of the below layer
}

int QPLA::NegotiateInitialParamsNode(){
try{
 this->acquire();

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
        this->RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
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


