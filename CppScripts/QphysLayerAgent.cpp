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
// Threads
#include <thread>
// Semaphore
#include <atomic>
// time points
#define WaitTimeToFutureTimePoint 399000000 // Max 999999999. It is the time barrier to try to achieve synchronization. Considered nanoseconds (it can be changed on the transformatoin used)
#define UTCoffsetBarrierErrorThreshold 37000000000 // Some BBB when synch with linuxPTP have an error on the UTC offset with respect TAI. Remove this sistemic offset and announce it!
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
	// Initialize some arrays at the beggining
	int CombinationLinksNumAux=static_cast<unsigned int>(1ULL<<LinkNumberMAX-1);
	for (int i=0;i<CombinationLinksNumAux;i++){
		SmallOffsetDriftPerLink[i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		ReferencePointSmallOffsetDriftPerLink[i]=0.0; // Identified by each link, annotate the first time offset that all other acquisitions should match to, so an offset with respect the SignalPeriod histogram
		// Filtering qubits
		NonInitialReferencePointSmallOffsetDriftPerLink[i]=false; // Identified by each link, annotate if the first capture has been done and hence the initial ReferencePoint has been stored
	}
	
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

int QPLA::countUnderscores(char* ParamsCharArray) {
  int underscoreCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == '_') {
      underscoreCount++;
    }
  }

  return underscoreCount;
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
char charNum[NumBytesPayloadBuffer] = {0};

int numUnderScores=countUnderscores(this->IPaddressesTimePointBarrier); // Which means the number of IP addresses to send the Time Point barrier
char IPaddressesTimePointBarrierAux[NumBytesBufferICPMAX]={0}; // Copy to not destroy original
strcpy(IPaddressesTimePointBarrierAux,IPaddressesTimePointBarrier);
for (int iIterIPaddr=0;iIterIPaddr<numUnderScores;iIterIPaddr++){// Iterate over the different nodes to tell
	// Mount the Parameters message for the other node
	if (iIterIPaddr==0){
		strcpy(ParamsCharArray,"IPdest_");// Initiates the ParamsCharArray, so use strcpy
		strcat(ParamsCharArray,strtok(IPaddressesTimePointBarrierAux,"_"));// Indicate the address to send the Future time Point
	} 
	else{
		strcat(ParamsCharArray,"IPdest_");// Continues the ParamsCharArray, so use strcat
		strcat(ParamsCharArray,strtok(NULL,"_"));// Indicate the address to send the Future time Point
	}
	strcat(ParamsCharArray,"_");// Add underscore separator
	strcat(ParamsCharArray,"OtherClientNodeFutureTimePoint_"); // Continues the ParamsCharArray, so use strcat		 
	sprintf(charNum, "%llu", TimePointFuture_time_as_count);//%llu: unsigned long long int
	strcat(ParamsCharArray,charNum);
	strcat(ParamsCharArray,"_"); // Final _
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
int MaxWhileRound=1500;// Amount of check to receive the other node Time Point Barrier
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

int QPLA::PurgeExtraordinaryTimePointsNodes(){
int MaxWhileRound=1500;// Amount of check to receive the other node Time Point Barrier
// Wait to receive the FutureTimePoint from other node
this->acquire();
while(MaxWhileRound>0){// Make sure to purge remaining Time Points from other nodes
	this->release();
	MaxWhileRound--;	
	this->RelativeNanoSleepWait((unsigned int)(0.1*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//Maybe some sleep to reduce CPU consumption	
	this->acquire();
};
// Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
return 0; // All ok
}

int QPLA::SimulateEmitQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,double* FineSynchAdjValAux){
this->acquire();
strcpy(this->ModeActivePassive,ModeActivePassiveAux);
strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
this->RetrieveOtherEmiterReceiverMethod();
strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
this->numReqQuBits=numReqQuBitsAux;
// Adjust the network synchronization values
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=CurrentSynchNetworkParamsLink[0]+FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]=CurrentSynchNetworkParamsLink[1]+FineSynchAdjValAux[1];// synch trig frequency
}
//cout << "this->FineSynchAdjVal[1]: " << this->FineSynchAdjVal[1] << endl;
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

int QPLA::SimulateEmitSynchQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass){
this->acquire();
if (iCenterMass==0 and iNumRunsPerCenterMass==0){
strcpy(this->ModeActivePassive,ModeActivePassiveAux);
strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
this->RetrieveOtherEmiterReceiverMethod();
strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
//this->NumRunsPerCenterMass=NumRunsPerCenterMassAux; hardcoded value
this->FreqSynchNormValuesArray[0]=FreqSynchNormValuesArrayAux[0];// first test frequency norm.
this->FreqSynchNormValuesArray[1]=FreqSynchNormValuesArrayAux[1];// second test frequency norm.
this->FreqSynchNormValuesArray[2]=FreqSynchNormValuesArrayAux[2];// third test frequency norm.
//cout << "this->FineSynchAdjVal[1]: " << this->FineSynchAdjVal[1] << endl;
// Remove previous synch values - probably not for the emitter (since calculation for synch values are done as receiver)
}
// Here run the several iterations with different testing frequencies
// Adjust the network synchronization values
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=CurrentSynchNetworkParamsLink[0]+FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]=CurrentSynchNetworkParamsLink[1]+FineSynchAdjValAux[1]+FreqSynchNormValuesArrayAux[iCenterMass];// synch trig frequency
}		
if (this->RunThreadSimulateEmitQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateEmitQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateEmitQuBitRefAux=std::thread(&QPLA::ThreadSimulateEmitQuBit,this);
threadSimulateEmitQuBitRefAux.join();//threadSimulateEmitQuBitRefAux.detach();//threadSimulateEmitQuBitRefAux.join();//
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
	this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// For sure we can clear OtherClientNodeFutureTimePoint, just to ensure resilence
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
 this->FutureTimePoint=this->FutureTimePoint+std::chrono::nanoseconds(TimePointMarginGPIOTrigTagQubits);// Give some margin so that ReadTimeStamps and coincide in the respective methods of GPIO
auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Add some margin 

PRUGPIO.SendTriggerSignals(this->FineSynchAdjVal,TimePointFuture_time_as_count);//PRUGPIO->SendTriggerSignals(); // It is long enough emitting sufficient qubits for the receiver to get the minimum amount of multiples of NumQuBitsPerRun
 
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
this->PurgeExtraordinaryTimePointsNodes();
this->RunThreadSimulateEmitQuBitFlag=true;//enable again that this thread can again be called. It is okey since it entered a block semaphore part and then no other sempahored part will run until this one finishes. At the same time, returning to true at this point allows the read to not go out of scope and losing this flag parameter
 this->release();
 
 //cout << "Qubit emitted" << endl;
cout << "End Emiting Qubits" << endl;

 return 0; // return 0 is for no error
}

