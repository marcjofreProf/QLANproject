/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2025
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
#include<unistd.h>
#include <algorithm> // For std::nth_element
// Watchdog
#include <sys/ioctl.h>
#include <linux/watchdog.h>
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
#define LAST_SHAREDRAMPOS 2999 // (AM33XX_PRUSS_SHAREDRAM_SIZE)/4-1 //Num address postion to the last one
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
	valpHolder=(unsigned short*)&sharedMem_int[OFFSET_SHAREDRAM+1];
	//valpAuxHolder=valpHolder+4+6*NumQuBitsPerRun;// 6* since each detection also includes the channels (2 Bytes) and 4 bytes for 32 bits counter, and plus 4 since the first tag is captured at the very beggining
	CalpHolder=(unsigned int*)&sharedMem_int[OFFSET_SHAREDRAM];//(unsigned int*)&pru0dataMem_int[2];// First tagg captured at the very beggining
	
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Timetagging
	// Execute program
	// Load and execute the PRU program on the PRU0
	pru0dataMem_int[0]=static_cast<unsigned int>(0); // set no command
	pru0dataMem_int[1]=static_cast<unsigned int>(this->NumQuBitsPerRun); // set number captures, with overflow clock
	pru0dataMem_int[2]=static_cast<unsigned int>(this->GuardPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru0dataMem_int[3]=static_cast<unsigned int>(1);
	pru0dataMem_int[4]=static_cast<unsigned int>(this->TTGcoincWin); // set coincidence window length
	//if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScript.bin") == -1){
	//	if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScript.bin") == -1){
	//		perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScript.bin");
	//	}
	//}
	if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUassTaggDetScriptSimple.bin") == -1){
		if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUassTaggDetScriptSimple.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassTaggDetScriptSimple.bin");
		}
	}
	////prussdrv_pru_enable(PRU_Operation_NUM);
	
	// Generate signals
	pru1dataMem_int[0]=static_cast<unsigned int>(0); // set no command
	pru1dataMem_int[1]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
	pru1dataMem_int[2]=static_cast<unsigned int>(1);// Referenced to the synch trig period
	pru1dataMem_int[3]=static_cast<unsigned int>(this->GuardPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru1dataMem_int[4]=static_cast<unsigned int>(this->ContCorr);
	pru1dataMem_int[5]=static_cast<unsigned int>(this->SigONPeriod);
	pru1dataMem_int[6]=static_cast<unsigned int>(this->ContCorrSign);
	pru1dataMem_int[7]=static_cast<unsigned int>(this->SigOFFPeriod);// Off time
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

	// Selection of the periodic synchronization correction
	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{cout << "GPIO::Time and frequency synchronization periodic correction selected!" << endl;break;}
		case 2:{cout << "GPIO::Time synchronization periodic correction selected!" << endl;break;}
		case 1:{cout << "GPIO::Frequency synchronization periodic correction selected!" << endl;break;}
		default:{cout << "GPIO::None synchronization periodic correction selected!" << endl;break;}
	}
	cout << "GPIO::Wait to proceed, calibrating synchronization!..." << endl;
	if (SynchPlaneDomainMode==true){
		cout << "GPIO::For the time being synch. controlled by control plane...adds jitter in timetaggs...in the future a real-time clock with phase reference synch should be used!!!" << endl;
	}
	else{
		cout << "GPIO::Synch. controlled by real-time plane..." << endl;
	}
	////prussdrv_pru_enable(PRU_Signal_NUM);
	sleep(30); // Give some time to load programs in PRUs and the synch protocols to initiate and lock after prioritazion and adjtimex. Very important, otherwise bad values might be retrieved
	
	// Reset values of the sharedMem_int at the beggining
	for (iIterDump=0; iIterDump<((NumQuBitsPerRun/2)*3); iIterDump++){
		sharedMem_int[OFFSET_SHAREDRAM+1+iIterDump]=static_cast<unsigned int>(0x00000000); // Put it all to zeros
	}
	// Some array initializations
	for (int i=0;i<MaxNumPulses;i++){
		duration_FinalInitialMeasTrigAuxArray[i]=0;
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
	//this->setMaxRrPriority();// for the main instance. But it stalls operation in RealTime kernel.
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
bool GPIO::setMaxRrPriority(int PriorityValAux){// For rapidly handling interrupts
	int max_priority=sched_get_priority_max(SCHED_FIFO);
	int Nice_priority=PriorityValAux;//73;// Higher priority. Very important parameter to have stability of the measurements. Slightly smaller than the priorities for clock control (ptp4l,...) but larger than for the general program
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
	//unsigned long long int ProtectionSemaphoreTrap=0;
	bool valueSemaphoreExpected=true;
	while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
		//ProtectionSemaphoreTrap++;
		//if (ProtectionSemaphoreTrap>UnTrapSemaphoreValueMaxCounter){this->release();cout << "GPIO::Releasing semaphore!!!" << endl;}// Avoid trapping situations
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
struct timespec GPIO::SemaphoreSetWhileWait(){
	struct timespec requestSemaphoreWhileWaitAux;	
	auto duration_since_epochFutureTimePointAux=QPLAFutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	long long int TimePointClockCurrentFinal_time_as_count = static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePointAux).count())-static_cast<long long int>(TimePRU1synchPeriod); // Add an offset, since the final barrier is implemented with a busy wait

	requestSemaphoreWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	requestSemaphoreWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);
	return requestSemaphoreWhileWaitAux;
}

struct timespec GPIO::CoincidenceSetWhileWait(){
	struct timespec requestCoincidenceWhileWaitAux;	
	auto duration_since_epochFutureTimePointAux=QPLAFutureTimePointSleep.time_since_epoch();
	// Convert duration to desired time
	long long int TimePointClockCurrentFinal_time_as_count = static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePointAux).count()); // Add an offset, since the final barrier is implemented with a busy wait

	requestCoincidenceWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	requestCoincidenceWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);
	return requestCoincidenceWhileWaitAux;
}

struct timespec GPIO::SetWhileWait(){
	struct timespec requestWhileWaitAux;
	this->TimePointClockCurrentSynchPRU1future=this->TimePointClockCurrentSynchPRU1future+std::chrono::nanoseconds(this->TimePRU1synchPeriod);
	
	auto duration_since_epochFutureTimePoint=this->TimePointClockCurrentSynchPRU1future.time_since_epoch();
	// Convert duration to desired time
	long long int TimePointClockCurrentFinal_time_as_count = static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count())-static_cast<long long int>(2*TimePRUcommandDelay); // Add an offset, since the final barrier is implemented with a busy wait
	//cout << "TimePointClockCurrentFinal_time_as_count: " << TimePointClockCurrentFinal_time_as_count << endl;

	requestWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	requestWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);

	// Timer file descriptor sets an interrupt that if not commented (when not in use) produces a long reaction time in the while loop (busy wait)
	//if (this->iIterPRUcurrentTimerVal==0){ // Needed to configure it only at the first iteration
	// By forcing it to renew the timerfd everytime, the awakenen time is not advanced (which probably is better)
		TimePointClockCurrentFinal_time_as_count = static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count());//-static_cast<long long int>(this->TimeClockMarging); // Add an offset, since the final barrier is implemented with a busy wait 
		//cout << "TimePointClockCurrentFinal_time_as_count: " << TimePointClockCurrentFinal_time_as_count << endl;
		
	    TimerTimeout.tv_sec = (int)((5*TimePRUcommandDelay)/((long)1000000000)); 
	    TimerTimeout.tv_usec = (long)((5*TimePRUcommandDelay)%(long)1000000000);

	    struct itimerspec its;
	    its.it_interval.tv_sec = (int)(static_cast<unsigned long long int>(static_cast<int>(TimePRU1synchPeriod)-duration_FinalInitialMeasTrigAuxAvg)/((long)1000000000));  // Periodic interval expiration // Make it periodic to try to be more deterministic. No interval, one-shot timer
	    its.it_interval.tv_nsec = (long)(static_cast<unsigned long long int>(static_cast<int>(TimePRU1synchPeriod)-duration_FinalInitialMeasTrigAuxAvg)%(long)1000000000);
	    its.it_value.tv_sec=(int)((TimePointClockCurrentFinal_time_as_count-static_cast<long long int>(duration_FinalInitialMeasTrigAuxAvg))/((long)1000000000)); // Initial expiration
		its.it_value.tv_nsec=(long)((TimePointClockCurrentFinal_time_as_count-static_cast<long long int>(duration_FinalInitialMeasTrigAuxAvg))%(long)1000000000);

		timerfd_settime(this->tfd, TFD_TIMER_ABSTIME, &its, NULL);

		// Watch timefd file descriptor
	    FD_ZERO(&rfds);
	    FD_SET(this->tfd, &rfds);
	//}

	return requestWhileWaitAux;
}

