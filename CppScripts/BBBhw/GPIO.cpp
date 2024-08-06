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
#include <cmath>// abs, fmodl, floor, ceil
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
	valpAuxHolder=valpHolder+4+6*NumRecords;// 6* since each detection also includes the channels (2 Bytes) and 4 bytes for 32 bits counter, and plus 4 since the first tag is captured at the very beggining
	CalpHolder=(unsigned int*)&pru0dataMem_int[2];// First tagg captured at the very beggining
	synchpHolder=(unsigned int*)&pru0dataMem_int[3];// Starts at 12
	
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Timetagging
	    // Execute program
	    // Load and execute the PRU program on the PRU0
	pru0dataMem_int[0]=static_cast<unsigned int>(0); // set no command
	pru0dataMem_int[1]=static_cast<unsigned int>(this->NumRecords); // set number captures, with overflow clock
	pru0dataMem_int[2]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
	pru0dataMem_int[3]=static_cast<unsigned int>(0);
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
	cout << "Wait to proceed, calibrating synchronization!" << endl;
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
	  ///////////////////////////////////////////////////////
	this->setMaxRrPriority();// For rapidly handling interrupts, for the main instance and the periodic thread (only applied to the periodic thread). But it stalls operation in RealTime kernel.
	//this->TimePointClockTagPRUinitialOld=Clock::now();// First time. Not needed because we do ti since epoch to have aboslute timming
	//this->TimePointClockSynchPRUinitial=Clock::now();// First time. Not needed because we do it since epoch to have absolute timming
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
int Nice_priority=10;
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
bool valueSemaphoreExpected=true;
while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
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
	auto duration_since_epochFutureTimePointAux=QPLAFutureTimePoint.time_since_epoch();
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
	SynchRem=static_cast<int>((static_cast<long double>(iepPRUtimerRange32bits)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockCurrentSynchPRU1future.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(iepPRUtimerRange32bits)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	this->TimePointClockCurrentSynchPRU1future=this->TimePointClockCurrentSynchPRU1future+std::chrono::nanoseconds(SynchRem);
	this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
	// First setting of time
	this->PRUoffsetDriftError=static_cast<double>(fmodl((static_cast<long double>(this->iIterPRUcurrentTimerVal*this->TimePRU1synchPeriod)+0.0*static_cast<long double>(ApproxInterruptTime))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits)));
	this->NextSynchPRUcorrection=static_cast<unsigned int>(static_cast<unsigned int>((static_cast<unsigned long long int>(this->PRUoffsetDriftError)+static_cast<unsigned long long int>(LostCounts))%iepPRUtimerRange32bits));
	this->NextSynchPRUcommand=static_cast<unsigned int>(5); // set command 5, do absolute correction
	
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
				//if ((this->iIterPRUcurrentTimerVal%50)==0){// Every now and then correct absolutelly, although some interrupt jitter will be present
				//	this->PRUoffsetDriftError=static_cast<double>(fmodl((static_cast<long double>(this->iIterPRUcurrentTimerVal*this->TimePRU1synchPeriod)+1.0*static_cast<long double>(duration_FinalInitialCountAuxArrayAvg))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits)));
				//	this->NextSynchPRUcorrection=static_cast<unsigned int>(static_cast<unsigned int>((static_cast<unsigned long long int>(PRUoffsetDriftError)+static_cast<unsigned long long int>(LostCounts))%iepPRUtimerRange32bits));
				//	this->NextSynchPRUcommand=static_cast<unsigned int>(5);// Hard setting of the time
				//}
				
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
				
				int duration_FinalInitialMeasTrig=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-this->TimePointClockCurrentSynchPRU1future).count());
				duration_FinalInitialCountAux=static_cast<double>(duration_FinalInitialMeasTrig);
				
				// Below for the triggering
				this->duration_FinalInitialMeasTrigAuxArray[TrigAuxIterCount%ExtraNumSynchMeasAvgAux]=duration_FinalInitialMeasTrig;
				this->duration_FinalInitialMeasTrigAuxAvg=this->IntMedianFilterSubArray(this->duration_FinalInitialMeasTrigAuxArray,ExtraNumSynchMeasAvgAux);
				this->TrigAuxIterCount++;
				// Below for synch calculation compensation
				duration_FinalInitialCountAuxArrayAvg=static_cast<double>(duration_FinalInitialMeasTrigAuxAvg);
				
				//pru1dataMem_int[2]// Current IEP timer sample
				//pru1dataMem_int[3]// Correction to apply to IEP timer
				this->PRUcurrentTimerValWrap=static_cast<double>(pru1dataMem_int[2]);
				// Correct for interrupt handling time might add a bias in the estimation/reading
				//this->PRUcurrentTimerValWrap=this->PRUcurrentTimerValWrap-duration_FinalInitialCountAux/static_cast<double>(PRUclockStepPeriodNanoseconds);//this->PRUcurrentTimerValWrap-duration_FinalInitialCountAuxArrayAvg/static_cast<double>(PRUclockStepPeriodNanoseconds);// Remove time for sending command //this->PRUcurrentTimerValWrap-duration_FinalInitialCountAux/static_cast<double>(PRUclockStepPeriodNanoseconds);// Remove time for sending command
				// Unwrap
				if (this->PRUcurrentTimerValWrap<=this->PRUcurrentTimerValOldWrap){this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap+(0xFFFFFFFF-this->PRUcurrentTimerValOldWrap);}
				else{this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap;}			
					// Computations for Synch calculation for PRU0 compensation
					// Compute Synch - Relative
					this->EstimateSynch=((static_cast<double>(this->iIterPRUcurrentTimerValPass*this->TimePRU1synchPeriod)+0*duration_FinalInitialCountAuxArrayAvg)/static_cast<double>(PRUclockStepPeriodNanoseconds))/(this->PRUcurrentTimerVal-static_cast<double>(this->PRUcurrentTimerValOldWrap));// Only correct for PRUcurrentTimerValOld with the PRUoffsetDriftErrorAppliedOldRaw to be able to measure the real synch drift and measure it (not affected by the correction).
					// Compute Synch - Absolute
					//this->EstimateSynch=static_cast<double>(fmodl((static_cast<long double>((this->iIterPRUcurrentTimerVal)*this->TimePRU1synchPeriod)+0.0*static_cast<long double>(duration_FinalInitialCountAux))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits))/(this->PRUcurrentTimerValWrap-0*this->PRUcurrentTimerValOldWrap));
					this->EstimateSynchArray[iIterPRUcurrentTimerValSynch%NumSynchMeasAvgAux]=this->EstimateSynch;
				this->ManualSemaphoreExtra=true;
					this->EstimateSynchAvg=DoubleMedianFilterSubArray(EstimateSynchArray,NumSynchMeasAvgAux);
					
					// Compute error - Relative correction				
					this->PRUoffsetDriftError=static_cast<double>((-fmodl((static_cast<long double>(this->iIterPRUcurrentTimerValPass*this->TimePRU1synchPeriod))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits))+(this->PRUcurrentTimerVal-1*this->PRUcurrentTimerValOldWrap))/static_cast<long double>(TimePRU1synchPeriod)*static_cast<long double>(SynchTrigPeriod));
					// Relative error
					this->PRUoffsetDriftErrorArray[iIterPRUcurrentTimerValSynch%NumSynchMeasAvgAux]=this->PRUoffsetDriftError;
					this->PRUoffsetDriftErrorAvg=DoubleMedianFilterSubArray(PRUoffsetDriftErrorArray,NumSynchMeasAvgAux);
					
					// Compute error - Absolute corrected error
					this->PRUoffsetDriftErrorAbs=static_cast<double>((-fmodl((static_cast<long double>(this->iIterPRUcurrentTimerVal*this->TimePRU1synchPeriod))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits))+(this->PRUcurrentTimerVal))/static_cast<long double>(TimePRU1synchPeriod)*static_cast<long double>(SynchTrigPeriod))-this->PRUoffsetDriftError; // Substract the relative error
					// Absolute corrected error
					this->PRUoffsetDriftErrorAbsArray[iIterPRUcurrentTimerValSynch%NumSynchMeasAvgAux]=this->PRUoffsetDriftErrorAbs;
					this->PRUoffsetDriftErrorAbsAvg=DoubleMedianFilterSubArray(PRUoffsetDriftErrorAbsArray,NumSynchMeasAvgAux);
					
				this->ManualSemaphoreExtra=false;
				this->ManualSemaphore=false;
				this->release();					
					
					this->NextSynchPRUcorrection=static_cast<unsigned int>(0);
					this->iIterPRUcurrentTimerValSynch++;
					this->iIterPRUcurrentTimerValPass=1;
					PRUoffsetDriftErrorLast=PRUoffsetDriftErrorAvg;// Update
					iIterPRUcurrentTimerValLast=iIterPRUcurrentTimerVal;// Update		
					this->PRUcurrentTimerValOld=this->PRUcurrentTimerValWrap;// Update
					this->NextSynchPRUcommand=static_cast<unsigned int>(4);// set command 4, to execute synch functions no correction
													
					// Updates for next round					
					this->PRUcurrentTimerValOldWrap=this->PRUcurrentTimerValWrap;// Update
					this->PRUoffsetDriftErrorAppliedOldRaw=this->PRUoffsetDriftErrorAppliedRaw;//update											
			}
			else{// does not enter in time
				this->iIterPRUcurrentTimerValPass++;
			}					
		} //end if
		else if (this->ManualSemaphoreExtra==true){
			// Double entry for some reason. Do not do anything
			////cout << "PRUs in use by other process TTU or SignalGen!" << endl
			this->iIterPRUcurrentTimerValPass++;
		}
		else{// does not enter in time
			this->iIterPRUcurrentTimerValPass++;
		}
		
		// Information
		if (this->ResetPeriodicallyTimerPRU1 and (this->iIterPRUcurrentTimerVal%(128*NumSynchMeasAvgAux)==0) and this->iIterPRUcurrentTimerVal>NumSynchMeasAvgAux){//if ((this->iIterPRUcurrentTimerVal%(128*NumSynchMeasAvgAux)==0) and this->iIterPRUcurrentTimerVal>NumSynchMeasAvgAux){//if ((this->iIterPRUcurrentTimerVal%5==0)){
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
		this->iIterPRUcurrentTimerVal++;
		if (this->iIterPRUcurrentTimerValSynch==(2*this->NumSynchMeasAvgAux)){
			cout << "Hardware synchronized, now proceding with the network synchronization managed by hosts..." << endl;
			// Update HardwareSynchStatus			
			this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
			HardwareSynchStatus=true;
			this->release();			
		}
	}// end while