int QPLA::SimulateReceiveQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux, char* IPaddressesAux,int numReqQuBitsAux,double* FineSynchAdjValAux){
this->acquire();
strcpy(this->ModeActivePassive,ModeActivePassiveAux);
strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
this->RetrieveOtherEmiterReceiverMethod();
//cout << "LinkIdentificationArray[0]: " << LinkIdentificationArray[0] << endl;
//cout << "LinkIdentificationArray[1]: " << LinkIdentificationArray[1] << endl;
///
strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
this->numReqQuBits=numReqQuBitsAux;
// Adjust the network synchronization values
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=CurrentSynchNetworkParamsLink[0]+FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]=CurrentSynchNetworkParamsLink[1]+FineSynchAdjValAux[1];// synch trig frequency
}
if (this->RunThreadSimulateReceiveQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateReceiveQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateReceiveQuBitRefAux=std::thread(&QPLA::ThreadSimulateReceiveQubit,this);
threadSimulateReceiveQuBitRefAux.join();//threadSimulateReceiveQuBitRefAux.detach();
this->SmallDriftContinuousCorrection();// Run after threadSimulateReceiveQuBitRefAux
}
else{
cout << "Not possible to launch ThreadSimulateReceiveQubit" << endl;
}
this->release();
return 0; // return 0 is for no error
}

int QPLA::SimulateReceiveSynchQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux, char* IPaddressesAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass){
this->acquire();
if (iCenterMass==0 and iNumRunsPerCenterMass==0){
	strcpy(this->ModeActivePassive,ModeActivePassiveAux);
	strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
	this->RetrieveOtherEmiterReceiverMethod();
	strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);				
	//this->NumRunsPerCenterMass=NumRunsPerCenterMassAux; hardcoded value
	this->FreqSynchNormValuesArray[0]=FreqSynchNormValuesArrayAux[0];// first test frequency norm.
	this->FreqSynchNormValuesArray[1]=FreqSynchNormValuesArrayAux[1];// second test frequency norm.
	this->FreqSynchNormValuesArray[2]=FreqSynchNormValuesArrayAux[2];// third test frequency norm.
	// Reset previous synch values to zero - Maybe comment it in order to do an iterative algorithm
	//SynchCalcValuesArray[0]=0.0;
	//SynchCalcValuesArray[1]=0.0;
	//SynchCalcValuesArray[2]=0.0;
	//double SynchParamValuesArrayAux[2];
	//SynchParamValuesArrayAux[0]=SynchCalcValuesArray[1];
	//SynchParamValuesArrayAux[1]=SynchCalcValuesArray[2];
	//PRUGPIO.SetSynchDriftParams(SynchParamValuesArrayAux);// Reset computed values to the agent below
}
// Here run the several iterations with different testing frequencies
// Adjust the network synchronization values
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=CurrentSynchNetworkParamsLink[0]+FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]=CurrentSynchNetworkParamsLink[1]+FineSynchAdjValAux[1];// synch trig frequency
}
if (this->RunThreadSimulateReceiveQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateReceiveQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateReceiveQuBitRefAux=std::thread(&QPLA::ThreadSimulateReceiveQubit,this);
threadSimulateReceiveQuBitRefAux.join();//threadSimulateReceiveQuBitRefAux.detach();
}
else{
cout << "Not possible to launch ThreadSimulateReceiveQubit" << endl;
}		
this->HistCalcPeriodTimeTags(iCenterMass,iNumRunsPerCenterMass);// Compute synch values

if (iCenterMass==(NumCalcCenterMass-1) and iNumRunsPerCenterMass==(NumRunsPerCenterMass-1)){
// Check if nan values, then convert them to 0 and inform thorugh the terminal
if (std::isnan(SynchCalcValuesArray[0])){
SynchCalcValuesArray[0]=0.0;
cout << "Attention QPLA HistCalcPeriodTimeTags nan values!!!" << endl;
}
if (std::isnan(SynchCalcValuesArray[1])){
SynchCalcValuesArray[1]=0.0;
cout << "Attention QPLA HistCalcPeriodTimeTags nan values!!!" << endl;
}
if (std::isnan(SynchCalcValuesArray[2])){
SynchCalcValuesArray[2]=0.0;
cout << "Attention QPLA HistCalcPeriodTimeTags nan values!!!" << endl;
}

// Update absolute values - The actual values applied and show in the Host
SynchCalcValuesAbsArray[0]=0.0*SynchCalcValuesAbsArray[0]+SynchCalcValuesArray[0];
SynchCalcValuesAbsArray[1]=SynchCalcValuesAbsArray[1]+SynchCalcValuesArray[1];
SynchCalcValuesAbsArray[2]=SynchCalcValuesAbsArray[2]+SynchCalcValuesArray[2];

//cout << "QPLA::SynchCalcValuesAbsArray[0]: " << SynchCalcValuesAbsArray[0] << endl;
//cout << "QPLA::SynchCalcValuesAbsArray[1]: " << SynchCalcValuesAbsArray[1] << endl;
//cout << "QPLA::SynchCalcValuesAbsArray[2]: " << SynchCalcValuesAbsArray[2] << endl;
	
// Update relative iterative values - Done for each Sending or Reading dependent on the specific link
//double SynchParamValuesArrayAux[2];
// The order below is not correct - debbug the protocol
//SynchParamValuesArrayAux[0]=SynchCalcValuesArray[2];// relative frequency correction
//SynchParamValuesArrayAux[1]=SynchCalcValuesArray[1];// offset correction - dependent on link
//PRUGPIO.SetSynchDriftParams(SynchParamValuesArrayAux);// Update computed values to the agent below
cout << "QPLA::Synchronization parameters updated for this node" << endl;
}
this->release();

return 0; // return 0 is for no error
}

unsigned long long int QPLA::IPtoNumber(char* IPaddressAux){
unsigned long long int IPnumberAux=0;
unsigned long long int OctetNumAux=0;
char IPaddressAuxHolder[IPcharArrayLengthMAX];
strcpy(IPaddressAuxHolder,IPaddressAux); // to not destroy the information
for (int i=0;i<4;i++){
	if (i==0){OctetNumAux=static_cast<unsigned long long int>(atoi(strtok(IPaddressAuxHolder,".")));}
	else{OctetNumAux=static_cast<unsigned long long int>(atoi(strtok(NULL,".")));}
	IPnumberAux+=OctetNumAux<<(8*(4-i-1));
}

return IPnumberAux;
}