int GPIO::PRUsignalTimerSynchJitterLessInterrupt(){
	try{
	this->setMaxRrPriority(PriorityValTop);// For rapidly handling interrupts, for the main instance and the periodic thread. It stalls operation RealTime Kernel (commented, then)
	this->TimePointClockCurrentSynchPRU1future=Clock::now();// First time
	//SynchRem=static_cast<int>((static_cast<long double>(iepPRUtimerRange32bits)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockCurrentSynchPRU1future.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(iepPRUtimerRange32bits)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	//this->TimePointClockCurrentSynchPRU1future=this->TimePointClockCurrentSynchPRU1future+std::chrono::nanoseconds(SynchRem);
	// Timer management
	tfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
	if (tfd==-1){
		cout << "GPIO::PRUsignalTimerSynchJitterLessInterrupt Failed to create timerfd!!!" << endl;
		exit(EXIT_SUCCESS);
	}
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
		//clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);
		//if (this->ManualSemaphoreExtra==false){	
		// In C++, when evaluating a compound condition with && (logical AND), the expressions are evaluated from left to right, and the evaluation stops as soon as the result is determined.
		if (Clock::now()<(this->TimePointClockCurrentSynchPRU1future-std::chrono::nanoseconds(3*TimePRUcommandDelay)) and clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL)==0 and this->ManualSemaphoreExtra==false){// It was possible to execute when needed, and still on time to be executed (otherwise skip it to not produce accumulations)
			if (this->ResetPeriodicallyTimerPRU1){
				this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
				this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
				// https://www.kernel.org/doc/html/latest/timers/timers-howto.html
				if (((this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux) and abs(duration_FinalInitialMeasTrig)>(ApproxInterruptTime/4) and abs(duration_FinalInitialMeasTrigAuxAvg)>(ApproxInterruptTime/1)) or this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux/2)) and this->iIterPRUcurrentTimerValSynch%2==0){// Initially run many times so that interrupt handling warms up
					this->NextSynchPRUcorrection=static_cast<unsigned int>(0); // resetting to 0
					this->NextSynchPRUcommand=static_cast<unsigned int>(11);// Hard setting of the time
					this->iIterPRUcurrentTimerVal=0;// reset this value
				}
				
				pru1dataMem_int[3]=static_cast<unsigned int>(this->NextSynchPRUcorrection);// apply correction.
				pru1dataMem_int[0]=static_cast<unsigned int>(this->NextSynchPRUcommand); // apply command
				// Not a good strategy to keep changing the priority since ther kernel scheduler is not capable to adapt ot it.
				//this->setMaxRrPriority(PriorityValTop);// Set top priority
				// There is a big variation if the waiting function to launch the measurement is not properly done (with the appropiate tools)
				// sleep_for seems to operate more stable although it adds a long overhead time, compared to while()
				// sleep_for takes longer in average maybe because it has to re-load all the context and so forth after each sleep...
				//std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockCurrentSynchPRU1future-Clock::now()));//while(Clock::now() < this->TimePointClockCurrentSynchPRU1future);//{//;// Busy waiting
				
				//std::this_thread::sleep_until(this->TimePointClockCurrentSynchPRU1future); // Better to use sleep_until because it will adapt to changes in the current time by the time synchronization protocol
				select(tfd+1, &rfds, NULL, NULL, &TimerTimeout);//TimerTFDretval = select(tfd+1, &rfds, NULL, NULL, NULL); /* Last parameter = NULL --> wait forever */
				
				//	// Yield the CPU to other threads
        		//	std::this_thread::yield();
				//}				
				//this->TimePointClockSendCommandFinal=Clock::now(); // Final measurement.
				// Probably by measuring the time of more relevant functions (generation of interrupt; and reception from PRU of its interrupt, it does a better job of estimating the real thing)
				prussdrv_pru_send_event(22);
				//this->TimePointClockSendCommandFinal=Clock::now(); // Final measurement.
				// Not a good strategy to keep changing the priority since ther kernel scheduler is not capable to adapt ot it.
				//this->setMaxRrPriority(PriorityValRegular); // Set regular priority
				retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRUShort);// timeout is sufficiently large because it it adjusted when generating signals, not synch whiis very fast (just reset the timer)										
				this->TimePointClockSendCommandFinal=Clock::now(); // Final measurement.
				// Not a good strategy to keep changing the priority since ther kernel scheduler is not capable to adapt ot it.
				//this->setMaxRrPriority(PriorityValRegular);// Set regular priority
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
				// Clear the timer
				if (FD_ISSET(tfd, &rfds)){
					read(tfd,&TimerExpirations,sizeof(TimerExpirations));
				}
				/*
				// Warm up interrupt handling for Timetagg PRU0
				pru0dataMem_int[0]=static_cast<unsigned int>(8); // set command warm-up
				prussdrv_pru_send_event(21);				
				retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRUShort);				

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
				}*/

				//pru1dataMem_int[2]// Current IEP timer sample
				//pru1dataMem_int[3]// Correction to apply to IEP timer
				this->PRUcurrentTimerValWrap=static_cast<double>(pru1dataMem_int[2]);
				
				/*// Correct for interrupt handling time might add a bias in the estimation/reading or correct in the timerfd
				//duration_FinalInitialCountAux=static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-this->TimePointClockCurrentSynchPRU1future).count());
				// If we assume that the larger part of the latency from the barrier wakeup to the actual interrupt handling is the awakening, we can try to substract his time.
				////this->PRUcurrentTimerValWrap=this->PRUcurrentTimerValWrap-(duration_FinalInitialCountAux-0.0*duration_FinalInitialCountAuxArrayAvg)/static_cast<double>(PRUclockStepPeriodNanoseconds);
				this->PRUcurrentTimerValWrap=this->PRUcurrentTimerValWrap-duration_FinalInitialCountAuxArrayAvg/static_cast<double>(PRUclockStepPeriodNanoseconds);
				if (this->PRUcurrentTimerValWrap<0.0){
					this->PRUcurrentTimerValWrap=static_cast<double>(static_cast<double>(iepPRUtimerRange32bits)-fmod(-this->PRUcurrentTimerValWrap,static_cast<double>(iepPRUtimerRange32bits)));
				}
				else{
					this->PRUcurrentTimerValWrap=static_cast<double>(fmod(this->PRUcurrentTimerValWrap,static_cast<double>(iepPRUtimerRange32bits)));
				}*/

				this->PRUcurrentTimerValWrapLong=this->PRUcurrentTimerValWrap;// Update value
				// Unwrap
				if (this->PRUcurrentTimerValWrap<=this->PRUcurrentTimerValOldWrap){this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap+(static_cast<double>(iepPRUtimerRange32bits)-this->PRUcurrentTimerValOldWrap);}
				else{this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap;}

				if (this->PRUcurrentTimerValWrapLong<=this->PRUcurrentTimerValOldWrapLong){this->PRUcurrentTimerValLong=this->PRUcurrentTimerValWrapLong+(static_cast<double>(iepPRUtimerRange32bits)-this->PRUcurrentTimerValOldWrapLong);}
				else{this->PRUcurrentTimerValLong=this->PRUcurrentTimerValWrapLong;}

				this->QPLAFutureTimePointOld=this->TimePointClockCurrentSynchPRU1future;// update value

				if (((this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux) and abs(duration_FinalInitialMeasTrig)>(ApproxInterruptTime/4) and abs(duration_FinalInitialMeasTrigAuxAvg)>(ApproxInterruptTime/1)) or this->iIterPRUcurrentTimerValSynch<static_cast<long long int>(NumSynchMeasAvgAux/2)) and this->iIterPRUcurrentTimerValSynch%2==1){// Initially compute the time for interrupt handling
					if (this->iIterPRUcurrentTimerValSynch>=(static_cast<long long int>(NumSynchMeasAvgAux)-2)){
						cout << "GPIO::PRUsignalTimerSynchJitterLessInterrupt initial time synchronization calibration not achieved! Increase number of NumSynchMeasAvgAux." << endl;
					}
					// To track time difference between last emission/reception - Not used
					//this->QPLAFutureTimePointOld=this->TimePointClockCurrentSynchPRU1future;// Initialization value
					//this->QPLAFutureTimePointOld1=this->QPLAFutureTimePointOld; // Initialization value
					//this->QPLAFutureTimePointOld2=this->QPLAFutureTimePointOld; // Initialization value
					//this->QPLAFutureTimePointOld3=this->QPLAFutureTimePointOld; // Initialization value
					//this->QPLAFutureTimePointOld4=this->QPLAFutureTimePointOld; // Initialization value
					//this->QPLAFutureTimePointOld5=this->QPLAFutureTimePointOld; // Initialization value
					//this->QPLAFutureTimePointOld6=this->QPLAFutureTimePointOld; // Initialization value
					//this->QPLAFutureTimePointOld7=this->QPLAFutureTimePointOld; // Initialization value

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
					if (abs(this->duration_FinalInitialMeasTrigAuxAvg)>ApproxInterruptTime and this->iIterPRUcurrentTimerValSynch>(2*NumSynchMeasAvgAux)){// Much longer than for client node (which typically is below 5000) maybe because more effort to serve PTP messages
						cout << "GPIO::Time for pre processing the time barrier is too short/long " << this->duration_FinalInitialMeasTrigAuxAvg << " ...adjust TimePRUcommandDelay! Set to nominal value of " << static_cast<int>(ApproxInterruptTime) << "..." << endl;
						this->duration_FinalInitialMeasTrigAuxAvg=ApproxInterruptTime;// For the time being adjust it to the nominal initial value
					}
					duration_FinalInitialCountAuxArrayAvg=static_cast<double>(duration_FinalInitialMeasTrigAuxAvg);
				}
				this->TrigAuxIterCount++;				
				// Short range measurements to retrieve offsets (little effected by relative frequency difference)
				// Compute error - Absolute corrected error of absolute error after removing the frequency difference. It adds jitter but probably ensures that hardwware clock offsets are removed periodically (a different story is the offset due to links which is calibrated with the algortm).
				// Dealing with long long int matters due to floating or not precition!!!!
				long double PRUoffsetDriftErrorAbsAux=0.0;
				// Maybe it is important to substract duration_FinalInitialCountAuxArrayAvgInitial to mak eit more time absolut and the synchronization algorith in QPLA always works
				// Not really, SUPER IMPORTANT, in order to not have jumps between periods, it has to be substracted the initial offset error static_cast<long double>(duration_FinalInitialCountAuxArrayAvgInitial)!!!
				// Import computation below for absolute PRU time comparison between nodes
				PRUoffsetDriftErrorAbsAux=-fmodl(static_cast<long double>(this->iIterPRUcurrentTimerVal)*static_cast<long double>(this->TimePRU1synchPeriod)/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits))+static_cast<long double>(this->PRUcurrentTimerValWrap)-static_cast<long double>(duration_FinalInitialCountAuxArrayAvgInitial);
				//this->PRUoffsetDriftErrorAbs=static_cast<double>(PRUoffsetDriftErrorAbsAux);//
				//// Below unwrap the difference
				if (PRUoffsetDriftErrorAbsAux>(static_cast<long double>(iepPRUtimerRange32bits)/2.0)){
					PRUoffsetDriftErrorAbsAux=-(static_cast<long double>(iepPRUtimerRange32bits)-PRUoffsetDriftErrorAbsAux);
				}	
				else if(PRUoffsetDriftErrorAbsAux<(-static_cast<long double>(iepPRUtimerRange32bits)/2.0)){
					PRUoffsetDriftErrorAbsAux=-(-static_cast<long double>(iepPRUtimerRange32bits)-PRUoffsetDriftErrorAbsAux);
				}
				if (PRUoffsetDriftErrorAbsAux<0.0){
					this->PRUoffsetDriftErrorAbs=static_cast<double>(-fmodl(-PRUoffsetDriftErrorAbsAux,static_cast<long double>(iepPRUtimerRange32bits)));
				}
				else{
					this->PRUoffsetDriftErrorAbs=static_cast<double>(fmodl(PRUoffsetDriftErrorAbsAux,static_cast<long double>(iepPRUtimerRange32bits)));
				}
				
				// Absolute corrected error
				this->PRUoffsetDriftErrorAbsArray[iIterPRUcurrentTimerValSynch%ExtraNumSynchMeasAvgAux]=this->PRUoffsetDriftErrorAbs;
				this->PRUoffsetDriftErrorAbsAvg=DoubleMedianFilterSubArray(PRUoffsetDriftErrorAbsArray,ExtraNumSynchMeasAvgAux);// Since we are applying a filter of length NumSynchMeasAvgAux, temporally it effects somehow the longer the filter. Altough it is difficult to correct
				
				if (this->iIterPRUcurrentTimerVal<(2*ExtraNumSynchMeasAvgAux)){// Update until offset fully calculated
					// First update of the values
					QPLAFutureTimePointSendTriggerSignalsOld=this->TimePointClockCurrentSynchPRU1future;// update value
					QPLAFutureTimePointReadTimeStampsOld=this->TimePointClockCurrentSynchPRU1future;// update value
				}

				// The absolute time error has a natural wander due to the fact that the conversion from PRU ticks to real time (and viceversa is not exactly PRUclockStepPeriodNanoseconds). Therefore, an effective relative frequency difference is present that it can be accounted for (this relative frequency difference is computed below).
				if (this->iIterPRUcurrentTimerValPassLong>DistTimePRU1synchPeriod){// Long range measurements to retrieve relative frequency differences
					// Computations for Synch calculation for PRU0 compensation
					// Compute Synch - Relative
					this->EstimateSynch=fmod((static_cast<double>(this->iIterPRUcurrentTimerValPassLong*this->TimePRU1synchPeriod))/static_cast<double>(PRUclockStepPeriodNanoseconds),static_cast<double>(iepPRUtimerRange32bits))/(this->PRUcurrentTimerValLong-this->PRUcurrentTimerValOldWrapLong);// Only correct for PRUcurrentTimerValOld with the PRUoffsetDriftErrorAppliedOldRaw to be able to measure the real synch drift and measure it (not affected by the correction).
					this->EstimateSynchArray[iIterPRUcurrentTimerValSynchLong%NumSynchMeasAvgAux]=this->EstimateSynch;
					//this->ManualSemaphoreExtra=true;
					this->EstimateSynchAvg=DoubleMedianFilterSubArray(EstimateSynchArray,NumSynchMeasAvgAux);
					this->EstimateSynchAvg=this->EstimateSynchAvg*1000000000.0;
					//cout << "GPIO::EstimateSynchAvg: " << this->EstimateSynchAvg << endl;
					if (abs(EstimateSynchAvg-PRUoffsetDriftErrorAbsAvgOldTruncatedPeriodic)>truncatedSynchTrigPeriodPeriodic){
						EstimateSynchAvg=round(EstimateSynchAvg/truncatedSynchTrigPeriodPeriodic)*truncatedSynchTrigPeriodPeriodic;
					}
					else{
						EstimateSynchAvg=truncatedPRUoffsetDriftErrorAbsAvgOldPeriodic;
					}
					PRUoffsetDriftErrorAbsAvgOldTruncatedPeriodic=EstimateSynchAvg;// Update value
					truncatedPRUoffsetDriftErrorAbsAvgOldPeriodic=EstimateSynchAvg; // Update value
					this->EstimateSynchAvg=this->EstimateSynchAvg/1000000000.0;
					if (this->EstimateSynchAvg>1.5 or this->EstimateSynchAvg<0.5){this->EstimateSynchAvg=1.0;}// Robustness
					// Frequency synchronization correction
					// Compute error - Relative correction of the frequency difference. This provides like the stability of the hardware clock referenced to the system clock (disciplined with network protocol)...so in the order of 10^-7
					//this->PRUoffsetDriftError=(-fmod((static_cast<double>(this->iIterPRUcurrentTimerValPassLong*this->TimePRU1synchPeriod))/static_cast<double>(PRUclockStepPeriodNanoseconds),static_cast<double>(iepPRUtimerRange32bits))+(this->PRUcurrentTimerValLong-this->PRUcurrentTimerValOldWrapLong))/static_cast<long double>(TimePRU1synchPeriod); // The multiplication by SynchTrigPeriod is done before applying it in the Triggering and TimeTagging functions
					
					//// Compute error - Relative correction of the frequency difference of the absolute time. This provides like the stability of the hardware clock referenced to the system clock (disciplined with network protocol)...so in the order of 10^-7
					//this->PRUoffsetDriftError=static_cast<long double>(this->PRUoffsetDriftErrorAbsAvg-this->PRUoffsetDriftErrorAbsAvgOld)/static_cast<long double>(this->iIterPRUcurrentTimerValPassLong*TimePRU1synchPeriod);
					//this->PRUoffsetDriftErrorAbsAvgOld=this->PRUoffsetDriftErrorAbsAvg;// Update value
					//// Relative error average
					//this->PRUoffsetDriftErrorArray[iIterPRUcurrentTimerValSynchLong%NumSynchMeasAvgAux]=this->PRUoffsetDriftError;
					//this->PRUoffsetDriftErrorAvg=LongDoubleMedianFilterSubArray(PRUoffsetDriftErrorArray,NumSynchMeasAvgAux);

					// Update values
					this->PRUcurrentTimerValOldWrapLong=this->PRUcurrentTimerValWrap;// Update value
					this->iIterPRUcurrentTimerValPassLong=0; // Reset value
					this->iIterPRUcurrentTimerValSynchLong++; // Update value
				//} Concatenated with the relative frequency difference calculation

				//// Compute error - Relative correction of the frequency difference of the absolute time. This provides like the stability of the hardware clock referenced to the system clock (disciplined with network protocol)...so in the order of ppb
				//if ((iIterPRUcurrentTimerValSynch%static_cast<unsigned long long int>(NumSynchMeasAvgAux/ExtraExtraNumSynchMeasAvgAux))==0 and CountPRUcurrentTimerValSynchLong!=0){
				
					// RElative implementation
					this->PRUoffsetDriftError=this->PRUoffsetDriftErrorAbsAvgOld-static_cast<long double>(this->PRUoffsetDriftErrorAbsAvg);
					this->PRUoffsetDriftErrorAbsAvgOld=static_cast<long double>(this->PRUoffsetDriftErrorAbsAvg);// Update value
					/*
					// Below unwrap the difference
					if (this->PRUoffsetDriftError>(static_cast<long double>(iepPRUtimerRange32bits)/2.0)){
						this->PRUoffsetDriftError=-(static_cast<long double>(iepPRUtimerRange32bits)-this->PRUoffsetDriftError);
					}	
					else if(this->PRUoffsetDriftError<(-static_cast<long double>(iepPRUtimerRange32bits)/2.0)){
						this->PRUoffsetDriftError=-(-static_cast<long double>(iepPRUtimerRange32bits)-this->PRUoffsetDriftError);
					}
					if (this->PRUoffsetDriftError<0.0){
						this->PRUoffsetDriftError=static_cast<double>(-fmodl(-this->PRUoffsetDriftError,static_cast<long double>(iepPRUtimerRange32bits)));
					}
					else{
						this->PRUoffsetDriftError=static_cast<double>(fmodl(this->PRUoffsetDriftError,static_cast<long double>(iepPRUtimerRange32bits)));
					}*/
					this->PRUoffsetDriftError=PRUoffsetDriftError/(static_cast<long double>(this->CountPRUcurrentTimerValSynchLong)*static_cast<long double>(TimePRU1synchPeriod)/static_cast<long double>(PRUclockStepPeriodNanoseconds));// Normalize to the measurement time
					
					//// Relative error average
					this->PRUoffsetDriftErrorArray[iIterPRUcurrentTimerValSynchLongExtra%ExtraExtraNumSynchMeasAvgAux]=this->PRUoffsetDriftError;
					this->PRUoffsetDriftErrorAvg=LongDoubleMedianFilterSubArray(PRUoffsetDriftErrorArray,ExtraExtraNumSynchMeasAvgAux);// averaging
					
					//cout << "GPI::PRUoffsetDriftErrorAvg: " << PRUoffsetDriftErrorAvg << endl;
					PRUoffsetDriftErrorAvg=PRUoffsetDriftErrorAvg*1000000000.0;// Make it integer like
					//cout << "GPI::PRUoffsetDriftErrorAvg: " << PRUoffsetDriftErrorAvg << " ppb" << endl;
					// Smart version of the truncation - avoid being at the border of transition
					if (abs(PRUoffsetDriftErrorAvg-PRUoffsetDriftErrorAvgOldTruncatedPeriodic)>static_cast<long double>(truncatedSynchAbsRelFreq)){
						if (PRUoffsetDriftErrorAvg<0.0){
							PRUoffsetDriftErrorAvg=-floorl(-PRUoffsetDriftErrorAvg/static_cast<long double>(truncatedSynchAbsRelFreq))*static_cast<long double>(truncatedSynchAbsRelFreq);
						}
						else{
							PRUoffsetDriftErrorAvg=floorl(PRUoffsetDriftErrorAvg/static_cast<long double>(truncatedSynchAbsRelFreq))*static_cast<long double>(truncatedSynchAbsRelFreq);
						}
					}
					else{
						PRUoffsetDriftErrorAvg=truncatedPRUoffsetDriftErrorAvgOldPeriodic;
					}
					PRUoffsetDriftErrorAvgOldTruncatedPeriodic=PRUoffsetDriftErrorAvg;// Update value
					truncatedPRUoffsetDriftErrorAvgOldPeriodic=PRUoffsetDriftErrorAvg; // Update value
					PRUoffsetDriftErrorAvg=PRUoffsetDriftErrorAvg/1000000000.0;// Scale it back

					if (abs(this->PRUoffsetDriftErrorAvg)<this->PRUoffsetDriftErrorAvgThresh and this->iIterPRUcurrentTimerValSynchLong>(1.5*NumSynchMeasAvgAux)){this->PRUoffsetDriftErrorAvg=0.0;}// Do not apply relative frequency difference if it is below a certain value
					
					CountPRUcurrentTimerValSynchLong=0;// Update value
					iIterPRUcurrentTimerValSynchLongExtra++;// Update value
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
		else{
			// Clear the timer
			if (FD_ISSET(tfd, &rfds)){
				read(tfd,&TimerExpirations,sizeof(TimerExpirations));
			}
		}
		
		// Information
		if (this->ResetPeriodicallyTimerPRU1 and (this->iIterPRUcurrentTimerVal%(8192*NumSynchMeasAvgAux)==0) and this->iIterPRUcurrentTimerValSynchLong>NumSynchMeasAvgAux){
			////cout << "PRUcurrentTimerVal: " << this->PRUcurrentTimerVal << endl;
			////cout << "PRUoffsetDriftError: " << this->PRUoffsetDriftError << endl;
			cout << "GPIO::Information about synchronization:" << endl;
			cout << "GPIO::Rel. freq. diff. to abs. time - unit conversion drift: " << this->PRUoffsetDriftErrorAvg*1000000000 << " ppb" << endl;
			cout << "GPIO::Abs. time diff. - unit conversion drift: " << PRUoffsetDriftErrorAbsAvg << " PRU units" << endl;
			cout << "GPIO::INDICATIVE only!!! Time to handle interrupt: " << this->duration_FinalInitialCountAuxArrayAvg << " ns." << endl; // A large variation does not imply that the correction offset (time and frequency) are wrong!!!!
			////cout << "PRUoffsetDriftErrorIntegral: " << this->PRUoffsetDriftErrorIntegral << endl;
			////cout << "PRUoffsetDriftErrorAppliedRaw: " << this->PRUoffsetDriftErrorAppliedRaw << endl;
			cout << "GPIO::Ratio rel. freq. diff: " << this->EstimateSynchAvg << endl;
			////cout << "EstimateSynchDirectionAvg: " << this->EstimateSynchDirectionAvg << endl;
			//if (this->EstimateSynchDirectionAvg<1.0){cout << "Clock EstimateSynch advancing" << endl;}
			//else if (this->EstimateSynchDirectionAvg>1.0){cout << "Clock EstimateSynch delaying" << endl;}
			//else{cout << "Clock EstimateSynch neutral" << endl;}
			////cout << "duration_FinalInitialDriftAux: " << duration_FinalInitialDriftAux << endl;
			////cout << "this->iIterPRUcurrentTimerValPass: "<< this->iIterPRUcurrentTimerValPass << endl;
			////cout << "this->iIterPRUcurrentTimerValSynch: "<< this->iIterPRUcurrentTimerValSynch << endl;
		}		
		// RE-upload some variables
		this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
		this->iIterPRUcurrentTimerVal++; // Increase value
		this->iIterPRUcurrentTimerValPass++; // Increase value
		this->iIterPRUcurrentTimerValPassLong++; // Increase value
		this->CountPRUcurrentTimerValSynchLong++;// Increase value
		if (this->iIterPRUcurrentTimerValSynchLong==(2*NumSynchMeasAvgAux) and HardwareSynchStatus==false){
			cout << "Hardware synchronized, now proceeding with the network synchronization managed by hosts..." << endl;
			// Update HardwareSynchStatus			
			this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
			HardwareSynchStatus=true;
			this->release();			
		}
	}// end while

	}
      catch (const std::exception& e) {
	// Handle the exception
      	this->release();
      	cout << "GPIO::PRUsignalTimerSynchJitterLessInterrupt Exception: " << e.what() << endl;
      }

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
	try{
	this->QPLAFlagTestSynch=QPLAFlagTestSynchAux;
	SynchTrigPeriod=SynchTrigPeriodAux;// Histogram/Period value
	NumQuBitsPerRun=NumQuBitsPerRunAux;
	//cout << "GPIO::ReadTimeStamps NumQuBitsPerRun: " << NumQuBitsPerRun << endl;
	//valpAuxHolder=valpHolder+4+6*NumQuBitsPerRun;// 6* since each detection also includes the channels (2 Bytes) and 4 bytes for 32 bits counter, and plus 4 since the first tag is captured at the very beggining
	AccumulatedErrorDriftAux=FineSynchAdjValAux[0];// Synch trig offset
	AccumulatedErrorDrift=FineSynchAdjValAux[1]; // Synch trig frequency
	QuadEmitDetecSelecGPIO=QuadEmitDetecSelecAux;// Update value
	std::chrono::nanoseconds duration_back(QPLAFutureTimePointNumber);
	this->QPLAFutureTimePoint=Clock::time_point(duration_back);
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint-std::chrono::nanoseconds(static_cast<unsigned long long int>(2.0*GuardPeriod*PRUclockStepPeriodNanoseconds));// Timetagger starts listening 2 periods in advance to avoid interrupt and signals to arrive concurrently at the timetagger
	requestSemaphoreWhileWait=SemaphoreSetWhileWait();
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(7*TimePRUcommandDelay);// Give some margin so that ReadTimeStamps and coincide in the respective methods of GPIO. Only for th einitial run, since the TimeStaps are run once (to enter the acquire in GPIO). What consumes time is writting to PRU, then times 4 since 4 writings to PRU before sleep in GPIO
	this->QPLAFutureTimePointSleep=this->QPLAFutureTimePoint;// Update value
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(1*TimePRUcommandDelay);// Crucial to make the link between PRU clock and system clock (already well synchronized). Two memory mapping to PRU
	SynchRem=static_cast<int>((static_cast<long double>(1.5*GuardPeriod)-fmodl(static_cast<long double>(PRUoffsetDriftErrorAbsAvg)+(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(GuardPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));// For time stamping it waits 1.5
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(SynchRem);
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestSemaphoreWhileWait,NULL); // Synch barrier. so the time within acquired semaphore is not so large
	//cout << "Before this->ManualSemaphore...to be commented" << endl;
	while (this->ManualSemaphore and whileProtAux>0){whileProtAux--;};// Wait other process// Very critical to not produce measurement deviations when assessing the periodic snchronization
	whileProtAux=whileProtAuxMax;
	//cout << "After this->ManualSemaphore...to be commented" << endl;
	this->ManualSemaphoreExtra=true;
	this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->AdjPulseSynchCoeffAverage=static_cast<long double>(this->EstimateSynchAvg);// Acquire this value for the this tag reading set
	///////////
	if (QPLAFlagTestSynchAux==true){TagsSeparationDetRelFreqAdpSlope=TagsSeparationDetRelFreqAdpSlopeSynch;}
	else{TagsSeparationDetRelFreqAdpSlope=TagsSeparationDetRelFreqAdpSlopeRegular;}
	if (this->GuardPeriod<=(this->MultFactorEffSynchPeriod*this->SynchTrigPeriod)){cout << "GPIO::ReadTimeStamps Attention!!! GuardPeriod smaller or equal than the effective period...check inconsistency!!!" << endl;}
	pru0dataMem_int[2]=static_cast<unsigned int>(this->GuardPeriod);// Indicate guard period of the sequence signal, so that it falls correctly and it is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru0dataMem_int[1]=static_cast<unsigned int>(this->NumQuBitsPerRun); // set number captures
	// Different modes of periodic correction
	//switch (QuadEmitDetecSelecGPIO){
	//	case 1: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld1;break;}
	//	case 2: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld2;break;}
	//	case 3: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld3;break;}
	//	case 4: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld4;break;}
	//	case 5: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld5;break;}
	//	case 6: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld6;break;}
	//	case 7: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld7;break;}
	//	default: {break;}
	//}
	// There is an intrinsic limitation estimating PRUoffsetDriftErrorAbsAvg, which is impaired by the time jitter of handling the interrupt to check the curren tPRU clock.
	// At some point, it could be that the PRU clock is more stable than this jitter (even more so if SyncE synchronizaed the clocks).
	// Then, for the triggering of the signals, it is better to truncate this value to a sub multiple of the SynchTrigPeriod; and let the qubits handle the time offset synchronization from this point onwards
	// The qubits continuously correct for the time offset difference due to the QPLA function (SmallDriftContinuousCorrection) that after each masurements send to the sender the correction estimated to the time offfseet
	// With godd enough hardware clocks, the jitter is limited by the BBB kernel determinisms, in the order of 5us. With hundreds of averaging it is reduced this jitter around 10 times (since it scales as sqrt(NumberAveraging))
	// Therefore, the truncation could be a submultiple of SynchTrigPeriod which is slightly larger than the averaged interrupt jitter (around 500ns, which in PRU units would be 100)
	// Better to be a number multiple of power of 2 and larger than this minim jitter PRU value
	//if (PRUoffsetDriftErrorAbsAvg<0.0){
	//	truncatedPRUoffsetDriftErrorAbsAvg=-round((-PRUoffsetDriftErrorAbsAvg)/truncatedSynchTrigPeriod)*truncatedSynchTrigPeriod;
	//}
	//else{
	//	truncatedPRUoffsetDriftErrorAbsAvg=round((PRUoffsetDriftErrorAbsAvg)/truncatedSynchTrigPeriod)*truncatedSynchTrigPeriod;
	//}
	// Version where PTP control plane tries to dominate the timing (not good idea)
	// Smart version of the truncation - avoid being at the border of transition
	if (SynchPlaneDomainMode==true){
		if (abs(PRUoffsetDriftErrorAbsAvg-PRUoffsetDriftErrorAbsAvgOldTruncatedRecv)>truncatedSynchTrigPeriod){
			truncatedPRUoffsetDriftErrorAbsAvg=round(PRUoffsetDriftErrorAbsAvg/truncatedSynchTrigPeriod)*truncatedSynchTrigPeriod;
			TimePointUpdateFlagAux=true;
		}
		else{
			truncatedPRUoffsetDriftErrorAbsAvg=truncatedPRUoffsetDriftErrorAbsAvgOldRecv;
		}
		PRUoffsetDriftErrorAbsAvgOldTruncatedRecv=PRUoffsetDriftErrorAbsAvg;// Update value
		truncatedPRUoffsetDriftErrorAbsAvgOldRecv=truncatedPRUoffsetDriftErrorAbsAvg; // Update value
	}
	else{ // Real-time clock dominates
		truncatedPRUoffsetDriftErrorAbsAvg=0.0;
	}
	
	// The time in PRU units to consider (as an approximation) for correction with relative frequency correction is composed of half the effective guard period due to interrupt alignment handling, the effective period, the time since last emission detection, then again MultFactorEffSynchPeriod*SynchTrigPeriod more or less
	// Correcting for relative frequency difference is an approximation game (due to all the variable involved). The best is to have all hardware clocks so in-phase synchronized that there is no relative frequency difference.
	//ldTimePointClockTagPRUDiff=static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(0.5*GuardPeriod)+0.5*NumSynchMeasAvgAux*static_cast<long double>(TimePRU1synchPeriod)/static_cast<long double>(PRUclockStepPeriodNanoseconds);//static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointOld).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value
	// Since we truncated the offset value (due to the jitter) it no longer makes sense that the QPLAFutureTimePointOld is used. Instead, it makes more sense, to take the last time the offset exceeded the truncation
	//ldTimePointClockTagPRUDiff=static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(0.5*GuardPeriod)+static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointReadTimeStampsOld).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value
	ldTimePointClockTagPRUDiff=static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value
	
	if (TimePointUpdateFlagAux==true){
		this->QPLAFutureTimePointReadTimeStampsOld=this->QPLAFutureTimePoint;
		TimePointUpdateFlagAux=false;
	}

	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{// Time and frequency correction			
			PRUoffFreqTotalAux=static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg)+ldTimePointClockTagPRUDiff*static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg);
			break;
		}
		case 2:{// Time correction
			PRUoffFreqTotalAux=static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg);
			break;
		}
		case 1:{ // Frequency correction
			PRUoffFreqTotalAux=ldTimePointClockTagPRUDiff*static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg);
			break;
		}
		default:{PRUoffFreqTotalAux=0.0;break;}// None time nor frequency correction
	}
	
	//switch (QuadEmitDetecSelecGPIO){// Update value	
	//	case 1: {this->QPLAFutureTimePointOld1=this->QPLAFutureTimePoint;break;}
	//	case 2: {this->QPLAFutureTimePointOld2=this->QPLAFutureTimePoint;break;}
	//	case 3: {this->QPLAFutureTimePointOld3=this->QPLAFutureTimePoint;break;}
	//	case 4: {this->QPLAFutureTimePointOld4=this->QPLAFutureTimePoint;break;}
	//	case 5: {this->QPLAFutureTimePointOld5=this->QPLAFutureTimePoint;break;}
	//	case 6: {this->QPLAFutureTimePointOld6=this->QPLAFutureTimePoint;break;}
	//	case 7: {this->QPLAFutureTimePointOld7=this->QPLAFutureTimePoint;break;}
	//	default: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePoint;break;}
	//}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Now, The time in PRU units to consider (as an approximation) for correction with relative frequency correction is composed of the effective period, and the period
	if (abs(AccumulatedErrorDrift)<AccumulatedErrorDriftThresh){AccumulatedErrorDrift=0.0;}// Do not apply computed synch relative frequency difference
	//cout << "GPIO::ReadTimeStamps AccumulatedErrorDrift: " << AccumulatedErrorDrift << endl;
	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{// Time and frequency correction			
			PRUoffFreqTotalAux+=static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(AccumulatedErrorDrift)*ldTimePointClockTagPRUDiff;
			break;
		}
		case 2:{// Time correction
			PRUoffFreqTotalAux+=static_cast<long double>(AccumulatedErrorDriftAux);
			break;
		}
		case 1:{ // Frequency correction
			PRUoffFreqTotalAux+=static_cast<long double>(AccumulatedErrorDrift)*ldTimePointClockTagPRUDiff;
			break;
		}
		default:{PRUoffFreqTotalAux+=0.0;break;}// None time nor frequency correction
	}
	
	// Attention ldTimePointClockTagPRUinitial is needed in DDRdumpdata!!!!
	ldTimePointClockTagPRUinitial=static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(GuardPeriod)+static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);;//+static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);//+static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod);// Time point after all PRU synch steps. The periodic synch offset is not accounted for
	
	// Accounting for the effective offset and frequency correction
	if (PRUoffFreqTotalAux<0.0){
		PRUoffFreqTotalAux=-fmodl(-PRUoffFreqTotalAux,static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	else{
		PRUoffFreqTotalAux=fmodl(PRUoffFreqTotalAux,static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	// Prepare the overall correction for PRU loops (so divide by 2)
	PRUoffFreqTotalAux=(static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)+PRUoffFreqTotalAux)/2.0;
	if (PRUoffFreqTotalAux<=0.0){PRUoffFreqTotalAux=1.0;}// To avoid having PRU stall

	pru0dataMem_int[3]=static_cast<unsigned int>(PRUoffFreqTotalAux);

	// Different time tagging window size, when synching with respect normal operation.
	if (QPLAFlagTestSynchAux==true){pru0dataMem_int[4]=static_cast<unsigned int>(this->TTGcoincWin);}// Minimum width for the timetagging coincidence window, to have accuracy
	else{pru0dataMem_int[4]=static_cast<unsigned int>(this->TTGcoincWin);} // set coincidence window length

	// Sleep barrier to synchronize the different nodes at this point, so the below calculations and entry times coincide
	requestCoincidenceWhileWait=CoincidenceSetWhileWait();
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestCoincidenceWhileWait,NULL); // Synch barrier. So that SendTriggerSignals and ReadTimeStamps of the different nodes coincide	
	pru0dataMem_int[0]=static_cast<unsigned int>(QuadEmitDetecSelecAux); // set command
	//QPLAFutureTimePoint=QPLAFutureTimePoint-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg);// Actually, the time measured duration_FinalInitialMeasTrigAuxAvg is not indicative of much (only if it changes a lot to high values it means trouble)
	// Set top priority
	//this->setMaxRrPriority(PriorityValTop);
	while (Clock::now()<QPLAFutureTimePoint);// Busy wait time synch sending signals. With while loop, it is more aggresive to take control (hence fall within the correct interrupt period) but has more variation (but it does not matter since there will be the proceedure to synch in the PRU)
	prussdrv_pru_send_event(21);
	//this->TimePointClockTagPRUfinal=Clock::now();// Compensate for delays
	// Set regular priority
	//this->setMaxRrPriority(PriorityValRegular);
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

	// Freeing the semaphore is done in the proper line inside DDRdumpdata
	//this->ManualSemaphore=false;
	//this->ManualSemaphoreExtra=false;
	//this->release();

	// Debugging
	//cout << "GPIO::ReadTimeStamps QuadEmitDetecSelecGPIO: " << QuadEmitDetecSelecGPIO << endl;

	this->DDRdumpdata(iIterRunsAux); // Pre-process tags. Needs to access memory of PRU, so better within the controlled acquired environment

	}
      catch (const std::exception& e) {
	// Handle the exception
      	this->release();
      	cout << "GPIO::ReadTimeStamps Exception: " << e.what() << endl;
     }
return 0;// all ok
}

