/*
 * Marc Jofre
 * Technical University of Catalonia
 * 2024
 */
#include"BBBclockKernelPhysicalDaemon.h"
#include <iostream>
#include <unistd.h> //for sleep
#include <signal.h>
#include <cstring>
#include<thread>
#include<pthread.h>
// Time/synchronization management
#include <chrono>
// Mathematical functions
#include <cmath> // floor function
// PRU programming
#include <poll.h>
#include <stdio.h>
#include <sys/mman.h>
#include <prussdrv.h>
#include <pruss_intc_mapping.h>
#define PRU_ClockPhys_NUM 1 // PRU1
#define PRU_HandlerSynch_NUM 0 // PRU0
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
#define DATARAMoffset	     0x00000200 // Offset from Base OWN_RAM to avoid collision with some data. // Already initiated at this position with LOCAL_DDMinit

#define PRUSS0_PRU0_DATARAM 0
#define PRUSS0_PRU1_DATARAM 1
#define PRUSS0_PRU0_IRAM 2
#define PRUSS0_PRU1_IRAM 3
#define PRUSS0_SHARED_DATARAM 4

using namespace std;

namespace exploringBBBCKPD {
void* exploringBBBCKPD::CKPD::ddrMem = nullptr; // Define and initialize ddrMem
void* exploringBBBCKPD::CKPD::sharedMem = nullptr; // Define and initialize
void* exploringBBBCKPD::CKPD::pru0dataMem = nullptr; // Define and initialize 
void* exploringBBBCKPD::CKPD::pru1dataMem = nullptr; // Define and initialize
void* exploringBBBCKPD::CKPD::pru_int = nullptr;// Define and initialize
unsigned int* exploringBBBCKPD::CKPD::sharedMem_int = nullptr;// Define and initialize
unsigned int* exploringBBBCKPD::CKPD::pru0dataMem_int = nullptr;// Define and initialize
unsigned int* exploringBBBCKPD::CKPD::pru1dataMem_int = nullptr;// Define and initialize
int exploringBBBCKPD::CKPD::mem_fd = -1;// Define and initialize 

///////////////////////////////////////////////////
void CKPD::acquire() {
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
		if (ProtectionSemaphoreTrap>UnTrapSemaphoreValueMaxCounter){this->release();cout << "CKPD::Releasing semaphore!!!" << endl;}// Avoid trapping situations
	if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
	break;
	}
}
}
 
void CKPD::release() {
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}

