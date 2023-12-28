/* Author: Marc Jofre
Agent script for Quantum Physical Layer
*/

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

int main(){

   
   

   

 return 0;
}

int emitQuBit(){
 GPIO outGPIO(60); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.

 // Basic Output - Generate a pulse of 1 second period
 outGPIO.setDirection(OUTPUT);
 
 outGPIO.setValue(HIGH);
 usleep(500000); //micro-second sleep 0.5 seconds
 outGPIO.setValue(LOW);
 usleep(500000);
  
 /* Not used. Just to know how to do fast writes
 // Fast write to GPIO 1 million times
   outGPIO.streamOpen();
   for (int i=0; i<1000000; i++){
      outGPIO.streamWrite(HIGH);
      outGPIO.streamWrite(LOW);
   }
   outGPIO.streamClose();
   */
 
 return 0; // return 0 is for no error
}

int receiveQuBit(){
 GPIO inGPIO(48); // Receiving GPIO. Of course gnd have to be connected accordingly.
 
 // Basic Input
 inGPIO.setDirection(INPUT);
 cout << "The value of the input is: "<< inGPIO.getValue() << endl;
  
 return 0; // return 0 is for no error
}
