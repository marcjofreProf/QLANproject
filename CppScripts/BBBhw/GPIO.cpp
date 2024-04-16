/*
 * GPIO.cpp  Created on: 29 Apr 2014
 * Copyright (c) 2014 Derek Molloy (www.derekmolloy.ie)
 * Made available for the book "Exploring BeagleBone"
 * If you use this code in your work please cite:
 *   Derek Molloy, "Exploring BeagleBone: Tools and Techniques for Building
 *   with Embedded Linux", Wiley, 2014, ISBN:9781118935125.
 * See: www.exploringbeaglebone.com
 * Licensed under the EUPL V.1.1
 *
 * This Software is provided to You under the terms of the European
 * Union Public License (the "EUPL") version 1.1 as published by the
 * European Union. Any use of this Software, other than as authorized
 * under this License is strictly prohibited (to the extent such use
 * is covered by a right of the copyright holder of this Software).
 *
 * This Software is provided under the License on an "AS IS" basis and
 * without warranties of any kind concerning the Software, including
 * without limitation merchantability, fitness for a particular purpose,
 * absence of defects or errors, accuracy, and non-infringement of
 * intellectual property rights other than copyright. This disclaimer
 * of warranty is an essential part of the License and a condition for
 * the grant of any rights to this Software.
 *
 * For more details, see http://www.derekmolloy.ie/
 */

#include "GPIO.h"
#include<iostream>
#include<fstream>
#include<bitset>
#include<string>
#include<sstream> // For istringstream
#include<cstdlib>
#include<cstdio>
#include<fcntl.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<thread>
#include<pthread.h>
// Time/synchronization management
#include <chrono>
// Mathemtical calculations
#include <cmath>
// PRU programming
#include<poll.h>
#include <stdio.h>
#include <sys/mman.h>
#include <prussdrv.h>
#include <pruss_intc_mapping.h>
#define PRU_Operation_NUM 0 // PRU operation and handling with PRU0
#define PRU_Signal_NUM 1 // Signals PINS with PRU1
/******************************************************************************
* Local Macro Declarations - Global Space point of View                       *
******************************************************************************/
#define AM33XX_PRUSS_IRAM_SIZE 8192 // Instructions RAM (where .p assembler instructions are loaded)
#define AM33XX_PRUSS_DRAM_SIZE 8192 // PRU's Data RAM
#define AM33XX_PRUSS_SHAREDRAM_SIZE 12000 // Shared Data RAM

#define PRU_ADDR        0x4A300000      // Start of PRU memory Page 184 am335x TRM
#define PRU_LEN         0x80000         // Length of PRU memory

#define DDR_BASEADDR 0x80000000 //0x80000000 is where DDR starts, but we leave some offset (0x00001000) to avoid conflicts with other critical data present// Already initiated at this position with LOCAL_DDMinit
#define OFFSET_DDR 0x00001000
#define SHAREDRAM 0x00010000 // Already initiated at this position with LOCAL_DDMinit
#define OFFSET_SHAREDRAM 0x00000000 //Global Memory Map (from the perspective of the host) equivalent with 0x00002000

#define PRU0_DATARAM 0x00000000 //Global Memory Map (from the perspective of the host)// Already initiated at this position with LOCAL_DDMinit
#define PRU1_DATARAM 0x00002000 //Global Memory Map (from the perspective of the host)// Already initiated at this position with LOCAL_DDMinit
#define DATARAMoffset	     0x00000200 // Offset from Base OWN_RAM to avoid collision with some data 

#define PRUSS0_PRU0_DATARAM 0
#define PRUSS0_PRU1_DATARAM 1
#define PRUSS0_PRU0_IRAM 2
#define PRUSS0_PRU1_IRAM 3
#define PRUSS0_SHARED_DATARAM 4

using namespace std;

namespace exploringBB {
void* exploringBB::GPIO::ddrMem = nullptr; // Define and initialize ddrMem
void* exploringBB::GPIO::sharedMem = nullptr; // Define and initialize
void* exploringBB::GPIO::pru0dataMem = nullptr; // Define and initialize 
void* exploringBB::GPIO::pru1dataMem = nullptr; // Define and initialize
void* exploringBB::GPIO::pru_int = nullptr;// Define and initialize
unsigned int* exploringBB::GPIO::sharedMem_int = nullptr;// Define and initialize
unsigned int* exploringBB::GPIO::pru0dataMem_int = nullptr;// Define and initialize
unsigned int* exploringBB::GPIO::pru1dataMem_int = nullptr;// Define and initialize
int exploringBB::GPIO::mem_fd = -1;// Define and initialize 
/**
 *
 * @param number The GPIO number for the BBB
 */
GPIO::GPIO(){// Redeclaration of constructor GPIO when no argument is specified
	// Initialize structure used by prussdrv_pruintc_intc
	// PRUSS_INTC_INITDATA is found in pruss_intc_mapping.h
	tpruss_intc_initdata pruss_intc_initdata = PRUSS_INTC_INITDATA;
	// Allocate and initialize memory
	prussdrv_init();
	// Interrupts
	if (prussdrv_open(PRU_EVTOUT_0) == -1) {// Event PRU-EVTOUT0 - Interrupt from PRU0
	   perror("prussdrv_open(PRU_EVTOUT_0) failed. Execute as root: sudo su or sudo. /boot/uEnv.txt has to be properly configured with iuo. Message: "); 
	  }
	
	if (prussdrv_open(PRU_EVTOUT_1) == -1) {// Event PRU-EVTOUT1 - Interrupt from PRU1
	   perror("prussdrv_open(PRU_EVTOUT_1) failed. Execute as root: sudo su or sudo. /boot/uEnv.txt has to be properly configured with iuo. Message: "); 
	  }
	
	// Map PRU's interrupts
	// prussdrv.pruintc_init(); // Init handling interrupts from PRUs
	prussdrv_pruintc_init(&pruss_intc_initdata);
    	
	// Clear prior interrupt events
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);
	
