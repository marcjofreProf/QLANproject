/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum Link Layer
*/
#include "QlinkLayerAgent.h"
#include<iostream>
#include<unistd.h>
#include <stdio.h>
#include <string.h>

#include "QphysLayerAgent.h"
// Threading
#include <thread>
#define WaitTimeAfterMainWhileLoop 10000000 // nanoseconds
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20

// Semaphore
#include <atomic>

using namespace std;

namespace nsQlinkLayerAgent {

QLLA::QLLA() { // Constructor
 
}
/////////////////////////////////////////////
void QLLA::acquire() {
/*while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
*/
// https://stackoverflow.com/questions/61493121/when-can-memory-order-acquire-or-memory-order-release-be-safely-removed-from-com
// https://medium.com/@pauljlucas/advanced-thread-safety-in-c-4cbab821356e
//int oldCount;
	//unsigned long long int ProtectionSemaphoreTrap=0;
	bool valueSemaphoreExpected=true;
	while(true){
		//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
		//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
		//ProtectionSemaphoreTrap++;
		//if (ProtectionSemaphoreTrap>UnTrapSemaphoreValueMaxCounter){this->release();cout << "QLLA::Releasing semaphore!!!" << endl;}// Avoid trapping situations
		if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
		break;
		}
	}
}
 
void QLLA::release() {
//this->valueSemaphore=1; // Make sure it stays at 1
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}
////////////////////////////////////////////////////
int QLLA::InitParametersAgent(){// Client node have some parameters to adjust to the server node

memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer

return 0; //All OK
}

int QLLA::SendParametersAgent(char* ParamsCharArray){// The upper layer gets the information to be send
this->acquire();

strcat(ParamsCharArray,"Link;");
if (string(this->PayloadSendBuffer)!=string("")){
	strcat(ParamsCharArray,this->PayloadSendBuffer);
}
else{
	strcat(ParamsCharArray,"none_none_");
}
strcat(ParamsCharArray,";");
QPLAagent.SendParametersAgent(ParamsCharArray);// Below Agent Method
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer
this->release();

return 0; // All OK

}

int QLLA::SetSendParametersAgent(char* ParamsCharArray){// Node accumulates parameters for the other node

strcat(this->PayloadSendBuffer,ParamsCharArray);

return 0; //All OK
}

int QLLA::ReadParametersAgent(){// Node checks parameters from the other node
if (string(this->PayloadReadBuffer)!=string("") and string(this->PayloadReadBuffer)==string("none_none_")){
	this->ProcessNewParameters();
}
memset(this->PayloadReadBuffer, 0, sizeof(this->PayloadReadBuffer));// Reset buffer
return 0; // All OK
}

int QLLA::SetReadParametersAgent(char* ParamsCharArray){// The upper layer sets information to be read
this->acquire();
//cout << "QLLA::ReadParametersAgent: " << ParamsCharArray << endl;
char DiscardBuffer[NumBytesPayloadBuffer]={0};
strcpy(DiscardBuffer,strtok(ParamsCharArray,";"));
strcpy(this->PayloadReadBuffer,strtok(NULL,";"));

char ParamsCharArrayAux[NumBytesPayloadBuffer]={0};
strcpy(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
strcat(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");

QPLAagent.SetReadParametersAgent(ParamsCharArrayAux); // Send respective information to the below layer agent
this->release();
return 0; // All OK
}
////////////////////////////////////////////////////
int QLLA::countDoubleColons(char* ParamsCharArray) {
  int colonCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ':') {
      colonCount++;
    }
  }

  return colonCount/2;
}

int QLLA::countDoubleUnderscores(char* ParamsCharArray) {
  int underscoreCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == '_') {
      underscoreCount++;
    }
  }

  return underscoreCount/2;
}

int QLLA::ProcessNewParameters(){
char ParamsCharArray[NumBytesPayloadBuffer]={0};
char HeaderCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
char ValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
char TokenValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};

strcpy(ParamsCharArray,this->PayloadReadBuffer);
memset(this->PayloadReadBuffer, 0, sizeof(this->PayloadReadBuffer));// Reset buffer

int NumDoubleUnderscores = this->countDoubleUnderscores(ParamsCharArray);

for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
	if (iHeaders==0){
		strcpy(HeaderCharArray[iHeaders],strtok(ParamsCharArray,"_"));		
	}
	else{
		strcpy(HeaderCharArray[iHeaders],strtok(NULL,"_"));
	}
	strcpy(ValuesCharArray[iHeaders],strtok(NULL,"_"));
}

for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
// Missing to develop if there are different values

}

return 0; // All OK
}
////////////////////////////////////////////////////////
int QLLA::InitAgentProcess(){
	// Then, regularly check for next job/action without blocking		  	
	// Not used void* params;
	// Not used this->threadRef=std::thread(&QTLAH::AgentProcessStaticEntryPoint,params);
	this->threadRef=std::thread(&QLLA::AgentProcessRequestsPetitions,this);
	  //if (ret) {
	    // Handle the error
	  //} 
	QPLAagent.InitAgentProcess();// Initialize thread of the agent below
	return 0; //All OK
}

int QLLA::RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep){
struct timespec ts;
ts.tv_sec=(int)(TimeNanoSecondsSleep/((long)1000000000));
ts.tv_nsec=(long)(TimeNanoSecondsSleep%(long)1000000000);
clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL); //

return 0; // All ok
}

QLLA::~QLLA() {
// destructor
 this->threadRef.join();// Terminate the process thread
 this->QPLAagent.~QPLA(); // Destruct the instance of the below layer
}

int QLLA::NegotiateInitialParamsNode(){
try{
 this->acquire();
if (string(this->SCmode[0])==string("client")){

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

void QLLA::AgentProcessRequestsPetitions(){// Check next thing to do
 this->NegotiateInitialParamsNode();
 bool isValidWhileLoop = true;
 while(isValidWhileLoop){
 	this->acquire();// Wait semaphore until it can proceed
 try{
   try {
   	
    	this->ReadParametersAgent();// Reads messages from above layer
        
	    }
	    catch (const std::exception& e) {
		// Handle the exception
	    	cout << "Exception: " << e.what() << endl;
	    }
	    } // upper try
	  catch (...) { // Catches any exception
	  cout << "Exception caught" << endl;
	    }
    this->release(); // Release the semaphore 
        this->RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
    } // while
}
} /* namespace nsQlinkLayerAgent */

/*
// For testing

using namespace nsQlinkLayerAgent;

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
 
 return 0;
}
*/
