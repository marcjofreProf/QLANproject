/* Author: Marc Jofre
Agent script for Quantum transport Layer Node
*/
#include "QtransportLayerAgentN.h"
#include<iostream>
#include<unistd.h> //for usleep
#include "QnetworkLayerAgent.h"
using namespace std;

namespace nsQtransportLayerAgentN {

QTLAN::QTLAN(int numberSessions) { // Constructor
 this->numberSessions = numberSessions; // Number of sessions of different services
}


QTLAN::~QTLAN() {
// destructor
}

} /* namespace nsQnetworkLayerAgentN */


// For running in the quantum node

using namespace nsQtransportLayerAgentN;

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
 
 QTLAN QTLANagent(0); // Initiate the instance with 0 sessions connected.
 QTLANagent.pause(); // Initiate in paused state.
 cout << "Starting in pause state the QtransportLayerAgentN" << endl;
 bool isValidWhileLoop = true;
 
 while(isValidWhileLoop){
       switch(QTLANagent.getState()) {
           case QTLAN::APPLICATION_RUNNING: {
               
               // Do Some Work
               QTLANagent.pause(); // After procesing the request, pass to paused state
               break;
           }
           case QTLAN::APPLICATION_PAUSED: {
               // Wait Till You Have Focus Or Continues
               if (true){
               	QTLANagent.start();
               }
               else if (false){
               	QTLANagent.exit();// Change The Application State to exit
               }
               else{               
	        QTLANagent.pause(); // Keep paused state
	        usleep(1000); // Wait 1ms, waiting for other jobs to process
               }
               break;
           }
           case QTLAN::APPLICATION_EXIT: {    
               cout << "Exiting the QtransportLayerAgentN" << endl;
               isValidWhileLoop=false;//break;
           }
           default: {
               // ErrorHandling Throw An Exception Etc.
           }

        } // switch
        
    }
   
 return 0; // Everything Ok
}

