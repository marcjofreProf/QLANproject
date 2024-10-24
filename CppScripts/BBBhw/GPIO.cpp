/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Script for PRU real-time handling
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
// Handling priority in task manager
#include<sched.h>
// Time/synchronization management
#include <chrono>
// Mathemtical calculations
#include <cmath>// abs, fmod, fmodl, floor, ceil
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
#define DATARAMoffset 0x00000200 // Offset from Base OWN_RAM to avoid collision with some data. // Already initiated at this position with LOCAL_DDMinit

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
	// Some variable initialization
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
	
	if (SlowMemoryPermanentStorageFlag){
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
	}
	//// Open file where temporally are stored synch - Not used	
	//streamSynchpru.open(string(PRUdataPATH1) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content	
	//if (!streamSynchpru.is_open()) {
	//	streamSynchpru.open(string(PRUdataPATH2) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
	//	if (!streamSynchpru.is_open()) {
	//        	cout << "Failed to open the streamSynchpru file." << endl;
	//        }
        //}
        //
        //if (streamSynchpru.is_open()){
	//	streamSynchpru.close();	
	//	//streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	//}
	
        // Initialize DDM
	LOCAL_DDMinit(); // DDR (Double Data Rate): A class of memory technology used in DRAM where data is transferred on both the rising and falling edges of the clock signal, effectively doubling the data rate without increasing the clock frequency.
	// Here we can update memory space assigned address
	valpHolder=(unsigned short*)&sharedMem_int[OFFSET_SHAREDRAM];
	valpAuxHolder=valpHolder+4+6*NumQuBitsPerRun;// 6* since each detection also includes the channels (2 Bytes) and 4 bytes for 32 bits counter, and plus 4 since the first tag is captured at the very beggining
	CalpHolder=(unsigned int*)&pru0dataMem_int[2];// First tagg captured at the very beggining
	synchpHolder=(unsigned int*)&pru0dataMem_int[3];// Starts at 12
	
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Timetagging
	// Execute program
	// Load and execute the PRU program on the PRU0
	pru0dataMem_int[0]=static_cast<unsigned int>(0); // set no command
	pru0dataMem_int[1]=static_cast<unsigned int>(this->NumQuBitsPerRun); // set number captures, with overflow clock
	pru0dataMem_int[2]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru0dataMem_int[3]=static_cast<unsigned int>(0);
	pru0dataMem_int[4]=static_cast<unsigned int>(PRUoffsetDriftErrorAbsAvgAux); // set periodic offset correction value
	pru0dataMem_int[5]=static_cast<unsigned int>(0); // set synch offset correction value
	//if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScript.bin") == -1){
	//	if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScript.bin") == -1){
	//		perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScript.bin");
	//	}
	//}
	if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScriptSimpleHist4.bin") == -1){
		if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScriptSimpleHist4.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScriptSimpleHist4.bin");
		}
	}
	////prussdrv_pru_enable(PRU_Operation_NUM);
	
	// Generate signals
	pru1dataMem_int[0]=static_cast<unsigned int>(0); // set no command
	pru1dataMem_int[1]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
	pru1dataMem_int[2]=static_cast<unsigned int>(0);// Referenced to the synch trig period
	pru1dataMem_int[3]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru1dataMem_int[4]=static_cast<unsigned int>(PRUoffsetDriftErrorAbsAvgAux); // set periodic offset correction value
	pru1dataMem_int[5]=static_cast<unsigned int>(0); // set synch offset correction value
	pru1dataMem_int[6]=static_cast<unsigned int>(SynchTrigPeriod);
	// Load and execute the PRU program on the PRU1
	if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScriptHist4Sig.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScriptHist4Sig.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScriptHist4Sig.bin");//perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}
	/*
	// Self Test Histogram - comment the PRU1 launching above "generate signals"
	if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScriptHist4SigSelfTest.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScriptHist4SigSelfTest.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScriptHist4SigSelfTest.bin");//perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}
	sleep(10);// Give some time to load programs in PRUs and initiate. Very important, otherwise bad values might be retrieved
	this->SendTriggerSignalsSelfTest(); // Self test initialization
	cout << "Attention doing SendTriggerSignalsSelfTest. To be removed" << endl;	
	*/
	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{cout << "GPIO::Time and frequency synchronization periodic correction selected!" << endl;break;}
		case 2:{cout << "GPIO::Time synchronization periodic correction selected!" << endl;break;}
		case 1:{cout << "GPIO::Frequency synchronization periodic correction selected!" << endl;break;}
		default:{cout << "GPIO::None synchronization periodic correction selected!" << endl;break;}
	}
	cout << "Wait to proceed, calibrating synchronization!" << endl;
	////prussdrv_pru_enable(PRU_Signal_NUM);
	sleep(10);// Give some time to load programs in PRUs and initiate. Very important, otherwise bad values might be retrieved
	
	// Reset values of the sharedMem_int at the beggining
	for (iIterDump=0; iIterDump<((NumQuBitsPerRun/2)*3); iIterDump++){
		sharedMem_int[OFFSET_SHAREDRAM+iIterDump]=static_cast<unsigned int>(0x00000000); // Put it all to zeros
	}
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
	  ///////////////////////////////////////////////////////
	this->setMaxRrPriority();// For rapidly handling interrupts, for the main instance and the periodic thread (only applied to the periodic thread). But it stalls operation in RealTime kernel.
	//////////////////////////////////////////////////////////
}

int GPIO::InitAgentProcess(){
	// Launch periodic synchronization of the IEP timer - like slotted time synchronization protocol
	//this->threadRefSynch=std::thread(&GPIO::PRUsignalTimerSynch,this);// More absolute in time
 	this->threadRefSynch=std::thread(&GPIO::PRUsignalTimerSynchJitterLessInterrupt,this);// reduce the interrupt jitter of non-real-time OS
 	//this->threadRefSynch.detach();// If detach, then at the end comment the join. Otherwise, uncomment the join().
	return 0; //All OK
}
/////////////////////////////////////////////////////////
bool GPIO::setMaxRrPriority(){// For rapidly handling interrupts
	int max_priority=sched_get_priority_max(SCHED_FIFO);
	int Nice_priority=75;// Higher priority. Very important parameter to have stability of the measurements. Slightly smaller than the priorities for clock control (ptp4l,...)
// SCHED_RR: Round robin
// SCHED_FIFO: First-In-First-Out
	sched_param sch_params;
	sch_params.sched_priority = Nice_priority;
	if (sched_setscheduler(0,SCHED_FIFO,&sch_params)==-1){
		cout <<" Failed to set maximum real-time priority." << endl;
		return false;
	}
	return true;
}
////////////////////////////////////////////////////////
void GPIO::acquire() {
/*while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
*/
// https://stackoverflow.com/questions/61493121/when-can-memory-order-acquire-or-memory-order-release-be-safely-removed-from-com
// https://medium.com/@pauljlucas/advanced-thread-safety-in-c-4cbab821356e
//int oldCount;
	unsigned long long int ProtectionSemaphoreTrap=0;
	bool valueSemaphoreExpected=true;
	while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
		ProtectionSemaphoreTrap++;
		if (ProtectionSemaphoreTrap>UnTrapSemaphoreValueMaxCounter){this->release();cout << "GPIO::Releasing semaphore!!!" << endl;}// Avoid trapping situations
		if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
			break;
		}
	}
}

void GPIO::release() {
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}
//////////////////////////////////////////////
struct timespec GPIO::CoincidenceSetWhileWait(){
	struct timespec requestCoincidenceWhileWaitAux;	
	auto duration_since_epochFutureTimePointAux=QPLAFutureTimePointSleep.time_since_epoch();
	// Convert duration to desired time
	unsigned long long int TimePointClockCurrentFinal_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePointAux).count(); // Add an offset, since the final barrier is implemented with a busy wait

	requestCoincidenceWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	requestCoincidenceWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);
	return requestCoincidenceWhileWaitAux;
}

struct timespec GPIO::SetWhileWait(){
	struct timespec requestWhileWaitAux;
	this->TimePointClockCurrentSynchPRU1future=this->TimePointClockCurrentSynchPRU1future+std::chrono::nanoseconds(this->TimePRU1synchPeriod);
	
	auto duration_since_epochFutureTimePoint=this->TimePointClockCurrentSynchPRU1future.time_since_epoch();
	// Convert duration to desired time
	unsigned long long int TimePointClockCurrentFinal_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-this->TimeClockMarging-(2*TimePRUcommandDelay); // Add an offset, since the final barrier is implemented with a busy wait
	//cout << "TimePointClockCurrentFinal_time_as_count: " << TimePointClockCurrentFinal_time_as_count << endl;

	requestWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	requestWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);
	return requestWhileWaitAux;
}