return 0; // All ok
}

int GPIO::PIDcontrolerTimeJiterlessInterrupt(){
//PRUoffsetDriftErrorDerivative=(PRUoffsetDriftErrorAvg-PRUoffsetDriftErrorLast);//*(static_cast<double>(iIterPRUcurrentTimerVal-iIterPRUcurrentTimerValLast));//*(static_cast<double>(this->TimePRU1synchPeriod)/static_cast<double>(PRUclockStepPeriodNanoseconds)));
//PRUoffsetDriftErrorIntegral=PRUoffsetDriftErrorIntegral+PRUoffsetDriftErrorAvg;//*static_cast<double>(iIterPRUcurrentTimerVal-iIterPRUcurrentTimerValLast);//*(static_cast<double>(this->TimePRU1synchPeriod)/static_cast<double>(PRUclockStepPeriodNanoseconds));

this->PRUoffsetDriftErrorAppliedRaw=PRUoffsetDriftErrorAvg;

if (this->PRUoffsetDriftErrorAppliedRaw<(-this->LostCounts)){this->PRUoffsetDriftErrorApplied=this->PRUoffsetDriftErrorAppliedRaw-LostCounts;}// The LostCounts is to compensate the lost counts in the PRU when applying the update
else if(this->PRUoffsetDriftErrorAppliedRaw>this->LostCounts){this->PRUoffsetDriftErrorApplied=this->PRUoffsetDriftErrorAppliedRaw+LostCounts;}// The LostCounts is to compensate the lost counts in the PRU when applying the update
else{this->PRUoffsetDriftErrorApplied=0;}

return 0; // All ok
}