    	// Open file where temporally are stored timetaggs	
	streamDDRpru.open(string(PRUdataPATH1) + string("TimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content	
	if (!streamDDRpru.is_open()) {
		streamDDRpru.open(string(PRUdataPATH2) + string("TimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
		if (!streamDDRpru.is_open()) {
	        	cout << "Failed to open the streamDDRpru file." << endl;
	        }
        }
        
        if (streamDDRpru.is_open()){
		streamDDRpru.close();	
		//streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	}
	
	// Open file where temporally are stored synch	
	streamSynchpru.open(string(PRUdataPATH1) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content	
	if (!streamSynchpru.is_open()) {
		streamSynchpru.open(string(PRUdataPATH2) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
		if (!streamSynchpru.is_open()) {
	        	cout << "Failed to open the streamSynchpru file." << endl;
	        }
        }
        
        if (streamSynchpru.is_open()){
		streamSynchpru.close();	
		//streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	}
	
        // Initialize DDM
	LOCAL_DDMinit(); // DDR (Double Data Rate): A class of memory technology used in DRAM where data is transferred on both the rising and falling edges of the clock signal, effectively doubling the data rate without increasing the clock frequency.
	// Here we can update memory space assigned address
	valpHolder=(unsigned char*)&sharedMem_int[OFFSET_SHAREDRAM];
	valpAuxHolder=valpHolder+4+5*NumRecords;
	synchpHolder=(unsigned int*)&pru0dataMem_int[2];
	
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Timetagging
	    // Execute program
	    // Load and execute the PRU program on the PRU0
	pru0dataMem_int[0]=static_cast<unsigned int>(0); // set no command
	pru0dataMem_int[1]=this->NumRecords; // set number captures
	if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScript.bin");
		}
	}
	////prussdrv_pru_enable(PRU_Operation_NUM);
	// Generate signals
	pru1dataMem_int[0]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
	pru1dataMem_int[1]=static_cast<unsigned int>(0); // set no command
	// Load and execute the PRU program on the PRU1
	if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScriptHist4Sig.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScriptHist4Sig.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScriptHist4Sig.bin");//perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}
	/*
	// Self Test Histogram
	if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScriptHist4SigSelfTest.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScriptHist4SigSelfTest.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScriptHist4SigSelfTest.bin");//perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}
	sleep(10);// Give some time to load programs in PRUs and initiate. Very important, otherwise bad values might be retrieved
	this->SendTriggerSignalsSelfTest(); // Self test initialization
	cout << "Attention doing SendTriggerSignalsSelfTest. To be removed" << endl;	
	*/
	
	////prussdrv_pru_enable(PRU_Signal_NUM);
	sleep(10);// Give some time to load programs in PRUs and initiate. Very important, otherwise bad values might be retrieved
	  
	  /*// Doing debbuging checks - Debugging 1	  
	  std::thread threadReadTimeStampsAux=std::thread(&GPIO::ReadTimeStamps,this);
	  std::thread threadSendTriggerSignalsAux=std::thread(&GPIO::SendTriggerSignals,this);
	  threadReadTimeStampsAux.join();	
	  threadSendTriggerSignalsAux.join();
	  //this->DDRdumpdata(); // Store to file
	  
	  //munmap(ddrMem, 0x0FFFFFFF); // remove any mappings for those entire pages containing any part of the address space of the process starting at addr and continuing for len bytes. 
	  //close(mem_fd); // Device
	  streamDDRpru.close();
	  prussdrv_pru_disable(PRU_Signal_NUM);
	  prussdrv_pru_disable(PRU_Operation_NUM);  
	  prussdrv_exit();*/
}

int GPIO::ReadTimeStamps(){// Read the detected timestaps in four channels
// Important, the following line at the very beggining to reduce the command jitter
pru0dataMem_int[0]=static_cast<unsigned int>(1); // set command
pru0dataMem_int[1]=this->NumRecords; // set number captures
prussdrv_pru_send_event(21);//pru0dataMem_int[1]=(unsigned int)2; // set to 2 means perform capture

this->TimePointClockCurrentPRU0meas=Clock::now();// To time stamp the current measurement, in contrast ot th eold last measurement

retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);

//this->TimePointClockCurrentPRU0measOld=Clock::now();// To time stamp the current measurement

//cout << "retInterruptsPRU0: " << retInterruptsPRU0 << endl;
if (retInterruptsPRU0>0){
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	this->DDRdumpdata(); // Store to file
}
else if (retInterruptsPRU0==0){
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "GPIO::ReadTimeStamps took to much time for the TimeTagg. Timetags might be inaccurate. Reset PRUO if necessary." << endl;
}
else{
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "PRU0 interrupt poll error" << endl;
}
/*
FutureTimePointPRU0 = Clock::now()+std::chrono::milliseconds(WaitTimeToFutureTimePointPRU0);
auto duration_since_epochFutureTimePointPRU0=FutureTimePointPRU0.time_since_epoch();
auto duration_since_epochTimeNowPRU0=FutureTimePointPRU0.time_since_epoch(); //just for initialization
// Convert duration to desired time
TimePointFuture_time_as_countPRU0 = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePointPRU0).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 

CheckTimeFlagPRU0=false;

finPRU0=false;
do // This is blocking
{
TimePointClockNowPRU0=Clock::now();
duration_since_epochTimeNowPRU0=TimePointClockNowPRU0.time_since_epoch();
TimeNow_time_as_countPRU0 = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNowPRU0).count();

if (TimeNow_time_as_countPRU0>TimePointFuture_time_as_countPRU0){CheckTimeFlagPRU0=true;}
else{CheckTimeFlagPRU0=false;}
	if (pru0dataMem_int[1] == (unsigned int)1 and CheckTimeFlagPRU0==false)// Seems that it checks if it has finished the acquisition
	{
		pru0dataMem_int[1] = (unsigned int)0; // Here clears the value
		this->DDRdumpdata(); // Store to file		
		finPRU0=true;
	}
	else if (CheckTimeFlagPRU0==true){// too much time
		pru0dataMem_int[1]=(unsigned int)0; // set to zero means no command.
		//prussdrv_pru_disable() will reset the program counter to 0 (zero), while after prussdrv_pru_reset() you can resume at the current position.
		//prussdrv_pru_reset(PRU_Operation_NUM);
		//prussdrv_pru_disable(PRU_Operation_NUM);// Disable the PRU
		//prussdrv_pru_enable(PRU_Operation_NUM);// Enable the PRU from 0		
		cout << "GPIO::ReadTimeStamps took to much time for the TimeTagg. Timetags might be inaccurate. Reset PRUO if necessary." << endl;
		//sleep(10);// Give some time to load programs in PRUs and initiate
		finPRU0=true;
	}
} while(!finPRU0);
*/

return 0;// all ok
}

int GPIO::SendTriggerSignals(){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
// Important, the following line at the very beggining to reduce the command jitter
pru1dataMem_int[0]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
pru1dataMem_int[1]=static_cast<unsigned int>(1); // set command
prussdrv_pru_send_event(22);//pru1dataMem_int[1]=(unsigned int)2; // set to 2 means perform signals//prussdrv_pru_send_event(22);

// Here there should be the instruction command to tell PRU1 to start generating signals
// We have to define a command, compatible with the memoryspace of PRU0 to tell PRU1 to initiate signals

retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);
//cout << "retInterruptsPRU1: " << retInterruptsPRU1 << endl;
if (retInterruptsPRU1>0){
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
}
else if (retInterruptsPRU1==0){
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "GPIO::SendTriggerSignals took to much time. Reset PRU1 if necessary." << endl;
}
else{
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "PRU1 interrupt error" << endl;
}

/*
FutureTimePointPRU1 = Clock::now()+std::chrono::milliseconds(WaitTimeToFutureTimePointPRU1);
auto duration_since_epochFutureTimePointPRU1=FutureTimePointPRU1.time_since_epoch();
auto duration_since_epochTimeNowPRU1=FutureTimePointPRU1.time_since_epoch();// JUST FOR INITIALIZATION
// Convert duration to desired time
TimePointFuture_time_as_countPRU1 = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePointPRU1).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds)
//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;

CheckTimeFlagPRU1=false;

// Here we should wait for the PRU1 to finish, we can check it with the value modified in command
finPRU1=false;
do // This is blocking
{
	TimePointClockNowPRU1=Clock::now();
	duration_since_epochTimeNowPRU1=TimePointClockNowPRU1.time_since_epoch();
	TimeNow_time_as_countPRU1 = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNowPRU1).count();
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	if (TimeNow_time_as_countPRU1>TimePointFuture_time_as_countPRU1){CheckTimeFlagPRU1=true;}
	else{CheckTimeFlagPRU1=false;}
	//cout << "CheckTimeFlag: " << CheckTimeFlag << endl;
	if (pru1dataMem_int[1] == (unsigned int)1 and CheckTimeFlagPRU1==false)// Seems that it checks if it has finished the sequence
	{	
		pru1dataMem_int[1] = (unsigned int)0; // Here clears the value
		//cout << "GPIO::SendTriggerSignals finished" << endl;
		finPRU1=true;
	}
	else if (CheckTimeFlagPRU1==true){// too much time		
		pru1dataMem_int[1]=(unsigned int)0; // set to zero means no command.	
		//prussdrv_pru_disable() will reset the program counter to 0 (zero), while after prussdrv_pru_reset() you can resume at the current position.
		//prussdrv_pru_reset(PRU_Signal_NUM);
		//prussdrv_pru_disable(PRU_Signal_NUM);// Disable the PRU
		//prussdrv_pru_enable(PRU_Signal_NUM);// Enable the PRU from 0		
		cout << "GPIO::SendTriggerSignals took to much time. Reset PRU1 if necessary." << endl;
		//sleep(10);// Give some time to load programs in PRUs and initiate
		finPRU1=true;
		}
} while(!finPRU1);
*/

return 0;// all ok	
}

int GPIO::SendEmulateQubits(){ // Emulates sending 2 entangled qubits through the 8 output pins (each qubits needs 4 pins)

return 0;// all ok
}

//PRU0 - Operation - getting iputs

int GPIO::DDRdumpdata(){
// Time elapsed between consecutive measurements
if (this->TimePointClockCurrentPRU0measOld==std::chrono::time_point<Clock>()){// First measurement
	this->TimeElpasedNow_time_as_count=0;
}
else{
	auto duration_since_lastMeasTime=this->TimePointClockCurrentPRU0meas.time_since_epoch()-this->TimePointClockCurrentPRU0measOld.time_since_epoch();
// Convert duration to desired time
	TimeElpasedNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_lastMeasTime).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds)
	cout << "TimeElpasedNow_time_as_count: " << TimeElpasedNow_time_as_count << endl;
}
this->TimePointClockCurrentPRU0measOld=this->TimePointClockCurrentPRU0meas;// Update old meas timestamp

// Reading data from PRU shared and own RAMs
//DDR_regaddr = (short unsigned int*)ddrMem + OFFSET_DDR;
valp=valpHolder; // Coincides with SHARED in PRUassTaggDetScript.p
valpAux=valpAuxHolder;
synchp=synchpHolder;
//for each capture bursts, at the beggining is stored the overflow counter of 32 bits. From there, each capture consists of 32 bits of the DWT_CYCCNT register and 8 bits of the channels detected (40 bits per detection tag).
// The shared memory space has 12KB=12×1024bytes=12×1024×8bits=98304bits.
//Doing numbers, we can store up to 2456 captures. To be in the safe side, we can do 2048 captures

///////////////////////////////////////////////////////////////////////////////////////
// Checking control - Clock skew and threshold
valSkewCounts=static_cast<unsigned int>(*valpAux);
valpAux++;// 1 times 8 bits
valSkewCounts=valSkewCounts | (static_cast<unsigned int>(*valpAux))<<8;
valpAux++;// 1 times 8 bits
valSkewCounts=valSkewCounts | (static_cast<unsigned int>(*valpAux))<<16;
valpAux++;// 1 times 8 bits
valSkewCounts=valSkewCounts | (static_cast<unsigned int>(*valpAux))<<24;
valpAux++;// 1 times 8 bits
cout << "valSkewCounts: " << valSkewCounts << endl;

valThresholdResetCounts=static_cast<unsigned int>(*valpAux);
valpAux++;// 1 times 8 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<8;
valpAux++;// 1 times 8 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<16;
valpAux++;// 1 times 8 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<24;
valpAux++;// 1 times 8 bits
cout << "valThresholdResetCounts: " << valThresholdResetCounts << endl;
//////////////////////////////////////////////////////////////////////////////*/

// First 32 bits is the overflow register for DWT_CYCCNT
valOverflowCycleCountPRU=static_cast<unsigned int>(*valp);
valp++;// 1 times 8 bits
valOverflowCycleCountPRU=valOverflowCycleCountPRU | (static_cast<unsigned int>(*valp))<<8;
valp++;// 1 times 8 bits
valOverflowCycleCountPRU=valOverflowCycleCountPRU | (static_cast<unsigned int>(*valp))<<16;
valp++;// 1 times 8 bits
valOverflowCycleCountPRU=valOverflowCycleCountPRU | (static_cast<unsigned int>(*valp))<<24;
valp++;// 1 times 8 bits
valOverflowCycleCountPRU=valOverflowCycleCountPRU-1;//Account that it starts with a 1 offset
//cout << "valOverflowCycleCountPRU: " << valOverflowCycleCountPRU << endl;

auxUnskewingFactorResetCycle=auxUnskewingFactorResetCycle+static_cast<unsigned long long int>(valSkewCounts)+2;//static_cast<unsigned long long int>(valOverflowCycleCountPRU-valOverflowCycleCountPRUold)*static_cast<unsigned long long int>(valSkewCounts); // Related to the number of instruction/cycles when a reset happens and are lost the counts; // 64 bits. The unskewing is for the deterministic part. The undeterministic part is accounted with valCarryOnCycleCountPRU. This parameter can be adjusted by setting it to 0 and running the analysis of synch and checking the periodicity and also it is better to do it with Precise Time Protocol activated (to reduce the clock difference drift).
valOverflowCycleCountPRUold=valOverflowCycleCountPRU; // Update
if (valOverflowCycleCountPRU>0){
extendedCounterPRUaux=((static_cast<unsigned long long int>(valOverflowCycleCountPRU)) << 31) + auxUnskewingFactorResetCycle + this->valCarryOnCycleCountPRU;// 31 because the overflow counter is increment every half the maxium time for clock (to avoid overflows during execution time)
}
else{
extendedCounterPRUaux=auxUnskewingFactorResetCycle + this->valCarryOnCycleCountPRU;// 31 because the overflow counter is increment every half the maxium time for clock (to avoid overflows during execution time)
}

// Reading or not Synch pulses
NumSynchPulses=static_cast<unsigned int>(*synchp);
synchp++;
//cout << "This slows down and unsynchronizes (comment) GPIO::NumSynchPulses: " << NumSynchPulses << endl;
if (NumSynchPulses>0){// There are synch pulses
	if (streamSynchpru.is_open()){
		streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations		
		streamSynchpru.write(reinterpret_cast<const char*>(&NumSynchPulses), sizeof(NumSynchPulses));
		for (unsigned int iIterSynch=0;iIterSynch<NumSynchPulses;iIterSynch++){
			valCycleCountPRU=static_cast<unsigned int>(*synchp);
			synchp++;// 1 times 32 bits
			extendedCounterPRU=extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);			
			streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			streamSynchpru.write(reinterpret_cast<const char*>(&extendedCounterPRU), sizeof(extendedCounterPRU));
		}
	}
	else{
		cout << "DDRdumpdata streamSynchpru is not open!" << endl;
	}
}

// Reading TimeTaggs
if (streamDDRpru.is_open()){
	streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	for (iIterDump=0; iIterDump<NumRecords; iIterDump++){
		// First 32 bits is the DWT_CYCCNT of the PRU
		valCycleCountPRU=static_cast<unsigned int>(*valp);
		valp++;// 1 times 8 bits
		valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<8;
		valp++;// 1 times 8 bits
		valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<16;
		valp++;// 1 times 8 bits
		valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<24;
		valp++;// 1 times 8 bits
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "valCycleCountPRU: " << valCycleCountPRU << endl;}
		// Mount the extended counter value
		extendedCounterPRU=extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "extendedCounterPRU: " << extendedCounterPRU << endl;}
		// Then, the last 32 bits is the channels detected. Equivalent to a 63 bit register at 5ns per clock equates to thousands of years before overflow :)
		valBitsInterest=static_cast<unsigned char>(*valp);
		valp++;// 1 times 8 bits
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "val: " << std::bitset<8>(val) << endl;}
		//valBitsInterest=this->packBits(val); // we're just interested in 4 bits
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "valBitsInterest: " << std::bitset<16>(valBitsInterest) << endl;}	
		//fprintf(outfile, "%d\n", val);
		streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
		streamDDRpru.write(reinterpret_cast<const char*>(&extendedCounterPRU), sizeof(extendedCounterPRU));
		streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
		streamDDRpru.write(reinterpret_cast<const char*>(&valBitsInterest), sizeof(valBitsInterest));
		//streamDDRpru << extendedCounterPRU << valBitsInterest << endl;
	}
}
else{
	cout << "DDRdumpdata streamDDRpru is not open!" << endl;
}

