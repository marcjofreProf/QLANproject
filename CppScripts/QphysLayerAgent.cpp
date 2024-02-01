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
#define WaitTimeAfterMainWhileLoop 1
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20
#include <thread>
// Semaphore
#include <atomic>
// time points
#define WaitTimeToFutureTimePoint 1000 // It is the time barrier to try to achieve synchronization. considered millisecons (it can be changed on the transformatoin used)

using namespace exploringBB; // API to easily use GPIO in c++
/* A Simple GPIO application
* Written by Derek Molloy for the book "Exploring BeagleBone: Tools and
* Techniques for Building with Embedded Linux" by John Wiley & Sons, 2018
* ISBN 9781119533160. Please see the file README.md in the repository root
* directory for copyright and GNU GPLv3 license information.*/
using namespace std;

namespace nsQphysLayerAgent {
QPLA::QPLA() {// Constructor

 
}

////////////////////////////////////////////////////////
void QPLA::acquire() {
while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
}
 
void QPLA::release() {
this->valueSemaphore=1; // Make sure it stays at 1
}

//////////////////////////////////////////////
int QPLA::InitParametersAgent(){// Client node have some parameters to adjust to the server node
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer
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
if (string(HeaderCharArray[iHeaders])==string("EmitLinkNumberArray[0]")){this->EmitLinkNumberArray[0]=(int)atoi(ValuesCharArray[iHeaders]);}
else if (string(HeaderCharArray[iHeaders])==string("ReceiveLinkNumberArray[0]")){this->ReceiveLinkNumberArray[0]=(int)atoi(ValuesCharArray[iHeaders]);}
else if (string(HeaderCharArray[iHeaders])==string("QuBitsPerSecondVelocity[0]")){this->QuBitsPerSecondVelocity[0]=(float)atoi(ValuesCharArray[iHeaders]);}
else if (string(HeaderCharArray[iHeaders])==string("OtherClientNodeFutureTimePoint")){// Also helps to wait here for the thread
	cout << "OtherClientNodeFutureTimePoint: " << (unsigned int)atoi(ValuesCharArray[iHeaders]) << endl;
	std::chrono::milliseconds duration_back((unsigned int)atoi(ValuesCharArray[iHeaders]));
	this->OtherClientNodeFutureTimePoint=Clock::time_point(duration_back);
	
	// Debugging
	TimePoint TimePointClockNow=Clock::now();
	auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	auto duration_since_epoch=this->OtherClientNodeFutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	unsigned int time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch).count(); // Convert 
	cout << "time_as_count: " << time_as_count << endl;
	//
}
else if (string(HeaderCharArray[iHeaders])==string("ClearOtherClientNodeFutureTimePoint")){//CLear this node OtherClientNodeFutureTimePoints to avoid having anon-zero value eventhough the other node has finished transmitting and this one for some reason could no execute it
if (this->threadEmitQuBitRefAux.joinable()){
	//this->release();
	//cout << "Check block release Process New Parameters" << endl;
	this->threadEmitQuBitRefAux.join();
	//cout << "Check block before acquire Process New Parameters" << endl;
	//this->acquire();
	//cout << "Check block after acquire Process New Parameters" << endl;
}// Wait for the thread to finish. If we wait for the thread to finish, the upper layers get also
// Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
}
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


int QPLA::emitQuBit(){
bool RunThreadFlag=true;
try{
//this->acquire();
 RunThreadFlag=!this->threadEmitQuBitRefAux.joinable();//(!this->threadEmitQuBitRefAux.joinable() && !this->threadReceiveQuBitRefAux.joinable());
 //this->release();
    } // upper try
  catch (...) { // Catches any exception
  	RunThreadFlag=true;  
  	//this->release();
    }

//this->acquire();
if (RunThreadFlag){// Protection, do not run if there is a previous thread running
this->threadEmitQuBitRefAux=std::thread(&QPLA::ThreadEmitQuBit,this);
}
else{
cout << "Not possible to launch ThreadEmitQuBit" << endl;
}
//this->release();

return 0; // return 0 is for no error
}

int QPLA::ThreadEmitQuBit(){
//this->acquire();
cout << "Emiting Qubits" << endl;

int MaxWhileRound=1000000000;
// Wait to receive the FutureTimePoint from client node
while(this->OtherClientNodeFutureTimePoint==std::chrono::time_point<Clock>() && MaxWhileRound>0){
	//this->release();
	usleep(100);//Maybe some sleep to reduce CPU consumption
	MaxWhileRound--;
	//this->acquire();
	};
if (MaxWhileRound<=0){this->OtherClientNodeFutureTimePoint=Clock::now();}// Provide a TimePoint to avoid blocking issues
cout << "MaxWhileRound: " << MaxWhileRound << endl;
MaxWhileRound=100;
/////////////////////////////////////////////
// Checks
	TimePoint TimePointClockNow=Clock::now();
	auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds)
	cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	
	auto duration_since_epochFutureTimePoint=this->OtherClientNodeFutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePoint).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;
        int TimePointsDiff_time_as_count=0;
        TimePointsDiff_time_as_count=(int)(TimeNow_time_as_count-TimePointFuture_time_as_count);
        cout << "TimePointsDiff_time_as_count: " << TimePointsDiff_time_as_count << endl;