int GPIO::PRUsignalTimerSynch(){
	this->setMaxRrPriority();// For rapidly handling interrupts, for the main instance and the periodic thread. It stalls operation RealTime Kernel (commented, then)
	this->TimePointClockCurrentSynchPRU1future=Clock::now();// First time	
	SynchRem=static_cast<int>((static_cast<long double>(iepPRUtimerRange32bits)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockCurrentSynchPRU1future.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(iepPRUtimerRange32bits)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
	this->TimePointClockCurrentSynchPRU1future=this->TimePointClockCurrentSynchPRU1future+std::chrono::nanoseconds(SynchRem);
	this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
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
				this->PRUoffsetDriftError=static_cast<double>(fmodl((static_cast<long double>(this->iIterPRUcurrentTimerVal*this->TimePRU1synchPeriod)+static_cast<long double>(duration_FinalInitialCountAuxArrayAvg))/static_cast<long double>(PRUclockStepPeriodNanoseconds),static_cast<long double>(iepPRUtimerRange32bits)));
				this->NextSynchPRUcorrection=static_cast<unsigned int>(static_cast<unsigned int>((static_cast<unsigned long long int>(this->PRUoffsetDriftError)+static_cast<unsigned long long int>(LostCounts))%iepPRUtimerRange32bits));
				pru1dataMem_int[3]=static_cast<unsigned int>(this->NextSynchPRUcorrection);// apply correction.
				
				//TimePoint TimePointClockCurrentSynchPRU1futureAux=this->TimePointClockCurrentSynchPRU1future-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg);//Important to discount the time to enter the interrupt here (in average) so that the rectification of the PRU clock is reference to an absolute time									
				while(Clock::now() < this->TimePointClockCurrentSynchPRU1future);//while(Clock::now() < TimePointClockCurrentSynchPRU1futureAux);//while(Clock::now() < this->TimePointClockCurrentSynchPRU1future);// If used with this->TimePointClockCurrentSynchPRU1futureAux, then the instaneous error interrupt can be removed with "Discounting the time to enter the interrupt to measure the deviation" and for the duration_FinalInitialMeasTrig has to be used this->TimePointClockCurrentSynchPRU1future// while(Clock::now() < TimePointClockCurrentSynchPRU1futureAux);// Busy waiting
				// Notice that if the duration for estimating the synch deviation is done with discounting the average time of the interrupt (and not removing the instantaneous error of the interrupt itme) then the time is referenced in average to the PRU time, but then there is a lot of fluctuation when transforming to the system time. Instead, if the synch deviation is computed removing the instantaneous error of te interupt time then, the system clock error is minimized but aflourish the relative small frequency differents of the different PRU clcokcs - this can be accounted for adding a general frequency deviation in the triggered sequences (specified in the python code).
				////this->TimePointClockSendCommandInitial=Clock::now(); // Initial measurement. info. Already computed in the steps before				// Important, the following line at the very beggining to reduce the command jitter
				pru1dataMem_int[0]=static_cast<unsigned int>(5);//static_cast<unsigned int>(this->NextSynchPRUcommand); // apply command
				//prussdrv_pru_send_event(22);
				this->TimePointClockSendCommandFinal=Clock::now(); // Final measurement.
				//retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);// First interrupt sent to measure time				
				//  PRU long execution making sure that notification interrupts do not overlap
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
				// Below for the triggering
				int duration_FinalInitialMeasTrig=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-TimePointClockCurrentSynchPRU1future).count());//static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-TimePointClockCurrentSynchPRU1futureAux).count());//static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSendCommandFinal-TimePointClockCurrentSynchPRU1future).count());
				// Below for the triggering
				this->duration_FinalInitialMeasTrigAuxArray[TrigAuxIterCount%ExtraNumSynchMeasAvgAux]=duration_FinalInitialMeasTrig;
				this->duration_FinalInitialMeasTrigAuxAvg=this->IntMedianFilterSubArray(this->duration_FinalInitialMeasTrigAuxArray,ExtraNumSynchMeasAvgAux);
				this->TrigAuxIterCount++;
				// Below for synch calculation compensation
				duration_FinalInitialCountAuxArrayAvg=static_cast<double>(duration_FinalInitialMeasTrigAuxAvg);						
				
				//pru1dataMem_int[2]// Current IEP timer sample
				//pru1dataMem_int[3]// Correction to apply to IEP timer
				this->PRUcurrentTimerValWrap=static_cast<double>(pru1dataMem_int[2]);
				// Discounting the time to enter the interrupt to measure the deviation is not a good idea because it introduces more error and besides in general de time between measurements is autocorrected for deviations.
				//this->PRUcurrentTimerValWrap=this->PRUcurrentTimerValWrap-duration_FinalInitialCountAux/static_cast<double>(PRUclockStepPeriodNanoseconds);// Remove time for sending command
				// Unwrap
				if (this->PRUcurrentTimerValWrap<=this->PRUcurrentTimerValOldWrap){this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap+(0xFFFFFFFF-this->PRUcurrentTimerValOldWrap);}
				else{this->PRUcurrentTimerVal=this->PRUcurrentTimerValWrap;}
				
				// Compute error
				// Relative error
				this->PRUoffsetDriftError=static_cast<double>((this->iIterPRUcurrentTimerValPass*this->TimePRU1synchPeriod/static_cast<double>(PRUclockStepPeriodNanoseconds)))-(this->PRUcurrentTimerVal-this->PRUcurrentTimerValOldWrap);
				this->ManualSemaphoreExtra=true;
					// Computations for Synch calculaton for PRU0 compensation
					this->EstimateSynch=(static_cast<double>(this->iIterPRUcurrentTimerValPass*this->TimePRU1synchPeriod)/static_cast<double>(PRUclockStepPeriodNanoseconds))/(this->PRUcurrentTimerVal-this->PRUcurrentTimerValOldWrap);
					this->EstimateSynchArray[iIterPRUcurrentTimerValSynch%NumSynchMeasAvgAux]=this->EstimateSynch;
					this->EstimateSynchAvg=DoubleMedianFilterSubArray(EstimateSynchArray,NumSynchMeasAvgAux);
					// Estimate synch direction					
					//this->EstimateSynch=1.0; // To disable synch adjustment
					// Error averaging
					this->PRUoffsetDriftErrorArray[iIterPRUcurrentTimerValSynch%NumSynchMeasAvgAux]=this->PRUoffsetDriftError;
					this->PRUoffsetDriftErrorAvg=DoubleMedianFilterSubArray(PRUoffsetDriftErrorArray,NumSynchMeasAvgAux);
				this->ManualSemaphoreExtra=false;
				this->ManualSemaphore=false;
				this->release();
				// SEt the value to IEP timer	
					this->PRUoffsetDriftErrorAppliedRaw=this->PRUoffsetDriftErrorAvg;
					this->PRUcurrentTimerValOldWrap=this->PRUoffsetDriftErrorAppliedRaw;//this->PRUcurrentTimerValWrap;// Update				
					this->iIterPRUcurrentTimerValSynch++;
					this->iIterPRUcurrentTimerValPass=1;									
			}
			else{// does not enter in time
				this->iIterPRUcurrentTimerValPass++;
			}					
		} //end if
		else if (this->ManualSemaphoreExtra==true){
			// Double entry for some reason. Do not do anything
			//cout << "PRUs in use by other process TTU or SignalGen!" << endl;
			this->iIterPRUcurrentTimerValPass++;
		}
		else{// does not enter in time
			this->iIterPRUcurrentTimerValPass++;
		}
		
		// Information
		if (this->ResetPeriodicallyTimerPRU1 and (this->iIterPRUcurrentTimerVal%(128*NumSynchMeasAvgAux)==0 and this->iIterPRUcurrentTimerVal>NumSynchMeasAvgAux)){//if ((this->iIterPRUcurrentTimerVal%(2*NumSynchMeasAvgAux)==0) and this->iIterPRUcurrentTimerVal>NumSynchMeasAvgAux){//if ((this->iIterPRUcurrentTimerVal%5==0)){
			//cout << "PRUcurrentTimerVal: " << this->PRUcurrentTimerVal << endl;
			//cout << "PRUoffsetDriftError: " << this->PRUoffsetDriftError << endl;
			cout << "PRUoffsetDriftErrorAvg: " << this->PRUoffsetDriftErrorAvg << endl;
			//cout << "PRUoffsetDriftErrorIntegral: " << this->PRUoffsetDriftErrorIntegral << endl;
			//cout << "PRUoffsetDriftErrorAppliedRaw: " << this->PRUoffsetDriftErrorAppliedRaw << endl;
			cout << "EstimateSynchAvg: " << this->EstimateSynchAvg << endl;
			//cout << "duration_FinalInitialDriftAux: " << duration_FinalInitialDriftAux << endl;
			//cout << "this->iIterPRUcurrentTimerValPass: "<< this->iIterPRUcurrentTimerValPass << endl;
			//cout << "this->iIterPRUcurrentTimerValSynch: "<< this->iIterPRUcurrentTimerValSynch << endl;
		}		
		this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
		this->iIterPRUcurrentTimerVal++;
		if (this->iIterPRUcurrentTimerValSynch==(2*this->NumSynchMeasAvgAux)){
			cout << "Synchronized, ready to proceed..." << endl;
		}
	}// end while