int GPIO::PRUsignalTimerSynchJitterLessInterrupt(){
	this->setMaxRrPriority();// For rapidly handling interrupts, for the main instance and the periodic thread. It stalls operation RealTime Kernel (commented, then)
	this->TimePointClockCurrentSynchPRU1future=Clock::now();// First time
	//SynchRem=static_cast<int>((static_cast<long double>(iepPRUtimerRange32bits)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockCurrentSynchPRU1future.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(iepPRUtimerRange32bits)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	//this->TimePointClockCurrentSynchPRU1future=this->TimePointClockCurrentSynchPRU1future+std::chrono::nanoseconds(SynchRem);
	int duration_FinalInitialMeasTrig=2*ApproxInterruptTime;
	unsigned long long int ULLISynchRem=(static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockCurrentSynchPRU1future.time_since_epoch()).count())/static_cast<unsigned long long int>(TimePRU1synchPeriod)+1)*static_cast<unsigned long long int>(TimePRU1synchPeriod);
	std::chrono::nanoseconds duration_back(ULLISynchRem);
	this->TimePointClockCurrentSynchPRU1future=Clock::time_point(duration_back);	
	this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
	// First setting of time - Probably IEP timer from PRU only accepts resetting to 0	
	//auto duration_since_epochTimeNow=(Clock::now()).time_since_epoch();
	//this->PRUoffsetDriftError=static_cast<double>(fmodl(static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count())/static_cast<unsigned long long int>(TimePRU1synchPeriod)+1)*static_cast<unsigned long long int>(TimePRU1synchPeriod)+static_cast<unsigned long long int>(duration_FinalInitialCountAuxArrayAvg))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits)));
	//this->NextSynchPRUcorrection=static_cast<unsigned int>(static_cast<unsigned int>((static_cast<unsigned long long int>(this->PRUoffsetDriftError)+0*static_cast<unsigned long long int>(LostCounts))%iepPRUtimerRange32bits));
	this->NextSynchPRUcorrection=static_cast<unsigned int>(0);// Resetting to 0
	this->NextSynchPRUcommand=static_cast<unsigned int>(11); // set command 11, do absolute correction
	while(true){		
		clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL);
		//if (Clock::now()<(this->TimePointClockCurrentSynchPRU1future-std::chrono::nanoseconds(this->TimeClockMargingExtra)) and this->ManualSemaphoreExtra==false){// It was possible to execute when needed
		if (this->ManualSemaphoreExtra==false){// It was possible to execute when needed			
			//cout << "Resetting PRUs timer!" << endl;
			//if (clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestWhileWait,NULL)==0 and this->ManualSemaphore==false and this->ResetPeriodicallyTimerPRU1){// Synch barrier. CLOCK_TAI (with steady_clock) instead of CLOCK_REALTIME (with system_clock).//https://opensource.com/article/17/6/timekeeping-linux-vms
			if (this->ResetPeriodicallyTimerPRU1){
				this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
				this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
				// https://www.kernel.org/doc/html/latest/timers/timers-howto.html
				if (((this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux) and abs(duration_FinalInitialMeasTrig)>(ApproxInterruptTime/4) and abs(duration_FinalInitialMeasTrigAuxAvg)>(ApproxInterruptTime/1)) or this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux/2)) and this->iIterPRUcurrentTimerValSynch%2==0){// Initially run many times so that interrupt handling warms up
				//	auto duration_since_epochTimeNow=(Clock::now()).time_since_epoch();
				//	this->PRUoffsetDriftError=static_cast<double>(fmodl(static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count())/static_cast<unsigned long long int>(TimePRU1synchPeriod)+1)*static_cast<unsigned long long int>(TimePRU1synchPeriod)+static_cast<unsigned long long int>(duration_FinalInitialCountAuxArrayAvg))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits)));
				//	this->NextSynchPRUcorrection=static_cast<unsigned int>(static_cast<unsigned int>((static_cast<unsigned long long int>(PRUoffsetDriftError)+0*static_cast<unsigned long long int>(LostCounts))%iepPRUtimerRange32bits));
				//	this->IEPtimerPRUreset=false;//reset flag
					this->NextSynchPRUcorrection=static_cast<unsigned int>(0); // resetting to 0
					this->NextSynchPRUcommand=static_cast<unsigned int>(11);// Hard setting of the time
					this->iIterPRUcurrentTimerVal=0;// reset this value
				//	// Reset values of Abs offset
				//	for (int i=0;i<NumSynchMeasAvgAux;i++){
				//		this->PRUoffsetDriftErrorAbsArray[i]=0.0;
				//	}
				}
				else if ((static_cast<long double>(this->iIterPRUcurrentTimerVal+1)*static_cast<long double>(this->TimePRU1synchPeriod)/static_cast<long double>(PRUclockStepPeriodNanoseconds))>static_cast<long double>(iepPRUtimerRange32bits)){
					this->NextSynchPRUcorrection=static_cast<unsigned int>(0); // resetting to 0
					this->NextSynchPRUcommand=static_cast<unsigned int>(11);// Hard setting of the time
				}
				
				pru1dataMem_int[3]=static_cast<unsigned int>(this->NextSynchPRUcorrection);// apply correction.
				pru1dataMem_int[0]=static_cast<unsigned int>(this->NextSynchPRUcommand); // apply command
				
				//this->TimePointClockSendCommandInitial=this->TimePointClockCurrentSynchPRU1future-0*std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg);
				while(Clock::now() < this->TimePointClockCurrentSynchPRU1future);// Busy waiting
				////this->TimePointClockSendCommandInitial=Clock::now(); // Initial measurement. info. Already computed in the steps before				// Important, the following line at the very beggining to reduce the command jitter				
				prussdrv_pru_send_event(22);
				this->TimePointClockSendCommandFinal=Clock::now(); // Final measurement.			
				retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);// timeout is sufficiently large because it it adjusted when generating signals, not synch whiis very fast (just reset the timer)
				//cout << "PRUsignalTimerSynch: retInterruptsPRU1: " << retInterruptsPRU1 << endl;
				if (retInterruptsPRU1>0){
					prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
				}
				else if (retInterruptsPRU1==0){
					prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
					cout << "GPIO::PRUsignalTimerSynch took to much time. Reset PRU1 if necessary." << endl;
				}
				else{
					prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
					cout << "PRU1 interrupt error" << endl;
				}

				//pru1dataMem_int[2]// Current IEP timer sample
				//pru1dataMem_int[3]// Correction to apply to IEP timer
				this->PRUcurrentTimerValWrap=static_cast<double>(pru1dataMem_int[2]);
				this->PRUcurrentTimerValWrapLong=this->PRUcurrentTimerValWrap;// Update value
				//cout << "GPIO::pru1dataMem_int[2]: " << pru1dataMem_int[2] << endl;
				// Correct for interrupt handling time might add a bias in the estimation/reading
				//this->PRUcurrentTimerValWrap=this->PRUcurrentTimerValWrap-duration_FinalInitialCountAux/static_cast<double>(PRUclockStepPeriodNanoseconds);//this->PRUcurrentTimerValWrap-duration_FinalInitialCountAuxArrayAvg/static_cast<double>(PRUclockStepPeriodNanoseconds);// Remove time for sending command //this->PRUcurrentTimerValWrap-duration_FinalInitialCountAux/static_cast<double>(PRUclockStepPeriodNanoseconds);// Remove time for sending command
				// Unwrap
				if (this->PRUcurrentTimerValWrap<=this->PRUcurrentTimerValOldWrap){this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap+(0xFFFFFFFF-this->PRUcurrentTimerValOldWrap);}
				else{this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap;}

				if (this->PRUcurrentTimerValWrapLong<=this->PRUcurrentTimerValOldWrapLong){this->PRUcurrentTimerValLong=this->PRUcurrentTimerValWrapLong+(0xFFFFFFFF-this->PRUcurrentTimerValOldWrapLong);}
				else{this->PRUcurrentTimerValLong=this->PRUcurrentTimerValWrapLong;}

				if (((this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux) and abs(duration_FinalInitialMeasTrig)>(ApproxInterruptTime/4) and abs(duration_FinalInitialMeasTrigAuxAvg)>(ApproxInterruptTime/1)) or this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux/2)) and this->iIterPRUcurrentTimerValSynch%2==1){// Initially compute the time for interrupt handling
					if (this->iIterPRUcurrentTimerValSynch>=(static_cast<long long int>(NumSynchMeasAvgAux)-2)){
						cout << "GPIO::PRUsignalTimerSynchJitterLessInterrupt initial time synchronization calibration not achieved! Increase number of NumSynchMeasAvgAux." << endl;
					}
					this->QPLAFutureTimePointOld=this->TimePointClockCurrentSynchPRU1future;// Initialization value
					this->QPLAFutureTimePointOld1=this->QPLAFutureTimePointOld; // Initialization value
					this->QPLAFutureTimePointOld2=this->QPLAFutureTimePointOld; // Initialization value
					this->QPLAFutureTimePointOld3=this->QPLAFutureTimePointOld; // Initialization value
					this->QPLAFutureTimePointOld4=this->QPLAFutureTimePointOld; // Initialization value
					this->QPLAFutureTimePointOld5=this->QPLAFutureTimePointOld; // Initialization value
					this->QPLAFutureTimePointOld6=this->QPLAFutureTimePointOld; // Initialization value
					this->QPLAFutureTimePointOld7=this->QPLAFutureTimePointOld; // Initialization value
					//int duration_FinalInitialMeasTrig=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-this->TimePointClockCurrentSynchPRU1future).count());
					duration_FinalInitialMeasTrig=static_cast<int>(this->PRUcurrentTimerValWrap)-static_cast<int>(static_cast<double>(this->iIterPRUcurrentTimerValPass*this->TimePRU1synchPeriod)/static_cast<double>(PRUclockStepPeriodNanoseconds));// static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-this->TimePointClockCurrentSynchPRU1future).count());
					//cout << "GPIO::duration_FinalInitialMeasTrig: " << duration_FinalInitialMeasTrig << endl;
					duration_FinalInitialCountAux=static_cast<double>(duration_FinalInitialMeasTrig);// This final value achieved is subtracted in the absolute time synch readings
					this->duration_FinalInitialMeasTrigAuxArray[TrigAuxIterCount%NumSynchMeasAvgAux]=duration_FinalInitialMeasTrig;
					this->duration_FinalInitialMeasTrigAuxAvg=this->IntMedianFilterSubArray(this->duration_FinalInitialMeasTrigAuxArray,static_cast<int>(this->iIterPRUcurrentTimerValSynch));
					//cout << "GPIO::duration_FinalInitialMeasTrigAuxAvg: " << duration_FinalInitialMeasTrigAuxAvg << endl;			
					// Below for initial synch calculation compensation
					duration_FinalInitialCountAuxArrayAvgInitial=static_cast<double>(duration_FinalInitialMeasTrigAuxAvg);
				}
				else{
					duration_FinalInitialMeasTrig=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-this->TimePointClockCurrentSynchPRU1future).count());
					//cout << "GPIO::duration_FinalInitialMeasTrig: " << duration_FinalInitialMeasTrig << endl;
					duration_FinalInitialCountAux=static_cast<double>(duration_FinalInitialMeasTrig);// This final value achieved is subtracted in the absolute time synch readings
					this->duration_FinalInitialMeasTrigAuxArray[TrigAuxIterCount%NumSynchMeasAvgAux]=duration_FinalInitialMeasTrig;
					this->duration_FinalInitialMeasTrigAuxAvg=this->IntMedianFilterSubArray(this->duration_FinalInitialMeasTrigAuxArray,NumSynchMeasAvgAux);
					//cout << "GPIO::duration_FinalInitialMeasTrigAuxAvg: " << duration_FinalInitialMeasTrigAuxAvg << endl;			
					// Below for synch calculation compensation
					duration_FinalInitialCountAuxArrayAvg=static_cast<double>(duration_FinalInitialMeasTrigAuxAvg);
					if (this->duration_FinalInitialMeasTrigAuxAvg>ApproxInterruptTime and this->iIterPRUcurrentTimerValSynch>(2*NumSynchMeasAvgAux)){// Much longer than for client node (which typically is below 5000) maybe because more effort to serve PTP messages
							cout << "GPIO::Time for pre processing the time barrier is too long " << this->duration_FinalInitialMeasTrigAuxAvg << " ...adjust TimePRUcommandDelay! Set to nominal value of " << static_cast<int>(ApproxInterruptTime) << "..." << endl;
						this->duration_FinalInitialMeasTrigAuxAvg=ApproxInterruptTime;// For the time being adjust it to the nominal initial value
					}	
				}
				this->TrigAuxIterCount++;				
				// Short range measurements to retrieve offsets (little effected by relative frequency difference)
				// Compute error - Absolute corrected error of absolute error after removing the frequency difference. It adds jitter but probably ensures that hardwware clock offsets are removed periodically (a different story is the offset due to links which is calibrated with the algortm).
				// Dealing with lon lon int matters due to floating or not precition!!!!
				long double PRUoffsetDriftErrorAbsAux=0.0;
				PRUoffsetDriftErrorAbsAux=-fmodl(static_cast<long double>(this->iIterPRUcurrentTimerVal)*static_cast<long double>(this->TimePRU1synchPeriod)/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits))+static_cast<long double>(this->PRUcurrentTimerValWrap);//-static_cast<long double>(duration_FinalInitialCountAuxArrayAvgInitial);
				// Below unwrap the difference
				if (PRUoffsetDriftErrorAbsAux>(static_cast<long double>(iepPRUtimerRange32bits)/2.0)){
					PRUoffsetDriftErrorAbsAux=static_cast<long double>(iepPRUtimerRange32bits)-PRUoffsetDriftErrorAbsAux+1;
				}	
				else if(PRUoffsetDriftErrorAbsAux<(-static_cast<long double>(iepPRUtimerRange32bits)/2.0)){
					PRUoffsetDriftErrorAbsAux=-static_cast<long double>(iepPRUtimerRange32bits)-PRUoffsetDriftErrorAbsAux-1;
				}
				if (PRUoffsetDriftErrorAbsAux<0.0){
					this->PRUoffsetDriftErrorAbs=static_cast<double>(-fmodl(-PRUoffsetDriftErrorAbsAux,static_cast<long double>(iepPRUtimerRange32bits)));
				}
				else{
					this->PRUoffsetDriftErrorAbs=static_cast<double>(fmodl(PRUoffsetDriftErrorAbsAux,static_cast<long double>(iepPRUtimerRange32bits)));
				}
				//this->PRUoffsetDriftErrorAbs=this->PRUoffsetDriftErrorAbs*static_cast<long double>(TimePRU1synchPeriod)/static_cast<long double>(1000000000);
				// Absolute corrected error
				this->PRUoffsetDriftErrorAbsArray[iIterPRUcurrentTimerValSynch%ExtraNumSynchMeasAvgAux]=this->PRUoffsetDriftErrorAbs;
				this->PRUoffsetDriftErrorAbsAvg=DoubleMedianFilterSubArray(PRUoffsetDriftErrorAbsArray,ExtraNumSynchMeasAvgAux);// Since we are applying a filter of length NumSynchMeasAvgAux, temporally it effects somehow the longer the filter. Altough it is difficult to correct
								
				if (this->iIterPRUcurrentTimerValPassLong>DistTimePRU1synchPeriod){// Long range measurements to retrieve relative frequency differences
					// Computations for Synch calculation for PRU0 compensation
					// Compute Synch - Relative
					this->EstimateSynch=fmod((static_cast<double>(this->iIterPRUcurrentTimerValPassLong*this->TimePRU1synchPeriod))/static_cast<double>(PRUclockStepPeriodNanoseconds),static_cast<double>(iepPRUtimerRange32bits))/(this->PRUcurrentTimerValLong-this->PRUcurrentTimerValOldWrapLong);// Only correct for PRUcurrentTimerValOld with the PRUoffsetDriftErrorAppliedOldRaw to be able to measure the real synch drift and measure it (not affected by the correction).
					this->EstimateSynchArray[iIterPRUcurrentTimerValSynchLong%NumSynchMeasAvgAux]=this->EstimateSynch;
					this->ManualSemaphoreExtra=true;
					this->EstimateSynchAvg=DoubleMedianFilterSubArray(EstimateSynchArray,NumSynchMeasAvgAux);

					// Frequency synchronization correction
					// Compute error - Relative correction of the frequency difference. This provides like the stability of the hardware clock referenced to the system clock (disciplined with network protocol)...so in the order of 10^-7
					this->PRUoffsetDriftError=(-fmod((static_cast<double>(this->iIterPRUcurrentTimerValPassLong*this->TimePRU1synchPeriod))/static_cast<double>(PRUclockStepPeriodNanoseconds),static_cast<double>(iepPRUtimerRange32bits))+(this->PRUcurrentTimerValLong-this->PRUcurrentTimerValOldWrapLong))/static_cast<long double>(TimePRU1synchPeriod); // The multiplication by SynchTrigPeriod is done before applying it in the Triggering and TimeTagging functions				
					
					// Relative error average
					this->PRUoffsetDriftErrorArray[iIterPRUcurrentTimerValSynchLong%NumSynchMeasAvgAux]=this->PRUoffsetDriftError;
					this->PRUoffsetDriftErrorAvg=DoubleMedianFilterSubArray(PRUoffsetDriftErrorArray,NumSynchMeasAvgAux);

					// Update values
					this->PRUcurrentTimerValOldWrapLong=this->PRUcurrentTimerValWrap;// Update value
					this->iIterPRUcurrentTimerValPassLong=0; // Reset value
					this->iIterPRUcurrentTimerValSynchLong++; // Update value
				}

				// Warm up interrupt handling for Timetagg PRU0
				pru0dataMem_int[0]=static_cast<unsigned int>(8); // set command warm-up
				prussdrv_pru_send_event(21);				
				retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);				

				//cout << "retInterruptsPRU0: " << retInterruptsPRU0 << endl;
				if (retInterruptsPRU0>0){
					prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
				}
				else if (retInterruptsPRU0==0){
					prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
					cout << "GPIO::PRU0 warm-up took to much time for the TimeTagg. Reset PRUO if necessary." << endl;		
				}
				else{
					prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
					cout << "PRU0 interrupt poll error" << endl;
				}
				//	
				this->ManualSemaphoreExtra=false;
				this->ManualSemaphore=false;
				this->release();					
				
				this->iIterPRUcurrentTimerValSynch++;
				this->NextSynchPRUcorrection=0;
				this->NextSynchPRUcommand=static_cast<unsigned int>(10);// set command 10, to execute synch functions no correction
				
				// Updates for next round				
				this->PRUcurrentTimerValOldWrap=this->PRUcurrentTimerValWrap;// Update
				this->iIterPRUcurrentTimerValPass=0; // reset value							
			}				
		} //end if
		
		// Information
		if (this->ResetPeriodicallyTimerPRU1 and (this->iIterPRUcurrentTimerVal%(3*NumSynchMeasAvgAux)==0) and this->iIterPRUcurrentTimerValSynchLong>NumSynchMeasAvgAux){
			////cout << "PRUcurrentTimerVal: " << this->PRUcurrentTimerVal << endl;
			////cout << "PRUoffsetDriftError: " << this->PRUoffsetDriftError << endl;
			cout << "PRUoffsetDriftErrorAvg: " << this->PRUoffsetDriftErrorAvg << endl;
			cout << "PRUoffsetDriftErrorAbsAvg: " << PRUoffsetDriftErrorAbsAvg << endl;
			cout << "duration_FinalInitialCountAuxArrayAvg: " << this->duration_FinalInitialCountAuxArrayAvg << endl;
			////cout << "PRUoffsetDriftErrorIntegral: " << this->PRUoffsetDriftErrorIntegral << endl;
			////cout << "PRUoffsetDriftErrorAppliedRaw: " << this->PRUoffsetDriftErrorAppliedRaw << endl;
			cout << "EstimateSynchAvg: " << this->EstimateSynchAvg << endl;
			////cout << "EstimateSynchDirectionAvg: " << this->EstimateSynchDirectionAvg << endl;
			//if (this->EstimateSynchDirectionAvg<1.0){cout << "Clock EstimateSynch advancing" << endl;}
			//else if (this->EstimateSynchDirectionAvg>1.0){cout << "Clock EstimateSynch delaying" << endl;}
			//else{cout << "Clock EstimateSynch neutral" << endl;}
			////cout << "duration_FinalInitialDriftAux: " << duration_FinalInitialDriftAux << endl;
			////cout << "this->iIterPRUcurrentTimerValPass: "<< this->iIterPRUcurrentTimerValPass << endl;
			////cout << "this->iIterPRUcurrentTimerValSynch: "<< this->iIterPRUcurrentTimerValSynch << endl;
		}		
		this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
		this->iIterPRUcurrentTimerVal++; // Increase value
		this->iIterPRUcurrentTimerValPass++; // Increase value
		this->iIterPRUcurrentTimerValPassLong++; // Increase value
		if (this->iIterPRUcurrentTimerValSynchLong==(2*this->NumSynchMeasAvgAux) and HardwareSynchStatus==false){
			cout << "Hardware synchronized, now proceeding with the network synchronization managed by hosts..." << endl;
			// Update HardwareSynchStatus			
			this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
			HardwareSynchStatus=true;
			this->release();			
		}
	}// end while