//////////////////////////////////////////////////////////////////////////
bool CKPD::setMaxRrPriority(int PriorityValAux){// For rapidly handling interrupts
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
/////////////////////////////////////////////////////////////////////////////
/// Errors handling
std::atomic<bool> signalReceivedFlag{false};
static void SignalINTHandler(int s) {
signalReceivedFlag.store(true);
cout << "Caught SIGINT" << endl;
}

static void SignalPIPEHandler(int s) {
signalReceivedFlag.store(true);
cout << "Caught SIGPIPE" << endl;
}

//static void SignalSegmentationFaultHandler(int s) {
//signalReceivedFlag.store(true);
//cout << "Caught SIGSEGV" << endl;
//}
///////////////////////////////////////////////////////
CKPD::CKPD(){// Redeclaration of constructor GPIO when no argument is specified
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
        	
        // Initialize DDM
	LOCAL_DDMinit(); // DDR (Double Data Rate): A class of memory technology used in DRAM where data is transferred on both the rising and falling edges of the clock signal, effectively doubling the data rate without increasing the clock frequency.
	
	/* PRU0 not used
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Handler
	//pru0dataMem_int[1]=(unsigned int)0; // set to zero means no command. PRU0 idle
	    // Execute program
	  //pru0dataMem_int[0]=this->NumClocksQuarterPeriodPRUclock; // set
	  sharedMem_int[0]=static_cast<unsigned int>(this->NumClocksQuarterPeriodPRUclock+this->AdjCountsFreq);//Information grabbed by PRU1
	  pru0dataMem_int[2]=static_cast<unsigned int>(0);
	// Load and execute the PRU program on the PRU0
	if (prussdrv_exec_program(PRU_HandlerSynch_NUM, "./BBBclockKernelPhysical/PRUassClockHandlerAdj.bin") == -1){
		if (prussdrv_exec_program(PRU_HandlerSynch_NUM, "./PRUassClockHandlerAdj.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassClockHandlerAdj.bin");
		}
	}
	//prussdrv_pru_enable(PRU_HandlerSynch_NUM);
	*/
	    // Load and execute the PRU program on the PRU1
	    pru1dataMem_int[0]=static_cast<unsigned int>(this->NumClocksQuarterPeriodPRUclock+this->AdjCountsFreq); // set the number of clocks that defines the Quarter period of the clock. 
	    pru1dataMem_int[1]=static_cast<unsigned int>(1);// Double start command//static_cast<unsigned int>(0);// No command
	if (prussdrv_exec_program(PRU_ClockPhys_NUM, "./BBBclockKernelPhysical/PRUassClockPhysicalAdjTrigHalf.bin") == -1){
		if (prussdrv_exec_program(PRU_ClockPhys_NUM, "./PRUassClockPhysicalAdjTrigHalf.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassClockPhysicalAdjTrigHalf.bin");
		}
	}
	//prussdrv_pru_enable(PRU_ClockPhys_NUM);
	sleep(150);// Give some time to load programs in PRUs and the synch protocols to initiate and lock after prioritazion and adjtimex. Very important, otherwise bad values might be retrieved
	this->setMaxRrPriority(PriorityValTop);// For rapidly handling interrupts, for the main instance and the periodic thread. It stalls operation RealTime Kernel (commented, then)
	// Timer management
	//tfd = timerfd_create(CLOCK_REALTIME,  0);
	// first time to get TimePoints for clock adjustment
	this->TimePointClockCurrentInitial=ClockWatch::now();
	// Absolute time reference	
	std::chrono::nanoseconds duration_back(static_cast<unsigned long long int>(floor(static_cast<long double>(std::chrono::duration_cast<std::chrono::nanoseconds>(TimePointClockCurrentInitial.time_since_epoch()).count())/static_cast<long double>(this->TimeAdjPeriod))*static_cast<long double>(this->TimeAdjPeriod)));
	this->TimePointClockCurrentInitial=ClockWatch::time_point(duration_back);
	this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
	this->TimePointClockCurrentInitialMeas=this->TimePointClockCurrentFinal-std::chrono::nanoseconds(this->duration_FinalInitialDriftAuxArrayAvg);// Important that it is discounted the time to enter the interrupt so that in this way the signal generation in the PRU is reference to an absolute time.
	cout << "Generating clock output..." << endl;	
}

int CKPD::GenerateSynchClockPRU(){// Only used once at the begging, because it runs continuosly
pru1dataMem_int[0]=static_cast<unsigned int>(this->NumClocksQuarterPeriodPRUclock+this->AdjCountsFreq);// Here it is really aplied to the PRU
pru1dataMem_int[1]=static_cast<unsigned int>(1);// Double start command
// Important, the following line at the very beggining to reduce the command jitter
prussdrv_pru_send_event(22);
cout << "Generating clock output..." << endl;

return 0;// all ok
}

int CKPD::HandleInterruptSynchPRU(){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);//CLOCK_TAI,CLOCK_REALTIME// https://opensource.com/article/17/6/timekeeping-linux-vms
// Not a good strategy to keep changing the priority since ther kernel scheduler is not capable to adapt ot it.
//this->setMaxRrPriority(PriorityValTop);// Set top priority
while(ClockWatch::now() < this->TimePointClockCurrentInitialMeas);//std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockCurrentInitialMeas-Clock::now()));// Busy waiting. With a while loop rapid response, but more variation; compared to sleep_for(). Also, the ApproxInterruptTime has to be adjusted (around 6000 for while loop and around 100000 for sleep_for())
//std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockCurrentInitialMeas-ClockWatch::now()));
//std::this_thread::sleep_until(this->TimePointClockCurrentInitialMeas); // Better to use sleep_until because it will adapt to changes in the current time by the time synchronization protocol
//this->TimePointClockCurrentInitialMeas=ClockWatch::now(); //Computed in the step before

//select(tfd+1, &rfds, NULL, NULL, &TimerTimeout);//TimerTFDretval = select(tfd+1, &rfds, NULL, NULL, NULL); /* Last parameter = NULL --> wait forever */
// Important, the following line at the very beggining to reduce the command jitter
prussdrv_pru_send_event(22);
//retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);// First interrupt sent to measure time
this->TimePointClockCurrentFinalMeas=ClockWatch::now(); //Masure time to act
// Not a good strategy to keep changing the priority since ther kernel scheduler is not capable to adapt ot it.
this->setMaxRrPriority(PriorityValRegular);// Set regular priority
// PRU long code running which ensures that it does not overlap in terms of interrupts of the subsequent executions
retInterruptsPRU1=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_1,WaitTimeInterruptPRU1);