int QPLA::RetrieveOtherEmiterReceiverMethod(){// Stores and retrieves the current other emiter receiver
// Store the potential IP identification of the emitters (for Receive) and receivers for (Emit) - in order to identify the link
CurrentSpecificLink=-1;// Reset value
numSpecificLinkmatches=0; // Reset value
int numCurrentEmitReceiveIP=countUnderscores(this->CurrentEmitReceiveIP); // Which means the number of IP addresses that currently will send/receive qubits
char CurrentEmitReceiveIPAuxAux[NumBytesBufferICPMAX]={0}; // Copy to not destroy original
strcpy(CurrentEmitReceiveIPAuxAux,CurrentEmitReceiveIP);
char SpecificCurrentEmitReceiveIPAuxAux[IPcharArrayLengthMAX]={0};
for (int j=0;j<numCurrentEmitReceiveIP;j++){
	if (j==0){strcpy(SpecificCurrentEmitReceiveIPAuxAux,strtok(CurrentEmitReceiveIPAuxAux,"_"));}
	else{strcpy(SpecificCurrentEmitReceiveIPAuxAux,strtok(NULL,"_"));}
	for (int i=0;i<CurrentNumIdentifiedEmitReceiveIP;i++){
		if (string(LinkIdentificationArray[i])==string(SpecificCurrentEmitReceiveIPAuxAux)){// IP already present
			CurrentSpecificLink=i;
			CurrentSpecificLinkMultipleIndices[numSpecificLinkmatches]=i; // For multiple links at the same time
			numSpecificLinkmatches++;
		}
	}
	if (CurrentSpecificLink<0){// Not previously identified, so stored them if possible
		if ((CurrentNumIdentifiedEmitReceiveIP+1)<=LinkNumberMAX){
			strcpy(LinkIdentificationArray[CurrentNumIdentifiedEmitReceiveIP],SpecificCurrentEmitReceiveIPAuxAux);// Update value
			CurrentSpecificLink=CurrentNumIdentifiedEmitReceiveIP;
			CurrentNumIdentifiedEmitReceiveIP++;
		}
		else{// Mal function we should not be here
			cout << "QPLA::Number of identified emitters/receivers to this node has exceeded the expected value!!!" << endl;
		}
	}	
}

// Develop for multiple links
//if (numSpecificLinkmatches>1){// For the time being only implemented for one-to-one link (otherwise it has to be develop...)
	//	cout << "QPLA::Multiple emitter/receivers nodes identified, so develop to correct small offset drift for each specific link...to be develop!!!" << endl;
	//}

// For holding multiple IP links
// First always order the list of IPs involved (which are separated by "_")
char ListUnOrderedCurrentEmitReceiveIP[NumBytesBufferICPMAX];
char ListSeparatedUnOrderedCurrentEmitReceiveIP[numCurrentEmitReceiveIP][IPcharArrayLengthMAX];
strcpy(ListUnOrderedCurrentEmitReceiveIP,this->CurrentEmitReceiveIP);// To not destroy array
unsigned long long int ListUnOrderedIPnum[numCurrentEmitReceiveIP]; // Declaration
unsigned long long int ListOrderedIPnum[numCurrentEmitReceiveIP]; // Declaration
for (int i=0;i<numCurrentEmitReceiveIP;i++){
	if (i==0){
		strcpy(ListSeparatedUnOrderedCurrentEmitReceiveIP[i],strtok(ListUnOrderedCurrentEmitReceiveIP,"_"));		
	}
	else{
		strcpy(ListSeparatedUnOrderedCurrentEmitReceiveIP[i],strtok(NULL,"_"));
	}
	ListUnOrderedIPnum[i]=this->IPtoNumber(ListSeparatedUnOrderedCurrentEmitReceiveIP[i]);
	ListOrderedIPnum[i]=ListUnOrderedIPnum[i];// Just copy them
}

this->ULLIBubbleSort(ListOrderedIPnum,numCurrentEmitReceiveIP); // Order the numbers

char ListOrderedCurrentEmitReceiveIP[NumBytesBufferICPMAX];
int jIndexMatch=0;// Reset
for (int i=0;i<numCurrentEmitReceiveIP;i++){
	jIndexMatch=0;// Reset
	for (int j=0;j<numCurrentEmitReceiveIP;j++){// Search the index matching
		if (ListOrderedIPnum[i]==ListUnOrderedIPnum[j]){jIndexMatch=j;}
	}
	if (i==0){
		strcpy(ListOrderedCurrentEmitReceiveIP,ListSeparatedUnOrderedCurrentEmitReceiveIP[jIndexMatch]);
	}
	else{
		strcat(ListOrderedCurrentEmitReceiveIP,ListSeparatedUnOrderedCurrentEmitReceiveIP[jIndexMatch]);
	}
	strcat(ListOrderedCurrentEmitReceiveIP,"_");// Separator
}

CurrentSpecificLinkMultiple=-1;// Reset value
// Then check if this entry exists
for (int i=0;i<CurrentNumIdentifiedMultipleIP;i++){
	if (string(ListCombinationSpecificLink[i])==string(ListOrderedCurrentEmitReceiveIP)){
		CurrentSpecificLinkMultiple=i;
	}

}
// If exists, just return the index identifying it; if it does not exists store it and return the index identifying it
int CombinationLinksNumAux=static_cast<unsigned int>(1ULL<<LinkNumberMAX-1);
if (CurrentSpecificLinkMultiple<0){
	if ((CurrentNumIdentifiedMultipleIP+1)<=CombinationLinksNumAux){
		strcpy(ListCombinationSpecificLink[CurrentNumIdentifiedMultipleIP],ListOrderedCurrentEmitReceiveIP);// Update value
		CurrentSpecificLinkMultiple=CurrentNumIdentifiedMultipleIP;
		CurrentNumIdentifiedMultipleIP++;		
	}
	else{// Mal function we should not be here
		cout << "QPLA::Number of identified multiple combinatoins of emitters/receivers to this node has exceeded the expected value!!!" << endl;
	}
}

// Update the holder values that need to be passed depending on the current link of interest
if (CurrentSpecificLink>=0 and numSpecificLinkmatches==1){
	CurrentSynchNetworkParamsLink[0]=SynchNetworkParamsLink[CurrentSpecificLink][0];
	CurrentSynchNetworkParamsLink[1]=SynchNetworkParamsLink[CurrentSpecificLink][1];
	CurrentSynchNetworkParamsLink[2]=SynchNetworkParamsLink[CurrentSpecificLink][2];
}
else if (CurrentSpecificLink>=0 and numSpecificLinkmatches>1){
	CurrentSynchNetworkParamsLink[0]=0.0; // Reset values
	CurrentSynchNetworkParamsLink[1]=0.0; // Reset values
	CurrentSynchNetworkParamsLink[2]=0.0; // Reset values
	for (int i=0;i<numSpecificLinkmatches; i++){// Use the average of the different involved links
		CurrentSynchNetworkParamsLink[0]+=(1.0/static_cast<double>(numSpecificLinkmatches))*SynchNetworkParamsLink[CurrentSpecificLinkMultipleIndices[i]][0];
		CurrentSynchNetworkParamsLink[1]+=(1.0/static_cast<double>(numSpecificLinkmatches))*SynchNetworkParamsLink[CurrentSpecificLinkMultipleIndices[i]][1];
		CurrentSynchNetworkParamsLink[2]+=(1.0/static_cast<double>(numSpecificLinkmatches))*SynchNetworkParamsLink[CurrentSpecificLinkMultipleIndices[i]][2];
	}
}
else{
	CurrentSynchNetworkParamsLink[0]=0.0;
	CurrentSynchNetworkParamsLink[1]=0.0;
	CurrentSynchNetworkParamsLink[2]=0.0;
}
return 0; // All ok
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
	this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// For sure we can clear OtherClientNodeFutureTimePoint, just to ensure resilence
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
 
 this->FutureTimePoint=this->FutureTimePoint+std::chrono::nanoseconds(TimePointMarginGPIOTrigTagQubits);// Give some margin so that ReadTimeStamps and coincide in the respective methods of GPIO. Only for th einitial run, since the TimeStaps are run once
 auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Add some margin
 for (iIterRuns=0;iIterRuns<DetRunsCount;iIterRuns++){	
	PRUGPIO.ReadTimeStamps(this->FineSynchAdjVal,TimePointFuture_time_as_count);//PRUGPIO->ReadTimeStamps();// Multiple reads can be done in multiples of NumQuBitsPerRun qubit timetags
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

this->LinearRegressionQuBitFilter();// Retrieve raw detected qubits and channel tags
this->PurgeExtraordinaryTimePointsNodes();
this->RunThreadSimulateReceiveQuBitFlag=true;//enable again that this thread can again be called
this->release();
cout << "End Receiving Qubits" << endl;

return 0; // return 0 is for no error
}

bool QPLA::GetGPIOHardwareSynchedNode(){
bool GPIOHardwareSynchedAux=false;
this->acquire();
GPIOHardwareSynchedAux=GPIOHardwareSynched;// Update value
this->release();
return GPIOHardwareSynchedAux;
}

int QPLA::SmallDriftContinuousCorrection(){

int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];// Number of qubits to process

// Check that we now exceed the QuBits buffer size
if (SimulateNumStoredQubitsNodeAux>NumQubitsMemoryBuffer){SimulateNumStoredQubitsNodeAux=NumQubitsMemoryBuffer;}
else if (SimulateNumStoredQubitsNodeAux==1){
cout << "QPLA::SmallDriftContinuousCorrection not executing since 0 qubits detected!" << endl;
return 0;
}

// Apply the small offset drift correction
if (ApplyProcQubitsSmallTimeOffsetContinuousCorrection==true){	
	if (CurrentSpecificLinkMultiple>-1){// The specific identification IP is present
		// If it is the first time, annotate the relative time offset with respect HostPeriodicityAux
		ReferencePointSmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]=0.0;// Reset value. ReferencePointSmallOffset could be used to allocate multiple channels separated by time
		if (NonInitialReferencePointSmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]==false){			
			SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]=0.0;// Reset value
			NonInitialReferencePointSmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]=true;// Update value, so that it is not run again
		}	
		// First compute the relative new time offset from last iteration
		double SmallOffsetDriftAux=0.0;
		long double ldSmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink=static_cast<long double>(SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]+ReferencePointSmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]);
		if (UseAllTagsForEstimation){
			double SmallOffsetDriftArrayAux[SimulateNumStoredQubitsNodeAux]={0.0};			
			for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
				// Mean averaging, not very resilent with glitches, eventhough filtered in liner regression
				//SmallOffsetDriftAux+=static_cast<double>(fmodl(HistPeriodicityAux/2.0+static_cast<long double>(TimeTaggs[i])-static_cast<long double>(SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]+ReferencePointSmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]),HistPeriodicityAux)-HistPeriodicityAux/2.0)/static_cast<double>(SimulateNumStoredQubitsNodeAux);//static_cast<double>((TimeTaggs[i]-static_cast<unsigned long long int>(SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]+ReferencePointSmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]))%HistPeriodicityAux)/static_cast<double>(SimulateNumStoredQubitsNodeAux);
				// Median averaging
				SmallOffsetDriftArrayAux[i]=static_cast<double>(fmodl(HistPeriodicityAux/2.0+static_cast<long double>(TimeTaggs[i])-ldSmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink,HistPeriodicityAux)-HistPeriodicityAux/2.0);
			}
			SmallOffsetDriftAux=DoubleMedianFilterSubArray(SmallOffsetDriftArrayAux,SimulateNumStoredQubitsNodeAux); // Median averaging
		}
		else{
			SmallOffsetDriftAux=static_cast<double>(fmodl(HistPeriodicityAux/2.0+static_cast<long double>(TimeTaggs[0])-ldSmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink,HistPeriodicityAux)-HistPeriodicityAux/2.0);
			cout << "QPLA::Using only first timetag for small offset correction!...to be deactivated" << endl;
		}
		
		if (abs(SmallOffsetDriftAux)>(HistPeriodicityAux/4.0)){// Large step
			cout << "QPLA::Large small offset drift encountered SmallOffsetDriftAux " << SmallOffsetDriftAux << ". Potentially lost ABSOLUTE temporal track of timetaggs from previous runs!!!" << endl;
		}
		// Update new value, just for monitoring of the wander
		SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]+=SmallOffsetDriftAux;
		
		//cout << "QPLA::Applying SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple] " << SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple] << endl;
		//cout << "QPLA::Applying SmallOffsetDriftAux " << SmallOffsetDriftAux << endl;
		
		long long int LLISmallOffsetDriftPerLinkCurrentSpecificLink=static_cast<long long int>(SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple]);
		for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
			TimeTaggs[i]=static_cast<unsigned long long int>(static_cast<long long int>(TimeTaggs[i])-LLISmallOffsetDriftPerLinkCurrentSpecificLink);
		}
	}
	else{// Mal function we should not be here
		cout << "QPLA::The Emitter nodes have not been previously identified, so no small offset drift correction applied" << endl;
	}
}
else
{
	cout << "QPLA::Not applying ApplyProcQubitsSmallTimeOffsetContinuousCorrection small drift offset correction...to be activated..." << endl;
}