return 0; // All ok
}

int GPIO::PIDcontrolerTimeJiterlessInterrupt(){// Not used
//PRUoffsetDriftErrorDerivative=(PRUoffsetDriftErrorAvg-PRUoffsetDriftErrorLast);//*(static_cast<double>(iIterPRUcurrentTimerVal-iIterPRUcurrentTimerValLast));//*(static_cast<double>(this->TimePRU1synchPeriod)/static_cast<double>(PRUclockStepPeriodNanoseconds)));
//PRUoffsetDriftErrorIntegral=PRUoffsetDriftErrorIntegral+PRUoffsetDriftErrorAvg;//*static_cast<double>(iIterPRUcurrentTimerVal-iIterPRUcurrentTimerValLast);//*(static_cast<double>(this->TimePRU1synchPeriod)/static_cast<double>(PRUclockStepPeriodNanoseconds));

	this->PRUoffsetDriftErrorAppliedRaw=PRUoffsetDriftErrorAvg;

if (this->PRUoffsetDriftErrorAppliedRaw<(-this->LostCounts)){this->PRUoffsetDriftErrorApplied=this->PRUoffsetDriftErrorAppliedRaw-LostCounts;}// The LostCounts is to compensate the lost counts in the PRU when applying the update
else if(this->PRUoffsetDriftErrorAppliedRaw>this->LostCounts){this->PRUoffsetDriftErrorApplied=this->PRUoffsetDriftErrorAppliedRaw+LostCounts;}// The LostCounts is to compensate the lost counts in the PRU when applying the update
else{this->PRUoffsetDriftErrorApplied=0;}

return 0; // All ok
}