return 0; // All ok
}

int GPIO::ReadTimeStamps(unsigned long long int QPLAFutureTimePointNumber){// Read the detected timestaps in four channels
/////////////
std::chrono::nanoseconds duration_back(QPLAFutureTimePointNumber);
this->QPLAFutureTimePoint=Clock::time_point(duration_back);
//while (this->ManualSemaphoreExtra);// Wait until periodic synch method finishes
while (this->ManualSemaphore);// Wait other process// Very critical to not produce measurement deviations when assessing the periodic snchronization
this->ManualSemaphoreExtra=true;
this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
this->AdjPulseSynchCoeffAverage=this->EstimateSynchAvg;// Acquire this value for the this tag reading set
///////////
pru0dataMem_int[2]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and it is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2
pru0dataMem_int[1]=static_cast<unsigned int>(this->NumRecords); // set number captures
// Sleep barrier to synchronize the different nodes at this point, so the below calculations and entry times coincide
requestCoincidenceWhileWait=CoincidenceSetWhileWait();
clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestCoincidenceWhileWait,NULL); // Synch barrier. So that SendTriggerSignals and ReadTimeStamps of the different nodes coincide

// From this point below, the timming is very critical to have long time synchronziation stability!!!!
this->TimePointClockTagPRUinitial=Clock::now()+std::chrono::nanoseconds(2*TimePRUcommandDelay);// Crucial to make the link between PRU clock and system clock (already well synchronized). Two memory mapping to PRU
SynchRem=static_cast<int>((static_cast<long double>(1.5*SynchTrigPeriod)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(SynchTrigPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));// For time stamping it waits 1.5 
TimePointClockTagPRUinitial=TimePointClockTagPRUinitial+std::chrono::nanoseconds(SynchRem);

InstantCorr=static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(256.0*AccumulatedErrorDrift)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod))+static_cast<long double>(PRUoffsetDriftErrorAvg)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod));

if (InstantCorr>0.0){SignAuxInstantCorr=1.0;}
else if (InstantCorr<0.0){SignAuxInstantCorr=-1.0;}
else {SignAuxInstantCorr=0.0;}
InstantCorr=SignAuxInstantCorr*fmodl(abs(InstantCorr),static_cast<long double>(SynchTrigPeriod));

pru0dataMem_int[3]=static_cast<unsigned int>(static_cast<long double>(SynchTrigPeriod)+InstantCorr);// Referenced to the synch trig period

pru0dataMem_int[0]=static_cast<unsigned int>(1); // set command

TimePointClockTagPRUinitial=TimePointClockTagPRUinitial-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg);

while (Clock::now()<TimePointClockTagPRUinitial);// Busy wait time synch sending signals
prussdrv_pru_send_event(21);
this->TimePointClockTagPRUfinal=Clock::now();// Compensate for delays
//retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);// First interrupt sent to measure time
//  PRU long execution making sure that notification interrupts do not overlap
retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);

// Better to not update the time to trig with this since the interrupt is different
//int duration_FinalInitialMeasTrig=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockTagPRUfinal-TimePointClockTagPRUinitial).count());
//this->duration_FinalInitialMeasTrigAuxArray[TrigAuxIterCount%ExtraNumSynchMeasAvgAux]=duration_FinalInitialMeasTrig;
//this->duration_FinalInitialMeasTrigAuxAvg=this->IntMedianFilterSubArray(this->duration_FinalInitialMeasTrigAuxArray,ExtraNumSynchMeasAvgAux);
//this->TrigAuxIterCount++;

//cout << "AccumulatedErrorDrift: " << AccumulatedErrorDrift << endl;
//long double AccumulatedErrorDriftEvolved=static_cast<long double>(256.0*AccumulatedErrorDrift)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod));
//cout << "AccumulatedErrorDriftEvolved: " << AccumulatedErrorDriftEvolved << endl;
//cout << "AccumulatedErrorDriftAux: " << AccumulatedErrorDriftAux << endl;
cout << "PRUoffsetDriftErrorAvg: " << PRUoffsetDriftErrorAvg << endl;
cout << "PRUoffsetDriftErrorAbsAvg: " << PRUoffsetDriftErrorAbsAvg << endl;
//cout << "InstantCorr: " << InstantCorr << endl;
////cout << "RecurrentAuxTime: " << RecurrentAuxTime << endl;
//cout << "pru0dataMem_int3aux: " << pru0dataMem_int3aux << endl;
////cout << "SynchRem: " << SynchRem << endl;
cout << "this->AdjPulseSynchCoeffAverage: " << this->AdjPulseSynchCoeffAverage << endl;
cout << "this->duration_FinalInitialMeasTrigAuxAvg: " << this->duration_FinalInitialMeasTrigAuxAvg << endl;

// Important check to do
if (duration_FinalInitialMeasTrigAuxAvg>5000){cout << "GPIO::Time for pre processing the time barrier is too long " << this->duration_FinalInitialMeasTrigAuxAvg << " ...adjust TimePRUcommandDelay!" << endl;}
this->ManualSemaphore=false;
this->ManualSemaphoreExtra=false;
this->release();

//cout << "retInterruptsPRU0: " << retInterruptsPRU0 << endl;
if (retInterruptsPRU0>0){
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
}
else if (retInterruptsPRU0==0){
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "GPIO::ReadTimeStamps took to much time for the TimeTagg. Timetags might be inaccurate. Reset PRUO if necessary." << endl;
	cout << "We have lost ttg counts! Lost of tags accuracy! Reduce the number of tags per run, and if needed increase the runs number." << endl;
	
}
else{
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "PRU0 interrupt poll error" << endl;
}
this->DDRdumpdata(); // Store to file

return 0;// all ok
}

