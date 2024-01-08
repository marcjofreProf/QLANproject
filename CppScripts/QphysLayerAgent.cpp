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

#define LinkNumberMAX 2
#include "./BBBhw/GPIO.h"
#include <stdlib.h>
// Threading
#define WaitTimeAfterMainWhileLoop 1
// Payload messages
#define NumBytesPayloadBuffer 1000
#include <thread>
// Semaphore
#include <atomic>

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
//////////////////////////////////////////////
int QPLA::InitParametersAgent(){// Client node have some parameters to adjust to the server node

strcpy(this->PayloadSendBuffer,"EmitLinkNumberArray_48_ReceiveLinkNumberArray_60_QuBitsPerSecondVelocity[0]_1000_");

return 0; //All OK
}

int QPLA::SendParametersAgent(){// The upper layer gets the information to be send
this->acquire();

this->release();

return 0; // All OK

}

int QPLA::SetSendParametersAgent(){// Node accumulates parameters for the other node

strcpy(this->PayloadSendBuffer,"none_none_");

return 0; //All OK
}

int QPLA::ReadParametersAgent(){// Node checks parameters from the other node

return 0; // All OK
}

int QPLA::SetReadParametersAgent(){// The upper layer sets information to be read
this->acquire();
//strcpy(this->PayloadReadBuffer,);
this->release();
return 0; // All OK
}
////////////////////////////////////////////////////
void QPLA::acquire() {
while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
}
 
void QPLA::release() {
this->valueSemaphore=1; // Make sure it stays at 1
}

////////////////////////////////////////////////////////
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
// this->outGPIO=exploringBB::GPIO(60); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.
 GPIO outGPIO(this->EmitLinkNumberArray[0]);
 // Basic Output - Generate a pulse of 1 second period
 outGPIO.setDirection(OUTPUT);
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;++iIterRead){
	 outGPIO.setValue(HIGH);
	 usleep(QuBitsUSecPeriodInt[0]);
	 outGPIO.setValue(LOW);
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
 
 return 0; // return 0 is for no error
}

int QPLA::receiveQuBit(){
// this->inGPIO=exploringBB::GPIO(48); // Receiving GPIO. Of course gnd have to be connected accordingly.
 GPIO inGPIO(this->ReceiveLinkNumberArray[0]);
 inGPIO.setDirection(INPUT);
 // Basic Input
 for (int iIterRead=0;iIterRead<NumQubitsMemoryBuffer;++iIterRead){
	 usleep(QuBitsUSecHalfPeriodInt[0]);
	 QuBitValueArray[iIterRead]=inGPIO.getValue();
	 usleep(QuBitsUSecHalfPeriodInt[0]);	 
 }
 //cout << "The value of the input is: "<< inGPIO.getValue() << endl;
  
 return 0; // return 0 is for no error
}

QPLA::~QPLA() {
// destructor
this->threadRef.join();// Terminate the process thread
}

void QPLA::AgentProcessRequestsPetitions(){// Check next thing to do
 bool isValidWhileLoop = true;
 while(isValidWhileLoop){
 try{
   try {
   	this->acquire();// Wait semaphore until it can proceed
    	
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