int GPIO::ReadTimeStamps(int iIterRunsAux,int QuadEmitDetecSelecAux, double SynchTrigPeriodAux,unsigned int NumQuBitsPerRunAux, double* FineSynchAdjValAux, unsigned long long int QPLAFutureTimePointNumber, bool QPLAFlagTestSynchAux){// Read the detected timestaps in four channels
/////////////
	std::chrono::nanoseconds duration_back(QPLAFutureTimePointNumber);
	this->QPLAFlagTestSynch=QPLAFlagTestSynchAux;
	this->QPLAFutureTimePoint=Clock::time_point(duration_back);
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(6*TimePRUcommandDelay);// Give some margin so that ReadTimeStamps and coincide in the respective methods of GPIO. Only for th einitial run, since the TimeStaps are run once (to enter the acquire in GPIO). What consumes time is writting to PRU, then times 4 since 4 writings to PRU before sleep in GPIO
	this->QPLAFutureTimePointSleep=this->QPLAFutureTimePoint;// Update value
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(1*TimePRUcommandDelay);// Crucial to make the link between PRU clock and system clock (already well synchronized). Two memory mapping to PRU
	SynchRem=static_cast<int>((static_cast<long double>(1.5*MultFactorEffSynchPeriod*SynchTrigPeriod)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));// For time stamping it waits 1.5
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(SynchRem);
	SynchTrigPeriod=SynchTrigPeriodAux;// Histogram/Period value
	NumQuBitsPerRun=NumQuBitsPerRunAux;
	valpAuxHolder=valpHolder+4+6*NumQuBitsPerRun;// 6* since each detection also includes the channels (2 Bytes) and 4 bytes for 32 bits counter, and plus 4 since the first tag is captured at the very beggining
	AccumulatedErrorDriftAux=FineSynchAdjValAux[0];// Synch trig offset
	AccumulatedErrorDrift=FineSynchAdjValAux[1]; // Synch trig frequency
	QuadEmitDetecSelecGPIO=QuadEmitDetecSelecAux;// Update value
	//while (this->ManualSemaphoreExtra);// Wait until periodic synch method finishes
	while (this->ManualSemaphore);// Wait other process// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->ManualSemaphoreExtra=true;
	this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->AdjPulseSynchCoeffAverage=this->EstimateSynchAvg;// Acquire this value for the this tag reading set
	///////////
	pru0dataMem_int[2]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and it is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru0dataMem_int[1]=static_cast<unsigned int>(this->NumQuBitsPerRun); // set number captures
	// Different modes of periodic correction
	switch (QuadEmitDetecSelecAux){
		case 1: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld1;break;}
		case 2: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld2;break;}
		case 3: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld3;break;}
		case 4: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld4;break;}
		case 5: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld5;break;}
		case 6: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld6;break;}
		case 7: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld7;break;}
		default: {break;}
	}
	ldTimePointClockTagPRUDiff=static_cast<long double>(PRUoffsetDriftErrorAbsAvgMax)+static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointOld).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value		
	LLITimePointPRUDiffSystemRelFreq=static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointOld).count());
	long long int WrapLLITimePointPRUDiffSystemRelFreq=static_cast<long long int>(static_cast<long double>(LLITimePointPRUDiffSystemRelFreq)*static_cast<long double>(PRUoffsetDriftErrorAvg));
	if (WrapLLITimePointPRUDiffSystemRelFreq<0){
		WrapLLITimePointPRUDiffSystemRelFreq=-(-WrapLLITimePointPRUDiffSystemRelFreq%static_cast<long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	else{
		WrapLLITimePointPRUDiffSystemRelFreq=(WrapLLITimePointPRUDiffSystemRelFreq%static_cast<long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	QPLAFutureTimePointBusyWaitInterrupt=QPLAFutureTimePoint+std::chrono::nanoseconds(WrapLLITimePointPRUDiffSystemRelFreq);
	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{// Time and frequency correction			
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvg+static_cast<double>(ldTimePointClockTagPRUDiff*static_cast<long double>(PRUoffsetDriftErrorAvg));
			break;
		}
		case 2:{// Time correction
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvg;
			ldTimePointClockTagPRUDiff=0;		
			break;
		}
		case 1:{ // Frequency correction
			PRUoffsetDriftErrorAbsAvgAux=static_cast<double>(ldTimePointClockTagPRUDiff*static_cast<long double>(PRUoffsetDriftErrorAvg));
			break;
		}
		default:{PRUoffsetDriftErrorAbsAvgAux=0;ldTimePointClockTagPRUDiff=0;break;}// None time nor frequency correction
	}
	if (PRUoffsetDriftErrorAbsAvgAux<0.0){
		PRUoffsetDriftErrorAbsAvgAux=-fmod(-PRUoffsetDriftErrorAbsAvgAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	else{
		PRUoffsetDriftErrorAbsAvgAux=fmod(PRUoffsetDriftErrorAbsAvgAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	switch (QuadEmitDetecSelecAux){// Update value	
		case 1: {this->QPLAFutureTimePointOld1=this->QPLAFutureTimePoint;break;}
		case 2: {this->QPLAFutureTimePointOld2=this->QPLAFutureTimePoint;break;}
		case 3: {this->QPLAFutureTimePointOld3=this->QPLAFutureTimePoint;break;}
		case 4: {this->QPLAFutureTimePointOld4=this->QPLAFutureTimePoint;break;}
		case 5: {this->QPLAFutureTimePointOld5=this->QPLAFutureTimePoint;break;}
		case 6: {this->QPLAFutureTimePointOld6=this->QPLAFutureTimePoint;break;}
		case 7: {this->QPLAFutureTimePointOld7=this->QPLAFutureTimePoint;break;}
		default: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePoint;break;}
	}

	if (abs(PRUoffsetDriftErrorAbsAvgAux)>PRUoffsetDriftErrorAbsAvgMax){
		cout << "GPIO::PRUoffsetDriftErrorAbsAvgAux magnitude is too large: " << PRUoffsetDriftErrorAbsAvgAux << "Increase PRUoffsetDriftErrorAbsAvgMax. Wrapping PRUoffsetDriftErrorAbsAvgAux around PRUoffsetDriftErrorAbsAvgMax." << endl;
		if (PRUoffsetDriftErrorAbsAvgAux<0.0){
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvgMax-fmod(-PRUoffsetDriftErrorAbsAvgAux,PRUoffsetDriftErrorAbsAvgMax);
		}
		else{
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvgMax+fmod(PRUoffsetDriftErrorAbsAvgAux,PRUoffsetDriftErrorAbsAvgMax);
		}
	}
	else{
		PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvgMax+PRUoffsetDriftErrorAbsAvgAux;
	}
	
	pru0dataMem_int[4]=static_cast<unsigned int>(PRUoffsetDriftErrorAbsAvgAux); // set periodic offset correction value
	// The synch offset
	if (this->AccumulatedErrorDriftAux<0.0){
		AccumulatedErrorDriftAux=(MultFactorEffSynchPeriod*SynchTrigPeriod)-fmod(-this->AccumulatedErrorDriftAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	else{
		AccumulatedErrorDriftAux=(MultFactorEffSynchPeriod*SynchTrigPeriod)+fmod(this->AccumulatedErrorDriftAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	pru0dataMem_int[5]=static_cast<unsigned int>(AccumulatedErrorDriftAux); // set periodic offset correction value
	// The synch rel. freq. diff. 
	// From this point below, the timming is very critical to have long time synchronziation stability!!!!
	InstantCorr=static_cast<long long int>(AccumulatedErrorDrift*SynchTrigPeriod);
	// Attention ldTimePointClockTagPRUinitial is needed in DDRdumpdata!!!!
	ldTimePointClockTagPRUinitial=static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);//+static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod);// Time point after all PRU synch steps. The periodic synch offset is not accounted for
	//InstantCorr=static_cast<long long int>(static_cast<long double>((1.0/6.0)*AccumulatedErrorDrift)*static_cast<long double>(SynchTrigPeriod)*fmodl(ldTimePointClockTagPRUinitial,static_cast<long double>(SynchTrigPeriod)));
	// Atenttion, since Histogram analysis has effectively 4 times the SynchTrigPeriod, for the SynchRem this period is multiplied by 4!!! It will have to be removed
	// The relative frequency difference is not module over MultFactorEffSynchPeriod, since its value is computed over the original synch period
	//InstantCorr=static_cast<long long int>(static_cast<long double>((1.0/6.0)*AccumulatedErrorDrift)*static_cast<long double>(SynchTrigPeriod)*fmodl(ldTimePointClockTagPRUinitial,static_cast<long double>(SynchTrigPeriod)));
	//InstantCorr=static_cast<long long int>(static_cast<long double>((1.0/6.0)*AccumulatedErrorDrift)*static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)*fmodl((ldTimePointClockTagPRUinitial/static_cast<long double>(1000000000)),static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)));

	if (InstantCorr>0){SignAuxInstantCorr=1;}
	else if (InstantCorr<0){SignAuxInstantCorr=-1;}
	else {SignAuxInstantCorr=0;}
	InstantCorr=SignAuxInstantCorr*(abs(InstantCorr)%static_cast<long long int>(SynchTrigPeriod));
	
	pru0dataMem_int[3]=static_cast<unsigned int>(static_cast<long long int>(SynchTrigPeriod)+InstantCorr);// Referenced to the synch trig period

	// Sleep barrier to synchronize the different nodes at this point, so the below calculations and entry times coincide
	requestCoincidenceWhileWait=CoincidenceSetWhileWait();
	clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestCoincidenceWhileWait,NULL); // Synch barrier. So that SendTriggerSignals and ReadTimeStamps of the different nodes coincide	
	pru0dataMem_int[0]=static_cast<unsigned int>(QuadEmitDetecSelecAux); // set command
	//QPLAFutureTimePoint=QPLAFutureTimePoint-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg);// Actually, the time measured duration_FinalInitialMeasTrigAuxAvg is not indicative of much (only if it changes a lot to high values it means trouble)
	
	while (Clock::now()<QPLAFutureTimePointBusyWaitInterrupt);// Busy wait time synch sending signals
	prussdrv_pru_send_event(21);
	//this->TimePointClockTagPRUfinal=Clock::now();// Compensate for delays
	//retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);// First interrupt sent to measure time
	//  PRU long execution making sure that notification interrupts do not overlap
	retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);

	/*
	//cout << "dPRUoffsetDriftErrorAvg: " << dPRUoffsetDriftErrorAvg << endl;
	//cout << "AccumulatedErrorDrift: " << AccumulatedErrorDrift << endl;
	cout << "AccumulatedErrorDriftAux: " << AccumulatedErrorDriftAux << endl;
	cout << "PRUoffsetDriftErrorAvg: " << PRUoffsetDriftErrorAvg << endl;
	cout << "PRUoffsetDriftErrorAbsAvg: " << PRUoffsetDriftErrorAbsAvg << endl;
	cout << "PRUoffsetDriftErrorAbsAvgAux: " << PRUoffsetDriftErrorAbsAvgAux << endl;
	cout << "SynchTrigPeriod: " << SynchTrigPeriod << endl;
	cout << "InstantCorr: " << InstantCorr << endl;
	////cout << "RecurrentAuxTime: " << RecurrentAuxTime << endl;
	//cout << "pru0dataMem_int3aux: " << pru0dataMem_int3aux << endl;
	////cout << "SynchRem: " << SynchRem << endl;
	cout << "this->AdjPulseSynchCoeffAverage: " << this->AdjPulseSynchCoeffAverage << endl;
	cout << "this->duration_FinalInitialMeasTrigAuxAvg: " << this->duration_FinalInitialMeasTrigAuxAvg << endl;
	*/

	//cout << "retInterruptsPRU0: " << retInterruptsPRU0 << endl;
	if (retInterruptsPRU0>0){
		prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	}
	else if (retInterruptsPRU0==0){
		prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
		cout << "GPIO::ReadTimeStamps took to much time for the TimeTagg. Timetags might be inaccurate. Reset PRUO if necessary." << endl;		
	}
	else{
		prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
		cout << "PRU0 interrupt poll error" << endl;
	}

	this->DDRdumpdata(iIterRunsAux); // Pre-process tags. Needs to access memory of PRU, so better within the controlled acquired environment

	this->ManualSemaphore=false;
	this->ManualSemaphoreExtra=false;
	this->release();
return 0;// all ok
}

int GPIO::SendTriggerSignals(int QuadEmitDetecSelecAux, double SynchTrigPeriodAux,unsigned int NumberRepetitionsSignalAux,double* FineSynchAdjValAux,unsigned long long int QPLAFutureTimePointNumber, bool QPLAFlagTestSynchAux){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
	std::chrono::nanoseconds duration_back(QPLAFutureTimePointNumber);
	this->QPLAFlagTestSynch=QPLAFlagTestSynchAux;
	this->QPLAFutureTimePoint=Clock::time_point(duration_back);
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(6*TimePRUcommandDelay);// Give some margin so that ReadTimeStamps and coincide in the respective methods of GPIO. Only for th einitial run, since the TimeStaps are run once (to enter the acquire in GPIO). What consumes time is writting to PRU, then times 4 since 4 writings to PRU before sleep in GPIO
	this->QPLAFutureTimePointSleep=this->QPLAFutureTimePoint;// Update value
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(1*TimePRUcommandDelay);// Since two memory mapping to PRU memory
	SynchRem=static_cast<int>((static_cast<long double>(1.5*MultFactorEffSynchPeriod*SynchTrigPeriod)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(SynchRem);
	SynchTrigPeriod=SynchTrigPeriodAux;// Histogram/Period value
	NumberRepetitionsSignal=static_cast<unsigned int>(NumberRepetitionsSignalAux);// Number of repetitions to send signals
	AccumulatedErrorDriftAux=FineSynchAdjValAux[0];// Synch trig offset
	AccumulatedErrorDrift=FineSynchAdjValAux[1]; // Synch trig frequency
	QuadEmitDetecSelecGPIO=QuadEmitDetecSelecAux;// Update value
	while (this->ManualSemaphore);// Wait other process// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->ManualSemaphoreExtra=true;
	this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
	//this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
	// Apply a slotted synch configuration (like synchronized Ethernet)
	this->AdjPulseSynchCoeffAverage=this->EstimateSynchAvg;
	pru1dataMem_int[3]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru1dataMem_int[1]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
	// Different modes of periodic correction
	switch (QuadEmitDetecSelecAux){
		case 1: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld1;break;}
		case 2: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld2;break;}
		case 3: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld3;break;}
		case 4: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld4;break;}
		case 5: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld5;break;}
		case 6: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld6;break;}
		case 7: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld7;break;}
		default: {break;}
	}
	ldTimePointClockTagPRUDiff=static_cast<long double>(PRUoffsetDriftErrorAbsAvgMax)+static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointOld).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value		
	LLITimePointPRUDiffSystemRelFreq=static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointOld).count());
	long long int WrapLLITimePointPRUDiffSystemRelFreq=static_cast<long long int>(static_cast<long double>(LLITimePointPRUDiffSystemRelFreq)*static_cast<long double>(PRUoffsetDriftErrorAvg));
	if (WrapLLITimePointPRUDiffSystemRelFreq<0){
		WrapLLITimePointPRUDiffSystemRelFreq=-(-WrapLLITimePointPRUDiffSystemRelFreq%static_cast<long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	else{
		WrapLLITimePointPRUDiffSystemRelFreq=(WrapLLITimePointPRUDiffSystemRelFreq%static_cast<long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	QPLAFutureTimePointBusyWaitInterrupt=QPLAFutureTimePoint+std::chrono::nanoseconds(WrapLLITimePointPRUDiffSystemRelFreq);
	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{// Time and frequency correction			
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvg+static_cast<double>(ldTimePointClockTagPRUDiff*static_cast<long double>(PRUoffsetDriftErrorAvg));
			break;
		}
		case 2:{// Time correction
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvg;
			ldTimePointClockTagPRUDiff=0;	
			break;
		}
		case 1:{ // Frequency correction
			PRUoffsetDriftErrorAbsAvgAux=static_cast<double>(ldTimePointClockTagPRUDiff*static_cast<long double>(PRUoffsetDriftErrorAvg));
			break;
		}
		default:{PRUoffsetDriftErrorAbsAvgAux=0;ldTimePointClockTagPRUDiff=0;break;}// None time nor frequency correction
	}
	if (PRUoffsetDriftErrorAbsAvgAux<0.0){
		PRUoffsetDriftErrorAbsAvgAux=-fmod(-PRUoffsetDriftErrorAbsAvgAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	else{
		PRUoffsetDriftErrorAbsAvgAux=fmod(PRUoffsetDriftErrorAbsAvgAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	switch (QuadEmitDetecSelecAux){// Update value	
		case 1: {this->QPLAFutureTimePointOld1=this->QPLAFutureTimePoint;break;}
		case 2: {this->QPLAFutureTimePointOld2=this->QPLAFutureTimePoint;break;}
		case 3: {this->QPLAFutureTimePointOld3=this->QPLAFutureTimePoint;break;}
		case 4: {this->QPLAFutureTimePointOld4=this->QPLAFutureTimePoint;break;}
		case 5: {this->QPLAFutureTimePointOld5=this->QPLAFutureTimePoint;break;}
		case 6: {this->QPLAFutureTimePointOld6=this->QPLAFutureTimePoint;break;}
		case 7: {this->QPLAFutureTimePointOld7=this->QPLAFutureTimePoint;break;}
		default: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePoint;break;}
	}
	if (abs(PRUoffsetDriftErrorAbsAvgAux)>PRUoffsetDriftErrorAbsAvgMax){
		cout << "GPIO::PRUoffsetDriftErrorAbsAvgAux magnitude is too large: " << PRUoffsetDriftErrorAbsAvgAux << "Increase PRUoffsetDriftErrorAbsAvgMax. Wrapping PRUoffsetDriftErrorAbsAvgAux around PRUoffsetDriftErrorAbsAvgMax." << endl;
		if (PRUoffsetDriftErrorAbsAvgAux<0.0){
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvgMax-fmod(-PRUoffsetDriftErrorAbsAvgAux,PRUoffsetDriftErrorAbsAvgMax);
		}
		else{
			PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvgMax+fmod(PRUoffsetDriftErrorAbsAvgAux,PRUoffsetDriftErrorAbsAvgMax);
		}
	}
	else{
		PRUoffsetDriftErrorAbsAvgAux=PRUoffsetDriftErrorAbsAvgMax+PRUoffsetDriftErrorAbsAvgAux;
	}

	pru1dataMem_int[4]=static_cast<unsigned int>(PRUoffsetDriftErrorAbsAvgAux); // set periodic offset correction value

	// The synch offset
	if (this->AccumulatedErrorDriftAux<0.0){
		AccumulatedErrorDriftAux=(MultFactorEffSynchPeriod*SynchTrigPeriod)-fmod(-this->AccumulatedErrorDriftAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	else{
		AccumulatedErrorDriftAux=(MultFactorEffSynchPeriod*SynchTrigPeriod)+fmod(this->AccumulatedErrorDriftAux,MultFactorEffSynchPeriod*SynchTrigPeriod);
	}
	pru1dataMem_int[5]=static_cast<unsigned int>(AccumulatedErrorDriftAux); // set periodic offset correction value

	// From this point below, the timming is very critical to have long time synchronziation stability!!!!	
	// Atenttion, since Histogram analysis has effectively 4 times the SynchTrigPeriod, for the SynchRem this period is multiplied by 4!!! It will have to be removed
	InstantCorr=static_cast<long long int>(AccumulatedErrorDrift*SynchTrigPeriod);
	// The relative frequency difference is not module over MultFactorEffSynchPeriod, since its value is computed over the original synch period
	//ldTimePointClockTagPRUinitial=static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)+static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod);// Time point after all PRU synch steps. The periodic synch offset is not accounted for
	//InstantCorr=static_cast<long long int>(static_cast<long double>((1.0/6.0)*AccumulatedErrorDrift)*static_cast<long double>(SynchTrigPeriod)*fmodl(ldTimePointClockTagPRUinitial,static_cast<long double>(SynchTrigPeriod)));
	//InstantCorr=static_cast<long long int>(static_cast<long double>((1.0/6.0)*AccumulatedErrorDrift)*static_cast<long double>(SynchTrigPeriod)*fmodl(ldTimePointClockTagPRUinitial,static_cast<long double>(SynchTrigPeriod)));
	//InstantCorr=static_cast<long long int>(static_cast<long double>((1.0/6.0)*AccumulatedErrorDrift)*static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)*fmodl((ldTimePointClockTagPRUinitial/static_cast<long double>(1000000000)),static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)));
	
	if (SynchCorrectionTimeFreqNoneFlag==0 or SynchCorrectionTimeFreqNoneFlag==2){ // It has to be a much finer control at PRU level (and also this level) in order to be able to compensate for the very low hardware wandering/drift.
		ContCorr=0;//InstantCorr;
	}
	else{
		ContCorr=0;//static_cast<long long int>(static_cast<long double>(SynchTrigPeriod)*static_cast<long double>(PRUoffsetDriftErrorAvg))+InstantCorr;
	}

	if (ContCorr>0){SignAuxInstantCorr=1;}
	else if (ContCorr<0){SignAuxInstantCorr=-1;}
	else {ContCorr=0;}
	ContCorr=SignAuxInstantCorr*(abs(ContCorr)%static_cast<long long int>(SynchTrigPeriod));

	if (InstantCorr>0){SignAuxInstantCorr=1;}
	else if (InstantCorr<0){SignAuxInstantCorr=-1;}
	else {SignAuxInstantCorr=0;}
	InstantCorr=SignAuxInstantCorr*(abs(InstantCorr)%static_cast<long long int>(SynchTrigPeriod));
	pru1dataMem_int[2]=static_cast<unsigned int>(static_cast<long long int>(SynchTrigPeriod)+InstantCorr);// Referenced to the synch trig period

	// Particular for histogram, tell the adjusted period accounting for relative frequency difference
	pru1dataMem_int[6]=static_cast<unsigned int>(static_cast<long long int>(SynchTrigPeriod)+ContCorr);// Referenced to the corrected synch period

	// Sleep barrier to synchronize the different nodes at this point, so the below calculations and entry times coincide
	requestCoincidenceWhileWait=CoincidenceSetWhileWait();
	clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestCoincidenceWhileWait,NULL); // Synch barrier. So that SendTriggerSignals and ReadTimeStamps of the different nodes coincide	

	pru1dataMem_int[0]=static_cast<unsigned int>(QuadEmitDetecSelecAux); // set command. Generate signals. Takes around 900000 clock ticks
	//this->QPLAFutureTimePoint=this->QPLAFutureTimePoint-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg); // Actually, the time measured duration_FinalInitialMeasTrigAuxAvg is not indicative of much (only if it changes a lot to high values it means trouble)

	////if (Clock::now()<this->QPLAFutureTimePoint){cout << "Check that we have enough time" << endl;}
	while (Clock::now()<this->QPLAFutureTimePointBusyWaitInterrupt);// Busy wait time synch sending signals
	// Important, the following line at the very beggining to reduce the command jitter
	prussdrv_pru_send_event(22);//Send host arm to PRU1 interrupt
	//this->TimePointClockSynchPRUfinal=Clock::now();
	// Here there should be the instruction command to tell PRU1 to start generating signals
	// We have to define a command, compatible with the memory space of PRU0 to tell PRU1 to initiate signals
	//  PRU long execution making sure that notification interrupts do not overlap
	retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);

	/*
	//cout << "ldPRUoffsetDriftErrorAvg: " << ldPRUoffsetDriftErrorAvg << endl;
	//cout << "dPRUoffsetDriftErrorAvg: " << dPRUoffsetDriftErrorAvg << endl;
	cout << "AccumulatedErrorDrift: " << AccumulatedErrorDrift << endl;
	cout << "AccumulatedErrorDriftAux: " << AccumulatedErrorDriftAux << endl;
	cout << "PRUoffsetDriftErrorAvg: " << PRUoffsetDriftErrorAvg << endl;
	cout << "PRUoffsetDriftErrorAbsAvg: " << PRUoffsetDriftErrorAbsAvg << endl;
	cout << "PRUoffsetDriftErrorAbsAvgAux: " << PRUoffsetDriftErrorAbsAvgAux << endl;	
	cout << "SynchTrigPeriod: " << SynchTrigPeriod << endl;
	cout << "InstantCorr: " << InstantCorr << endl;
	////cout << "RecurrentAuxTime: " << RecurrentAuxTime << endl;
	//cout << "pru1dataMem_int2aux: " << pru1dataMem_int2aux << endl;
	////cout << "SynchRem: " << SynchRem << endl;
	cout << "this->AdjPulseSynchCoeffAverage: " << this->AdjPulseSynchCoeffAverage << endl;
	cout << "this->duration_FinalInitialMeasTrigAuxAvg: " << this->duration_FinalInitialMeasTrigAuxAvg << endl;
	*/

	//cout << "SendTriggerSignals: retInterruptsPRU1: " << retInterruptsPRU1 << endl;
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

	this->ManualSemaphore=false;
	this->ManualSemaphoreExtra=false;
	this->release();

return 0;// all ok	
}

int GPIO::SendEmulateQubits(){ // Emulates sending 2 entangled qubits through the 8 output pins (each qubits needs 4 pins)

return 0;// all ok
}

int GPIO::SetSynchDriftParams(double* AccumulatedErrorDriftParamsAux){// Not Used
	this->acquire();
	// Make it iterative algorithm
	AccumulatedErrorDrift=AccumulatedErrorDrift+static_cast<long double>(AccumulatedErrorDriftParamsAux[0]); // For retrieved relative frequency difference from protocol
	AccumulatedErrorDriftAux=AccumulatedErrorDriftAux+static_cast<long double>(AccumulatedErrorDriftParamsAux[1]);// For retrieved relative offset difference from protocol
	//cout << "AccumulatedErrorDrift: " << AccumulatedErrorDrift << endl;
	//cout << "AccumulatedErrorDriftAux: " << AccumulatedErrorDriftAux << endl;
	this->release();
	return 0; // All Ok
	}

	bool GPIO::GetHardwareSynchStatus(){// Provide information to the above agents
	bool HardwareSynchStatusAux=false;
	this->acquire();
	HardwareSynchStatusAux=HardwareSynchStatus;
	this->release();
return HardwareSynchStatusAux; // All Ok
}

//PRU0 - Operation - getting iputs

int GPIO::DDRdumpdata(int iIterRunsAux){
// Reading data from PRU shared and own RAMs
//DDR_regaddr = (short unsigned int*)ddrMem + OFFSET_DDR;
valp=valpHolder; // Coincides with SHARED in PRUassTaggDetScript.p
valpAux=valpAuxHolder;
//synchp=synchpHolder;
//for each capture bursts, at the beggining is stored the overflow counter of 32 bits. From there, each capture consists of 32 bits of the DWT_CYCCNT register and 8 bits of the channels detected (40 bits per detection tag).
// The shared memory space has 12KB=12×1024bytes=12×1024×8bits=98304bits.
//Doing numbers, we can store up to 1966 captures. To be in the safe side, we can do 1964 captures

// When unsgined char
//valThresholdResetCounts=static_cast<unsigned int>(*valpAux);
//valpAux++;// 1 times 8 bits
//valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<8;
//valpAux++;// 1 times 8 bits
//valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<16;
//valpAux++;// 1 times 8 bits
//valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<24;
//valpAux++;// 1 times 8 bits
// When unsigned short
valThresholdResetCounts=static_cast<unsigned int>(*valpAux);
valpAux++;// 1 times 16 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<16;
valpAux++;// 1 times 16 bits
//cout << "valThresholdResetCounts: " << valThresholdResetCounts << endl;
//////////////////////////////////////////////////////////////////////////////

// Reading first calibration tag and link it to the system clock
OldLastTimeTagg=static_cast<unsigned long long int>(*CalpHolder);//extendedCounterPRUaux + static_cast<unsigned long long int>(*CalpHolder);
//cout << "OldLastTimeTagg: " << OldLastTimeTagg << endl;

// Slot the TimeTaggsLast, since it eventually has to start at the beggining of the effective period
this->TimeTaggsLast=(static_cast<unsigned long long int>(ldTimePointClockTagPRUinitial)/static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod))*static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod);

//Furthermore, remove some time from epoch - in multiples of the SynchTrigPeriod, so it is easier to handle in the above agents
this->TimeTaggsLast=static_cast<unsigned long long int>(static_cast<long long int>(this->TimeTaggsLast)-static_cast<long long int>((this->ULLIEpochReOffset/static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod))*static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod)));

if (iIterRunsAux==0){TimeTaggsLastStored=TimeTaggsLast;TotalCurrentNumRecords=0;}// First iteration of current runs, store the value for synchronization time difference calibration

long long int LLIOldLastTimeTagg=static_cast<long long int>(OldLastTimeTagg);
unsigned int valCycleCountPRUAux1;
unsigned int valCycleCountPRUAux2;
//cout << "GPIO::NumQuBitsPerRun " << NumQuBitsPerRun << endl;
//cout << "GPIO::MaxNumQuBitsMemStored " << MaxNumQuBitsMemStored << endl;
//for (iIterDump=0; iIterDump<NumQuBitsPerRun; iIterDump++){
CurrentiIterDump=0;
extendedCounterPRUholder=1;// Re-initialize at each run. 1 so that at least the first is checked and stored
extendedCounterPRUholderOld=0;// Re-initialize at each run
int TotalCurrentNumRecordsOld=TotalCurrentNumRecords;
while (CurrentiIterDump<NumQuBitsPerRun and extendedCounterPRUholder>extendedCounterPRUholderOld){// Do it until a timetagg is smaller in value than the previous one, because it means that it could not achieve to capture NumQuBitsPerRun
	extendedCounterPRUholderOld=extendedCounterPRUholder;
	// When unsigned short
	//valCycleCountPRU=static_cast<unsigned int>(0);// Reset value
	valCycleCountPRUAux1=static_cast<unsigned int>(*valp) & 0x0000FFFF;
	//cout << "GPIO::DDRdumpdata::static_cast<unsigned int>(*valp): " << static_cast<unsigned int>(*valp) << endl;
	//cout << "GPIO::DDRdumpdata::valCycleCountPRUAux1: " << valCycleCountPRUAux1 << endl;
	valp++;// 1 times 16 bits
	valCycleCountPRUAux2=((static_cast<unsigned int>(*valp))<<16) & 0xFFFF0000;
	//cout << "GPIO::DDRdumpdata::static_cast<unsigned int>(*valp): " << static_cast<unsigned int>(*valp) << endl;
	//cout << "GPIO::DDRdumpdata::valCycleCountPRUAux2: " << valCycleCountPRUAux2 << endl;
	valCycleCountPRU=valCycleCountPRUAux1 | valCycleCountPRUAux2;
	valp++;// 1 times 16 bits
	//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "valCycleCountPRU: " << valCycleCountPRU << endl;}
	// Mount the extended counter value
	extendedCounterPRUholder=static_cast<unsigned long long int>(valCycleCountPRU);//extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);
	//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "extendedCounterPRU: " << extendedCounterPRU << endl;}
	// Apply system clock corrections
	TimeTaggsStored[TotalCurrentNumRecords]=static_cast<unsigned long long int>(static_cast<double>(static_cast<long long int>(extendedCounterPRUholder)-LLIOldLastTimeTagg)*AdjPulseSynchCoeffAverage)+TimeTaggsLast;	// The fist OldLastTimeTagg and TimeTaggsLast of the iteration is compensated for with the calibration tag together with the accumulated synchronization error	    
	//////////////////////////////////////////////////////////////		
	// When unsigned short
	ChannelTagsStored[TotalCurrentNumRecords]=this->packBits(static_cast<unsigned short>(*valp)); // we're just interested in 12 bits which we have to re-order
	valp++;// 1 times 16 bits
	//cout << "GPIO::TotalCurrentNumRecords: " << TotalCurrentNumRecords << endl;
	//cout << "GPIO::extendedCounterPRUholder: " << extendedCounterPRUholder << endl;
	//cout << "GPIO::extendedCounterPRUholder>0: " << (extendedCounterPRUholder>0) << endl;
	if (TotalCurrentNumRecords<MaxNumQuBitsMemStored and extendedCounterPRUholder>0){TotalCurrentNumRecords++;}//Variable to hold the number of currently stored records in memory	
	CurrentiIterDump++;
}
if (TotalCurrentNumRecords>MaxNumQuBitsMemStored){cout << "GPIO::We have reached the maximum number of qubits storage!" << endl;}
else if (TotalCurrentNumRecords==TotalCurrentNumRecordsOld){cout << "GPIO::No detection of qubits!" << endl;}
//cout << "GPIO::TotalCurrentNumRecords: " << TotalCurrentNumRecords << endl;

// Reset values of the sharedMem_int after each iteration
for (iIterDump=0; iIterDump<((NumQuBitsPerRun/2)*3); iIterDump++){
	sharedMem_int[OFFSET_SHAREDRAM+iIterDump]=static_cast<unsigned int>(0x00000000); // Put it all to zeros
}
// Notify lost of track of counts due to timer overflow - Not really used
//if (this->FirstTimeDDRdumpdata or this->valThresholdResetCounts==0){this->AfterCountsThreshold=24+5;}// First time the Threshold reset counts of the timetagg is not well computed, hence estimated as the common value
//else{this->AfterCountsThreshold=this->valThresholdResetCounts+5;};// Related to the number of instruciton counts after the last read of the counter. It is a parameter to adjust
/*this->AfterCountsThreshold=24+5;
this->FirstTimeDDRdumpdata=false;
if(valCycleCountPRU >= (0xFFFFFFFF-this->AfterCountsThreshold)){// The counts that we will lose because of the reset
	cout << "We have lost ttg counts! Lost of tags accuracy! Reduce the number of tags per run, and if needed increase the runs number." << endl;
	cout << "AfterCountsThreshold: " << AfterCountsThreshold << endl;
	cout << "valCycleCountPRU: " << valCycleCountPRU << endl;
}*/
//else if (valCycleCountPRU > (0x80000000-this->AfterCountsThreshold)){// The exceeded counts, remove them
//this->valCarryOnCycleCountPRU=this->valCarryOnCycleCountPRU-(AboveThresoldCycleCountPRUCompValue-1)*static_cast<unsigned long long int>((this->AfterCountsThreshold+valCycleCountPRU)-0x80000000);
////cout << "this->valCarryOnCycleCountPRU: " << this->valCarryOnCycleCountPRU << endl;
//}
//cout << "sharedMem_int: " << sharedMem_int << endl;
////////////////////////////////////////////
// Checks of proper values handling
//long long int CheckValueAux=(static_cast<long long int>(SynchTrigPeriod/2.0)+static_cast<long long int>(TimeTaggsStored[0]))%static_cast<long long int>(SynchTrigPeriod)-static_cast<long long int>(SynchTrigPeriod/2.0);
//cout << "GPIO::DDRdumpdata::CheckValueAux: "<< CheckValueAux << endl;
//cout << "GPIO::DDRdumpdata::SynchTrigPeriod: " << SynchTrigPeriod << endl;
//cout << "GPIO::DDRdumpdata::NumQuBitsPerRun: " << NumQuBitsPerRun << endl;
///////////////////////////////////////////////

// Correct the detected qubits relative frequency difference (due to the sender node) and split between quad groups of 4 channels
PRUdetCorrRelFreq(iIterRunsAux,CurrentiIterDump);

if (SlowMemoryPermanentStorageFlag==true){ // We save into file the relative frequency corrected info (so it might be time disorded for different QuadNumChGroups)
	// Reading TimeTaggs
	if (streamDDRpru.is_open()){	        
		if (iIterRunsAux==0){
			streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			streamDDRpru.write(reinterpret_cast<const char*>(&TimeTaggsLast), sizeof(TimeTaggsLast));// Store this reference value
		}
		for (iIterDump=0; iIterDump<CurrentiIterDump; iIterDump++){
			streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			streamDDRpru.write(reinterpret_cast<const char*>(&TimeTaggsStored[iIterDump]), sizeof(TimeTaggsStored[iIterDump]));
			streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			streamDDRpru.write(reinterpret_cast<const char*>(&ChannelTagsStored[iIterDump]), sizeof(ChannelTagsStored[iIterDump]));
			//streamDDRpru << extendedCounterPRU << valBitsInterest << endl;
		}
	}
	else{
		cout << "DDRdumpdata streamDDRpru is not open!" << endl;
	}
}

return 0; // all ok
}

int GPIO::PRUdetCorrRelFreq(int iIterRunsAux,int CurrentiIterDump){
// Separate the detection by quad channels and do the processing independently
	int CurrentiIterDumpAux=0;
	// First (reset)compute the number of detections per quad channel
	if (iIterRunsAux==0){ // Reset values
		for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
			TotalCurrentNumRecordsQuadCh[iQuadChIter]=0;
			TotalCurrentNumRecordsQuadChOld[iQuadChIter]=0;
		}
	}
	else{// Update old values
		for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
			TotalCurrentNumRecordsQuadChOld[iQuadChIter]=TotalCurrentNumRecordsQuadCh[iQuadChIter];
		}
	}
	for (int i=0;i<CurrentiIterDump;i++){
		for (unsigned short iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
			if ((ChannelTagsStored[i]&(0x000F<<(4*iQuadChIter)))>0){  
				TimeTaggsSplitted[iQuadChIter][TotalCurrentNumRecordsQuadCh[iQuadChIter]]=TimeTaggsStored[i];
				ChannelTagsSplitted[iQuadChIter][TotalCurrentNumRecordsQuadCh[iQuadChIter]]=ChannelTagsStored[i]&(0x000F<<(4*iQuadChIter));
				TotalCurrentNumRecordsQuadCh[iQuadChIter]++;
			}
		}
	}
	if (GPIOFlagRelFreqTest==false){
		for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
			TotalCurrentNumRecordsQuadChNewOldAux=TotalCurrentNumRecordsQuadCh[iQuadChIter]-TotalCurrentNumRecordsQuadChOld[iQuadChIter];
			if (TotalCurrentNumRecordsQuadChNewOldAux>=TagsSeparationDetRelFreq){
	    		unsigned long long int ULLIInitialTimeTaggs=TimeTaggsLast;//TimeTaggs[iQuadChIter][0];// Normalize to the first reference timetag (it is not a detect qubit, but the timetagg of entering the timetagg PRU), which is a strong reference
	    		long long int LLIInitialTimeTaggs=static_cast<long long int>(TimeTaggsLast);//static_cast<long long int>(TimeTaggs[iQuadChIter][0]);
	    		//cout << "GPIO::LastTimeTaggRef[0]: " << LastTimeTaggRef[0] << endl;
	    		//cout << "GPIO::TimeTaggs[iQuadChIter][0]: " << TimeTaggs[iQuadChIter][0] << endl;
	    		long long int LLITimeTaggs[TotalCurrentNumRecordsQuadChNewOldAux]={0};
	    		for (int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
	    			LLITimeTaggs[i]=static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]])-LLIInitialTimeTaggs;
	    		}
	    		double SlopeDetTagsAux=1.0;

			    // Calculate the "x" values
	    		long long int xAux[TotalCurrentNumRecordsQuadChNewOldAux]={0};
	    		long long int LLISynchTrigPeriod=static_cast<long long int>(SynchTrigPeriod);
	    		long long int LLISynchTrigPeriodHalf=static_cast<long long int>(SynchTrigPeriod/2.0);
	    		for (int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
	    			xAux[i]=((LLITimeTaggs[i]+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin
	    		}

			    // Compute the candidate slope
	    		int iAux=0;
	    		for (int i=0;i<(TotalCurrentNumRecordsQuadChNewOldAux-TagsSeparationDetRelFreq);i++){
	    			if ((xAux[i+TagsSeparationDetRelFreq]-xAux[i])>0){
	    				SlopeDetTagsAuxArray[iAux]=static_cast<double>(LLITimeTaggs[i+TagsSeparationDetRelFreq]-LLITimeTaggs[i])/static_cast<double>(xAux[i+TagsSeparationDetRelFreq]-xAux[i]);
	    				iAux++;
	    			}
	    		}

	    		SlopeDetTagsAux=DoubleMedianFilterSubArray(SlopeDetTagsAuxArray,iAux);
			    //cout << "GPIO::SlopeDetTagsAux: " << SlopeDetTagsAux << endl;

	    		if (SlopeDetTagsAux<=0.0){
	    			cout << "GPIO::PRUdetCorrRelFreq wrong computation of the SlopeDetTagsAux " << SlopeDetTagsAux << " for quad channel " << iQuadChIter << ". Not applying the correction..." << endl;
	    			SlopeDetTagsAux=1.0;
	    		}
	    		//cout << "GPIO::PRUdetCorrRelFreq SlopeDetTagsAux " << SlopeDetTagsAux << " for quad channel " << iQuadChIter << endl;
			    // Un-normalize
	    		for (int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
	    			TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]]=static_cast<unsigned long long int>(static_cast<long double>(1.0/SlopeDetTagsAux)*static_cast<long double>(LLITimeTaggs[i]))+ULLIInitialTimeTaggs;
	    			// Also update the information in the original array
	    			TimeTaggsStored[CurrentiIterDumpAux]=TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]];
	    			ChannelTagsStored[CurrentiIterDumpAux]=ChannelTagsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]];
	    			CurrentiIterDumpAux++;// update value
	    		}
			    //////////////////////////////////////////
			    // Checks of proper values handling
			    //long long int CheckValueAux=(static_cast<long long int>(SynchTrigPeriod/2.0)+static_cast<long long int>(TimeTaggsStored[0]))%static_cast<long long int>(SynchTrigPeriod)-static_cast<long long int>(SynchTrigPeriod/2.0);
			    //cout << "GPIO::PRUdetCorrRelFreq::CheckValueAux: "<< CheckValueAux << endl;
			    ////////////////////////////////////////
			}// if
			else if (TotalCurrentNumRecordsQuadChNewOldAux>0){
				cout << "GPIO::PRUdetCorrRelFreq not enough detections " << TotalCurrentNumRecordsQuadChNewOldAux << "<" << TagsSeparationDetRelFreq << " in iQuadChIter " << iQuadChIter << " quad channel to correct emitter rel. frequency deviation!" << endl;
			}
		} // for
	}
	else{
		if (GPIOFlagRelFreqTest==true){cout << "GPIO::PRUdetCorrRelFreq deactivated..." << endl;}
	}