return 0; // All Ok
}

int QPLA::GetSimulateNumStoredQubitsNode(double* TimeTaggsDetAnalytics){
this->acquire();
while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
this->RunThreadAcquireSimulateNumStoredQubitsNode=false;
int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];// Number of qubits to process
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

// Generally mean averaging can be used since outliers (either noise or glitches) have been removed in LinearRegressionQuBitFilter
if (SimulateNumStoredQubitsNodeAux>1){
for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
//cout << "TimeTaggs[i]: "<< TimeTaggs[i] << endl;
//cout << "ChannelTags[i]: "<< std::bitset<8>(ChannelTags[i]) << endl;
if (ChannelTags[i]&0x0001==1 or (ChannelTags[i]>>4)&0x0001==1 or (ChannelTags[i]>>8)&0x0001==1){TimeTaggsDetAnalytics[0]++;}
if ((ChannelTags[i]>>1)&0x0001==1 or (ChannelTags[i]>>5)&0x0001==1 or (ChannelTags[i]>>9)&0x0001==1){TimeTaggsDetAnalytics[1]++;}
if ((ChannelTags[i]>>2)&0x0001==1 or (ChannelTags[i]>>6)&0x0001==1 or (ChannelTags[i]>>10)&0x0001==1){TimeTaggsDetAnalytics[2]++;}
if ((ChannelTags[i]>>3)&0x0001==1 or (ChannelTags[i]>>7)&0x0001==1 or (ChannelTags[i]>>11)&0x0001==1){TimeTaggsDetAnalytics[3]++;}

if (((ChannelTags[i]&0x0001)+((ChannelTags[i]>>1)&0x0001)+((ChannelTags[i]>>2)&0x0001)+((ChannelTags[i]>>3)&0x0001)+((ChannelTags[i]>>4)&0x0001)+((ChannelTags[i]>>5)&0x0001)+((ChannelTags[i]>>6)&0x0001)+((ChannelTags[i]>>7)&0x0001)+((ChannelTags[i]>>8)&0x0001)+((ChannelTags[i]>>9)&0x0001)+((ChannelTags[i]>>10)&0x0001)+((ChannelTags[i]>>11)&0x0001))>1){
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
cout << "When not using hist analysis, it can be changed back to NumQuBitsPerRun NUMRECORDS in PRUassTaggDetScript.p, GPIO.h and top of QphysLayerAgent.h" << endl;
cout << "It has to be used PRUassTrigSigScriptHist4Sig in PRU1" << endl;
cout << "It has to have connected only ch1 timetagger" << endl;
cout << "Attention TimeTaggsDetAnalytics[5] stores the mean wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[6] stores the std wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[7] stores the syntethically corrected first timetagg" << endl;
cout << "In GPIO it can be increased NumberRepetitionsSignal when deactivating this hist. analysis" << endl;
if (SimulateNumStoredQubitsNodeAux>1){
unsigned long long int TimeTaggs0Aux=TimeTaggs[0];
unsigned long long int TimeTaggsLastAux=TimeTaggs[SimulateNumStoredQubitsNodeAux-1];
//for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){//// To have synchronisms in between inter captures. For long range synch testing with histogram, this could be commented
//TimeTaggs[i]=TimeTaggs[i]-TimeTaggs0Aux+OldLastTimeTagg+static_cast<unsigned long long int>(HistPeriodicityAux);
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

TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*(((double)((static_cast<unsigned long long int>(HistPeriodicityAux)/2+TimeTaggs[i+1]-TimeTaggs[i])%(static_cast<unsigned long long int>(HistPeriodicityAux))))-(double)(static_cast<unsigned long long int>(HistPeriodicityAux)/2));
}

for (int i=0;i<(SimulateNumStoredQubitsNodeAux-1);i++){
TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*pow(((double)((static_cast<unsigned long long int>(HistPeriodicityAux)/2+TimeTaggs[i+1]-TimeTaggs[i])%(static_cast<unsigned long long int>(HistPeriodicityAux))))-(double)(static_cast<unsigned long long int>(HistPeriodicityAux)/2)-TimeTaggsDetAnalytics[5],2.0);
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
if (SimulateNumStoredQubitsNodeAux>1){
int iIterCoincidence=0;
unsigned long long int TimeCoincidenceTaggs[NumQubitsMemoryBuffer]={0}; // Coincidence Timetaggs of the detections

for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
//cout << "TimeTaggs[i]: "<< TimeTaggs[i] << endl;
//cout << "ChannelTags[i]: "<< std::bitset<8>(ChannelTags[i]) << endl;
if (ChannelTags[i]&0x0001==1 and (ChannelTags[i]>>4)&0x0001==1){TimeTaggsDetAnalytics[0]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}
if ((ChannelTags[i]>>1)&0x0001==1 and (ChannelTags[i]>>5)&0x0001==1){TimeTaggsDetAnalytics[1]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}
if ((ChannelTags[i]>>2)&0x0001==1 and (ChannelTags[i]>>6)&0x0001==1){TimeTaggsDetAnalytics[2]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}
if ((ChannelTags[i]>>3)&0x0001==1 and (ChannelTags[i]>>7)&0x0001==1){TimeTaggsDetAnalytics[3]++;TimeCoincidenceTaggs[iIterCoincidence]=TimeTaggs[i];iIterCoincidence++;}

if (((ChannelTags[i]&0x0001)+((ChannelTags[i]>>1)&0x0001)+((ChannelTags[i]>>2)&0x0001)+((ChannelTags[i]>>3)&0x0001)+((ChannelTags[i]>>4)&0x0001)+((ChannelTags[i]>>5)&0x0001)+((ChannelTags[i]>>6)&0x0001)+((ChannelTags[i]>>7)&0x0001)+((ChannelTags[i]>>8)&0x0001)+((ChannelTags[i]>>9)&0x0001)+((ChannelTags[i]>>10)&0x0001)+((ChannelTags[i]>>11)&0x0001))>1){
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
cout << "When not using hist analysis, it can be changed back to NumQuBitsPerRun NUMRECORDS in PRUassTaggDetScript.p, GPIO.h and top of QphysLayerAgent.h" << endl;
cout << "It has to be used PRUassTrigSigScriptHist4Sig in PRU1" << endl;
cout << "It has to have connected only ch1 timetagger" << endl;
cout << "Attention TimeTaggsDetAnalytics[5] stores the mean wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[6] stores the std wrap count difference" << endl;
cout << "Attention TimeTaggsDetAnalytics[7] stores the syntethically corrected first timetagg" << endl;
cout << "In GPIO it can be increased NumberRepetitionsSignal when deactivating this hist. analysis" << endl;
unsigned long long int TimeTaggs0Aux=TimeTaggs[0];
unsigned long long int TimeTaggsLastAux=TimeTaggs[SimulateNumStoredQubitsNodeAux-1];
//for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){//// To have synchronisms in between inter captures. For long range synch testing with histogram, this could be commented
//TimeTaggs[i]=TimeTaggs[i]-TimeTaggs0Aux+OldLastTimeTagg+static_cast<unsigned long long int>(HistPeriodicityAux);
//}

TimeTaggsDetAnalytics[7]=(double)(TimeCoincidenceTaggs[0]);

TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
for (int iIterCoincidence=0;iIterCoincidence<(NumCoincidences-1);iIterCoincidence++){
if (iIterCoincidence==0){cout << "TimeCoincidenceTaggs[1]-TimeCoincidenceTaggs[0]: " << TimeCoincidenceTaggs[1]-TimeCoincidenceTaggs[0] << endl;}
else if(iIterCoincidence==(NumCoincidences-2)){cout << "TimeTaggs[iIterCoincidence+1]-TimeTaggs[iIterCoincidence]: " << TimeCoincidenceTaggs[iIterCoincidence+1]-TimeCoincidenceTaggs[iIterCoincidence] << endl;}

TimeTaggsDetAnalytics[5]=TimeTaggsDetAnalytics[5]+(1.0/((double)NumCoincidences-1.0))*(((double)((static_cast<unsigned long long int>(HistPeriodicityAux)/2+TimeCoincidenceTaggs[iIterCoincidence+1]-TimeCoincidenceTaggs[iIterCoincidence])%(static_cast<unsigned long long int>(HistPeriodicityAux))))-(double)(static_cast<unsigned long long int>(HistPeriodicityAux)/2));
}

for (int i=0;i<(SimulateNumStoredQubitsNodeAux-1);i++){
TimeTaggsDetAnalytics[6]=TimeTaggsDetAnalytics[6]+(1.0/((double)NumCoincidences-1.0))*pow(((double)((static_cast<unsigned long long int>(HistPeriodicityAux)/2+TimeCoincidenceTaggs[iIterCoincidence+1]-TimeCoincidenceTaggs[iIterCoincidence])%(static_cast<unsigned long long int>(HistPeriodicityAux))))-(double)(static_cast<unsigned long long int>(HistPeriodicityAux)/2)-TimeTaggsDetAnalytics[5],2.0);
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

int QPLA::GetSimulateSynchParamsNode(double* TimeTaggsDetSynchParams){
this->acquire();
while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
this->RunThreadAcquireSimulateNumStoredQubitsNode=false;

if (CurrentSpecificLink>=0){
TimeTaggsDetSynchParams[0]=CurrentSynchNetworkParamsLink[0];
TimeTaggsDetSynchParams[1]=CurrentSynchNetworkParamsLink[1];
TimeTaggsDetSynchParams[2]=CurrentSynchNetworkParamsLink[2];
}
else{
TimeTaggsDetSynchParams[0]=0.0;
TimeTaggsDetSynchParams[1]=0.0;
TimeTaggsDetSynchParams[2]=0.0;
}

this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
this->release();

return 0; // All ok
}

int QPLA::HistCalcPeriodTimeTags(int iCenterMass,int iNumRunsPerCenterMass){
//this->acquire();
//while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
//this->RunThreadAcquireSimulateNumStoredQubitsNode=false;
int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];

// Check that we not exceed the QuBits buffer size
if (SimulateNumStoredQubitsNodeAux>NumQubitsMemoryBuffer){SimulateNumStoredQubitsNodeAux=NumQubitsMemoryBuffer;}

if (SimulateNumStoredQubitsNodeAux>0){
if (UseAllTagsForEstimation){
	// Mean averaging
	//for (int i=0;i<(SimulateNumStoredQubitsNodeAux);i++){
	//CenterMassVal=CenterMassVal+(1.0/((double)SimulateNumStoredQubitsNodeAux-1.0))*(((double)((static_cast<unsigned long long int>(HistPeriodicityAux)/2+TimeTaggs[i])%(static_cast<unsigned long long int>(HistPeriodicityAux))))-(double)(static_cast<unsigned long long int>(HistPeriodicityAux)/2));
	//}
	// Median averaging
	long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);
	for (int i=0;i<SimulateNumStoredQubitsNodeAux;i++){
		SynchFirstTagsArrayAux[i]=TimeTaggs[i]%LLIHistPeriodicityAux;
	}
	SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass]=LLIMedianFilterSubArray(SynchFirstTagsArrayAux,SimulateNumStoredQubitsNodeAux);
}
else{
	// Single value
	SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass]=TimeTaggs[0]; // Considering only the first timetagg. Might not be very resilence with noise
	cout << "QPLA::Using only first timetag for network synch computations!...to be deactivated" << endl;
}
}

// If the first iteration, since no extra relative frequency difference added, store the values, for at the end compute the offset, at least within theHistPeriodicityAux
if (iCenterMass==0){
	SynchFirstTagsArrayOffsetCalc[iNumRunsPerCenterMass]=SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass];
}

//this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
//this->release();

if (iNumRunsPerCenterMass==(NumRunsPerCenterMass-1)){
// Mean averaging
//double CenterMassVal=0.0;
//for (int i=0;i<(NumRunsPerCenterMass-1);i++){
//CenterMassVal=CenterMassVal+(1.0/((double)NumRunsPerCenterMass-1.0))*(((double)((static_cast<unsigned long long int>(HistPeriodicityAux)/2+SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[iCenterMass][i])%(static_cast<unsigned long long int>(HistPeriodicityAux))))-(double)(static_cast<unsigned long long int>(HistPeriodicityAux)/2));
//}
//SynchHistCenterMassArray[iCenterMass]=CenterMassVal;

// Median averaging
double CenterMassValAux[NumRunsPerCenterMass-1]={0.0};
long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);
for (int i=0;i<(NumRunsPerCenterMass-1);i++){
CenterMassValAux[i]=static_cast<double>(((LLIHistPeriodicityAux/2+SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[iCenterMass][i])%LLIHistPeriodicityAux)-(LLIHistPeriodicityAux/2));
}
SynchHistCenterMassArray[iCenterMass]=DoubleMedianFilterSubArray(CenterMassValAux,(NumRunsPerCenterMass-1));

//cout << "QPLA::SynchHistCenterMassArray[0]: " << SynchHistCenterMassArray[0] << endl;
//cout << "QPLA::SynchHistCenterMassArray[1]: " << SynchHistCenterMassArray[1] << endl;
//cout << "QPLA::SynchHistCenterMassArray[2]: " << SynchHistCenterMassArray[2] << endl;
}

if (iCenterMass==(NumCalcCenterMass-1) and iNumRunsPerCenterMass==(NumRunsPerCenterMass-1)){// Achieved number measurements to compute values
	adjFreqSynchNormRatiosArray[0]=1.0;
	adjFreqSynchNormRatiosArray[1]=((SynchHistCenterMassArray[1]-SynchHistCenterMassArray[0])/(FreqSynchNormValuesArray[1] - FreqSynchNormValuesArray[0]))/static_cast<double>(HistPeriodicityAux);
	adjFreqSynchNormRatiosArray[2]=((SynchHistCenterMassArray[2]-SynchHistCenterMassArray[1])/(FreqSynchNormValuesArray[2] - FreqSynchNormValuesArray[1]))/static_cast<double>(HistPeriodicityAux);

	SynchCalcValuesArray[0]=(SynchHistCenterMassArray[1]-SynchHistCenterMassArray[0])/(adjFreqSynchNormRatiosArray[1]*FreqSynchNormValuesArray[1] - adjFreqSynchNormRatiosArray[0]*FreqSynchNormValuesArray[0]); //Period adjustment	
	SynchCalcValuesArray[2]=(SynchHistCenterMassArray[0]-(adjFreqSynchNormRatiosArray[0]*FreqSynchNormValuesArray[0])*SynchCalcValuesArray[0])/static_cast<double>(HistPeriodicityAux);  // Relative frequency difference adjustment (so it is already a correction, since in GPIO a positive value will make a delay so equivalent to negative compesation)	
	
	double SynchCalcValuesArrayAux[NumRunsPerCenterMass]={0.0};
	double DHistPeriodicityAux=static_cast<double>(HistPeriodicityAux);
	for (int i=0;i<NumRunsPerCenterMass;i++){
		SynchCalcValuesArrayAux[i]=fmod(static_cast<double>(SynchFirstTagsArrayOffsetCalc[i])+SynchCalcValuesArray[2]*DHistPeriodicityAux,DHistPeriodicityAux)/DHistPeriodicityAux; // Offset adjustment - watch out, maybe it is not here the place since it is dependent on link
	}
	SynchCalcValuesArray[1]=DoubleMedianFilterSubArray(SynchCalcValuesArrayAux,NumRunsPerCenterMass);
	
	//cout << "QPLA::SynchCalcValuesArray[0]: " << SynchCalcValuesArray[0] << endl;
	//cout << "QPLA::SynchCalcValuesArray[1]: " << SynchCalcValuesArray[1] << endl;
	//cout << "QPLA::SynchCalcValuesArray[2]: " << SynchCalcValuesArray[2] << endl;
	
	// Identify the specific link and store/update iteratively the values
	if (CurrentSpecificLink>=0){
		SynchNetworkParamsLink[CurrentSpecificLink][0]+=SynchCalcValuesArray[1];// Offset
		SynchNetworkParamsLink[CurrentSpecificLink][1]+=SynchCalcValuesArray[2];// Relative frequency difference
		SynchNetworkParamsLink[CurrentSpecificLink][2]=SynchCalcValuesArray[0];// Estimated period
	}
}

return 0; // All Ok
}