int GPIO::SendTriggerSignals(int QuadEmitDetecSelecAux, double SynchTrigPeriodAux,unsigned int NumberRepetitionsSignalAux,double* FineSynchAdjValAux,unsigned long long int QPLAFutureTimePointNumber, bool QPLAFlagTestSynchAux){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
	try{
	this->QPLAFlagTestSynch=QPLAFlagTestSynchAux;
	SynchTrigPeriod=SynchTrigPeriodAux;// Histogram/Period value
	NumberRepetitionsSignal=static_cast<unsigned int>(NumberRepetitionsSignalAux);// Number of repetitions to send signals
	AccumulatedErrorDriftAux=FineSynchAdjValAux[0];// Synch trig offset
	AccumulatedErrorDrift=FineSynchAdjValAux[1]; // Synch trig frequency
	QuadEmitDetecSelecGPIO=QuadEmitDetecSelecAux;// Update value
	std::chrono::nanoseconds duration_back(QPLAFutureTimePointNumber);
	this->QPLAFutureTimePoint=Clock::time_point(duration_back);
	requestSemaphoreWhileWait=SemaphoreSetWhileWait();
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(7*TimePRUcommandDelay);// Give some margin so that ReadTimeStamps and coincide in the respective methods of GPIO. Only for th einitial run, since the TimeStaps are run once (to enter the acquire in GPIO). What consumes time is writting to PRU, then times 4 since 4 writings to PRU before sleep in GPIO
	this->QPLAFutureTimePointSleep=this->QPLAFutureTimePoint;// Update value
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(1*TimePRUcommandDelay);// Since one memory mapping to PRU memory
	//SynchRem=static_cast<int>((static_cast<long double>(1.5*GuardPeriod)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(GuardPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	SynchRem=static_cast<int>((static_cast<long double>(1.5*GuardPeriod)-fmodl(static_cast<long double>(PRUoffsetDriftErrorAbsAvg)+(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(GuardPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	this->QPLAFutureTimePoint=this->QPLAFutureTimePoint+std::chrono::nanoseconds(SynchRem);
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestSemaphoreWhileWait,NULL); // Synch barrier. so the time within acquired semaphore is not so large
	while (this->ManualSemaphore and whileProtAux>0){whileProtAux--;};// Wait other process// Very critical to not produce measurement deviations when assessing the periodic snchronization
	whileProtAux=whileProtAuxMax;
	this->ManualSemaphoreExtra=true;
	this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
	this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
	//this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
	// Apply a slotted synch configuration (like synchronized Ethernet)
	this->AdjPulseSynchCoeffAverage=static_cast<long double>(this->EstimateSynchAvg);
	// Different Signal ON time, when synching with respect normal operation.
	if (QPLAFlagTestSynchAux==true){pru1dataMem_int[5]=static_cast<unsigned int>(minSigONPeriod);this->SigOFFPeriod=this->SynchTrigPeriod-minSigONPeriod;}// Minimum width time on, to have accuracy
	else{pru1dataMem_int[5]=static_cast<unsigned int>(this->SigONPeriod);this->SigOFFPeriod=this->SynchTrigPeriod-this->SigONPeriod;} // signal width time on in regular operation
	if (SigOFFPeriod<1.0){SigOFFPeriod=1.0; cout << "GPIO::SendTriggerSignals SigOFFPeriod smaller than 1 PRU unit...check inconsistency!!!" << endl;}
	if (this->GuardPeriod<=(this->MultFactorEffSynchPeriod*this->SynchTrigPeriod)){cout << "GPIO::SendTriggerSignals Attention!!! GuardPeriod smaller or equal than the effective period...check inconsistency!!!" << endl;}
	pru1dataMem_int[7]=static_cast<unsigned int>(this->SigOFFPeriod);// Off time
	pru1dataMem_int[3]=static_cast<unsigned int>(this->GuardPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru1dataMem_int[1]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
	// Different modes of periodic correction
	//switch (QuadEmitDetecSelecGPIO){
	//	case 1: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld1;break;}
	//	case 2: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld2;break;}
	//	case 3: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld3;break;}
	//	case 4: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld4;break;}
	//	case 5: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld5;break;}
	//	case 6: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld6;break;}
	//	case 7: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePointOld7;break;}
	//	default: {break;}
	//}
	
	// There is an intrinsic limitation estimating PRUoffsetDriftErrorAbsAvg, which is impaired by the time jitter of handling the interrupt to check the curren tPRU clock.
	// At some point, it could be that the PRU clock is more stable than this jitter (even more so if SyncE synchronizaed the clocks).
	// Then, for the triggering of the signals, it is better to truncate this value to a sub multiple of the SynchTrigPeriod; and let the qubits handle the time offset synchronization from this point onwards
	// The qubits continuously correct for the time offset difference due to the QPLA function (SmallDriftContinuousCorrection) that after each masurements send to the sender the correction estimated to the time offfseet
	// With godd enough hardware clocks, the jitter is limited by the BBB kernel determinisms, in the order of 5us. With hundreds of averaging it is reduced this jitter around 10 times (since it scales as sqrt(NumberAveraging))
	// Therefore, the truncation could be a submultiple of SynchTrigPeriod which is slightly larger than the averaged interrupt jitter (around 500ns, which in PRU units would be 100)
	// Better to be a number multiple of power of 2 and larger than this minim jitter PRU value
	//if (PRUoffsetDriftErrorAbsAvg<0.0){
	//	truncatedPRUoffsetDriftErrorAbsAvg=-round((-PRUoffsetDriftErrorAbsAvg+truncatedSynchTrigPeriod/2.0)/truncatedSynchTrigPeriod)*truncatedSynchTrigPeriod;
	//}
	//else{
	//	truncatedPRUoffsetDriftErrorAbsAvg=round((PRUoffsetDriftErrorAbsAvg+truncatedSynchTrigPeriod/2.0)/truncatedSynchTrigPeriod)*truncatedSynchTrigPeriod;
	//}
	if (SynchPlaneDomainMode==true){
		// Smart version of the truncation - avoid being at the border of transition
		if (abs(PRUoffsetDriftErrorAbsAvg-PRUoffsetDriftErrorAbsAvgOldTruncatedEmit)>truncatedSynchTrigPeriod){
			truncatedPRUoffsetDriftErrorAbsAvg=round(PRUoffsetDriftErrorAbsAvg/truncatedSynchTrigPeriod)*truncatedSynchTrigPeriod;
			TimePointUpdateFlagAux=true;
		}
		else{
			truncatedPRUoffsetDriftErrorAbsAvg=truncatedPRUoffsetDriftErrorAbsAvgOldEmit;
		}
		PRUoffsetDriftErrorAbsAvgOldTruncatedEmit=PRUoffsetDriftErrorAbsAvg;// Update value
		truncatedPRUoffsetDriftErrorAbsAvgOldEmit=truncatedPRUoffsetDriftErrorAbsAvg; // Update value
	}
	else{// Real-time clock dominates
		truncatedPRUoffsetDriftErrorAbsAvg=0.0;
	}
		
	// The time in PRU units to consider (as an approximation) for correction with relative frequency correction is composed of half the effective period due to interrupt alignment handling, the effective period, the time since last emission detection, then again MultFactorEffSynchPeriod*SynchTrigPeriod more or less
	//ldTimePointClockTagPRUDiff=static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(0.5*GuardPeriod)+0.5*NumSynchMeasAvgAux*static_cast<long double>(TimePRU1synchPeriod)/static_cast<long double>(PRUclockStepPeriodNanoseconds);//static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointOld).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value
	// Since we truncated the offset value (due to the jitter) it no longer makes sense that the QPLAFutureTimePointOld is used. Instead, it makes more sense, to take the last time the offset exceeded the truncation
	//ldTimePointClockTagPRUDiff=static_cast<long double>(0.5*MultFactorEffSynchPeriod*SynchTrigPeriod)+static_cast<long double>(0.5*GuardPeriod)+static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint-this->QPLAFutureTimePointSendTriggerSignalsOld).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value
	ldTimePointClockTagPRUDiff=static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->QPLAFutureTimePoint.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds);// update value
	
	if(TimePointUpdateFlagAux==true){
		this->QPLAFutureTimePointSendTriggerSignalsOld=this->QPLAFutureTimePoint;
		TimePointUpdateFlagAux=false;
	}

	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{// Time and frequency correction			
			PRUoffFreqTotalAux=static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg)+ldTimePointClockTagPRUDiff*static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg);
			break;
		}
		case 2:{// Time correction
			PRUoffFreqTotalAux=static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg);
			break;
		}
		case 1:{ // Frequency correction
			PRUoffFreqTotalAux=ldTimePointClockTagPRUDiff*static_cast<long double>(truncatedPRUoffsetDriftErrorAbsAvg);
			break;
		}
		default:{PRUoffFreqTotalAux=0.0;break;}// None time nor frequency correction
	}

	//switch (QuadEmitDetecSelecGPIO){// Update value	
	//	case 1: {this->QPLAFutureTimePointOld1=this->QPLAFutureTimePoint;break;}
	//	case 2: {this->QPLAFutureTimePointOld2=this->QPLAFutureTimePoint;break;}
	//	case 3: {this->QPLAFutureTimePointOld3=this->QPLAFutureTimePoint;break;}
	//	case 4: {this->QPLAFutureTimePointOld4=this->QPLAFutureTimePoint;break;}
	//	case 5: {this->QPLAFutureTimePointOld5=this->QPLAFutureTimePoint;break;}
	//	case 6: {this->QPLAFutureTimePointOld6=this->QPLAFutureTimePoint;break;}
	//	case 7: {this->QPLAFutureTimePointOld7=this->QPLAFutureTimePoint;break;}
	//	default: {this->QPLAFutureTimePointOld=this->QPLAFutureTimePoint;break;}
	//}

	////////////////////////////////////////////////////////////////////////////////////////////
	// The synch offset
	// Now, The time in PRU units to consider (as an approximation) for correction with relative frequency correction is composed of the effective period, and the period		
	if (abs(AccumulatedErrorDrift)<AccumulatedErrorDriftThresh){AccumulatedErrorDrift=0.0;}// Do not apply computed synch relative frequency difference
	switch (SynchCorrectionTimeFreqNoneFlag){
		case 3:{// Time and frequency correction			
			PRUoffFreqTotalAux+=static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(AccumulatedErrorDrift)*ldTimePointClockTagPRUDiff;
			break;
		}
		case 2:{// Time correction
			PRUoffFreqTotalAux+=static_cast<long double>(AccumulatedErrorDriftAux);
			break;
		}
		case 1:{ // Frequency correction
			PRUoffFreqTotalAux+=static_cast<long double>(AccumulatedErrorDrift)*ldTimePointClockTagPRUDiff;
			break;
		}
		default:{PRUoffFreqTotalAux+=0.0;break;}// None time nor frequency correction
	}
		
	if (SynchCorrectionTimeFreqNoneFlag==1 or SynchCorrectionTimeFreqNoneFlag==3){ // It has to be a much finer control at PRU level (and also this level) in order to be able to compensate for the very low hardware wandering/drift.
		// Compute the number of quarter passes since it is histogram analysis of 4 symbols to add/substract 1 PRU unit of compensation
		long double AccumulatedErrorDriftPRUoffsetDriftErrorAvg=static_cast<long double>(AccumulatedErrorDrift)+PRUoffsetDriftErrorAvg;
		// First sum the adimensional relative frequency differences, due to the internal system_clock and the one wanted to be compensated from the synch algorithm.
		// Then, invert the value, since it will tell the number of PRU units so that this value produces 1 PRU unit difference (either positive of negative).
		// Then, it is divided by 4 because it is histogram analysis
		// Then, it is divided by the period length
		// Then is multiplied by 2, because the assembler code can do loops that consume double
		//cout << "GPIO::SendTriggerSignals AccumulatedErrorDriftPRUoffsetDriftErrorAvg: " << AccumulatedErrorDriftPRUoffsetDriftErrorAvg << endl;
		if (AccumulatedErrorDriftPRUoffsetDriftErrorAvg==0.0){ContCorr=static_cast<unsigned int>(4294967295);}
		else{
			long double AccumulatedErrorDriftPRUoffsetDriftErrorAvgAux=2.0*(1.0/abs(AccumulatedErrorDriftPRUoffsetDriftErrorAvg))/static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod);
			if (AccumulatedErrorDriftPRUoffsetDriftErrorAvgAux>4294967295.0){AccumulatedErrorDriftPRUoffsetDriftErrorAvgAux=4294967295.0;}
			else if (AccumulatedErrorDriftPRUoffsetDriftErrorAvgAux<1.0){AccumulatedErrorDriftPRUoffsetDriftErrorAvgAux=1.0;}
			ContCorr=static_cast<unsigned int>(AccumulatedErrorDriftPRUoffsetDriftErrorAvgAux);
		}
		if (AccumulatedErrorDriftPRUoffsetDriftErrorAvg<0.0){
			ContCorrSign=static_cast<unsigned int>(((SynchTrigPeriod-SigONPeriod)-4.0-4.0)/2.0+1.0);// 1 positive unit intra pulses relative frequency difference correction
		}
		else if(AccumulatedErrorDriftPRUoffsetDriftErrorAvg>0.0){
			ContCorrSign=static_cast<unsigned int>(((SynchTrigPeriod-SigONPeriod)-4.0-4.0)/2.0-1.0);// 1 negative unit intra pulses relative frequency difference correction
		}
		else{
			ContCorrSign=static_cast<unsigned int>(((SynchTrigPeriod-SigONPeriod)-4.0-4.0)/2.0);// No intra pulses relative frequency difference correction
		}
	}
	else{// Do not apply relative frequency correction in the PRU script
		ContCorr=static_cast<unsigned int>(4294967295);
		ContCorrSign=static_cast<unsigned int>(((SynchTrigPeriod-SigONPeriod)-4.0-4.0)/2.0);// No intra pulses relative frequency difference correction
	}

	// Accounting for the effective offset and frequency correction
	if (PRUoffFreqTotalAux<0.0){
		PRUoffFreqTotalAux=-fmodl(-PRUoffFreqTotalAux,static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	else{
		PRUoffFreqTotalAux=fmodl(PRUoffFreqTotalAux,static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod));
	}
	// Prepare the overall correction for PRU loops (so divide by 2)
	PRUoffFreqTotalAux=(static_cast<long double>(MultFactorEffSynchPeriod*SynchTrigPeriod)+PRUoffFreqTotalAux)/2.0;
	if (PRUoffFreqTotalAux<=0.0){PRUoffFreqTotalAux=1.0;}
	pru1dataMem_int[2]=static_cast<unsigned int>(PRUoffFreqTotalAux);
	// Particular for histogram, tell the adjusted period accounting for relative frequency difference
	pru1dataMem_int[4]=static_cast<unsigned int>(ContCorr);// Referenced to the corrected synch period
	pru1dataMem_int[6]=static_cast<unsigned int>(ContCorrSign);// Referenced to the corrected synch period

	// Sleep barrier to synchronize the different nodes at this point, so the below calculations and entry times coincide
	requestCoincidenceWhileWait=CoincidenceSetWhileWait();
	clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestCoincidenceWhileWait,NULL); // Synch barrier. So that SendTriggerSignals and ReadTimeStamps of the different nodes coincide	

	pru1dataMem_int[0]=static_cast<unsigned int>(QuadEmitDetecSelecAux); // set command. Generate signals. Takes around 900000 clock ticks
	//this->QPLAFutureTimePoint=this->QPLAFutureTimePoint-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg); // Actually, the time measured duration_FinalInitialMeasTrigAuxAvg is not indicative of much (only if it changes a lot to high values it means trouble)
	// Set top priority
	//this->setMaxRrPriority(PriorityValTop);
	////if (Clock::now()<this->QPLAFutureTimePoint){cout << "Check that we have enough time" << endl;}
	while (Clock::now()<this->QPLAFutureTimePoint);// Busy wait time synch sending signals. With while loop, it is more aggresive to take control (hence fall within the correct interrupt period) but has more variation (but it does not matter since there will be the proceedure to synch in the PRU)
	// Important, the following line at the very beggining to reduce the command jitter
	prussdrv_pru_send_event(22);//Send host arm to PRU1 interrupt
	//this->TimePointClockSynchPRUfinal=Clock::now();
	// Set regular priority
	//this->setMaxRrPriority(PriorityValRegular);
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

	}
      catch (const std::exception& e) {
	// Handle the exception
      	this->release();
      	cout << "GPIO::SendTriggerSignals Exception: " << e.what() << endl;
     }

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
//cout << "GPIO::Reading timetags" << endl;
// Reading data from PRU shared and own RAMs
//DDR_regaddr = (short unsigned int*)ddrMem + OFFSET_DDR;
valp=valpHolder; // Coincides with SHARED in PRUassTaggDetSimpleScript.p
//valpAux=valpAuxHolder;
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
// When unsigned short - Not used
//valThresholdResetCounts=static_cast<unsigned int>(*valpAux);
//valpAux++;// 1 times 16 bits
//valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<16;
//valpAux++;// 1 times 16 bits
//cout << "valThresholdResetCounts: " << valThresholdResetCounts << endl;
//////////////////////////////////////////////////////////////////////////////