//cout << "GPIO::PRUdetCorrRelFreq completed!" << endl;
return 0; // All ok
}

// Function to pack bits 0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15 of an unsigned int into the lower values
unsigned short GPIO::packBits(unsigned short value) {
    // Rearrange the lower two bytes so that they are correctly splitted for each quad group channel. For each group of 4 bits the order does not follow an arranged order for channel detectors
    unsigned short byte0aux0 = ((value & 0x0070) | ((value & 0x0002)<<6))>>4; // Are the bits 0x0072
    unsigned short byte0aux1 = (((value & 0x0080)>> 6) | (value & 0x000D))<<4; // Are the bits 0x008D
    unsigned short byte1 = ((value & 0xF000) >> 4); // Byte 1 shifts to the right four bit positions (the interesting ones)

    // Combine the bytes into a single unsigned short
    return byte0aux0 | byte0aux1 | byte1;
}

int GPIO::ClearStoredQuBits(){
	if (SlowMemoryPermanentStorageFlag==true){
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

	//// Synch data
	//if (streamSynchpru.is_open()){
	//	streamSynchpru.close();	
	//	//streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	//}
	//
	//streamSynchpru.open(string(PRUdataPATH1) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content	
	//if (!streamSynchpru.is_open()) {
	//	streamSynchpru.open(string(PRUdataPATH2) + string("SynchTimetaggingData"), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);// Open for write and read, and clears all previous content
	//	if (!streamSynchpru.is_open()) {
	//        	cout << "Failed to re-open the streamSynchpru file." << endl;
	//        	return -1;
	//        }
	//}
	//streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
	//streamSynchpru.seekp(0, std::ios::beg); // the put (writing) pointer back to the start!
}
TotalCurrentNumRecords=0;

return 0; // all ok
}