int GPIO::SendTriggerSignals(double* FineSynchAdjValAux,unsigned long long int QPLAFutureTimePointNumber){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
std::chrono::nanoseconds duration_back(QPLAFutureTimePointNumber);
this->QPLAFutureTimePoint=Clock::time_point(duration_back);
this->FineSynchAdjOffVal=FineSynchAdjValAux[0];// Synch trig offset
this->FineSynchAdjFreqVal=FineSynchAdjValAux[1]; // Synch trig frequency
while (this->ManualSemaphore);// Wait other process// Very critical to not produce measurement deviations when assessing the periodic snchronization
this->ManualSemaphoreExtra=true;
this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
this->acquire();// Very critical to not produce measurement deviations when assessing the periodic snchronization
//this->ManualSemaphore=true;// Very critical to not produce measurement deviations when assessing the periodic snchronization
// Apply a slotted synch configuration (like synchronized Ethernet)
this->AdjPulseSynchCoeffAverage=this->EstimateSynchAvg;

pru1dataMem_int[3]=static_cast<unsigned int>(this->SynchTrigPeriod);// Indicate period of the sequence signal, so that it falls correctly and is picked up by the Signal PRU. Link between system clock and PRU clock. It has to be a power of 2

pru1dataMem_int[1]=static_cast<unsigned int>(this->NumberRepetitionsSignal); // set the number of repetitions
// Sleep barrier to synchronize the different nodes at this point, so the below calculations and entry times coincide
requestCoincidenceWhileWait=CoincidenceSetWhileWait();
clock_nanosleep(CLOCK_TAI,TIMER_ABSTIME,&requestCoincidenceWhileWait,NULL); // Synch barrier. So that SendTriggerSignals and ReadTimeStamps of the different nodes coincide

// From this point below, the timming is very critical to have long time synchronziation stability!!!!
this->TimePointClockTagPRUinitial=Clock::now()+std::chrono::nanoseconds(2*TimePRUcommandDelay);// Since two memory mapping to PRU memory

SynchRem=static_cast<int>((static_cast<long double>(1.5*SynchTrigPeriod)-fmodl((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds)),static_cast<long double>(SynchTrigPeriod)))*static_cast<long double>(PRUclockStepPeriodNanoseconds));
this->TimePointClockTagPRUinitial=this->TimePointClockTagPRUinitial+std::chrono::nanoseconds(SynchRem);

InstantCorr=static_cast<long double>(FineSynchAdjOffVal)*static_cast<long double>(SynchTrigPeriod)+static_cast<long double>(AccumulatedErrorDriftAux)+static_cast<long double>(256.0*AccumulatedErrorDrift)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod))+static_cast<long double>(PRUoffsetDriftErrorAvg)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod))+static_cast<long double>(256.0*FineSynchAdjFreqVal)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod));

if (InstantCorr>0.0){SignAuxInstantCorr=1.0;}
else if (InstantCorr<0.0){SignAuxInstantCorr=-1.0;}
else {SignAuxInstantCorr=0.0;}
InstantCorr=SignAuxInstantCorr*fmodl(abs(InstantCorr),static_cast<long double>(SynchTrigPeriod));

pru1dataMem_int[2]=static_cast<unsigned int>(static_cast<long double>(SynchTrigPeriod)+InstantCorr);// Referenced to the synch trig period

pru1dataMem_int[0]=static_cast<unsigned int>(1); // set command. Generate signals. Takes around 900000 clock ticks

this->TimePointClockTagPRUinitial=this->TimePointClockTagPRUinitial-std::chrono::nanoseconds(duration_FinalInitialMeasTrigAuxAvg);

////if (Clock::now()<this->TimePointClockTagPRUinitial){cout << "Check that we have enough time" << endl;}
while (Clock::now()<this->TimePointClockTagPRUinitial);// Busy wait time synch sending signals
// Important, the following line at the very beggining to reduce the command jitter
prussdrv_pru_send_event(22);//Send host arm to PRU1 interrupt
this->TimePointClockSynchPRUfinal=Clock::now();
// Here there should be the instruction command to tell PRU1 to start generating signals
// We have to define a command, compatible with the memory space of PRU0 to tell PRU1 to initiate signals
//  PRU long execution making sure that notification interrupts do not overlap
retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);

// Synch trig part - not needed here, it is done in the periodic synch checker
//int duration_FinalInitialMeasTrig=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockSynchPRUfinal-this->TimePointClockTagPRUinitial).count());
//this->duration_FinalInitialMeasTrigAuxArray[TrigAuxIterCount%ExtraNumSynchMeasAvgAux]=duration_FinalInitialMeasTrig;
//this->duration_FinalInitialMeasTrigAuxAvg=this->IntMedianFilterSubArray(this->duration_FinalInitialMeasTrigAuxArray,ExtraNumSynchMeasAvgAux);
//this->TrigAuxIterCount++;

//cout << "AccumulatedErrorDrift: " << AccumulatedErrorDrift << endl;
//long double AccumulatedErrorDriftEvolved=static_cast<long double>(256.0*AccumulatedErrorDrift)*static_cast<long double>((static_cast<unsigned long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<unsigned long long int>(1000000000))%static_cast<unsigned long long int>(SynchTrigPeriod));
//cout << "AccumulatedErrorDriftEvolved: " << AccumulatedErrorDriftEvolved << endl;
//cout << "AccumulatedErrorDriftAux: " << AccumulatedErrorDriftAux << endl;
cout << "PRUoffsetDriftErrorAvg: " << PRUoffsetDriftErrorAvg << endl;
cout << "PRUoffsetDriftErrorAbsAvg: " << PRUoffsetDriftErrorAbsAvg << endl;
//cout << "InstantCorr: " << InstantCorr << endl;
////cout << "RecurrentAuxTime: " << RecurrentAuxTime << endl;
//cout << "pru1dataMem_int2aux: " << pru1dataMem_int2aux << endl;
////cout << "SynchRem: " << SynchRem << endl;
cout << "this->AdjPulseSynchCoeffAverage: " << this->AdjPulseSynchCoeffAverage << endl;
cout << "this->duration_FinalInitialMeasTrigAuxAvg: " << this->duration_FinalInitialMeasTrigAuxAvg << endl;

// Important check to do
if (duration_FinalInitialMeasTrigAuxAvg>5000){cout << "GPIO::Time for pre processing the time barrier is too long " << this->duration_FinalInitialMeasTrigAuxAvg << " ...adjust TimePRUcommandDelay!" << endl;}
this->ManualSemaphore=false;
this->ManualSemaphoreExtra=false;
this->release();

//TimePointClockSynchPRUinitial=this->TimePointClockTagPRUinitial;// Update. When commented is in absolute value. Might create precition errors.

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

return 0;// all ok	
}

int GPIO::SendEmulateQubits(){ // Emulates sending 2 entangled qubits through the 8 output pins (each qubits needs 4 pins)

return 0;// all ok
}