if (retInterruptsPRU1>0){
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
}
else if (retInterruptsPRU1==0){
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "CKPD::HandleInterruptSynchPRU took to much time for the ClockHandler. Reset PRU1 if necessary." << endl;	
}
else{
	prussdrv_pru_clear_event(PRU_EVTOUT_1, PRU1_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "PRU1 interrupt poll error" << endl;
}

duration_FinalInitialDriftAux=static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(this->TimePointClockCurrentFinalMeas-this->TimePointClockCurrentInitialMeas).count());//-((this->CounterHandleInterruptSynchPRU+1)*this->TimeAdjPeriod);

switch(FilterMode) {
case 2:{// Mean implementation
this->duration_FinalInitialDriftAuxArray[this->CounterHandleInterruptSynchPRU%MeanFilterFactor]=this->duration_FinalInitialDriftAux;
this->duration_FinalInitialDriftAuxArrayAvg=IMeanFilterSubArray(this->duration_FinalInitialDriftAuxArray);
break;
}
case 1:{// Median implementation
this->duration_FinalInitialDriftAuxArray[this->CounterHandleInterruptSynchPRU%MedianFilterFactor]=this->duration_FinalInitialDriftAux;
this->duration_FinalInitialDriftAuxArrayAvg=IMedianFilterSubArray(this->duration_FinalInitialDriftAuxArray);
break;
}
default:{// Average implementation
this->duration_FinalInitialDriftAuxArrayAvg = this->RatioAverageFactorClockQuarterPeriod*this->duration_FinalInitialDriftAuxArrayAvg+(1.0-this->RatioAverageFactorClockQuarterPeriod)*this->duration_FinalInitialDriftAux;
}
}

if (this->duration_FinalInitialDriftAuxArrayAvg>ApproxInterruptTime){// Much longer than for client node (which typically is below 5000) maybe because more effort to serve PTP messages
	cout << "Time for pre processing the time barrier is too long " << this->duration_FinalInitialDriftAuxArrayAvg << " ns ...adjust TimeClockMarging! Set to nominal value of " << ApproxInterruptTime << " ns ..." << endl;
	this->duration_FinalInitialDriftAuxArrayAvg=ApproxInterruptTime;// For the time being adjust it to the nominal initial value
}

this->requestWhileWait = this->SetWhileWait();// Used with non-busy wait
this->TimePointClockCurrentInitialMeas=this->TimePointClockCurrentFinal;//-std::chrono::nanoseconds(this->duration_FinalInitialDriftAuxArrayAvg);// Actually, the time measured duration_FinalInitialDriftAuxArrayAvg is not indicative of much (only if it changes a lot to high values it means trouble)