// Reading first calibration tag and link it to the system clock
OldLastTimeTagg=static_cast<unsigned long long int>(*CalpHolder);//extendedCounterPRUaux + static_cast<unsigned long long int>(*CalpHolder);
//cout << "GPIO::OldLastTimeTagg: " << OldLastTimeTagg << endl;

// Slot the TimeTaggsLast, since it eventually has to start at the beggining of the effective period
this->TimeTaggsLast=(static_cast<unsigned long long int>(ldTimePointClockTagPRUinitial)/static_cast<unsigned long long int>(GuardPeriod)+2)*static_cast<unsigned long long int>(GuardPeriod);//(static_cast<unsigned long long int>(ldTimePointClockTagPRUinitial)/static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod))*static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod);

//Furthermore, remove some time from epoch - in multiples of the SynchTrigPeriod, so it is easier to handle in the above agents
//cout << "GPIO::DDRdumpdata TimeTaggsLast before removing Epoch: " << TimeTaggsLast << endl;
if (this->TimeTaggsLast<((this->ULLIEpochReOffset/static_cast<unsigned long long int>(GuardPeriod))*static_cast<unsigned long long int>(GuardPeriod))){
	cout << "GPIO::DDRdumpdata ULLIEpochReOffset is too large...not removing actual time since epoch!!!! check: " << endl;
}
else{
	this->TimeTaggsLast=static_cast<unsigned long long int>(static_cast<long long int>(this->TimeTaggsLast)-static_cast<long long int>((this->ULLIEpochReOffset/static_cast<unsigned long long int>(GuardPeriod))*static_cast<unsigned long long int>(GuardPeriod)));//static_cast<unsigned long long int>(static_cast<long long int>(this->TimeTaggsLast)-static_cast<long long int>((this->ULLIEpochReOffset/static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod))*static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod)));
}
//cout << "GPIO::DDRdumpdata TimeTaggsLast after removing Epoch: " << TimeTaggsLast << endl;
if (iIterRunsAux==0){TimeTaggsLastStored=TimeTaggsLast;TotalCurrentNumRecords=0;}// First iteration of current runs, store the value for synchronization time difference calibration