int GPIO::SetSynchDriftParams(double* AccumulatedErrorDriftParamsAux){
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

int GPIO::DDRdumpdata(){
// Reading data from PRU shared and own RAMs
//DDR_regaddr = (short unsigned int*)ddrMem + OFFSET_DDR;
valp=valpHolder; // Coincides with SHARED in PRUassTaggDetScript.p
valpAux=valpAuxHolder;
//synchp=synchpHolder;
//for each capture bursts, at the beggining is stored the overflow counter of 32 bits. From there, each capture consists of 32 bits of the DWT_CYCCNT register and 8 bits of the channels detected (40 bits per detection tag).
// The shared memory space has 12KB=12×1024bytes=12×1024×8bits=98304bits.
//Doing numbers, we can store up to 2456 captures. To be in the safe side, we can do 2048 captures

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

// Slot the final time - to remove interrupt jitter
//std::chrono::nanoseconds duration_back(static_cast<unsigned long long int>(static_cast<long long int>(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUfinal.time_since_epoch()).count()-duration_InterruptTag)/static_cast<long double>(PRUclockStepPeriodNanoseconds))*static_cast<long double>(PRUclockStepPeriodNanoseconds)));
//TimePoint TimePointClockTagPRUfinalAux=Clock::time_point(duration_back);
//this->TimeTaggsLast=static_cast<unsigned long long int>(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUfinalAux.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds));

//this->TimeTaggsLast=static_cast<unsigned long long int>(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUinitial.time_since_epoch()).count()+static_cast<long long int>(this->duration_FinalInitialMeasTrigAuxAvg))/static_cast<long double>(PRUclockStepPeriodNanoseconds));

//this->TimeTaggsLast=static_cast<unsigned long long int>(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUfinal.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds));

// Slot the TimeTaggsLast
//this->TimeTaggsLast=static_cast<unsigned long long int>(ceil((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUinitial.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds))/static_cast<long double>(SynchTrigPeriod))*static_cast<long double>(SynchTrigPeriod));

this->TimeTaggsLast=static_cast<unsigned long long int>(ceil((static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUfinal.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds))/static_cast<long double>(SynchTrigPeriod))*static_cast<long double>(SynchTrigPeriod));

//this->TimeTaggsLast=static_cast<unsigned long long int>(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockTagPRUfinal.time_since_epoch()).count())/static_cast<long double>(PRUclockStepPeriodNanoseconds));

//else{Use the latest used, so do not update
//}
//cout << "OldLastTimeTagg: " << OldLastTimeTagg << endl; 
//cout << "TimeTaggsLast: " << TimeTaggsLast << endl; 

//// External synch pulses not used
//// Reading or not Synch pulses
//NumSynchPulses=static_cast<unsigned int>(*synchp);
//synchp++;
////cout << "This slows down and unsynchronizes (comment) GPIO::NumSynchPulses: " << NumSynchPulses << endl;
//if (NumSynchPulses>0){// There are synch pulses
//	if (streamSynchpru.is_open()){
//		streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations		
//		streamSynchpru.write(reinterpret_cast<const char*>(&NumSynchPulses), sizeof(NumSynchPulses));
//		for (unsigned int iIterSynch=0;iIterSynch<NumSynchPulses;iIterSynch++){
//			valCycleCountPRU=static_cast<unsigned int>(*synchp);
//			synchp++;// 1 times 32 bits
//			extendedCounterPRU=extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);			
//			streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
//			streamSynchpru.write(reinterpret_cast<const char*>(&extendedCounterPRU), sizeof(extendedCounterPRU));
//		}
//	}
//	else{
//		cout << "DDRdumpdata streamSynchpru is not open!" << endl;
//	}
//}


if (SlowMemoryPermanentStorageFlag==true){
	// Reading TimeTaggs
	if (streamDDRpru.is_open()){
		streamDDRpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
		for (iIterDump=0; iIterDump<NumRecords; iIterDump++){
			// First 32 bits is the DWT_CYCCNT of the PRU
			// When unsigned char
			//valCycleCountPRU=static_cast<unsigned int>(*valp);
			//valp++;// 1 times 8 bits
			//valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<8;
			//valp++;// 1 times 8 bits
			//valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<16;
			//valp++;// 1 times 8 bits
			//valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<24;
			//valp++;// 1 times 8 bits
			// When unsigned short
			valCycleCountPRU=static_cast<unsigned int>(*valp);
			valp++;// 1 times 16 bits
			valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<16;
			valp++;// 1 times 16 bits
			//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "valCycleCountPRU: " << valCycleCountPRU << endl;}
			// Mount the extended counter value
			extendedCounterPRUholder=static_cast<unsigned long long int>(valCycleCountPRU);//extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);
			//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "extendedCounterPRU: " << extendedCounterPRU << endl;}
			// Apply system clock corrections
			extendedCounterPRU=static_cast<unsigned long long int>(static_cast<double>(extendedCounterPRUholder-OldLastTimeTagg)*AdjPulseSynchCoeffAverage)+TimeTaggsLast;	// The fist OldLastTimeTagg and TimeTaggsLast of the iteration is compensated for with the calibration tag together with the accumulated synchronization error	    
			//////////////////////////////////////////////////////////////
			// Then, the last 32 bits is the channels detected. Equivalent to a 63 bit register at 5ns per clock equates to thousands of years before overflow :)
			// When unsigned char
			//valBitsInterest=static_cast<unsigned short>(*valp);
			//valp++;// 1 times 8 bits
			//valBitsInterest=static_cast<unsigned short>(static_cast<unsigned char>(*valp>>4))<<8;
			//valp++;// 1 times 8 bits
			// When unsigned short
			valBitsInterest=static_cast<unsigned short>(*valp);
			valp++;// 1 times 16 bits
			valBitsInterest=this->packBits(valBitsInterest); // we're just interested in 12 bits which we have to re-order
			//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "val: " << std::bitset<8>(val) << endl;}
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
}
else{//Allocation in memory array
	for (iIterDump=0; iIterDump<NumRecords; iIterDump++){
		// When unsigned short
		valCycleCountPRU=static_cast<unsigned int>(*valp);
		valp++;// 1 times 16 bits
		valCycleCountPRU=valCycleCountPRU | (static_cast<unsigned int>(*valp))<<16;
		valp++;// 1 times 16 bits
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "valCycleCountPRU: " << valCycleCountPRU << endl;}
		// Mount the extended counter value
		extendedCounterPRUholder=static_cast<unsigned long long int>(valCycleCountPRU);//extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "extendedCounterPRU: " << extendedCounterPRU << endl;}
		// Apply system clock corrections
		TimeTaggsStored[TotalCurrentNumRecords]=static_cast<unsigned long long int>(static_cast<double>(extendedCounterPRUholder-OldLastTimeTagg)*AdjPulseSynchCoeffAverage)+TimeTaggsLast;	// The fist OldLastTimeTagg and TimeTaggsLast of the iteration is compensated for with the calibration tag together with the accumulated synchronization error	    
		//////////////////////////////////////////////////////////////		
		// When unsigned short
		ChannelTagsStored[TotalCurrentNumRecords]=this->packBits(static_cast<unsigned short>(*valp)); // we're just interested in 12 bits which we have to re-order
		valp++;// 1 times 16 bits
		if (TotalCurrentNumRecords<(MaxNumQuBitsMemStored-1)){TotalCurrentNumRecords++;}//Variable to hold the number of currently stroed records in memory
		else{cout << "GPIO::We have reach the maximum number of qubits storage" << endl;}
	}
}

// Notify lost of track of counts due to timer overflow
//if (this->FirstTimeDDRdumpdata or this->valThresholdResetCounts==0){this->AfterCountsThreshold=24+5;}// First time the Threshold reset counts of the timetagg is not well computed, hence estimated as the common value
//else{this->AfterCountsThreshold=this->valThresholdResetCounts+5;};// Related to the number of instruciton counts after the last read of the counter. It is a parameter to adjust
this->AfterCountsThreshold=24+5;
this->FirstTimeDDRdumpdata=false;
if(valCycleCountPRU >= (0xFFFFFFFF-this->AfterCountsThreshold)){// The counts that we will lose because of the reset
cout << "We have lost ttg counts! Lost of tags accuracy! Reduce the number of tags per run, and if needed increase the runs number." << endl;
cout << "AfterCountsThreshold: " << AfterCountsThreshold << endl;
cout << "valCycleCountPRU: " << valCycleCountPRU << endl;
}
//else if (valCycleCountPRU > (0x80000000-this->AfterCountsThreshold)){// The exceeded counts, remove them
//this->valCarryOnCycleCountPRU=this->valCarryOnCycleCountPRU-(AboveThresoldCycleCountPRUCompValue-1)*static_cast<unsigned long long int>((this->AfterCountsThreshold+valCycleCountPRU)-0x80000000);
////cout << "this->valCarryOnCycleCountPRU: " << this->valCarryOnCycleCountPRU << endl;
//}

//cout << "sharedMem_int: " << sharedMem_int << endl;

return 0; // all ok
}
/* With timer overflow - results in spike errors
int GPIO::DDRdumpdata(){
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
//cout << "valSkewCounts: " << valSkewCounts << endl;

valThresholdResetCounts=static_cast<unsigned int>(*valpAux);
valpAux++;// 1 times 8 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<8;
valpAux++;// 1 times 8 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<16;
valpAux++;// 1 times 8 bits
valThresholdResetCounts=valThresholdResetCounts | (static_cast<unsigned int>(*valpAux))<<24;
valpAux++;// 1 times 8 bits
//cout << "valThresholdResetCounts: " << valThresholdResetCounts << endl;
//////////////////////////////////////////////////////////////////////////////

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

auxUnskewingFactorResetCycle=auxUnskewingFactorResetCycle+static_cast<unsigned long long int>(valSkewCounts)+static_cast<unsigned long long int>(valOverflowCycleCountPRU-valOverflowCycleCountPRUold)*AboveThresoldCycleCountPRUCompValue;//static_cast<unsigned long long int>(valOverflowCycleCountPRU-valOverflowCycleCountPRUold)*static_cast<unsigned long long int>(valSkewCounts); // Related to the number of instruction/cycles when a reset happens and are lost the counts; // 64 bits. The unskewing is for the deterministic part. The undeterministic part is accounted with valCarryOnCycleCountPRU. This parameter can be adjusted by setting it to 0 and running the analysis of synch and checking the periodicity and also it is better to do it with Precise Time Protocol activated (to reduce the clock difference drift).
//cout << "valOverflowCycleCountPRU-valOverflowCycleCountPRUold: " << (valOverflowCycleCountPRU-valOverflowCycleCountPRUold) << endl;
valOverflowCycleCountPRUold=valOverflowCycleCountPRU; // Update
extendedCounterPRUaux=((static_cast<unsigned long long int>(valOverflowCycleCountPRU)) << 31) + auxUnskewingFactorResetCycle + this->valCarryOnCycleCountPRU+static_cast<unsigned long long int>(valOverflowCycleCountPRU);// The last addition of static_cast<unsigned long long int>(valOverflowCycleCountPRU) is to compensate for a continuous drift

// Reading first calibration tag and link it to the system clock
OldLastTimeTagg=extendedCounterPRUaux + static_cast<unsigned long long int>(*CalpHolder);
auto duration_InitialTag=this->TimePointClockTagPRUinitial-this->TimePointClockPRUinitial;
TimeTaggsLast=static_cast<unsigned long long int>(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_InitialTag).count())/static_cast<double>(PRUclockStepPeriodNanoseconds));

//else{Use the latest used, so do not update
//}
//cout << "OldLastTimeTagg: " << OldLastTimeTagg << endl; 
//cout << "TimeTaggsLast: " << TimeTaggsLast << endl; 

// External synch pulses not used
// Reading or not Synch pulses
//NumSynchPulses=static_cast<unsigned int>(*synchp);
//synchp++;
////cout << "This slows down and unsynchronizes (comment) GPIO::NumSynchPulses: " << NumSynchPulses << endl;
//if (NumSynchPulses>0){// There are synch pulses
//	if (streamSynchpru.is_open()){
//		streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations		
//		streamSynchpru.write(reinterpret_cast<const char*>(&NumSynchPulses), sizeof(NumSynchPulses));
//		for (unsigned int iIterSynch=0;iIterSynch<NumSynchPulses;iIterSynch++){
//			valCycleCountPRU=static_cast<unsigned int>(*synchp);
//			synchp++;// 1 times 32 bits
//			extendedCounterPRU=extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);			
//			streamSynchpru.clear(); // will reset these state flags, allowing you to continue using the stream for additional I/O operations
//			streamSynchpru.write(reinterpret_cast<const char*>(&extendedCounterPRU), sizeof(extendedCounterPRU));
//		}
//	}
//	else{
//		cout << "DDRdumpdata streamSynchpru is not open!" << endl;
//	}
//}


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
		extendedCounterPRUholder=extendedCounterPRUaux + static_cast<unsigned long long int>(valCycleCountPRU);
		//if (iIterDump==0 or iIterDump== 512 or iIterDump==1023){cout << "extendedCounterPRU: " << extendedCounterPRU << endl;}
		// Apply system clock corrections
		extendedCounterPRU=static_cast<unsigned long long int>(static_cast<double>(extendedCounterPRUholder-OldLastTimeTagg)*AdjPulseSynchCoeffAverage)+TimeTaggsLast;	// The fist OldLastTimeTagg and TimeTaggsLast of the iteration is compensated for with the calibration tag together with the accumulated synchronization error		    
		OldLastTimeTagg=extendedCounterPRUholder;
		TimeTaggsLast=extendedCounterPRU;// For the next tagg		    
		//////////////////////////////////////////////////////////////
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

// Correct the last Clock counter carry over if it exceed 0x80000000; Because there is a multiplication of 8, and here we remove it reducing by 7 de excees
if (this->FirstTimeDDRdumpdata or this->valThresholdResetCounts==0){this->AfterCountsThreshold=24+4096;}// First time the Threshold reset counts of the timetagg is not well computed, hence estimated as the common value
else{this->AfterCountsThreshold=this->valThresholdResetCounts+4096;};// Related to the number of instruciton counts after the last read of the counter. It is a parameter to adjust
this->FirstTimeDDRdumpdata=false;
if(valCycleCountPRU >= (0xFFFFFFFF-this->AfterCountsThreshold)){// The counts that we will lose because of the reset
this->valCarryOnCycleCountPRU=this->valCarryOnCycleCountPRU+static_cast<unsigned long long int>((this->AfterCountsThreshold+valCycleCountPRU)-0xFFFFFFFF);
cout << "We have lost ttg counts! Lost of tags accuracy! Reduce the number of tags per run, and if needed increase the runs number." << endl;
cout << "this->valCarryOnCycleCountPRU: " << this->valCarryOnCycleCountPRU << endl;
}
else if (valCycleCountPRU > (0x80000000-this->AfterCountsThreshold)){// The exceeded counts, remove them
this->valCarryOnCycleCountPRU=this->valCarryOnCycleCountPRU-(AboveThresoldCycleCountPRUCompValue-1)*static_cast<unsigned long long int>((this->AfterCountsThreshold+valCycleCountPRU)-0x80000000);
//cout << "this->valCarryOnCycleCountPRU: " << this->valCarryOnCycleCountPRU << endl;
}

//cout << "sharedMem_int: " << sharedMem_int << endl;

return 0; // all ok
}
*/

// Function to pack bits 0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15 of an unsigned int into the lower values
unsigned short GPIO::packBits(unsigned short value) {
    // Isolate bits 1, 2, 3, and 5 and shift them to their new positions
    unsigned short byte0 = value & 0x0F; // Byte 0 stays in position 0
    unsigned short byte1 = (value & 0xF0) >> 4; // Byte 1 shifts to the right four bit positions (the interesting ones)

    // Combine the bytes into a single unsigned short
    return byte0 | byte1;
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
else{
	TotalCurrentNumRecords=0;
}

return 0; // all ok
}

int GPIO::RetrieveNumStoredQuBits(unsigned long long int* TimeTaggs, unsigned short* ChannelTags){
if (SlowMemoryPermanentStorageFlag==true){
	/* External synch pulses not used - done with system clock calibration
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
				double CoeffSynchAdjAux1=0.0;// Number theoretical counts given the number of cycles
				double CoeffSynchAdjAux2=0.0;// PRU counting
				double CoeffSynchAdjAux3=0.0;// Number theoretical counts given the number of cycles
				//double CoeffSynchAdjAux4=0.0;// Number theoretical counts given the number of cycles
				for (int iIter=0;iIter<(NumSynchPulsesRed-1);iIter++){
					//CoeffSynchAdjAux0=(unsigned long long int)(((double)(SynchPulsesTags[iIter+2]-SynchPulsesTags[iIter+1])+PeriodCountsPulseAdj/2.0)/PeriodCountsPulseAdj); // Distill how many pulse synch periods passes...1, 2, 3....To round ot the nearest integer value add half of the dividend to the divisor
					//CoeffSynchAdjAux1=(double)(CoeffSynchAdjAux0)*PeriodCountsPulseAdj;//((double)((SynchPulsesTags[iIter+1]-SynchPulsesTags[iIter])/((unsigned long long int)(PeriodCountsPulseAdj))));
					// It makes a lot of difference to compute the pulse synchs fixed from the first pulse tagg rather than against the last one!!!!!
					CoeffSynchAdjAux1=(double)((unsigned long long int)(((double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[1*iIter])+PeriodCountsPulseAdj/2.0)/PeriodCountsPulseAdj))*PeriodCountsPulseAdj;//(double)((unsigned long long int)(((double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[1*iIter])+PeriodCountsPulseAdj/2.0)/PeriodCountsPulseAdj))*PeriodCountsPulseAdj; // Distill how many pulse synch periods passes...1, 2, 3....To round ot the nearest integer value add half of the dividend to the divisor
					CoeffSynchAdjAux2=(double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[1*iIter]);
					//if (CoeffSynchAdjAux3!=0.0 and CoeffSynchAdjAux4!=0.0){CoeffSynchAdjAux2=(double)(SynchPulsesTags[iIter+2]-SynchPulsesTags[iIter+1])/CoeffSynchAdjAux4-(double)(SynchPulsesTags[iIter+1]-SynchPulsesTags[iIter+0])/CoeffSynchAdjAux3;}
					if (CoeffSynchAdjAux2>0.0){CoeffSynchAdjAux3=CoeffSynchAdjAux1/CoeffSynchAdjAux2;}
					if (CoeffSynchAdjAux1>0.0 and CoeffSynchAdjAux2>0.0){// and CoeffSynchAdjAux4!=0.0){
						//SynPulse detect anomalies
						if (CoeffSynchAdjAux3>1.11 or CoeffSynchAdjAux3<0.99){
							cout << "Synch pulse anomaly CoeffSynchAdjAux3: " << CoeffSynchAdjAux3 << endl;
						}
						AdjPulseSynchCoeffArray[NumSynchPulseAvgAux]=1.0+this->SynchAdjconstant*(CoeffSynchAdjAux3-1.0);//sqrt(CoeffSynchAdjAux3);//AdjPulseSynchCoeff+(CoeffSynchAdjAux2/CoeffSynchAdjAux1);					
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
					AdjPulseSynchCoeffAverage=this->DoubleMeanFilterSubArray(AdjPulseSynchCoeffArray,NumSynchPulseAvgAux);//this->DoubleMedianFilterSubArray(AdjPulseSynchCoeffArray,NumSynchPulseAvgAux);
					cout << "AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
				}// Mean average//this->DoubleMedianFilterSubArray(AdjPulseSynchCoeffArray,NumAvgAux);//Median AdjPulseSynchCoeff/((double)(NumAvgAux));}// Average
				else{AdjPulseSynchCoeffAverage=1.0;}// Reset
				//cout << "GPIO: AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
			}
			else if(this->ResetPeriodicallyTimerPRU1){ // Using the estimation from the re-synchronization function		
				this->AdjPulseSynchCoeff=this->AdjPulseSynchCoeffAverage;
				cout << "Applying re-synch estimated AdjPulseSynchCoeffAverage!" << endl;
				cout << "GPIO: AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
			}
			else{
				AdjPulseSynchCoeffAverage=1.0;
				cout << "GPIO: AdjPulseSynchCoeffAverage: " << AdjPulseSynchCoeffAverage << endl;
			}
			if (NumSynchPulsesRed>=MaxNumPulses){cout << "Too many pulses stored, increase buffer size or reduce number pulses: " << NumSynchPulsesRed << endl;}
		    	//else if (NumSynchPulsesRed==0){cout << "RetrieveNumStoredQuBits: No Synch pulses present!" << endl;}
		    	//cout << "GPIO: NumSynchPulsesRed: " << NumSynchPulsesRed << endl;
		}
		else{
			cout << "RetrieveNumStoredQuBits: BBB streamSynchpru is not open!" << endl;
		return -1;
		}
	*/
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
			    TimeTaggs[lineCount]=static_cast<unsigned long long int>(ValueReadTest);		    
			    ////////////////////////////////////////////////////////////////////////////////
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
else{// Memory allocation
	for (int i=0; i<TotalCurrentNumRecords;i++){
		TimeTaggs[i]=TimeTaggsStored[i];
		ChannelTags[i]=ChannelTagsStored[i];
	}
	return TotalCurrentNumRecords;
}

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
pru1dataMem_int[0]=static_cast<unsigned int>(1); // set command
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