///////////////////////////////////

while(Clock::now()<this->OtherClientNodeFutureTimePoint && MaxWhileRound>0){
	//this->release();	
	TimePoint TimePointClockNow=Clock::now();
	auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	
	auto duration_since_epochFutureTimePoint=this->OtherClientNodeFutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePoint).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;
        unsigned int TimePointsDiff_time_as_count=0;
        if (TimeNow_time_as_count>=TimePointFuture_time_as_count){TimePointsDiff_time_as_count=TimeNow_time_as_count-TimePointFuture_time_as_count;}
        else{TimePointsDiff_time_as_count=TimePointFuture_time_as_count-TimeNow_time_as_count;}
        if (TimePointsDiff_time_as_count>WaitTimeToFutureTimePoint){TimePointsDiff_time_as_count=WaitTimeToFutureTimePoint;}//conditions to not get extremely large sleeps
        cout << "TimePointsDiff_time_as_count: " << TimePointsDiff_time_as_count << endl;
	usleep(TimePointsDiff_time_as_count*999);//Maybe some sleep to reduce CPU consumption
	MaxWhileRound--;
	//this->acquire();
	};
cout << "MaxWhileRound: " << MaxWhileRound << endl;
// Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();

//this->release();
//this->acquire();
 //exploringBB::GPIO outGPIO=exploringBB::GPIO(this->EmitLinkNumberArray[0]); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.
 GPIO outGPIO(this->EmitLinkNumberArray[0]);
 // Basic Output - Generate a pulse of 1 second period
 outGPIO.setDirection(OUTPUT);
 usleep(QuBitsUSecQuarterPeriodInt[0]);
 for (int iIterWrite=0;iIterWrite<NumQubitsMemoryBuffer;iIterWrite++){
	 outGPIO.setValue(HIGH);
	 usleep(QuBitsUSecHalfPeriodInt[0]);
	 outGPIO.setValue(LOW);
	 usleep(QuBitsUSecHalfPeriodInt[0]);
 }
 //usleep(QuBitsUSecHalfPeriodInt[0]);
  
 /* Not used. Just to know how to do fast writes
 // Fast write to GPIO 1 million times
   outGPIO.streamOpen();
   for (int i=0; i<1000000; i++){
      outGPIO.streamWrite(HIGH);
      outGPIO.streamWrite(LOW);
   }
   outGPIO.streamClose();
   */
   
 //cout << "Qubit emitted" << endl;

//this->release();
 return 0; // return 0 is for no error
}

int QPLA::receiveQuBit(){
bool RunThreadFlag=true;
try{
//this->acquire();
RunThreadFlag=!this->threadReceiveQuBitRefAux.joinable();//(!this->threadReceiveQuBitRefAux.joinable() && !this->threadEmitQuBitRefAux.joinable() ) ;
//this->release();
    } // upper try
  catch (...) { // Catches any exception
  	RunThreadFlag=true;  
  	//this->release();
    }
    
if (RunThreadFlag){// Protection, do not run if there is a previous thread running
//this->acquire();
this->threadReceiveQuBitRefAux=std::thread(&QPLA::ThreadReceiveQubit,this);
//this->release();
}
else{
cout << "Not possible to launch ThreadReceiveQubit" << endl;
}

return 0; // return 0 is for no error
}