// Store the last IEP counter carry over if it exceed 0x7FFFFFFF; Maybe deterministically account a lower limit since there are operations that will make it pass
// The number below is an estimation since there are instructions that are not accounted for
if (this->FirstTimeDDRdumpdata or this->valThresholdResetCounts==0){this->AfterCountsThreshold=13+0;}// First time the Threshold reset counts of the timetagg is not well computed, hence estimated as the common value
else{this->AfterCountsThreshold=this->valThresholdResetCounts+0;};//0x00000000;// Related to the number of instruciton counts after the last read of the counter. It is a parameter to adjust
this->FirstTimeDDRdumpdata=false;
if (valCycleCountPRU >= (0x80000000-this->AfterCountsThreshold)){// The counts that we will lose because of the reset
this->valCarryOnCycleCountPRU=this->valCarryOnCycleCountPRU+static_cast<unsigned long long int>((this->AfterCountsThreshold+valCycleCountPRU)-0x80000000);//static_cast<unsigned long long int>(valCycleCountPRU & 0x7FFFFFFF);
}

//cout << "valCarryOnCycleCountPRU: " << valCarryOnCycleCountPRU << endl;

//cout << "sharedMem_int: " << sharedMem_int << endl;

return 0; // all ok
}

// Function to pack bits 1, 2, 3, and 5 of an unsigned int into a single byte
unsigned char GPIO::packBits(unsigned char value) {
    // Isolate bits 1, 2, 3, and 5 and shift them to their new positions
    unsigned char bit1 = (value >> 1) & 0x01; // Bit 0 stays in position 0
    unsigned char bit2 = (value >> 1) & 0x02; // Bit 2 shifts to position 1
    unsigned char bit3 = (value >> 1) & 0x04; // Bit 3 shifts to position 2
    unsigned char bit5 = (value >> 2) & 0x08; // Bit 5 shifts to position 3, skipping the original position of bit 4

    // Combine the bits into a single byte
    return bit1 | bit2 | bit3 | bit5;
}

