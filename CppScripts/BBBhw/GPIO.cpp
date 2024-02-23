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
#include <sstream> // For istringstream
#include<cstdlib>
#include<cstdio>
#include<fcntl.h>
#include<unistd.h>
#include<sys/epoll.h>
#include <thread>
#include<pthread.h>
// Time/synchronization management
#include <chrono>
// PRU programming
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
#define AM33XX_PRUSS_DRAM_SIZE 8192 // Data RAM
#define AM33XX_PRUSS_SHAREDRAM_SIZE 12000 // Data RAM

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
	if (prussdrv_open(PRU_EVTOUT_0) == -1) {  
	   perror("prussdrv_open() failed. Execute as root: sudo su or sudo. /boot/uEnv.txt has to be properly configured with iuo. Message: "); 
	  } 
	// Map PRU's interrupts
	prussdrv_pruintc_init(&pruss_intc_initdata);
    	
    	// Open file where temporally are stored timetaggs
    	//outfile=fopen("data.csv", "w");
	
	streamDDRpru.open(string(PRUdataPATH1) + string("TimetaggingData"), std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
	
	if (!streamDDRpru.is_open()) {
		streamDDRpru.open(string(PRUdataPATH2) + string("TimetaggingData"), std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
		if (!streamDDRpru.is_open()) {
	        	cout << "Failed to open the streamDDRpru file." << endl;
	        }
	        else{// Clear up the old file
        	streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations	        
	        }
        }
        else{// Clear up the old file
        	streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
        }
	
        // Initialize DDM
	LOCAL_DDMinit(); // DDR (Double Data Rate): A class of memory technology used in DRAM where data is transferred on both the rising and falling edges of the clock signal, effectively doubling the data rate without increasing the clock frequency.
	
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Timetagging
	pru0dataMem_int[0]=(unsigned int)0; // set to zero means no command. PRU0 idle
	    // Execute program
	    // Load and execute the PRU program on the PRU0
	if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScript.bin");
		}
	}
	
	// Generate signals
	pru1dataMem_int[0]=(unsigned int)0; // set to zero means no command. PRU1 idle
	// Load and execute the PRU program on the PRU1
	if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}
	
	  
	  /*// Doing debbuging checks - Debugging 1
	  sleep(2);// Give some time to load programs in PRUs and initiate
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
//unsigned int ret;
//int i;
//void *DDR_paramaddr;
//void *DDR_ackaddr;
//bool fin;
//char fname_new[255];     
//DDR_paramaddr = (short unsigned int*)ddrMem + OFFSET_DDR - 8;
//DDR_ackaddr = (short unsigned int*)ddrMem + OFFSET_DDR - 4;
int WaitTimeToFutureTimePoint=500;

TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds)
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;

TimePoint FutureTimePoint = Clock::now()+std::chrono::milliseconds(WaitTimeToFutureTimePoint);
auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePoint).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 

bool CheckTimeFlag=false;
pru0dataMem_int[0]=(unsigned int)2; // set to 2 means perform capture

bool fin=false;
do // This is blocking
{
TimePointClockNow=Clock::now();
duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count();

if (TimeNow_time_as_count>TimePointFuture_time_as_count){CheckTimeFlag=true;}
else{CheckTimeFlag=false;}
	if (pru0dataMem_int[0] == (unsigned int)1 and CheckTimeFlag==false)// Seems that it checks if it has finished the acquisition
	{
		// we have received the ack!
		this->DDRdumpdata(); // Store to file
		pru0dataMem_int[0] = (unsigned int)0; // Here clears the value
		fin=true;
		//printf("Ack\n");
	}
	else if (CheckTimeFlag==true){// too much time
		pru0dataMem_int[0]=(unsigned int)0; // set to zero means no command.
		//if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScript.bin") == -1){
		//	if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScript.bin") == -1){
		//		perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScript.bin");
		//	}
		//}
		//prussdrv_pru_disable() will reset the program counter to 0 (zero), while after prussdrv_pru_reset() you can resume at the current position.
		//prussdrv_pru_disable(PRU_Operation_NUM);// Disable the PRU
		//prussdrv_pru_enable(PRU_Operation_NUM);// Enable the PRU from 0
		//prussdrv_pru_reset(PRU_Operation_NUM);
		cout << "GPIO::ReadTimeStamps took to much time the TimeTagg. Reset PRUO if necessari." << endl;
		fin=true;
	}
} while(!fin);

		
// Never accomplished because this signals are in the EXIT part of the assembler that never arrive	
//prussdrv_pru_wait_event(PRU_EVTOUT_0);
//prussdrv_pru_clear_event(PRU0_ARM_INTERRUPT); 	
  
return 0;// all ok
}

int GPIO::SendTriggerSignals(){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
// Here there should be the instruction command to tell PRU1 to start generating signals
// We have to define a command, compatible with the memoryspace of PRU0 to tell PRU1 to initiate signals
int WaitTimeToFutureTimePoint=500;

TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds)
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;

TimePoint FutureTimePoint = Clock::now()+std::chrono::milliseconds(WaitTimeToFutureTimePoint);
auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochFutureTimePoint).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds)
//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;

bool CheckTimeFlag=false;
pru1dataMem_int[0]=(unsigned int)2; // set to 2 means perform signals

// Here we should wait for the PRU1 to finish, we can check it with the value modified in command
bool fin=false;
do // This is blocking
{
	TimePointClockNow=Clock::now();
	duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
	TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epochTimeNow).count();
	//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
	if (TimeNow_time_as_count>TimePointFuture_time_as_count){CheckTimeFlag=true;}
	else{CheckTimeFlag=false;}
	//cout << "CheckTimeFlag: " << CheckTimeFlag << endl;
	if (pru1dataMem_int[0] == (unsigned int)1 and CheckTimeFlag==false)// Seems that it checks if it has finished the sequence
	{	
		pru1dataMem_int[0] = (unsigned int)0; // Here clears the value
		//cout << "GPIO::SendTriggerSignals finished" << endl;
		fin=true;
	}
	else if (CheckTimeFlag==true){// too much time		
		pru1dataMem_int[0]=(unsigned int)0; // set to zero means no command.
		//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		//	if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
		//		perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		//	}
		//}		
		//prussdrv_pru_disable() will reset the program counter to 0 (zero), while after prussdrv_pru_reset() you can resume at the current position.
		//prussdrv_pru_disable(PRU_Signal_NUM);// Disable the PRU
		//prussdrv_pru_enable(PRU_Signal_NUM);// Enable the PRU from 0
		//prussdrv_pru_reset(PRU_Signal_NUM);
		cout << "GPIO::SendTriggerSignals took to much time. Reset PRU1 if necessari." << endl;
		fin=true;
		}
} while(!fin);


// Never accomplished because this signals are in the EXIT part of the assembler that never arrive
//prussdrv_pru_wait_event(PRU_EVTOUT_0);
//prussdrv_pru_clear_event(PRU1_ARM_INTERRUPT); 

return 0;// all ok	
}

int GPIO::SendEmulateQubits(){ // Emulates sending 2 entangled qubits through the 8 output pins (each qubits needs 4 pins)

return 0;// all ok
}

//PRU0 - Operation - getting iputs

int GPIO::DDRdumpdata(){
//unsigned short int *DDR_regaddr;
//unsigned char* test;
//int ln;
int x;
//unsigned char tv;

unsigned short int* valp; // 16 bits
unsigned int valCycleCountPRU; // 32 bits // Made relative to each acquition run
unsigned int valOverflowCycleCountPRU; // 32 bits
unsigned long long int extendedCounterPRU; // 64 bits
unsigned long long int auxUnskewingFactor=6; // Related to the number of instruction/cycles when a reset happens and are lost the counts; // 64 bits
//unsigned short int val; // 16 bits
unsigned short int valBitsInterest; // 16 bits
//unsigned char rgb24[4];
//unsigned char v1, v2;
//rgb24[3]=0;

//DDR_regaddr = (short unsigned int*)ddrMem + OFFSET_DDR;
valp=(unsigned short int*)&sharedMem_int[OFFSET_SHAREDRAM]; // Coincides with SHARED in PRUassTaggDetScript.p
unsigned int NumRecords=1024; //Number of records per run. It is also defined in PRUassTaggDetScript.p. 12KB=12×1024bytes=12×1024×8bits=98304bits; maybe a max of 1200 is safe (since each capture takes 80 bits)
for (x=0; x<NumRecords; x++){
	// First 32 bits is the DWT_CYCCNT of the PRU
	valCycleCountPRU=*valp;
	//if (x==0 or x== 512 or x==1023){cout << "valCycleCountPRU: " << valCycleCountPRU << endl;}
	valp=valp+2;// 2 times 16 bits
	// Second 32 bits is the overflow register for DWT_CYCCNT
	valOverflowCycleCountPRU=*valp-1;//Account that it starts with a 1 offset
	//if (x==0 or x== 512 or x==1023){cout << "valOverflowCycleCountPRU: " << valOverflowCycleCountPRU << endl;}
	valp=valp+2;// 2 times 16 bits
	// Mount the extended counter value
	extendedCounterPRU=((static_cast<unsigned long long int>(valOverflowCycleCountPRU)) << 31) + (static_cast<unsigned long long int>(valOverflowCycleCountPRU)*auxUnskewingFactor) + static_cast<unsigned long long int>(valCycleCountPRU);// 31 because the overflow counter is increment every half the maxium time for clock (to avoid overflows during execution time)
	//if (x==0 or x== 512 or x==1023){cout << "extendedCounterPRU: " << extendedCounterPRU << endl;}
	// Then, the last 32 bits is the channels detected. Equivalent to a 63 bit register at 5ns per clock equates to thousands of years before overflow :)
	valBitsInterest=*valp;
	//if (x==0 or x== 512 or x==1023){cout << "val: " << std::bitset<16>(val) << endl;}
	//valBitsInterest=this->packBits(val); // we're just interested in 4 bits
	//if (x==0 or x== 512 or x==1023){cout << "valBitsInterest: " << std::bitset<16>(valBitsInterest) << endl;}
	valp=valp+1;// 1 times 16 bits
	//fprintf(outfile, "%d\n", val);
	streamDDRpru << extendedCounterPRU << valBitsInterest << endl;
}
streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations

//cout << "sharedMem_int: " << sharedMem_int << endl;

return 0; // all ok
}

// Function to pack bits 1, 2, 3, and 5 of an unsigned int into a single byte
unsigned short int GPIO::packBits(unsigned short int value) {
    // Isolate bits 1, 2, 3, and 5 and shift them to their new positions
    unsigned short int bit1 = (value >> 1) & 0x0001; // Bit 0 stays in position 0
    unsigned short int bit2 = (value >> 1) & 0x0002; // Bit 2 shifts to position 1
    unsigned short int bit3 = (value >> 1) & 0x0004; // Bit 3 shifts to position 2
    unsigned short int bit5 = (value >> 2) & 0x0008; // Bit 5 shifts to position 3, skipping the original position of bit 4

    // Combine the bits into a single byte
    return bit1 | bit2 | bit3 | bit5;
}

int GPIO::ClearStoredQuBits(){
if (streamDDRpru.is_open()){
	streamDDRpru.seekp(0, std::ios::beg); // the put (writing) pointer back to the start!
	streamDDRpru<<""; // Really clearing the content of the file
	streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	return 0; // all ok
}
else{
cout << "ClearStoredQuBits: BBB streamDDRpru is not open!" << endl;
return -1;
}
}

int GPIO::RetrieveNumStoredQuBits(unsigned long long int* TimeTaggs, unsigned short int* ChannelTags){
if (streamDDRpru.is_open()){
	streamDDRpru.seekg(0, std::ios::beg); // the get (reading) pointer back to the start!
	string StrLine;
	int lineCount = 0;
	streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	int iIter=0;
        while (getline(streamDDRpru, StrLine)) {// While true
            lineCount++; // Increment line count for each line read
            std::istringstream iss(StrLine); // Use the line as a source for the istringstream
    	    iss >> TimeTaggs[iIter]>> ChannelTags[iIter];
    	    cout << "TimeTaggs[iIter]: " << TimeTaggs[iIter] << endl;
    	    cout << "ChannelTags[iIter]: " << ChannelTags[iIter] << endl;
    	    }
        streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	return lineCount;
}
else{
cout << "RetrieveNumStoredQuBits: BBB streamDDRpru is not open!" << endl;
return -1;
}
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
}

} /* namespace exploringBB */