int GPIO::RetrieveNumStoredQuBits(unsigned long long int* LastTimeTaggRefAux, unsigned int* TotalCurrentNumRecordsQuadChAux, unsigned long long int TimeTaggsAux[QuadNumChGroups][MaxNumQuBitsMemStored], unsigned short int ChannelTagsAux[QuadNumChGroups][MaxNumQuBitsMemStored]){
	if (SlowMemoryPermanentStorageFlag==true){
		LastTimeTaggRefAux[0]=static_cast<unsigned long long int>(0.0*PRUclockStepPeriodNanoseconds);// Since whole number. Initiation value
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
			streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			streamDDRpru.read(reinterpret_cast<char*>(&TimeTaggsLast), sizeof(TimeTaggsLastStored));
			LastTimeTaggRefAux[0]=static_cast<unsigned long long int>(static_cast<long double>(TimeTaggsLast));// Since whole number. Initiation value
			int lineCount = 0;
			unsigned long long int ValueReadTest;		
			int iIterMovAdjPulseSynchCoeff=0;
			while (streamDDRpru.read(reinterpret_cast<char*>(&ValueReadTest), sizeof(ValueReadTest))) {// While true == not EOF
			    streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			    TimeTaggsStored[lineCount]=static_cast<unsigned long long int>(ValueReadTest);		    
			    ////////////////////////////////////////////////////////////////////////////////
			    streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
			    streamDDRpru.read(reinterpret_cast<char*>(&ChannelTagsStored[lineCount]), sizeof(ChannelTagsStored[lineCount]));
	    	    //cout << "TimeTaggs[lineCount]: " << TimeTaggs[lineCount] << endl;
	    	    //cout << "ChannelTags[lineCount]: " << ChannelTags[lineCount] << endl;
	    	    lineCount++; // Increment line count for each line read	    
	    	}
	    	if (lineCount==0){cout << "RetrieveNumStoredQuBits: No timetaggs present!" << endl;}
	    	TotalCurrentNumRecords=lineCount;
	    	// Place the information in the structure for the upper layer agents
	    	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
				TotalCurrentNumRecordsQuadCh[iQuadChIter]=0;
			}
			for (int i=0;i<TotalCurrentNumRecords;i++){
				for (unsigned short iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
					if ((ChannelTagsStored[i]&(0x000F<<(4*iQuadChIter)))>0){  
						TimeTaggsSplitted[iQuadChIter][TotalCurrentNumRecordsQuadCh[iQuadChIter]]=TimeTaggsStored[i];
						ChannelTagsSplitted[iQuadChIter][TotalCurrentNumRecordsQuadCh[iQuadChIter]]=ChannelTagsStored[i]&(0x000F<<(4*iQuadChIter));
						TotalCurrentNumRecordsQuadCh[iQuadChIter]++;
					}
				}
			}
	    }
	    else{
	    	cout << "RetrieveNumStoredQuBits: BBB streamDDRpru is not open!" << endl;
	    	return -1;
	    }
	}
	else{// Memory allocation
		LastTimeTaggRefAux[0]=static_cast<unsigned long long int>(static_cast<long double>(TimeTaggsLastStored));// Since whole number. It is meant for computing the time between measurements to estimate the relative frequency difference. It is for synchronization purposes which generally will be under control so even if it is a multiple adquisiton the itme difference will be mantained so it generally ok.
	}
	// Place the information in the upper layer agent arrays
	for (unsigned short iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		for (int i=0;i<TotalCurrentNumRecordsQuadCh[iQuadChIter];i++){  
			TimeTaggsAux[iQuadChIter][i]=TimeTaggsSplitted[iQuadChIter][i];
			ChannelTagsAux[iQuadChIter][i]=ChannelTagsSplitted[iQuadChIter][i];			
		}
		TotalCurrentNumRecordsQuadChAux[iQuadChIter]=TotalCurrentNumRecordsQuadCh[iQuadChIter];
	}

	return TotalCurrentNumRecords;
}