long long int LLIOldLastTimeTagg=static_cast<long long int>(OldLastTimeTagg);
// It is important to adjust the OldLastTimeTagg so that absolute timetagg references match among nodes
LLIOldLastTimeTagg=LLIOldLastTimeTagg%(static_cast<unsigned long long int>(MultFactorEffSynchPeriod*SynchTrigPeriod));

unsigned int valCycleCountPRUAux1;
unsigned int valCycleCountPRUAux2;
//cout << "GPIO::NumQuBitsPerRun " << NumQuBitsPerRun << endl;
//cout << "GPIO::MaxNumQuBitsMemStored " << MaxNumQuBitsMemStored << endl;
//for (iIterDump=0; iIterDump<NumQuBitsPerRun; iIterDump++){
CurrentiIterDump=0;
int CurrentiIterDumpAux=0;
bool ValidTag=false;
unsigned short ValidTagMask=0;
switch (QuadEmitDetecSelecGPIO){// Once bits are re-ordered, make sure to keep only the channels of interest
	case 7:{			
		ValidTagMask=0x0FFF;
		break;
	}
	case 6:{			
		ValidTagMask=0x0FF0;
		break;
	}
	case 5:{		
		ValidTagMask=0x0F0F;
		break;
	}
	case 4:{			
		ValidTagMask=0x0F00;
		break;
	}
	case 3:{			
		ValidTagMask=0x00FF;
		break;
	}
	case 2:{
		ValidTagMask=0x00F0;
		break;
	}
	case 1:{
		ValidTagMask=0x000F;
		break;
	}
	default:{ValidTagMask=0x0000;break;}// None time nor frequency correction
}
extendedCounterPRUholder=1;// Re-initialize at each run. 1 so that at least the first is checked and stored
extendedCounterPRUholderOld=0;// Re-initialize at each run
int TotalCurrentNumRecordsOld=TotalCurrentNumRecords;
int PendingNumQuBitsPerRun=static_cast<int>(sharedMem_int[OFFSET_SHAREDRAM+LAST_SHAREDRAMPOS]);
//cout << "GPIO::DDRdumpdata NumQuBitsPerRun-PendingNumQuBitsPerRun: " << (NumQuBitsPerRun-PendingNumQuBitsPerRun) << endl;
while (CurrentiIterDumpAux<NumQuBitsPerRun and extendedCounterPRUholder>extendedCounterPRUholderOld and (NumQuBitsPerRun-PendingNumQuBitsPerRun)>CurrentiIterDump){// Do it until a timetagg is smaller in value than the previous one, because it means that it could not achieve to capture NumQuBitsPerRun
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
	//TimeTaggsStored[TotalCurrentNumRecords]=static_cast<unsigned long long int>(static_cast<long double>(static_cast<long long int>(extendedCounterPRUholder)-LLIOldLastTimeTagg)*AdjPulseSynchCoeffAverage)+TimeTaggsLast;	// The fist OldLastTimeTagg and TimeTaggsLast of the iteration is compensated for with the calibration tag together with the accumulated synchronization error	    
	TimeTaggsStored[TotalCurrentNumRecords]=static_cast<unsigned long long int>(static_cast<long long int>(extendedCounterPRUholder)-LLIOldLastTimeTagg)+TimeTaggsLast;	// The fist OldLastTimeTagg and TimeTaggsLast of the iteration is compensated for with the calibration tag together with the accumulated synchronization error	    
	//////////////////////////////////////////////////////////////		
	// When unsigned short
	ChannelTagsStored[TotalCurrentNumRecords]=this->packBits(static_cast<unsigned short>(*valp)); // we're just interested in 12 bits which we have to re-order
	valp++;// 1 times 16 bits
	// Check that it belong to a channel of interest
	//cout << "GPIO::ChannelTagsStored[TotalCurrentNumRecords]: " << ChannelTagsStored[TotalCurrentNumRecords] << endl;
	//cout << "GPIO::ValidTagMask: " << ValidTagMask << endl;
	if ((ChannelTagsStored[TotalCurrentNumRecords]&ValidTagMask)>0){ValidTag=true;}
	else{ValidTag=false;}
	//cout << "GPIO::ValidTag: " << ValidTag << endl;
	//cout << "GPIO::TotalCurrentNumRecords: " << TotalCurrentNumRecords << endl;
	//cout << "GPIO::extendedCounterPRUholder: " << extendedCounterPRUholder << endl;
	//cout << "GPIO::extendedCounterPRUholder>0: " << (extendedCounterPRUholder>0) << endl;
	if (TotalCurrentNumRecords<MaxNumQuBitsMemStored and extendedCounterPRUholder>0 and ValidTag==true){TotalCurrentNumRecords++;CurrentiIterDump++;}//Variable to hold the number of currently stored records in memory	
	CurrentiIterDumpAux++; //Safety variable
}
if (TotalCurrentNumRecords>MaxNumQuBitsMemStored){cout << "GPIO::We have reached the maximum number of qubits storage!" << endl;}
else if (TotalCurrentNumRecords==TotalCurrentNumRecordsOld){cout << "GPIO::No detection of qubits!" << endl;}
//cout << "GPIO::TotalCurrentNumRecords: " << TotalCurrentNumRecords << endl;
//cout << "GPIO::Clearing PRU timetags" << endl;
//// Reset values of the sharedMem_int after each iteration
//for (iIterDump=0; iIterDump<((NumQuBitsPerRun/2)*3); iIterDump++){
//	sharedMem_int[OFFSET_SHAREDRAM+iIterDump]=static_cast<unsigned int>(0x00000000); // Put it all to zeros
//}
// Actually only needed to zero the first memory position
sharedMem_int[OFFSET_SHAREDRAM+1]=static_cast<unsigned int>(0x00000000); // Put it all to zeros

