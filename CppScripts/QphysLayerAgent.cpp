/* Author: Marc Jofre
Agent script for Quantum Physical Layer
*/
#include "QphysLayerAgent.h"
#include<iostream>
#include<unistd.h> //for usleep
#include "./BBBhw/GPIO.h"
using namespace exploringBB; // API to easily use GPIO in c++
/* A Simple GPIO application
* Written by Derek Molloy for the book "Exploring BeagleBone: Tools and
* Techniques for Building with Embedded Linux" by John Wiley & Sons, 2018
* ISBN 9781119533160. Please see the file README.md in the repository root
* directory for copyright and GNU GPLv3 license information.*/
using namespace std;

namespace nsQphysLayerAgent {

QPLA::QPLA(int numberLinks, int* EmitLinkNumberArray, int* ReceiveLinkNumberArray) {
 this->numberLinks = numberLinks; // Number of links directly connected to this physical quantum node
 this->EmitLinkNumberArray = EmitLinkNumberArray;
 this->ReceiveLinkNumberArray = ReceiveLinkNumberArray;
}

int QPLA::emitQuBit(){
 GPIO outGPIO(60); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.

 // Basic Output - Generate a pulse of 1 second period
 outGPIO.setDirection(OUTPUT);
 
 outGPIO.setValue(HIGH);
 usleep(500); //micro-second sleep 0.0005 seconds
 outGPIO.setValue(LOW);
 usleep(500);
  
 /* Not used. Just to know how to do fast writes
 // Fast write to GPIO 1 million times
   outGPIO.streamOpen();
   for (int i=0; i<1000000; i++){
      outGPIO.streamWrite(HIGH);
      outGPIO.streamWrite(LOW);
   }
   outGPIO.streamClose();
   */
   
 cout << "Qubit emitted" << endl;
 
 return 0; // return 0 is for no error
}

int QPLA::receiveQuBit(){
 GPIO inGPIO(48); // Receiving GPIO. Of course gnd have to be connected accordingly.
 
 // Basic Input
 inGPIO.setDirection(INPUT);
 cout << "The value of the input is: "<< inGPIO.getValue() << endl;
  
 return 0; // return 0 is for no error
}

QPLA::~QPLA() {
// destructor
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


