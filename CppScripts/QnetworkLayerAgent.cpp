/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum network Layer
*/
#include "QnetworkLayerAgent.h"
#include<iostream>
#include<unistd.h> //for usleep
#include <stdio.h>
#include <string.h>

#include "QlinkLayerAgent.h"
// Threading
#define WaitTimeAfterMainWhileLoop 1
// Payload messages
#define NumBytesPayloadBuffer 1000
#include <thread>
// Semaphore
#include <atomic>

using namespace std;

namespace nsQnetworkLayerAgent {

QNLA::QNLA() { // Constructor
 
}
////////////////////////////////////////////////////
void QNLA::acquire() {
while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
}
 
void QNLA::release() {
this->valueSemaphore=1; // Make sure it stays at 1
}
///////////////////////////////////////////////////
int QNLA::InitParametersAgent(){// Client node have some parameters to adjust to the server node

strcpy(this->PayloadSendBuffer,"");

return 0; //All OK
}

int QNLA::SendParametersAgent(char* ParamsCharArray){// The upper layer gets the information to be send
this->acquire();

strcat(ParamsCharArray,"Net;");
if (string(this->PayloadSendBuffer)!=string("")){
	strcat(ParamsCharArray,this->PayloadSendBuffer);
}
else{
	strcat(ParamsCharArray,"none_none_");
}
strcat(ParamsCharArray,";");
QLLAagent.SendParametersAgent(ParamsCharArray);// Below Agent Method
strcpy(this->PayloadSendBuffer,"");// Reset buffer

this->release();

return 0; // All OK

}

int QNLA::SetSendParametersAgent(char* ParamsCharArray){// Node accumulates parameters for the other node

strcpy(this->PayloadSendBuffer,"none_none_");

return 0; //All OK
}

int QNLA::ReadParametersAgent(){// Node checks parameters from the other node

return 0; // All OK
}

int QNLA::SetReadParametersAgent(char* ParamsCharArray){// The upper layer sets information to be read
this->acquire();
//strcpy(this->PayloadReadBuffer,);
this->release();
return 0; // All OK
}
////////////////////////////////////////////////////////
int QNLA::InitAgentProcess(){
	// Then, regularly check for next job/action without blocking		  	
	// Not used void* params;
	// Not used this->threadRef=std::thread(&QTLAH::AgentProcessStaticEntryPoint,params);
	this->threadRef=std::thread(&QNLA::AgentProcessRequestsPetitions,this);
	  //if (ret) {
	    // Handle the error
	  //} 
	QLLAagent.InitAgentProcess();// Initialize thread of the agent below
	return 0; //All OK
}

QNLA::~QNLA() {
// destructor
 this->threadRef.join();// Terminate the process thread
 this->QLLAagent.~QLLA(); // Destruct the instance of the below agent
}

void QNLA::AgentProcessRequestsPetitions(){// Check next thing to do
 bool isValidWhileLoop = true;
 while(isValidWhileLoop){
 try{
   try {
   	this->acquire();// Wait semaphore until it can proceed
    	this->ReadParametersAgent();// Reads messages from above layer
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

} /* namespace nsQnetworkLayerAgent */

/*
// For testing

using namespace nsQnetworkLayerAgent;

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