// Freeeing the semaphore block after all the access to the PRU shared memory
this->ManualSemaphore=false;
this->ManualSemaphoreExtra=false;
this->release();
/////////////////////////////////////////////////////////////////////////
// Debbugin relative frequency difference
//cout << "GPIO::DDRdumpdata AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
//////////////////////////////////////////////////////////////////////////
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
// Check that TimeTaggsStored are increasingly ordered. This can be commented, it is just for checking
//
//for (int i=0;i<(CurrentiIterDump-1);i++){
//	if ((static_cast<long long int>(TimeTaggsStored[i+1])-static_cast<long long int>(TimeTaggsStored[i]))<=0){
//		cout << "GPIO::DDRdumpdata disorded tags before PRUdetCorrRelFreq!!!" << endl;
//	}
//}
///////////////////////////////////////////////
// Correct the detected qubits relative frequency difference (due to the sender node) and split between quad groups of 4 channels. Computed at each iteration so that the time span is not too large
PRUdetCorrRelFreq(iIterRunsAux,CurrentiIterDump);

///////////////////////////////////////////////
// Check that TimeTaggsStored are increasingly ordered. This can be commented, it is just for checking
// TimeTaggsStored will be disordered
//for (int i=0;i<(CurrentiIterDump-1);i++){
//	if ((static_cast<long long int>(TimeTaggsStored[i+1])-static_cast<long long int>(TimeTaggsStored[i]))<=0){
//		cout << "GPIO::DDRdumpdata disorded tags after PRUdetCorrRelFreq!!!" << endl;
//	}
//}
//
//bool CheckOnceAux=false; //bool CheckOnceAux=false; 
//for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
//	CheckOnceAux=false;
//	if (TotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
//		for (unsigned int i=0;i<(TotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){		
//			if ((static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i]))<=0){
//				CheckOnceAux=true;
//			}
//		}
//	}
//	if (CheckOnceAux==true){
//		cout << "GPIO::DDRdumpdata disorded TimeTaggsSplitted!!! for iQuadChIter: " << iQuadChIter << endl;
//	}
//}
///////////////////////////////////////////////

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
		cout << "GPIO::DDRdumpdata streamDDRpru is not open!" << endl;
	}
}

return 0; // all ok
}