int GPIO::ClearStoredQuBits(){
// Timetagging data
if (streamDDRpru.is_open()){
	streamDDRpru.close();	
	//streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
}

streamDDRpru.open(string(PRUdataPATH1) + string("TimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content	
if (!streamDDRpru.is_open()) {
	streamDDRpru.open(string(PRUdataPATH2) + string("TimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
	if (!streamDDRpru.is_open()) {
        	cout << "Failed to re-open the streamDDRpru file." << endl;
        	return -1;
        }
}
streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
streamDDRpru.seekp(0, std::ios::beg); // the put (writing) pointer back to the start!

// Synch data
if (streamSynchpru.is_open()){
	streamSynchpru.close();	
	//streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
}

streamSynchpru.open(string(PRUdataPATH1) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content	
if (!streamSynchpru.is_open()) {
	streamSynchpru.open(string(PRUdataPATH2) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
	if (!streamSynchpru.is_open()) {
        	cout << "Failed to re-open the streamSynchpru file." << endl;
        	return -1;
        }
}
streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
streamSynchpru.seekp(0, std::ios::beg); // the put (writing) pointer back to the start!
return 0; // all ok
}

int GPIO::RetrieveNumStoredQuBits(unsigned long long int* TimeTaggs, unsigned char* ChannelTags){
unsigned int ValueReadNumSynchPulses;
int NumSynchPulseAvgAux=0;
	// Synch taggs
	if (streamSynchpru.is_open()){
		streamSynchpru.close();	
		//streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	}

	streamSynchpru.open(string(PRUdataPATH1) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out);// Open for write and read, and clears all previous content	
	if (!streamSynchpru.is_open()) {
		streamSynchpru.open(string(PRUdataPATH2) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out);// Open for write and read, and clears all previous content
		if (!streamSynchpru.is_open()) {
			cout << "Failed to re-open the streamSynchpru file." << endl;
			return -1;
		}
	}
	streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations

	if (streamSynchpru.is_open()){
		streamSynchpru.seekg(0, std::ios::beg); // the get (reading) pointer back to the start!
		int lineCount = 0;		
		streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
		while (streamSynchpru.read(reinterpret_cast<char*>(&ValueReadNumSynchPulses), sizeof(ValueReadNumSynchPulses)) and lineCount<MaxNumPulses){		
			for (int iIter=0;iIter<ValueReadNumSynchPulses;iIter++){
				//cout << "GPIO::ValueReadNumSynchPulses: " << ValueReadNumSynchPulses << endl;
				streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations				
			    	streamSynchpru.read(reinterpret_cast<char*>(&SynchPulsesTags[lineCount]), sizeof(SynchPulsesTags[lineCount]));
			    	lineCount++; // Increment line count for each line read			    	    
		    	}
		    	streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
		}
		NumSynchPulsesRed=lineCount;		
		if (NumSynchPulsesRed>1){//At least two points (two cycles separated by one cycle in between), we can generate a calibration curve
			//unsigned long long int CoeffSynchAdjAux0=1;
			//double CoeffSynchAdjAux1=0.0;// Number theoretical counts given the number of cycles
			double CoeffSynchAdjAux2=0.0;// PRU counting
			double CoeffSynchAdjAux3=0.0;// Number theoretical counts given the number of cycles
			//double CoeffSynchAdjAux4=0.0;// Number theoretical counts given the number of cycles
			for (int iIter=0;iIter<(NumSynchPulsesRed-1);iIter++){
				//CoeffSynchAdjAux0=(unsigned long long int)(((double)(SynchPulsesTags[iIter+2]-SynchPulsesTags[iIter+1])+PeriodCountsPulseAdj/2.0)/PeriodCountsPulseAdj); // Distill how many pulse synch periods passes...1, 2, 3....To round ot the nearest integer value add half of the dividend to the divisor
				//CoeffSynchAdjAux1=(double)(CoeffSynchAdjAux0)*PeriodCountsPulseAdj;//((double)((SynchPulsesTags[iIter+1]-SynchPulsesTags[iIter])/((unsigned long long int)(PeriodCountsPulseAdj))));
				CoeffSynchAdjAux3=(double)((unsigned long long int)(((double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[iIter+0])+PeriodCountsPulseAdj/2.0)/PeriodCountsPulseAdj))*PeriodCountsPulseAdj; // Distill how many pulse synch periods passes...1, 2, 3....To round ot the nearest integer value add half of the dividend to the divisor
				//CoeffSynchAdjAux4=(double)((unsigned long long int)(((double)(SynchPulsesTags[iIter+2]-SynchPulsesTags[iIter+1])+PeriodCountsPulseAdj/2.0)/PeriodCountsPulseAdj))*PeriodCountsPulseAdj; // Distill how many pulse synch periods passes...1, 2, 3....To round ot the nearest integer value add half of the dividend to the divisor
				//if (CoeffSynchAdjAux3!=0.0 and CoeffSynchAdjAux4!=0.0){CoeffSynchAdjAux2=(double)(SynchPulsesTags[iIter+2]-SynchPulsesTags[iIter+1])/CoeffSynchAdjAux4-(double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[iIter+0])/CoeffSynchAdjAux3;}
				if (CoeffSynchAdjAux3!=0.0){CoeffSynchAdjAux2=(double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[iIter+0])/CoeffSynchAdjAux3;}
				if (CoeffSynchAdjAux3!=0.0){// and CoeffSynchAdjAux4!=0.0){
					AdjPulseSynchCoeffArray[NumSynchPulseAvgAux]=CoeffSynchAdjAux2;//sqrt(CoeffSynchAdjAux2);//AdjPulseSynchCoeff+(CoeffSynchAdjAux2/CoeffSynchAdjAux1);					
					//cout << "AdjPulseSynchCoeffArray[NumAvgAux]: " << AdjPulseSynchCoeffArray[NumAvgAux] << endl;
					SynchPulsesTagsUsed[NumSynchPulseAvgAux]=SynchPulsesTags[iIter+1];
					NumSynchPulseAvgAux++;
				}
			}
			//cout << "PeriodCountsPulseAdj: " << PeriodCountsPulseAdj << endl;
			//cout << "Last CoeffSynchAdjAux0: " << CoeffSynchAdjAux0 << endl;
			//cout << "Last CoeffSynchAdjAux1: " << CoeffSynchAdjAux1 << endl;
			cout << "Last CoeffSynchAdjAux2: " << CoeffSynchAdjAux2 << endl;
			cout << "Last CoeffSynchAdjAux3: " << CoeffSynchAdjAux3 << endl;
			//cout << "Last CoeffSynchAdjAux4: " << CoeffSynchAdjAux4 << endl;
			if (NumSynchPulseAvgAux>0){
				AdjPulseSynchCoeffAverage=this->DoubleMeanFilterSubArray(AdjPulseSynchCoeffArray,NumSynchPulseAvgAux);
				cout << "AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
			}// Mean average//this->DoubleMedianFilterSubArray(AdjPulseSynchCoeffArray,NumAvgAux);//Median AdjPulseSynchCoeff/((double)(NumAvgAux));}// Average
			else{AdjPulseSynchCoeffAverage=1.0;}// Reset
			cout << "GPIO: AdjPulseSynchCoeff: " << AdjPulseSynchCoeffAverage << endl;
		}
		else{
			AdjPulseSynchCoeffAverage=1.0;
			cout << "GPIO: AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
		}
		if (NumSynchPulsesRed>=MaxNumPulses){cout << "Too many pulses stored, increase buffer size or reduce number pulses: " << NumSynchPulsesRed << endl;}
	    	else if (NumSynchPulsesRed==0){cout << "RetrieveNumStoredQuBits: No Synch pulses present!" << endl;}
	    	cout << "GPIO: NumSynchPulsesRed: " << NumSynchPulsesRed << endl;
	}
	else{
		cout << "RetrieveNumStoredQuBits: BBB streamSynchpru is not open!" << endl;
	return -1;
	}

	// Detection tags
	if (streamDDRpru.is_open()){
		streamDDRpru.close();	
		//streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	}

	streamDDRpru.open(string(PRUdataPATH1) + string("TimetaggingData"), std::ios::binary | std::ios::in | std::ios::out);// Open for write and read, and clears all previous content	
	if (!streamDDRpru.is_open()) {
		streamDDRpru.open(string(PRUdataPATH2) + string("TimetaggingData"), std::ios::binary | std::ios::in | std::ios::out);// Open for write and read, and clears all previous content
		if (!streamDDRpru.is_open()) {
			cout << "Failed to re-open the streamDDRpru file." << endl;
			return -1;
		}
	}
	streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations

	if (streamDDRpru.is_open()){
		streamDDRpru.seekg(0, std::ios::beg); // the get (reading) pointer back to the start!
		int lineCount = 0;
		unsigned long long int ValueReadTest;
		streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
		int iIterMovAdjPulseSynchCoeff=0;
		while (streamDDRpru.read(reinterpret_cast<char*>(&ValueReadTest), sizeof(ValueReadTest))) {// While true == not EOF
		    // Apply pulses time drift correction
		    // using doubles, to represent usigned long long int can hold, with the 5ns PRU count, up to 2 years with presition!!!
		    // Aimply apply the average value for adjusting synch pulses
		    // AdjPulseSynchCoeff=AdjPulseSynchCoeffAverage;
		    //TimeTaggs[lineCount]=(unsigned long long int)(((double)(ValueReadTest))/AdjPulseSynchCoeff); // Simply apply the average value of Synch pulses
		    // Advanced application of the AdjPulseSynchCoeff per ranges
		    if (NumSynchPulsesRed>1){
			    if (ValueReadTest<=SynchPulsesTagsUsed[iIterMovAdjPulseSynchCoeff]){
			    	AdjPulseSynchCoeff=AdjPulseSynchCoeffArray[iIterMovAdjPulseSynchCoeff];}// Use the value of adjust synch
			    else{// Increase
			    	if (iIterMovAdjPulseSynchCoeff<(NumSynchPulseAvgAux-1)){
			    	iIterMovAdjPulseSynchCoeff++;
			    	}
			    	AdjPulseSynchCoeff=AdjPulseSynchCoeffArray[iIterMovAdjPulseSynchCoeff];
			    }
		    }
		    
		    if (lineCount==0){
		    	TimeTaggs[0]=(unsigned long long int)(((double)(ValueReadTest))/AdjPulseSynchCoeff);
		    	
		    	} // Simply apply the average value of Synch pulses
		    else{// Not the first tagg
		    	TimeTaggs[lineCount]=(unsigned long long int)(((double)(ValueReadTest-OldLastTimeTagg))/AdjPulseSynchCoeff)+TimeTaggs[lineCount-1];
		    }
		    OldLastTimeTagg=ValueReadTest;
		    
		    streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	    	    streamDDRpru.read(reinterpret_cast<char*>(&ChannelTags[lineCount]), sizeof(ChannelTags[lineCount]));
	    	    //cout << "TimeTaggs[lineCount]: " << TimeTaggs[lineCount] << endl;
	    	    //cout << "ChannelTags[lineCount]: " << ChannelTags[lineCount] << endl;
	    	    lineCount++; // Increment line count for each line read
	    	    streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations 	    
	    	    }
	    	if (lineCount==0){cout << "RetrieveNumStoredQuBits: No timetaggs present!" << endl;}
		return lineCount;
	}
	else{
		cout << "RetrieveNumStoredQuBits: BBB streamDDRpru is not open!" << endl;
	return -1;
}

}

double GPIO::DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor){
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
int GPIO::DoubleBubbleSort(double* arr,int MedianFilterFactor) {
    for (int i = 0; i < MedianFilterFactor-1; i++) {
        for (int j = 0; j < MedianFilterFactor-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
                double temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0; // All ok
}

double GPIO::DoubleMeanFilterSubArray(double* ArrayHolderAux,int MeanFilterFactor){
if (MeanFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    double temp=0.0;
    for(int i = 0; i < MeanFilterFactor; i++) {
        temp = temp + ArrayHolderAux[i];
    }
    
    temp=temp/((double)(MeanFilterFactor));
    return temp;
}
}

int GPIO::SendTriggerSignalsSelfTest(){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
// Important, the following line at the very beggining to reduce the command jitter
pru1dataMem_int[0]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
pru1dataMem_int[1]=static_cast<unsigned int>(1); // set command
prussdrv_pru_send_event(22);//pru1dataMem_int[1]=(unsigned int)2; // set to 2 means perform signals//prussdrv_pru_send_event(22);

// Here there should be the instruction command to tell PRU1 to start generating signals
// We have to define a command, compatible with the memoryspace of PRU0 to tell PRU1 to initiate signals

retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);
//cout << "retInterruptsPRU1: " << retInterruptsPRU1 << endl;

prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations

return 0;// all ok	
}
/*****************************************************************************
* Local Function Definitions                                                 *
*****************************************************************************/

int GPIO::LOCAL_DDMinit(){
    //void *DDR_regaddr1, *DDR_regaddr2, *DDR_regaddr3;    
    prussdrv_map_prumem(PRUSS0_SHARED_DATARAM, &sharedMem);// Maps the PRU shared RAM memory is then accessed by an array.
    sharedMem_int = (unsigned int*) sharedMem;
        
    prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pru0dataMem);// Maps the PRU0 DRAM memory to input pointer. Memory is then accessed by an array.
    pru0dataMem_int = (unsigned int*) pru0dataMem;
    
    prussdrv_map_prumem(PRUSS0_PRU1_DATARAM, &pru1dataMem);// Maps the PRU1 DRAM memory to input pointer. Memory is then accessed by an array.
    pru1dataMem_int = (unsigned int*) pru1dataMem;
    /*
    // open the device 
    mem_fd = open("/dev/mem", O_RDWR);
    if (mem_fd < 0) {
        perror("Failed to open /dev/mem: ");
        return -1;
    }	

    // map the DDR memory
    ddrMem = mmap(0, 0x0FFFFFFF, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, DDR_BASEADDR);
    if (ddrMem == NULL) {
        perror("Failed to map the device: ");
        close(mem_fd);
        return -1;
    }*/
    /*
    mem_fd = open ("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd == -1) {
        printf ("ERROR: could not open /dev/mem.\n\n");
        return -1;
    }
    pru_int = mmap (0, PRU_LEN, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, PRU_ADDR);
    if (pru_int == MAP_FAILED) {
        printf ("ERROR: could not map memory.\n\n");
        return -1;
    }*/
    
    //pru0dataMem_int =     (unsigned int*)pru_int + PRU0_DATARAM/4 + DATARAMoffset/4;   // Points to 0x200 of PRU0 memory
    //pru1dataMem_int =     (unsigned int*)pru_int + PRU1_DATARAM/4 + DATARAMoffset/4;   // Points to 0x200 of PRU1 memory
    //sharedMem_int   = 	  (unsigned int*)pru_int + SHAREDRAM/4; // Points to start of shared memory
    /*
    prussdrv_map_prumem(PRUSS0_PRU0_DATARAM, &pru0dataMem);// Maps the PRU0 DRAM memory to input pointer. Memory is then accessed by an array.
    pru0dataMem_int = (unsigned int*)pru0dataMem;// + DATARAMoffset/4;
    
    sharedMem_int = (unsigned int*)pru0dataMem + SHAREDRAM/4;
    
    prussdrv_map_prumem(PRUSS0_PRU1_DATARAM, &pru1dataMem);// Maps the PRU1 DRAM memory to input pointer. Memory is then accessed by an array.
    pru1dataMem_int = (unsigned int*)pru1dataMem;// + DATARAMoffset/4;   
    */

    return 0;
}

// Operating system GPIO access (slow but simple)
GPIO::GPIO(int number) {
	this->number = number;
	this->debounceTime = 0;
	this->togglePeriod=100;
	this->toggleNumber=-1; //infinite number
	this->callbackFunction = NULL;
	this->threadRunning = false;

	ostringstream s;
	s << "gpio" << number;
	this->name = string(s.str());
	this->path = GPIO_PATH + this->name + "/";
	//this->exportGPIO();
	// need to give Linux time to set up the sysfs structure
	usleep(250000); // 250ms delay
}

int GPIO::write(string path, string filename, string value){
   ofstream fs;
   fs.open((path + filename).c_str());
   if (!fs.is_open()){
	   perror("GPIO: write failed to open file ");
	   return -1;
   }
   fs << value;
   fs.close();
   return 0;
}

string GPIO::read(string path, string filename){
   ifstream fs;
   fs.open((path + filename).c_str());
   if (!fs.is_open()){
	   perror("GPIO: read failed to open file ");
    }
   string input;
   getline(fs,input);
   fs.close();
   return input;
}

int GPIO::write(string path, string filename, int value){
   stringstream s;
   s << value;
   return this->write(path,filename,s.str());
}

//int GPIO::exportGPIO(){
//   return this->write(GPIO_PATH, "export", this->number);
//}

//int GPIO::unexportGPIO(){
//   return this->write(GPIO_PATH, "unexport", this->number);
//}

int GPIO::setDirection(GPIO_DIRECTION dir){
   switch(dir){
   case INPUT: return this->write(this->path, "direction", "in");
      break;
   case OUTPUT:return this->write(this->path, "direction", "out");
      break;
   }
   return -1;
}

int GPIO::setValue(GPIO_VALUE value){
   switch(value){
   case HIGH: return this->write(this->path, "value", "1");
      break;
   case LOW: return this->write(this->path, "value", "0");
      break;
   }
   return -1;
}

int GPIO::setEdgeType(GPIO_EDGE value){
   switch(value){
   case NONE: return this->write(this->path, "edge", "none");
      break;
   case RISING: return this->write(this->path, "edge", "rising");
      break;
   case FALLING: return this->write(this->path, "edge", "falling");
      break;
   case BOTH: return this->write(this->path, "edge", "both");
      break;
   }
   return -1;
}

int GPIO::setActiveLow(bool isLow){
   if(isLow) return this->write(this->path, "active_low", "1");
   else return this->write(this->path, "active_low", "0");
}

int GPIO::setActiveHigh(){
   return this->setActiveLow(false);
}

GPIO_VALUE GPIO::getValue(){
	string input = this->read(this->path, "value");
	if (input == "0") return LOW;
	else return HIGH;
}

GPIO_DIRECTION GPIO::getDirection(){
	string input = this->read(this->path, "direction");
	if (input == "in") return INPUT;
	else return OUTPUT;
}

GPIO_EDGE GPIO::getEdgeType(){
	string input = this->read(this->path, "edge");
	if (input == "rising") return RISING;
	else if (input == "falling") return FALLING;
	else if (input == "both") return BOTH;
	else return NONE;
}

int GPIO::streamInOpen(){
	streamIn.open((path + "value").c_str());
	return 0;
}

int GPIO::streamOutOpen(){
	streamOut.open((path + "value").c_str());
	return 0;
}

int GPIO::streamOutWrite(GPIO_VALUE value){
if (streamOut.is_open())
    {
	streamOut << value << std::flush;
}
else{
cout << "BBB streamOut is not open!" << endl;
}
	return 0;
}

int GPIO::streamInRead(){
	//string StrValue;	
	//streamIn >> StrValue;//std::flush;
	//return stoi(StrValue);
if (streamIn.is_open())
    {
	string StrValue;
	//int IntValue;
	//streamIn >> IntValue;	
	getline(streamIn,StrValue);
	streamIn.clear(); //< Now we can read again
	streamIn.seekg(0, std::ios::beg); // back to the start!
	//cout<<StrValue<<endl;
	//cout<<IntValue<<endl;
	if (StrValue == "0") return LOW;
	else return HIGH;
	//return IntValue;
}
else{
cout << "BBB streamIn is not open!" << endl;
return 0;
}
}

int GPIO::streamInClose(){
	streamIn.close();
	return 0;
}

int GPIO::streamOutClose(){
	streamOut.close();
	return 0;
}

int GPIO::toggleOutput(){
	this->setDirection(OUTPUT);
	if ((bool) this->getValue()) this->setValue(LOW);
	else this->setValue(HIGH);
    return 0;
}

int GPIO::toggleOutput(int time){ return this->toggleOutput(-1, time); }
int GPIO::toggleOutput(int numberOfTimes, int time){
	this->setDirection(OUTPUT);
	this->toggleNumber = numberOfTimes;
	this->togglePeriod = time;
	this->threadRunning = true;
    if(pthread_create(&this->thread, NULL, &threadedToggle, static_cast<void*>(this))){
    	perror("GPIO: Failed to create the toggle thread");
    	this->threadRunning = false;
    	return -1;
    }
    return 0;
}

// This thread function is a friend function of the class
void* threadedToggle(void *value){
	GPIO *gpio = static_cast<GPIO*>(value);
	bool isHigh = (bool) gpio->getValue(); //find current value
	while(gpio->threadRunning){
		if (isHigh)	gpio->setValue(HIGH);
		else gpio->setValue(LOW);
		usleep(gpio->togglePeriod * 500);
		isHigh=!isHigh;
		if(gpio->toggleNumber>0) gpio->toggleNumber--;
		if(gpio->toggleNumber==0) gpio->threadRunning=false;
	}
	return 0;
}

// Blocking Poll - based on the epoll socket code in the epoll man page
int GPIO::waitForEdge(){
	this->setDirection(INPUT); // must be an input pin to poll its value
	int fd, i, epollfd, count=0;
	struct epoll_event ev;
	epollfd = epoll_create(1);
    if (epollfd == -1) {
	   perror("GPIO: Failed to create epollfd");
	   return -1;
    }
    if ((fd = open((this->path + "value").c_str(), O_RDONLY | O_NONBLOCK)) == -1) {
       perror("GPIO: Failed to open file");
       return -1;
    }

    //ev.events = read operation | edge triggered | urgent data
    ev.events = EPOLLIN | EPOLLET | EPOLLPRI;
    ev.data.fd = fd;  // attach the file file descriptor

    //Register the file descriptor on the epoll instance, see: man epoll_ctl
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
       perror("GPIO: Failed to add control interface");
       return -1;
    }
	while(count<=1){  // ignore the first trigger
		i = epoll_wait(epollfd, &ev, 1, -1);
		if (i==-1){
			perror("GPIO: Poll Wait fail");
			count=5; // terminate loop
		}
		else {
			count++; // count the triggers up
		}
	}
    close(fd);
    if (count==5) return -1;
	return 0;
}

// This thread function is a friend function of the class
void* threadedPoll(void *value){
	GPIO *gpio = static_cast<GPIO*>(value);
	while(gpio->threadRunning){
		gpio->callbackFunction(gpio->waitForEdge());
		usleep(gpio->debounceTime * 1000);
	}
	return 0;
}

int GPIO::waitForEdge(CallbackType callback){
	this->threadRunning = true;
	this->callbackFunction = callback;
    // create the thread, pass the reference, address of the function and data
    if(pthread_create(&this->thread, NULL, &threadedPoll, static_cast<void*>(this))){
    	perror("GPIO: Failed to create the poll thread");
    	this->threadRunning = false;
    	return -1;
    }
    return 0;
}

int GPIO::DisablePRUs(){
// Disable PRU and close memory mappings
prussdrv_pru_disable(PRU_Signal_NUM);
prussdrv_pru_disable(PRU_Operation_NUM);

return 0;
}

GPIO::~GPIO() {
//	this->unexportGPIO();
	this->DisablePRUs();
	//fclose(outfile); 
	prussdrv_exit();
	//munmap(ddrMem, 0x0FFFFFFF);
	//close(mem_fd); // Device
	//if(munmap(pru_int, PRU_LEN)) {
	//	cout << "GPIO destructor: munmap failed" << endl;
	//}
	streamDDRpru.close();
	streamSynchpru.close();
}

} /* namespace exploringBB */