int GPIO::IntMedianFilterSubArray(int* ArrayHolderAux,int MedianFilterFactor){
	if (MedianFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
	// Step 1: Copy the array to a temporary array
		int temp[MedianFilterFactor]={0};
		for(int i = 0; i < MedianFilterFactor; i++) {
			temp[i] = ArrayHolderAux[i];
		}
		
    // Step 2: Sort the temporary array
		this->IntBubbleSort(temp,MedianFilterFactor);
    // If odd, middle number
		return temp[MedianFilterFactor/2];
	}
}

// Function to implement Bubble Sort
int GPIO::IntBubbleSort(int* arr,int MedianFilterFactor) {
	int temp=0;
	for (int i = 0; i < MedianFilterFactor-1; i++) {
		for (int j = 0; j < MedianFilterFactor-i-1; j++) {
			if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
				temp = arr[j];
				arr[j] = arr[j+1];
				arr[j+1] = temp;
			}
		}
	}
    return 0; // All ok
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
	double temp=0.0;
	for (int i = 0; i < MedianFilterFactor-1; i++) {
		for (int j = 0; j < MedianFilterFactor-i-1; j++) {
			if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
				temp = arr[j];
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
pru1dataMem_int[1]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
pru1dataMem_int[0]=static_cast<unsigned int>(7); // set command
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
	this->threadRefSynch.join();
	this->DisablePRUs();
	//fclose(outfile); 
	prussdrv_exit();
	//munmap(ddrMem, 0x0FFFFFFF);
	//close(mem_fd); // Device
	//if(munmap(pru_int, PRU_LEN)) {
	//	cout << "GPIO destructor: munmap failed" << endl;
	//}
	if (SlowMemoryPermanentStorageFlag==true){streamDDRpru.close();}
	//streamSynchpru.close(); //Not used
}

} /* namespace exploringBB */