int GPIO::PRUdetCorrRelFreq(int iIterRunsAux,int CurrentiIterDump){// Correct relative frequency difference due to the sender. It is very dangerous if not well estimated. Actually, it is better to estiamte it by hardware means having a clock derived froma synchronized network system like synchronous ethernet
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
	// The separate the raw timetagging detection for each quad channel (since it quad channel will have to correct for a different relative frequency difference)
	for (int i=0;i<CurrentiIterDump;i++){
		for (unsigned short iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
			if ((ChannelTagsStored[i]&(0x000F<<(4*iQuadChIter)))>0){  
				TimeTaggsSplitted[iQuadChIter][TotalCurrentNumRecordsQuadCh[iQuadChIter]]=TimeTaggsStored[i];
				ChannelTagsSplitted[iQuadChIter][TotalCurrentNumRecordsQuadCh[iQuadChIter]]=ChannelTagsStored[i]&(0x000F<<(4*iQuadChIter));
				TotalCurrentNumRecordsQuadCh[iQuadChIter]++;
			}
		}
	}
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		//int iQuadChIter=QuadEmitDetecSelecGPIO;// Only process the expected channel group
		TotalCurrentNumRecordsQuadChNewOldAux=TotalCurrentNumRecordsQuadCh[iQuadChIter]-TotalCurrentNumRecordsQuadChOld[iQuadChIter];
		//////////////////////////////////////////////////////////////////////////
		// Check. It can be commented for normal operation
		//cout << "GPIO::PRUdetCorrRelFreq TotalCurrentNumRecordsQuadChNewOldAux: " << TotalCurrentNumRecordsQuadChNewOldAux << endl;
		//bool CheckOnceAux=false;
		//if (TotalCurrentNumRecordsQuadChNewOldAux>1){
		//	for (int i=0;i<(TotalCurrentNumRecordsQuadChNewOldAux-1);i++){		
		//		if ((static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i]))<=0){
		//			CheckOnceAux=true;
		//		}
		//	}
		//	if (CheckOnceAux==true){
		//		cout << "GPIO::PRUdetCorrRelFreq disorded TimeTaggsSplitted before processing!!! for iQuadChIter: " << iQuadChIter << endl;
		//	}
		//}
		/////////////////////////////////////////////////////////////////////////
		//cout << "GPIO::PRUdetCorrRelFreq iQuadChIter: " << iQuadChIter << endl;
		//cout << "GPIO::PRUdetCorrRelFreq TotalCurrentNumRecordsQuadChNewOldAux: " << TotalCurrentNumRecordsQuadChNewOldAux << endl;
		if (TotalCurrentNumRecordsQuadChNewOldAux>TagsSeparationDetRelFreq and GPIOFlagRelFreqTest==false){
			// Good strategy to substrat the system absolute time which is multiple to the effective period, since we want to see the deviation with respect this reference values
    		long long int LLIInitialTimeTaggs=static_cast<long long int>(TimeTaggsLast);//static_cast<long long int>(TimeTaggs[iQuadChIter][0]);
    		//cout << "GPIO::LastTimeTaggRef[0]: " << LastTimeTaggRef[0] << endl;
    		//cout << "GPIO::TimeTaggs[iQuadChIter][0]: " << TimeTaggs[iQuadChIter][0] << endl;
    		long long int LLITimeTaggs[TotalCurrentNumRecordsQuadChNewOldAux]={0};
    		for (unsigned int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
    			LLITimeTaggs[i]=static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]])-LLIInitialTimeTaggs;
    		}
    		double SlopeDetTagsAux=1.0;
    		long long int InterDetTagsAux=0;
		    // Calculate the "x" values
    		long long int xAux[TotalCurrentNumRecordsQuadChNewOldAux]={0};
    		long long int InterDetTagsAuxArray[TotalCurrentNumRecordsQuadChNewOldAux]={0};
    		long long int LLISynchTrigPeriod=static_cast<long long int>(SynchTrigPeriod);
    		long long int LLISynchTrigPeriodHalf=static_cast<long long int>(SynchTrigPeriod/2.0);
    		long long int LLIMultFactorEffSynchPeriod=static_cast<long long int>(MultFactorEffSynchPeriod);
    		for (unsigned int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
    			// Absolute slope calculation & intercept point
    			xAux[i]=((LLITimeTaggs[i]+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin
    			// Relative slope calculation
    			//if (i==0){
    			//	xAux[i]=((LLITimeTaggs[i]+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin
    			//}
    			//else{
    			//	xAux[i]=LLITimeTaggs[i-1]+(((LLITimeTaggs[i]-LLITimeTaggs[i-1])+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin
    			//}
    			// Intercept point; it is like the offset to be retrieved and it should not consider the histogram period if needed
    			//InterDetTagsAuxArray[i]=((LLISynchTrigPeriodHalf)+LLITimeTaggs[i])%(LLISynchTrigPeriod)-(LLISynchTrigPeriodHalf);
    			//InterDetTagsAuxArray[i]=(LLITimeTaggs[i])%(LLISynchTrigPeriod); // Used in the adaptive approach. It is like 
    		}

    		//InterDetTagsAux=LLIMedianFilterSubArray(InterDetTagsAuxArray,static_cast<int>(TotalCurrentNumRecordsQuadChNewOldAux));//LLIMeanFilterSubArray(InterDetTagsAuxArray,static_cast<int>(TotalCurrentNumRecordsQuadChNewOldAux))
		    //cout << "GPIO::PRUdetCorrRelFreq InterDetTagsAux original iQuadChIter[" << iQuadChIter << "]: " << InterDetTagsAux << endl;

		    // Compute the absolute candidate slope
    		int iAux=0;
    		for (unsigned int i=0;i<(TotalCurrentNumRecordsQuadChNewOldAux-TagsSeparationDetRelFreq);i++){
    			if ((xAux[i+TagsSeparationDetRelFreq]-xAux[i])>0){
    				// Relative slope calculation
    				SlopeDetTagsAuxArray[iAux]=static_cast<double>(LLITimeTaggs[i+TagsSeparationDetRelFreq]-LLITimeTaggs[i])/static_cast<double>(xAux[i+TagsSeparationDetRelFreq]-xAux[i]);
    				iAux++;
    			}
    			//if (xAux[i]>0){//if ((xAux[i+TagsSeparationDetRelFreq]-xAux[i])>0){
    			//	// Absolute slope calculation
    			//	//SlopeDetTagsAuxArray[iAux]=static_cast<double>(LLITimeTaggs[i]-InterDetTagsAux)/static_cast<double>(xAux[i]);
    			//	iAux++;
    			//}
    		}

    		// Absolute slope calculation
    		SlopeDetTagsAux=DoubleMedianFilterSubArray(SlopeDetTagsAuxArray,iAux);//DoubleMeanFilterSubArray(SlopeDetTagsAuxArray,iAux);
		    //SlopeDetTagsAux=1.0;// For the time being set to 1. 
		    //cout << "GPIO::PRUdetCorrRelFreq SlopeDetTagsAux original iQuadChIter[" << iQuadChIter << "]: " << SlopeDetTagsAux << endl;

    		if (SlopeDetTagsAux<0.9 or SlopeDetTagsAux>1.1){
    			cout << "GPIO::PRUdetCorrRelFreq wrong computation of the SlopeDetTagsAux " << SlopeDetTagsAux << " for quad channel " << iQuadChIter << ". Not applying the correction..." << endl;
    			SlopeDetTagsAux=1.0;
    		}
    		for (unsigned int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
    			TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]]=static_cast<unsigned long long int>(static_cast<long long int>(static_cast<long double>(1.0/SlopeDetTagsAux)*static_cast<long double>(LLITimeTaggs[i]))+LLIInitialTimeTaggs);
    		}
    		
    		//cout << "GPIO::PRUdetCorrRelFreq SlopeDetTagsAux " << SlopeDetTagsAux << " for quad channel " << iQuadChIter << endl;
		    /*
		    // Relative slope calculation
		    double SlopeDetTagsAuxArrayAdap[TagsSeparationDetRelFreqAdpSlope]={0.0};
    		for (unsigned int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
    			// Non-adaptive slope
    			//TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]]=static_cast<unsigned long long int>(static_cast<long long int>(static_cast<long double>(1.0/SlopeDetTagsAux)*static_cast<long double>(LLITimeTaggs[i]))+LLIInitialTimeTaggs);
    			// Applying adaptive slope
    			if (i<(TagsSeparationDetRelFreqAdpSlope/2)){
    				for (unsigned int iAdapAux=0;iAdapAux<TagsSeparationDetRelFreqAdpSlope;iAdapAux++){
	    				SlopeDetTagsAuxArrayAdap[static_cast<int>(iAdapAux)]=SlopeDetTagsAuxArray[iAdapAux];    				
	    			}
    			}
    			else if (i>=(TagsSeparationDetRelFreqAdpSlope/2) and (i+TagsSeparationDetRelFreqAdpSlope/2)<TotalCurrentNumRecordsQuadChNewOldAux){
	    			for (unsigned int iAdapAux=static_cast<unsigned int>(static_cast<int>(i)-static_cast<int>(TagsSeparationDetRelFreqAdpSlope/2));iAdapAux<(i+TagsSeparationDetRelFreqAdpSlope/2);iAdapAux++){
	    				SlopeDetTagsAuxArrayAdap[static_cast<int>(iAdapAux)-(static_cast<int>(i)-static_cast<int>(TagsSeparationDetRelFreqAdpSlope/2))]=SlopeDetTagsAuxArray[iAdapAux];    				
	    			}	    			
	    		}
	    		else{
	    			for (unsigned int iAdapAux=static_cast<unsigned int>(static_cast<int>(TotalCurrentNumRecordsQuadChNewOldAux)-static_cast<int>(TagsSeparationDetRelFreqAdpSlope));iAdapAux<TotalCurrentNumRecordsQuadChNewOldAux;iAdapAux++){
	    				SlopeDetTagsAuxArrayAdap[static_cast<int>(iAdapAux)-(static_cast<int>(TotalCurrentNumRecordsQuadChNewOldAux)-static_cast<int>(TagsSeparationDetRelFreqAdpSlope))]=SlopeDetTagsAuxArray[iAdapAux];    				
	    			}
	    		}
    			SlopeDetTagsAux=1.0;// For the time being set to 1. DoubleMedianFilterSubArray(SlopeDetTagsAuxArrayAdap,static_cast<int>(TagsSeparationDetRelFreqAdpSlope));//DoubleMeanFilterSubArray(SlopeDetTagsAuxArrayAdap,static_cast<int>(TagsSeparationDetRelFreqAdpSlope));
    			//if (i%75==0){// To be commented when not being check
    			//	cout << "GPIO::PRUdetCorrRelFreq SlopeDetTagsAux i[" << i << "] current adaptive: " << SlopeDetTagsAux << endl;
    			//}
    			if (SlopeDetTagsAux<0.5 or SlopeDetTagsAux>1.5){
	    			cout << "GPIO::PRUdetCorrRelFreq wrong computation of the adaptive SlopeDetTagsAux " << SlopeDetTagsAux << " for quad channel " << iQuadChIter << ". Not applying the correction..." << endl;
	    			SlopeDetTagsAux=1.0;
	    		}
	    		// We have to put back the intercept value, so that the other algorithms in upper layers are aware of offsets to correct for synchronization
    			TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]]=static_cast<unsigned long long int>(static_cast<long long int>(static_cast<long double>(1.0/SlopeDetTagsAux)*static_cast<long double>(LLITimeTaggs[i]-InterDetTagsAux))+InterDetTagsAux+LLIInitialTimeTaggs);
    			// Also update the information in the original array - Bad idea because they become disorded
    			//TimeTaggsStored[CurrentiIterDumpAux]=TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]]; 
    			//ChannelTagsStored[CurrentiIterDumpAux]=ChannelTagsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]];
    			//CurrentiIterDumpAux++;// update value
    		}*/

		    //////////////////////////////////////////////////////////////////////////////////////////
		    ////// Checks of proper relative frequency correction. It can be commented
		    //LLIInitialTimeTaggs=static_cast<long long int>(TimeTaggsLast);//static_cast<long long int>(TimeTaggs[iQuadChIter][0]);
    		////cout << "GPIO::LastTimeTaggRef[0]: " << LastTimeTaggRef[0] << endl;
    		////cout << "GPIO::TimeTaggs[iQuadChIter][0]: " << TimeTaggs[iQuadChIter][0] << endl;
    		//LLITimeTaggs[TotalCurrentNumRecordsQuadChNewOldAux]={0};
    		//for (unsigned int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
    		//	LLITimeTaggs[i]=static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i+TotalCurrentNumRecordsQuadChOld[iQuadChIter]])-LLIInitialTimeTaggs;
    		//}
    		//SlopeDetTagsAux=1.0;
    		//
		    //// Calculate the "x" values
    		//xAux[TotalCurrentNumRecordsQuadChNewOldAux]={0};
    		//LLISynchTrigPeriod=static_cast<long long int>(SynchTrigPeriod);
    		//LLISynchTrigPeriodHalf=static_cast<long long int>(SynchTrigPeriod/2.0);
    		//for (unsigned int i=0;i<TotalCurrentNumRecordsQuadChNewOldAux;i++){
    		//	// Absolute calculation
    		//	xAux[i]=((LLITimeTaggs[i]+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin    		
    		//	//// Relative calculation
    		//	//if (i==0){
    		//	//	xAux[i]=((LLITimeTaggs[i]+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin
    		//	//}
    		//	//else{
    		//	//	xAux[i]=LLITimeTaggs[i-1]+(((LLITimeTaggs[i]-LLITimeTaggs[i-1])+LLISynchTrigPeriodHalf)/LLISynchTrigPeriod)*LLISynchTrigPeriod;// Important to consider from -Period/2 to Period/2 fall in the specific x bin
    		//	//}
    		//	// Intercept point
    		//	InterDetTagsAuxArray[i]=(LLISynchTrigPeriodHalf+LLITimeTaggs[i])%LLISynchTrigPeriod-LLISynchTrigPeriodHalf;
    		//}
    		//
    		//InterDetTagsAux=LLIMeanFilterSubArray(InterDetTagsAuxArray,static_cast<int>(TotalCurrentNumRecordsQuadChNewOldAux));//LLIMedianFilterSubArray(InterDetTagsAuxArray,static_cast<int>(TotalCurrentNumRecordsQuadChNewOldAux))
		    //cout << "GPIO::PRUdetCorrRelFreq InterDetTagsAux final iQuadChIter[" << iQuadChIter << "]: " << InterDetTagsAux << endl;
    		//
		    //// Compute the candidate slope
    		//iAux=0;
    		//for (unsigned int i=0;i<(TotalCurrentNumRecordsQuadChNewOldAux-TagsSeparationDetRelFreq);i++){
    		//	if (xAux[i]>0){//if ((xAux[i+TagsSeparationDetRelFreq]-xAux[i])>0){
    		//		// Absolute slope calculation
    		//		SlopeDetTagsAuxArray[iAux]=static_cast<double>(LLITimeTaggs[i])/static_cast<double>(xAux[i]);
    		//		// Relative slope calculation
    		//		//SlopeDetTagsAuxArray[iAux]=static_cast<double>(LLITimeTaggs[i+TagsSeparationDetRelFreq]-LLITimeTaggs[i])/static_cast<double>(xAux[i+TagsSeparationDetRelFreq]-xAux[i]);
    		//		iAux++;
    		//	}
    		//}
    		//
    		//SlopeDetTagsAux=DoubleMeanFilterSubArray(SlopeDetTagsAuxArray,iAux);//DoubleMedianFilterSubArray(SlopeDetTagsAuxArray,iAux);
		    //cout << "GPIO::PRUdetCorrRelFreq SlopeDetTagsAux final iQuadChIter[" << iQuadChIter << "]: " << SlopeDetTagsAux << endl;
		    //////////////////////////////////////////////////////////////////////////////////////////////////
		}// if
		else {//(TotalCurrentNumRecordsQuadChNewOldAux>0 or GPIOFlagRelFreqTest==true){
			// It is a very dangerous function. Better to deactivate it. Also, not promp the user.
			//if (TotalCurrentNumRecordsQuadChNewOldAux>0 and GPIOFlagRelFreqTest==false){cout << "GPIO::PRUdetCorrRelFreq not enough detections " << TotalCurrentNumRecordsQuadChNewOldAux << "<" << TagsSeparationDetRelFreq << " in iQuadChIter " << iQuadChIter << " quad channel to correct emitter rel. frequency deviation!" << endl;}
			//else if (GPIOFlagRelFreqTest==true){cout << "GPIO::PRUdetCorrRelFreq deactivated..." << endl;}
		}
		//////////////////////////////////////////////////////////////////////////
		// Check. It can be commented for normal operation
		//CheckOnceAux=false; //bool CheckOnceAux=false;
		//if (TotalCurrentNumRecordsQuadChNewOldAux>1){
		//	for (int i=0;i<(TotalCurrentNumRecordsQuadChNewOldAux-1);i++){		
		//		if ((static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggsSplitted[iQuadChIter][i]))<=0){
		//			CheckOnceAux=true;
		//		}
		//	}
		//	if (CheckOnceAux==true){
		//		cout << "GPIO::PRUdetCorrRelFreq disorded TimeTaggsSplitted after processing!!! for iQuadChIter: " << iQuadChIter << endl;
		//	}
		//}
		/////////////////////////////////////////////////////////////////////////
	} // for