// Compute error
if (retInterruptsPRU1>0){
	// Compute clocks adjustment
	//auto duration_FinalInitial=this->TimePointClockCurrentFinalMeas-this->TimePointClockCurrentInitialMeas;
	//unsigned long long int duration_FinalInitialCountAux=std::chrono::duration_cast<std::chrono::nanoseconds>(duration_FinalInitial).count();
	
	// Compute absolute error
	if (this->CounterHandleInterruptSynchPRU>=WaitCyclesBeforeAveraging){// Error should not be filtered
	this->TimePointClockCurrentAdjError=0.0;//(static_cast<double>(this->TimeAdjPeriod)/2.0-static_cast<double>(duration_FinalInitialCountAux));//(this->TimePointClockCurrentAdjError-static_cast<int>(this->PIDconstant*static_cast<double>(this->TimePointClockCurrentAdjFilError)))+(static_cast<int>(this->TimeAdjPeriod)-static_cast<int>(duration_FinalInitialCountAux));//static_cast<int>(duration_FinalInitialAdjCountAux-this->TimeAdjPeriod);// Error to be compensated for. Critical part to not have continuous drift. The old error we substract the part corrected sent to PRU and we add the new computed error
	}
	else{
		this->TimePointClockCurrentAdjError=0.0;
	}

	// Error filtering
	switch(FilterMode) {
	case 2:{// Mean implementation
	this->TimePointClockCurrentAdjFilErrorArray[this->CounterHandleInterruptSynchPRU%MeanFilterFactor]=this->TimePointClockCurrentAdjError;// Error to be compensated for
	this->TimePointClockCurrentAdjFilError=this->DoubleMeanFilterSubArray(this->TimePointClockCurrentAdjFilErrorArray,this->MeanFilterFactor);
	break;
	}
	case 1:{// Median implementation
	this->TimePointClockCurrentAdjFilErrorArray[this->CounterHandleInterruptSynchPRU%MedianFilterFactor]=this->TimePointClockCurrentAdjError;// Error to be compensated for
	this->TimePointClockCurrentAdjFilError=this->DoubleMedianFilterSubArray(this->TimePointClockCurrentAdjFilErrorArray);
	break;
	}
	default:{// Average implementation
	this->TimePointClockCurrentAdjFilError = this->RatioAverageFactorClockQuarterPeriod*this->TimePointClockCurrentAdjFilError+(1.0-this->RatioAverageFactorClockQuarterPeriod)*this->TimePointClockCurrentAdjError;
	}
	}
	// Limit applied error correction
	if (this->TimePointClockCurrentAdjFilError>this->MaxTimePointClockCurrentAdjFilError){this->TimePointClockCurrentAdjFilError=this->MaxTimePointClockCurrentAdjFilError;}
	else if (this->TimePointClockCurrentAdjFilError<this->MinTimePointClockCurrentAdjFilError){this->TimePointClockCurrentAdjFilError=this->MinTimePointClockCurrentAdjFilError;}
	
	// PID implementation
	//this->PIDcontrolerTime();
	
	// Apply period scaling selected by the user
	if (this->CounterHandleInterruptSynchPRU<WaitCyclesBeforeAveraging){// Do not apply the averaging in the first ones since everything is adjusting
		this->AdjCountsFreq=0.0;
	}
	else{
		this->AdjCountsFreq=this->AdjCountsFreqHolder/PRUclockStepPeriodNanoseconds/4.0;// divided by 4 because it is for Quarter period for the PRU1.
	}
	this->MinAdjCountsFreq=-this->NumClocksQuarterPeriodPRUclock+static_cast<double>(MinNumPeriodColcksPRUnoHalt);
	if (this->AdjCountsFreq>this->MaxAdjCountsFreq){this->AdjCountsFreq=this->MaxAdjCountsFreq;}
	else if(this->AdjCountsFreq<this->MinAdjCountsFreq){this->AdjCountsFreq=this->MinAdjCountsFreq;}
}
// Update values
//this->TimePointClockCurrentAdjFilErrorAppliedArray[this->CounterHandleInterruptSynchPRU%this->AppliedMeanFilterFactor]=this->TimePointClockCurrentAdjFilErrorApplied;// Averaging of PId Not used