int QPLA::LinearRegressionQuBitFilter(){
//this->acquire(); It is already within an acquire/release
if (ApplyRawQubitFilteringFlag==true){	
	int RawNumStoredQubits=PRUGPIO.RetrieveNumStoredQuBits(RawTimeTaggs,RawChannelTags); // Get raw values
	unsigned long long int NormInitialTimeTaggsVal=RawTimeTaggs[0];
	// Normalize values to work with more plausible values
	//for (int i=0;i<RawNumStoredQubits;i++){
	//	RawTimeTaggs[i]=RawTimeTaggs[i]-NormInitialTimeTaggsVal;
	//}
	// If the SNR is not well above 20 dB or 30dB, this methods perform really bad
	// Estimate the x values for the linear regression from the y values (RawTimeTaggs)
	unsigned long long int xEstimateRawTimeTaggs[RawNumStoredQubits]={0}; // Timetaggs of the detections raw
	//long long int RoundingAux;
	unsigned long long int ULLIHistPeriodicityAux=static_cast<unsigned long long int>(HistPeriodicityAux);
	for (int i=0;i<RawNumStoredQubits;i++){
		/*if (i==0){
			RoundingAux=(HistPeriodicityAux/2+RawTimeTaggs[i])%HistPeriodicityAux-HistPeriodicityAux/2;
			if (RoundingAux>=(HistPeriodicityAux/4)){RoundingAux=1;}
			else if (RoundingAux<=(-HistPeriodicityAux/4)){RoundingAux=-1;}
			else{RoundingAux=0;}
			xEstimateRawTimeTaggs[i]=(RawTimeTaggs[i]/HistPeriodicityAux+RoundingAux)*HistPeriodicityAux;
		}
		else{
			RoundingAux=(HistPeriodicityAux/2+RawTimeTaggs[i]-RawTimeTaggs[i-1])%HistPeriodicityAux-HistPeriodicityAux/2;
			if (RoundingAux>=(HistPeriodicityAux/4)){RoundingAux=1;}
			else if (RoundingAux<=(-HistPeriodicityAux/4)){RoundingAux=-1;}
			else{RoundingAux=0;}
			xEstimateRawTimeTaggs[i]=xEstimateRawTimeTaggs[i-1]+((RawTimeTaggs[i]-RawTimeTaggs[i-1])/HistPeriodicityAux+RoundingAux)*HistPeriodicityAux;
		}*/
		//RoundingAux=(HistPeriodicityAux/2+RawTimeTaggs[i])%HistPeriodicityAux-HistPeriodicityAux/2;
		//if (RoundingAux>=(HistPeriodicityAux/4)){RoundingAux=1;}
		//else if (RoundingAux<=(-HistPeriodicityAux/4)){RoundingAux=-1;}
		//else{RoundingAux=0;}
		xEstimateRawTimeTaggs[i]=(RawTimeTaggs[i]/ULLIHistPeriodicityAux)*ULLIHistPeriodicityAux;		
	}

	// Find the intercept, since the slope is supposed to be know and equal to 1 (because it has been normalized to HistPeriodicityAux)
	double y_mean = 0.0;
	double x_mean = 0.0;
	double y_meanArray[RawNumStoredQubits]={0.0};
	//double x_meanArray[RawNumStoredQubits]={0.0};
	// Relative
        //for (int i=0; i < (RawNumStoredQubits-1); i++) {
        //    y_meanArray[i]=static_cast<double>((HistPeriodicityAux/2+RawTimeTaggs[i+1]-RawTimeTaggs[i])%HistPeriodicityAux)-HistPeriodicityAux/2.0;
        //    //x_meanArray[i]=static_cast<double>(xEstimateRawTimeTaggs[i]%HistPeriodicityAux);// Not really needed
        //    // We cannot use mean averaging since there might be outliers
	//    //y_mean += static_cast<double>(RawTimeTaggs[i]%HistPeriodicityAux)/static_cast<double>(RawNumStoredQubits);
	//    //x_mean += static_cast<double>(xEstimateRawTimeTaggs[i]%HistPeriodicityAux)/static_cast<double>(RawNumStoredQubits);
        //}
        //y_mean=DoubleMedianFilterSubArray(y_meanArray,(RawNumStoredQubits-1)); // Median average
        // Absolute
        for (int i=0; i < RawNumStoredQubits; i++) {
            y_meanArray[i]=static_cast<double>(RawTimeTaggs[i]%ULLIHistPeriodicityAux);
            //x_meanArray[i]=static_cast<double>(xEstimateRawTimeTaggs[i]%HistPeriodicityAux);// Not really needed
            // We cannot use mean averaging since there might be outliers
	    //y_mean += static_cast<double>(RawTimeTaggs[i]%HistPeriodicityAux)/static_cast<double>(RawNumStoredQubits);
	    //x_mean += static_cast<double>(xEstimateRawTimeTaggs[i]%HistPeriodicityAux)/static_cast<double>(RawNumStoredQubits);
        }
        y_mean=DoubleMedianFilterSubArray(y_meanArray,RawNumStoredQubits); // Median average
        //x_mean=DoubleMedianFilterSubArray(x_meanArray,RawNumStoredQubits); // Median average. Not really needed x_mean
        //cout << "QPLA::y_mean: " << y_mean << endl;
        //cout << "QPLA::x_mean: " << x_mean << endl;
	unsigned long long int EstInterceptVal = static_cast<unsigned long long int>(y_mean - x_mean); // x_mean is not multiplied by slope because it has been normalized to 1
	//cout << "QPLA::LinearRegressionQuBitFilter EstInterceptVal: " << EstInterceptVal << endl;
		
	// Re-escale the xEstimated values with the intercept point
	for (int i=0;i<RawNumStoredQubits;i++){
		xEstimateRawTimeTaggs[i]=xEstimateRawTimeTaggs[i]+EstInterceptVal;
	}
		
	int FilteredNumStoredQubits=0;
	// Filter out detections not falling within the defined detection window and calculated signal positions
	
	for (int i=0;i<RawNumStoredQubits;i++){
		if (abs(static_cast<long long int>(RawTimeTaggs[i])-static_cast<long long int>(xEstimateRawTimeTaggs[i]))<FilteringAcceptWindowSize){// Within acceptance window
			TimeTaggs[FilteredNumStoredQubits]=RawTimeTaggs[i];
			ChannelTags[FilteredNumStoredQubits]=RawChannelTags[i];
			FilteredNumStoredQubits++;
		}
	}
	
	// Compute quality of estimation, related to the SNR
	double EstimatedSNRqubitsRatio=1.0-static_cast<double>(FilteredNumStoredQubits)/static_cast<double>(RawNumStoredQubits);// in linear	

	if (EstimatedSNRqubitsRatio>0.1){ // 0.1 equivalent to 10 dB// < Bad SNR
		cout << "QPLA::LinearRegressionQuBitFilter EstimatedSNRqubitsRatio " << EstimatedSNRqubitsRatio << " does not have enough SNR (>10 dB) to perform good when filtering raw qubits!!!" << endl;
		cout << "QPLA::Not filtering outlier qubits!!!" << endl;
		FilteredNumStoredQubits=0;// Reset value
		for (int i=0;i<RawNumStoredQubits;i++){
			TimeTaggs[FilteredNumStoredQubits]=RawTimeTaggs[i];
			ChannelTags[FilteredNumStoredQubits]=RawChannelTags[i];
			FilteredNumStoredQubits++;
		}
	}
	
	// Un-normalize values to have absolute values
	//for (int i=0;i<FilteredNumStoredQubits;i++){
	//	TimeTaggs[i]=TimeTaggs[i]+NormInitialTimeTaggsVal;
	//}
	
	// Update final values
	this->SimulateNumStoredQubitsNode[0]=FilteredNumStoredQubits; // Update value
}
else{ // Do not apply filtering
	this->SimulateNumStoredQubitsNode[0]=PRUGPIO.RetrieveNumStoredQuBits(TimeTaggs,ChannelTags); // Get raw values
	cout << "QPLA::Not applying ApplyRawQubitFilteringFlag...to be activated" << endl;
}
//this->release(); It is already within an acquire/release

return 0; // All Ok
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

int QPLA::RegularCheckToPerform(){
if (GPIOHardwareSynched==false){// Only until it is synched
	GPIOHardwareSynched=PRUGPIO.GetHardwareSynchStatus();// Since only transportN agent can send messages to host, then it is ersponsibility of transportN to also check for this and then send message to host
}

return 0; // All ok
}

void QPLA::AgentProcessRequestsPetitions(){// Check next thing to do

 this->NegotiateInitialParamsNode();

 bool isValidWhileLoop = true;
 while(isValidWhileLoop){
 try{
   try {
   	this->acquire();// Wait semaphore until it can proceed
    	this->ReadParametersAgent(); // Reads messages from above layer
    	this->RegularCheckToPerform();// Every now and then some checks have to happen    	
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

double QPLA::DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor){
if (MedianFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    double temp[MedianFilterFactor]={0.0};
    for(int i = 0; i < MedianFilterFactor; i++) {
        temp[i] = ArrayHolderAux[i];
    }
    
    // Step 2: Sort the temporary array
    this->DoubleBubbleSort(temp,MedianFilterFactor);
    // If odd, middle number
    return temp[MedianFilterFactor/2];
}
}

// Function to implement Bubble Sort
int QPLA::DoubleBubbleSort(double* arr,int MedianFilterFactor) {
    double temp=0.0;
    for (int i = 0; i < MedianFilterFactor-1; i++) {
        for (int j = 0; j < MedianFilterFactor-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
                temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0; // All ok
}

unsigned long long int QPLA::ULLIMedianFilterSubArray(unsigned long long int* ArrayHolderAux,int MedianFilterFactor){
if (MedianFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    unsigned long long int temp[MedianFilterFactor]={0};
    for(int i = 0; i < MedianFilterFactor; i++) {
        temp[i] = ArrayHolderAux[i];
    }
    
    // Step 2: Sort the temporary array
    this->ULLIBubbleSort(temp,MedianFilterFactor);
    // If odd, middle number
    return temp[MedianFilterFactor/2];
}
}

// Function to implement Bubble Sort
int QPLA::ULLIBubbleSort(unsigned long long int* arr,int MedianFilterFactor) {
    unsigned long long int temp=0;
    for (int i = 0; i < MedianFilterFactor-1; i++) {
        for (int j = 0; j < MedianFilterFactor-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
                temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0; // All ok
}

long long int QPLA::LLIMedianFilterSubArray(long long int* ArrayHolderAux,int MedianFilterFactor){
if (MedianFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    long long int temp[MedianFilterFactor]={0};
    for(int i = 0; i < MedianFilterFactor; i++) {
        temp[i] = ArrayHolderAux[i];
    }
    
    // Step 2: Sort the temporary array
    this->LLIBubbleSort(temp,MedianFilterFactor);
    // If odd, middle number
    return temp[MedianFilterFactor/2];
}
}

// Function to implement Bubble Sort
int QPLA::LLIBubbleSort(long long int* arr,int MedianFilterFactor) {
    long long int temp=0;
    for (int i = 0; i < MedianFilterFactor-1; i++) {
        for (int j = 0; j < MedianFilterFactor-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
                temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0; // All ok
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