//cout << "GPIO::PRUdetCorrRelFreq completed!" << endl;
return 0; // All ok
}

// Function to pack bits 0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15 of an unsigned short into the lower values
unsigned short GPIO::packBits(unsigned short value) {
    // Rearrange the lower two bytes so that they are correctly splitted for each quad group channel. For each group of 4 bits the order does not follow an arranged order for channel detectors
    unsigned short byte0aux = ((value & 0x0010) >> 4) | ((value & 0x0040) >> 5) | ((value & 0x0002) << 1) | ((value & 0x0020) >> 2) ; // Channel 0 // Are the bits 0x0072, moved to 0x000F
    unsigned short byte1aux = ((value & 0x8000) >> 11) | ((value & 0x0001) << 5) | ((value & 0x4000) >> 8) | ((value & 0x0004) << 5); // Channel 1 // Are the bits 0x0C05, moved to 0x00F0
        
    unsigned short byte2aux = value & 0x0000; //((value & 0x1000) >> 4) | ((value & 0x2000) >> 4) | ((value & 0x4000) >> 4) | ((value & 0x8000) >> 4); // Channel 2 // To be check that the ordering is correct!!!! // Byte 1 shifts to the right four bit positions (the interesting ones) // Are the bits 0xF000, moved to 0x0F00

    // Debugging
    //cout << "GPIO::packBits value: " << std::bitset<16>(value) << endl; // It tells the original position of the bits of interest to shift them in order to their places

    if (byte2aux!=0){cout << "GPIO::packBits byte2aux has never been tested (check synchronization network ordering of bits)!!" << endl;} // Check the packBits byte2aux ordering as well as the mask in the PRUassTaggDetScriptSimple.p

    // Combine the bytes into a single unsigned short
    return byte0aux | byte1aux | byte2aux;
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
		for (unsigned int i=0;i<TotalCurrentNumRecordsQuadCh[iQuadChIter];i++){  
			TimeTaggsAux[iQuadChIter][i]=TimeTaggsSplitted[iQuadChIter][i];
			ChannelTagsAux[iQuadChIter][i]=ChannelTagsSplitted[iQuadChIter][i];			
		}
		TotalCurrentNumRecordsQuadChAux[iQuadChIter]=TotalCurrentNumRecordsQuadCh[iQuadChIter];

		//////////////////////////////////////////////////////////////////////////
		// Check. It can be commented for normal operation
		//bool CheckOnceAux=false; //bool CheckOnceAux=false;
		//if (TotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
		//	for (unsigned int i=0;i<(TotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){		
		//		if ((static_cast<long long int>(TimeTaggsAux[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggsAux[iQuadChIter][i]))<=0){
		//			cout << "GPIO::RetrieveNumStoredQuBits disorded TimeTaggsAux before processing!!! for i: " << i << ". Involved values TimeTaggsAux[iQuadChIter][i+1]: " << static_cast<long long int>(TimeTaggsAux[iQuadChIter][i+1]) << " and static_cast<long long int>(TimeTaggsAux[iQuadChIter][i]): " << static_cast<long long int>(TimeTaggsAux[iQuadChIter][i]) << endl;
		//			CheckOnceAux=true;
		//		}
		//	}
		//	if (CheckOnceAux==true){
		//		cout << "GPIO::RetrieveNumStoredQuBits disorded TimeTaggsAux before processing!!! for iQuadChIter: " << iQuadChIter << endl;
		//	}
		//}
		/////////////////////////////////////////////////////////////////////////
	}

	return TotalCurrentNumRecords;
}

int GPIO::IntMeanFilterSubArray(int* ArrayHolderAux,int MeanFilterFactor){
	if (MeanFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
		int temp=0.0;
		for(int i = 0; i < MeanFilterFactor; i++) {
			temp += ArrayHolderAux[i];
		}
		
		return temp/static_cast<int>(MeanFilterFactor);
	}
}

int GPIO::IntMedianFilterSubArray(int* ArrayHolderAux,int MedianFilterFactor){
	if (MedianFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
		/*
		// Non-efficient code
		// Step 1: Copy the array to a temporary array
		int temp[MedianFilterFactor]={0};
		for(int i = 0; i < MedianFilterFactor; i++) {
			temp[i] = ArrayHolderAux[i];
		}		
    	// Step 2: Sort the temporary array
		this->IntBubbleSort(temp,MedianFilterFactor);
    	// If odd, middle number
		return temp[MedianFilterFactor/2];*/

		// Efficient code
		// Step 0. Copy the array otherwise the original is modified!
		int ArrayHolderAuxTemp[MedianFilterFactor];
		for(int i = 0; i < MedianFilterFactor; i++) {
			ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
		}
		// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
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

long long int GPIO::LLIMeanFilterSubArray(long long int* ArrayHolderAux,int MeanFilterFactor){
	if (MeanFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
	// Step 1: Copy the array to a temporary array
		long long int temp=0.0;
		for(int i = 0; i < MeanFilterFactor; i++) {
			temp = temp + ArrayHolderAux[i];
		}
		
		temp=static_cast<long long int>(static_cast<long double>(temp)/static_cast<long double>(MeanFilterFactor));
		return temp;
	}
}

long double GPIO::LongDoubleMeanFilterSubArray(long double* ArrayHolderAux,int MeanFilterFactor){
	if (MeanFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
		long double temp=0.0;
		for(int i = 0; i < MeanFilterFactor; i++) {
			temp += ArrayHolderAux[i];
		}
		
		return temp/static_cast<long double>(MeanFilterFactor);
	}
}

long long int GPIO::LLIMedianFilterSubArray(long long int* ArrayHolderAux,int MedianFilterFactor){
	if (MedianFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
		/* // Non-efficient code
		// Step 1: Copy the array to a temporary array
		long double temp[MedianFilterFactor]={0.0};
		for(int i = 0; i < MedianFilterFactor; i++) {
			temp[i] = ArrayHolderAux[i];
		}
		
    	// Step 2: Sort the temporary array
		this->LongDoubleBubbleSort(temp,MedianFilterFactor);
    	// If odd, middle number
		return temp[MedianFilterFactor/2];*/

		// Efficient code
		// Step 0. Copy the array otherwise the original is modified!
		long long int ArrayHolderAuxTemp[MedianFilterFactor];
		for(int i = 0; i < MedianFilterFactor; i++) {
			ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
		}
		// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
	}
}

long double GPIO::LongDoubleMedianFilterSubArray(long double* ArrayHolderAux,int MedianFilterFactor){
	if (MedianFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
		/* // Non-efficient code
		// Step 1: Copy the array to a temporary array
		long double temp[MedianFilterFactor]={0.0};
		for(int i = 0; i < MedianFilterFactor; i++) {
			temp[i] = ArrayHolderAux[i];
		}
		
    	// Step 2: Sort the temporary array
		this->LongDoubleBubbleSort(temp,MedianFilterFactor);
    	// If odd, middle number
		return temp[MedianFilterFactor/2];*/

		// Efficient code
		// Step 0. Copy the array otherwise the original is modified!
		long double ArrayHolderAuxTemp[MedianFilterFactor];
		for(int i = 0; i < MedianFilterFactor; i++) {
			ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
		}
		// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
	}
}

double GPIO::DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor){
	if (MedianFilterFactor<=1){
		return ArrayHolderAux[0];
	}
	else{
		/* // Non-efficient code
		// Step 1: Copy the array to a temporary array
		double temp[MedianFilterFactor]={0.0};
		for(int i = 0; i < MedianFilterFactor; i++) {
			temp[i] = ArrayHolderAux[i];
		}		
	    // Step 2: Sort the temporary array
		this->DoubleBubbleSort(temp,MedianFilterFactor);
	    // If odd, middle number
		return temp[MedianFilterFactor/2];*/

		// Efficient code
		// Step 0. Copy the array otherwise the original is modified!
		double ArrayHolderAuxTemp[MedianFilterFactor];
		for(int i = 0; i < MedianFilterFactor; i++) {
			ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
		}
		// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
	}
}

// Function to implement Bubble Sort
int GPIO::LongDoubleBubbleSort(long double* arr,int MedianFilterFactor) {
	long double temp=0.0;
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

//int GPIO::write(string path, string filename, string value){
//	ofstream fs;
//	fs.open((path + filename).c_str());
//	if (!fs.is_open()){
//		perror("GPIO: write failed to open file ");
//		return -1;
//	}
//	fs << value;
//	fs.close();
//	return 0;
//}

//string GPIO::read(string path, string filename){
//	ifstream fs;
//	fs.open((path + filename).c_str());
//	if (!fs.is_open()){
//		perror("GPIO: read failed to open file ");
//	}
//	string input;
//	getline(fs,input);
//	fs.close();
//	return input;
//}

//int GPIO::write(string path, string filename, int value){
//	stringstream s;
//	s << value;
//	return this->write(path,filename,s.str());
//}

//int GPIO::exportGPIO(){
//   return this->write(GPIO_PATH, "export", this->number);
//}

//int GPIO::unexportGPIO(){
//   return this->write(GPIO_PATH, "unexport", this->number);
//}
/*
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
*/

int GPIO::KillcodePRUs(){
	if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUkillSignal1.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUkillSignal1.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUkillSignal1.bin");//perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}

	if (prussdrv_exec_program(PRU_Operation_NUM, "./CppScripts/BBBhw/PRUkillSignal0.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./CppScripts/BBBhw/PRUassTrigSigScript.bin") == -1){
		if (prussdrv_exec_program(PRU_Operation_NUM, "./BBBhw/PRUkillSignal0.bin") == -1){//if (prussdrv_exec_program(PRU_Signal_NUM, "./BBBhw/PRUassTrigSigScript.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUkillSignal0.bin");//perror("prussdrv_exec_program non successfull writing of PRUassTrigSigScript.bin");
		}
	}
	sleep(2); // Give time to load to the PRU memory
return 0;
}

int GPIO::DisablePRUs(){
// Disable PRU and close memory mappings
	prussdrv_pru_disable(PRU_Signal_NUM);
	prussdrv_pru_disable(PRU_Operation_NUM);

	return 0;
}

GPIO::~GPIO() { // Destructor
//	this->unexportGPIO();
	this->threadRefSynch.join();
	this->KillcodePRUs();
	this->DisablePRUs();
	//fclose(outfile); 
	prussdrv_exit();
	close(tfd);// close the time descriptor
	//munmap(ddrMem, 0x0FFFFFFF);
	//close(mem_fd); // Device
	//if(munmap(pru_int, PRU_LEN)) {
	//	cout << "GPIO destructor: munmap failed" << endl;
	//}
	if (SlowMemoryPermanentStorageFlag==true){streamDDRpru.close();}
	//streamSynchpru.close(); //Not used
}

} /* namespace exploringBB */