PRU1QuarterClocksAux=static_cast<unsigned int>(this->NumClocksQuarterPeriodPRUclock+this->AdjCountsFreq+this->TimePointClockCurrentAdjFilErrorApplied/PRUclockStepPeriodNanoseconds/4.0);// Here the correction is inserted

if (PRU1QuarterClocksAux>this->MaxNumPeriodColcksPRUnoHalt){PRU1QuarterClocksAux=this->MaxNumPeriodColcksPRUnoHalt;}
else if (PRU1QuarterClocksAux<this->MinNumPeriodColcksPRUnoHalt){PRU1QuarterClocksAux=this->MinNumPeriodColcksPRUnoHalt;}

this->CounterHandleInterruptSynchPRU++;// Update counter

if (this->CounterHandleInterruptSynchPRU%30==0){
	this->CounterHandleInterruptSynchPRU=0;// Reset value
	if (PlotPIDHAndlerInfo){	
	//cout << "pru0dataMem_int[1]: " << pru0dataMem_int[1] << endl;
	//cout << "this->NumClocksQuarterPeriodPRUclock: " << this->NumClocksQuarterPeriodPRUclock << endl;
	// Not used cout << "this->TimePointClockCurrentFinalInitialAdj_time_as_count: " << this->TimePointClockCurrentFinalInitialAdj_time_as_count << endl;
	cout << "this->duration_FinalInitialDriftAuxArrayAvg: " << this->duration_FinalInitialDriftAuxArrayAvg << " ns." << endl;
	cout << "this->TimePointClockCurrentAdjError: " << this->TimePointClockCurrentAdjError << endl;
	cout << "this->TimePointClockCurrentAdjFilError: " << this->TimePointClockCurrentAdjFilError << endl;
	cout << "this->TimePointClockCurrentAdjFilErrorApplied: " << this->TimePointClockCurrentAdjFilErrorApplied << endl;
	//cout << "this->TimePointClockCurrentAdjFilErrorAppliedOld: " << this->TimePointClockCurrentAdjFilErrorAppliedOld << endl;
	//cout << "PRU1QuarterClocksAux: " << PRU1QuarterClocksAux << endl;
	}
}

// Applu PRU command for next roung
pru1dataMem_int[0]=PRU1QuarterClocksAux;//Information sent to and grabbed by PRU1

//Triggered part
pru1dataMem_int[1]=static_cast<unsigned int>(1);// Double start command

return 0;// All ok
}

int CKPD::PIDcontrolerTime(){
TimePointClockCurrentAdjFilErrorDerivative=0.0;//(TimePointClockCurrentAdjFilError-TimePointClockCurrentAdjFilErrorLast)/(static_cast<double>(CounterHandleInterruptSynchPRU-CounterHandleInterruptSynchPRUlast));
TimePointClockCurrentAdjFilErrorIntegral=0.0;//TimePointClockCurrentAdjFilErrorIntegral+TimePointClockCurrentAdjFilError*static_cast<double>(CounterHandleInterruptSynchPRU-CounterHandleInterruptSynchPRUlast);

this->TimePointClockCurrentAdjFilErrorApplied=PIDconstant*TimePointClockCurrentAdjFilError+PIDintegral*TimePointClockCurrentAdjFilErrorIntegral+PIDderiv*TimePointClockCurrentAdjFilErrorDerivative;
TimePointClockCurrentAdjFilErrorLast=TimePointClockCurrentAdjFilError;// Update
CounterHandleInterruptSynchPRUlast=CounterHandleInterruptSynchPRU;// Update
return 0; // All ok
}

struct timespec CKPD::SetWhileWait(){
	struct timespec requestWhileWaitAux;
	// Relative incrment
	this->TimePointClockCurrentFinal=this->TimePointClockCurrentInitial+std::chrono::nanoseconds(this->TimeAdjPeriod);//-std::chrono::nanoseconds(duration_FinalInitialDriftAux);//-std::chrono::nanoseconds(static_cast<unsigned long long int>(this->PIDconstant*static_cast<double>(this->TimePointClockCurrentAdjFilError)));
	this->TimePointClockCurrentInitial=this->TimePointClockCurrentFinal;//+std::chrono::nanoseconds(static_cast<unsigned long long int>(this->PIDconstant*static_cast<double>(this->TimePointClockCurrentAdjFilError))); //Update value
		
