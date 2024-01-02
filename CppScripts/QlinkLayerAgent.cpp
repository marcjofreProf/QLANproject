/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

2024

Agent script for Quantum Link Layer
*/
#include "QlinkLayerAgent.h"
#include<iostream>
#include<unistd.h> //for usleep
#include "QphysLayerAgent.h"
using namespace std;

namespace nsQlinkLayerAgent {

QLLA::QLLA(int numberHops) { // Constructor
 this->numberHops = numberHops; // Number of hops to reach destination
}


QLLA::~QLLA() {
// destructor
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
