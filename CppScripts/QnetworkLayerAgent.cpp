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
#include "QlinkLayerAgent.h"
// Threading
#define WaitTimeAfterMainWhileLoop 1
#include <thread>
// Semaphore
#include <atomic>

using namespace std;

namespace nsQnetworkLayerAgent {

QNLA::QNLA() { // Constructor
 
}

void QNLA::acquire() {
while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
}
 
void QNLA::release() {
this->valueSemaphore=1; // Make sure it stays at 1
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