	auto duration_since_epochFutureTimePoint=this->TimePointClockCurrentFinal.time_since_epoch();
	// Convert duration to desired time
	long long int TimePointClockCurrentFinal_time_as_count = static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count())-static_cast<long long int>(this->TimeClockMarging); // Add an offset, since the final barrier is implemented with a busy wait 
	//cout << "TimePointClockCurrentFinal_time_as_count: " << TimePointClockCurrentFinal_time_as_count << endl;

	requestWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	requestWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);

	// Timer sets an interrupt that if not commented (when not in use) produces a long reaction time in the while loop (busy wait)
	// Set the timer to expire at the desired time
	//TimePointClockCurrentFinal_time_as_count = static_cast<long long int>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count());//-static_cast<long long int>(this->TimeClockMarging); // Add an offset, since the final barrier is implemented with a busy wait 
	//cout << "TimePointClockCurrentFinal_time_as_count: " << TimePointClockCurrentFinal_time_as_count << endl;
	
    //TimerTimeout.tv_sec = (int)((2*this->TimeClockMarging)/((long)1000000000)); 
    //TimerTimeout.tv_usec = (long)((2*this->TimeClockMarging)%(long)1000000000);
	//
    //struct itimerspec its;
    //its.it_interval.tv_sec = 0;  // No interval, one-shot timer
    //its.it_interval.tv_nsec = 0;
    //its.it_value.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
	//its.it_value.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);

	//timerfd_settime(this->tfd, TFD_TIMER_ABSTIME, &its, NULL);

	// Watch timefd file descriptor
    //FD_ZERO(&rfds);
    //FD_SET(this->tfd, &rfds);

	return requestWhileWaitAux;
}

int CKPD::SetFutureTimePoint(){
	this->TimePointClockCurrentFinal=this->TimePointClockCurrentInitial+std::chrono::nanoseconds(this->TimeAdjPeriod);//-std::chrono::nanoseconds(duration_FinalInitialDriftAux);//-std::chrono::nanoseconds(static_cast<unsigned long long int>(this->PIDconstant*static_cast<double>(this->TimePointClockCurrentAdjFilError)));
	this->TimePointClockCurrentInitial=this->TimePointClockCurrentFinal;
	return 0; // All Ok
}

/*****************************************************************************
* Local Function Definitions                                                 *
*****************************************************************************/

int CKPD::LOCAL_DDMinit(){
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

int CKPD::RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep){
struct timespec ts;
ts.tv_sec=(int)(TimeNanoSecondsSleep/((long)1000000000));
ts.tv_nsec=(long)(TimeNanoSecondsSleep%(long)1000000000);
clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL); //

return 0; // All ok
}

int CKPD::DisablePRUs(){
// Disable PRU and close memory mappings
prussdrv_pru_disable(PRU_ClockPhys_NUM);
prussdrv_pru_disable(PRU_HandlerSynch_NUM);

return 0;
}

double CKPD::DoubleMedianFilterSubArray(double* ArrayHolderAux){
if (this->MedianFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    double temp[this->MedianFilterFactor]={0.0};
    for(int i = 0; i < this->MedianFilterFactor; i++) {
        temp[i] = ArrayHolderAux[i];
    }
    
    // Step 2: Sort the temporary array
    this->DoubleBubbleSort(temp);
    // If odd, middle number
      return temp[this->MedianFilterFactor/2];
}
}

double CKPD::DoubleMeanFilterSubArray(double* ArrayHolderAux,unsigned long long int FilterFactor){
if (FilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    double temp=0.0;
    for(int i = 0; i < FilterFactor; i++) {
        temp = temp + ArrayHolderAux[i];
    }
    
    temp=temp/((double)(FilterFactor));
    return temp;
}
}