int QPLA::ThreadReceiveQubit(){

int NumStoredQubitsNodeAux=0;
cout << "Receiving Qubits" << endl;

// Client sets a future TimePoint for measurement and communicates it to the server (the one sending the qubits)
// Somehow, here it is assumed that the two system clocks are quite snchronized (maybe with the Precise Time Protocol)

// Debugging
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
//
TimePoint FutureTimePoint = Clock::now()+std::chrono::milliseconds(WaitTimeToFutureTimePoint);// Set a time point in the future
auto duration_since_epoch=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned int time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch).count(); // Convert 
cout << "time_as_count: " << time_as_count << endl;
// Mount the Parameters message for the other node
char ParamsCharArray[NumBytesPayloadBuffer] = {0};
strcpy(ParamsCharArray,"OtherClientNodeFutureTimePoint_"); // Initiates the ParamsCharArray, so use strcpy

char charNum[NumBytesPayloadBuffer] = {0}; 
sprintf(charNum, "%u", time_as_count);
strcat(ParamsCharArray,charNum);

strcat(ParamsCharArray,"_"); // Final _
//cout << "ParamsCharArray: " << ParamsCharArray << endl;
this->acquire();
this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other node
this->release();

int MaxWhileRound=100;
while(Clock::now()<FutureTimePoint && MaxWhileRound>0){
	MaxWhileRound--;
	TimePoint TimePointClockNow=Clock::now();
	auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	
	auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	unsigned int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePoint).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
	//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;
        unsigned int TimePointsDiff_time_as_count=0;
        if (TimeNow_time_as_count>=TimePointFuture_time_as_count){TimePointsDiff_time_as_count=TimeNow_time_as_count-TimePointFuture_time_as_count;}
        else{TimePointsDiff_time_as_count=TimePointFuture_time_as_count-TimeNow_time_as_count;}
        if (TimePointsDiff_time_as_count>WaitTimeToFutureTimePoint){TimePointsDiff_time_as_count=WaitTimeToFutureTimePoint;}//conditions to not get extremely large sleeps
	usleep(TimePointsDiff_time_as_count*999);//Maybe some sleep to reduce CPU consumption	
};
//this->acquire();
cout << "MaxWhileRound: " << MaxWhileRound << endl;
// Tell the other node to clear the TimePoint (this avoids having a time point in the other node after having finished this one (because it was not ocnsumed)
ParamsCharArray[NumBytesPayloadBuffer] = {0};
strcpy(ParamsCharArray,"ClearOtherClientNodeFutureTimePoint_0_"); // Initiates the ParamsCharArray, so use strcpy
this->acquire();
this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other node
this->release();


// Start measuring
 //exploringBB::GPIO inGPIO=exploringBB::GPIO(this->ReceiveLinkNumberArray[0]); // Receiving GPIO. Of course gnd have to be connected accordingly.
 GPIO inGPIO(this->ReceiveLinkNumberArray[0]);
 inGPIO.setDirection(INPUT);
 // Basic Input
 usleep(QuBitsUSecHalfPeriodInt[0]);
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;iIterRead++){	 
	 QuBitValueArray[iIterRead]=inGPIO.getValue();
	 usleep(QuBitsUSecPeriodInt[0]);	 
 }
 // Count received QuBits
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;iIterRead++){// Count how many qubits 
 	if (QuBitValueArray[iIterRead]==1){
 		NumStoredQubitsNodeAux++;
 	} 	
 }

this->acquire();
this->NumStoredQubitsNode[0]=NumStoredQubitsNodeAux;
//cout << "The value of the input is: "<< inGPIO.getValue() << endl;
this->release();
return 0; // return 0 is for no error
}

int QPLA::GetNumStoredQubitsNode(){
//this->acquire();

try{
if (this->threadReceiveQuBitRefAux.joinable()){
//this->release();
this->threadReceiveQuBitRefAux.join();
//this->acquire();
}// Wait for the thread to finish. If we wait for the thread to finish, the upper layers get also
    } // upper try
  catch (...) { // Catches any exception
    }
this->acquire();
int NumStoredQubitsNodeAux=this->NumStoredQubitsNode[0];
this->release();
return NumStoredQubitsNodeAux;
}

QPLA::~QPLA() {
// destructor
this->threadRef.join();// Terminate the process thread
}

int QPLA::NegotiateInitialParamsNode(){
try{
 
if (string(this->SCmode[0])==string("client")){
 char ParamsCharArray[NumBytesPayloadBuffer]="EmitLinkNumberArray[0]_48_ReceiveLinkNumberArray[0]_60_QuBitsPerSecondVelocity[0]_10000_";// Set initialization value for the other node
 this->SetSendParametersAgent(ParamsCharArray);// Set initialization values for the other node
}
else{//server
// Expect to receive some information
}

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
        usleep(WaitTimeAfterMainWhileLoop);// Wait a few microseconds for other processes to enter
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