// Function to implement Bubble Sort
int CKPD::DoubleBubbleSort(double* arr) {
    for (int i = 0; i < this->MedianFilterFactor-1; i++) {
        for (int j = 0; j < this->MedianFilterFactor-i-1; j++) {
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

unsigned long long int CKPD::ULLIMedianFilterSubArray(unsigned long long int* ArrayHolderAux){
if (this->MedianFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    unsigned long long int temp[this->MedianFilterFactor]={0};
    for(int i = 0; i < this->MedianFilterFactor; i++) {
        temp[i] = ArrayHolderAux[i];
    }
    
    // Step 2: Sort the temporary array
    this->ULLIBubbleSort(temp);
    // If odd, middle number
    return temp[this->MedianFilterFactor/2];
}
}

unsigned long long int CKPD::ULLIMeanFilterSubArray(unsigned long long int* ArrayHolderAux){
if (this->MeanFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    unsigned long long int temp=0.0;
    for(int i = 0; i < this->MeanFilterFactor; i++) {
        temp = temp + ArrayHolderAux[i];
    }
    
    temp=(unsigned long long int)(((double)(temp))/((double)(this->MeanFilterFactor)));
    return temp;
}
}

// Function to implement Bubble Sort
int CKPD::ULLIBubbleSort(unsigned long long int* arr) {
    for (int i = 0; i < this->MedianFilterFactor-1; i++) {
        for (int j = 0; j < this->MedianFilterFactor-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
                unsigned long long int temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0; // All ok
}

int CKPD::IMedianFilterSubArray(int* ArrayHolderAux){
if (this->MedianFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    int temp[this->MedianFilterFactor]={0};
    for(int i = 0; i < this->MedianFilterFactor; i++) {
        temp[i] = ArrayHolderAux[i];
    }
    
    // Step 2: Sort the temporary array
    this->IBubbleSort(temp);
    // If odd, middle number
      return temp[this->MedianFilterFactor/2];
}
}

int CKPD::IMeanFilterSubArray(int* ArrayHolderAux){
if (this->MeanFilterFactor<=1){
	return ArrayHolderAux[0];
}
else{
	// Step 1: Copy the array to a temporary array
    int temp=0.0;
    for(int i = 0; i < this->MeanFilterFactor; i++) {
        temp = temp + ArrayHolderAux[i];
    }
    
    temp=(int)(((double)(temp))/((double)(this->MeanFilterFactor)));
    return temp;
}
}

// Function to implement Bubble Sort
int CKPD::IBubbleSort(int* arr) {
    for (int i = 0; i < this->MedianFilterFactor-1; i++) {
        for (int j = 0; j < this->MedianFilterFactor-i-1; j++) {
            if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
                int temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
    return 0; // All ok
}

CKPD::~CKPD() {
//	this->unexportGPIO();
	this->DisablePRUs();
	//fclose(outfile); 
	prussdrv_exit();
	//munmap(ddrMem, 0x0FFFFFFF);
	//close(mem_fd); // Device
	//close(tfd);// close the time descriptor
}

}

using namespace exploringBBBCKPD;

int main(int argc, char const * argv[]){
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
 
 cout << "CKPDagent started..." << endl;
 
 CKPD CKPDagent; // Initiate the instance
 
 if (argc>1){// Arguments passed
 	try{
 	 CKPDagent.AdjCountsFreq=stod(argv[1]);
 	 CKPDagent.AdjCountsFreqHolder=CKPDagent.AdjCountsFreq;// Update of the value for ever	 
	 
	 switch(FilterMode) {
	 case 2:{// Mean implementation
		cout << "Using mean filtering." << endl;
		CKPDagent.MeanFilterFactor=stoull(argv[2]);
		 if (CKPDagent.MeanFilterFactor>MaxMedianFilterArraySize){
		 	CKPDagent.MeanFilterFactor=MaxMedianFilterArraySize;
		 	cout << "Attention, mean filter size too large." << endl;
		 }
		 else if (CKPDagent.MeanFilterFactor<1){
		 	CKPDagent.MeanFilterFactor=1;
		 	cout << "Attention, mean filter size too small." << endl;
		 }
		 else{// For fast median computing the length should be odd
		 	CKPDagent.MeanFilterFactor=(CKPDagent.MeanFilterFactor/2)*2+1;
		 }
		 break;
	}
	case 1:{// Median implementation
		cout << "Using median filtering." << endl;
		CKPDagent.MedianFilterFactor=stoull(argv[2]);
		 if (CKPDagent.MedianFilterFactor>MaxMedianFilterArraySize){
		 	CKPDagent.MedianFilterFactor=MaxMedianFilterArraySize;
		 	CKPDagent.MedianFilterFactor=(CKPDagent.MedianFilterFactor/2)*2+1;// odd
		 	cout << "Attention, median filter size too large." << endl;
		 }
		 else if (CKPDagent.MedianFilterFactor<1){
		 	CKPDagent.MedianFilterFactor=1;
		 	cout << "Attention, median filter size too small." << endl;
		 }
		 else{// For fast median computing the length should be odd
		 	CKPDagent.MedianFilterFactor=(CKPDagent.MedianFilterFactor/2)*2+1;// odd
		 }
		 break;
	}
	default:{// Average implementation
		cout << "Using average filtering." << endl;
		CKPDagent.RatioAverageFactorClockQuarterPeriod=stod(argv[2]);
	}
	}

	//cout << "argv[3]: " << argv[3] << endl;
	//cout << "strcmp(argv[3],true): " << strcmp(argv[3],"true") << endl;

	if (strcmp(argv[3],"true")==0){CKPDagent.PlotPIDHAndlerInfo=true;}// true or false to output information
	else{CKPDagent.PlotPIDHAndlerInfo=false;}

	 } catch(const std::invalid_argument& e) {
            cout << "Invalid argument: Could not convert to double." << endl;
        } catch(const std::out_of_range& e) {
            cout << "Out of range: The argument is too large or too small." << endl;
        }
 }
 
 CKPDagent.m_start(); // Initiate in start state.
 
 /// Errors handling
 signal(SIGINT, SignalINTHandler);// Interruption signal
 signal(SIGPIPE, SignalPIPEHandler);// Error trying to write/read to a socket
 //signal(SIGSEGV, SignalSegmentationFaultHandler);// Segmentation fault
 
 bool isValidWhileLoop=true;
 if (CKPDagent.getState()==CKPD::APPLICATION_EXIT){isValidWhileLoop = false;}
 else{isValidWhileLoop = true;}
 
 //CKPDagent.GenerateSynchClockPRU();// Launch the generation of the clock
 cout << "Starting to actively adjust clock output..." << endl;
 
 while(isValidWhileLoop){ 
 	//CKPDagent.acquire();
   //try{
 	//try {
    	// Code that might throw an exception 
 	// Check if there are need messages or actions to be done by the node
 	
       switch(CKPDagent.getState()) {
           case CKPD::APPLICATION_RUNNING: {               
               // Do Some Work
               CKPDagent.HandleInterruptSynchPRU();
               break;
           }
           case CKPD::APPLICATION_PAUSED: {
               // Maybe do some checks if necessary 
               break;
           }
           case CKPD::APPLICATION_EXIT: {                  
               isValidWhileLoop=false;//break;
           }
           default: {
               // ErrorHandling Throw An Exception Etc.
           }

        } // switch
        
	if (signalReceivedFlag.load()){CKPDagent.~CKPD();}// Destroy the instance
        // Main barrier is in HandleInterruptSynchPRU function. No need for this CKPDagent.RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop));
        
    //}
    //catch (const std::exception& e) {
    //	// Handle the exception
    //	cout << "Exception: " << e.what() << endl;
    //	}
  //} // upper try
  //catch (...) { // Catches any exception
  //cout << "Exception caught" << endl;
  //  }
	//CKPDagent.release();
	//CKPDagent.RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop));// Used in busy-wait
    } // while
  cout << "Exiting the BBBclockKernelPhysicalDaemon" << endl;
  
  
 return 0; // Everything Ok
}
