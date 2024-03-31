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
#define WaitTimeAfterMainWhileLoop 100000 //nanoseconds
// Time/synchronization management
#include <chrono>
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
#define DATARAMoffset	     0x00000200 // Offset from Base OWN_RAM to avoid collision with some data 

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
bool valueSemaphoreExpected=true;
while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
	if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
	break;
	}
}
}
 
void CKPD::release() {
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}
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
	
	// Launch the PRU0 (timetagging) and PR1 (generating signals) codes but put them in idle mode, waiting for command
	// Handler
	//pru0dataMem_int[1]=(unsigned int)0; // set to zero means no command. PRU0 idle
	    // Execute program
	  //pru0dataMem_int[0]=this->NumClocksHalfPeriodPRUclock; // set
	  sharedMem_int[0]=this->NumClocksHalfPeriodPRUclock;//Information grabbed by PRU1
	  pru0dataMem_int[2]=(unsigned int)0;
	// Load and execute the PRU program on the PRU0
	if (prussdrv_exec_program(PRU_HandlerSynch_NUM, "./BBBclockKernelPhysical/PRUassClockHandlerAdj.bin") == -1){
		if (prussdrv_exec_program(PRU_HandlerSynch_NUM, "./PRUassClockHandlerAdj.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassClockHandlerAdj.bin");
		}
	}
	//prussdrv_pru_enable(PRU_HandlerSynch_NUM);
	
	    // Load and execute the PRU program on the PRU1
	    pru1dataMem_int[0]=this->NumClocksHalfPeriodPRUclock; // set the number of clocks that defines the half period of the clock. For 32Khz, with a PRU clock of 5ns is 6250 
	    pru1dataMem_int[1]=(unsigned int)0;
	if (prussdrv_exec_program(PRU_ClockPhys_NUM, "./BBBclockKernelPhysical/PRUassClockPhysicalAdj.bin") == -1){
		if (prussdrv_exec_program(PRU_ClockPhys_NUM, "./PRUassClockPhysicalAdj.bin") == -1){
			perror("prussdrv_exec_program non successfull writing of PRUassClockPhysicalAdj.bin");
		}
	}
	//prussdrv_pru_enable(PRU_ClockPhys_NUM);
	sleep(10);// Give some time to load programs in PRUs and initiate. Very important, otherwise bad values might be retrieved
	// first time to get TimePoints for clock adjustment
	this->TimePointClockCurrentInitial=Clock::now();
	this->requestWhileWait = this->SetFutureTimePoint();
}

int CKPD::GenerateSynchClockPRU(){// Only used once at the begging, because it runs continuosly
pru1dataMem_int[0]=this->NumClocksHalfPeriodPRUclock;
pru1dataMem_int[1]=(unsigned int)1;// Double start command
// Important, the following line at the very beggining to reduce the command jitter
prussdrv_pru_send_event(22);
sleep(1);// Give some time
cout << "Generating clock output..." << endl;
return 0;// all ok
}

int CKPD::HandleInterruptSynchPRU(){ // Uses output pins to clock subsystems physically generating qubits or entangled qubits
//pru0dataMem_int[0]=this->NumClocksHalfPeriodPRUclock; // set
sharedMem_int[0]=this->NumClocksHalfPeriodPRUclock-AdjCountsFreq;//Information grabbed by PRU1
// The following two lines set the maximum synchronizity possible (so do not add lines in between)(critical part)
clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);// Synch barrier
pru0dataMem_int[2]=(unsigned int)1;
prussdrv_pru_send_event(21); // Send interrupt to tell PR0 to handle the clock adjustment

this->requestWhileWait = this->SetFutureTimePoint();

retInterruptsPRU0=prussdrv_pru_wait_event_timeout(PRU_EVTOUT_0,WaitTimeInterruptPRU0);
//cout << "retInterruptsPRU0: " << retInterruptsPRU0 << endl;
if (retInterruptsPRU0>0){
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
}
else if (retInterruptsPRU0==0){
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "CKPD::HandleInterruptSynchPRU took to much time for the ClockHandler. Timetags might be inaccurate. Reset PRUO if necessary." << endl;
}
else{
	prussdrv_pru_clear_event(PRU_EVTOUT_0, PRU0_ARM_INTERRUPT);// So it has time to clear the interrupt for the later iterations
	cout << "PRU0 interrupt poll error" << endl;
}
// Update sharedMem_int[0]=this->NumClocksHalfPeriodPRUclock;//Information grabbed by PRU1
// Also it can be played with the time between updates, both in terms of nanosleep time and number of cycles for updating

if (PlotPIDHAndlerInfo and iIterPlotPIDHAndlerInfo%1000000000000000){cout << "pru0dataMem_int[1]: " << pru0dataMem_int[1] << endl;}
this->NumClocksHalfPeriodPRUclock=(unsigned int)(this->RatioAverageFactorClockHalfPeriod*((double)(this->NumClocksHalfPeriodPRUclock))+(1.0-RatioAverageFactorClockHalfPeriod)*0.5*((double)(pru0dataMem_int[1]+AdjCountsFreq)/(double)(ClockCyclePeriodAdjustment)));
if (PlotPIDHAndlerInfo and iIterPlotPIDHAndlerInfo%1000000000000000){cout << "this->NumClocksHalfPeriodPRUclock: " << this->NumClocksHalfPeriodPRUclock << endl;}
// Set limits of adjustment
if (this->NumClocksHalfPeriodPRUclock<this->MinNumClocksHalfPeriodPRUclock){this->NumClocksHalfPeriodPRUclock=this->MinNumClocksHalfPeriodPRUclock;}
else if (this->NumClocksHalfPeriodPRUclock>this->MaxNumClocksHalfPeriodPRUclock){this->NumClocksHalfPeriodPRUclock=this->MaxNumClocksHalfPeriodPRUclock;}

if (PlotPIDHAndlerInfo and iIterPlotPIDHAndlerInfo%1000000000000000){
cout << "Max Limit this->MaxNumClocksHalfPeriodPRUclock: " << this->MaxNumClocksHalfPeriodPRUclock << endl;
cout << "Limited this->NumClocksHalfPeriodPRUclock: " << this->NumClocksHalfPeriodPRUclock << endl;
cout << "Min Limit this->MinNumClocksHalfPeriodPRUclock: " << this->MinNumClocksHalfPeriodPRUclock << endl;
}

if (PlotPIDHAndlerInfo){iIterPlotPIDHAndlerInfo++;}
return 0;// all ok	
}

struct timespec CKPD::SetFutureTimePoint(){
struct timespec requestWhileWaitAux;
TimePoint TimePointClockCurrentFinal=this->TimePointClockCurrentInitial+std::chrono::nanoseconds(this->TimeAdjPeriod);
this->TimePointClockCurrentInitial=TimePointClockCurrentFinal; //Update value
auto duration_since_epochFutureTimePoint=TimePointClockCurrentFinal.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointClockCurrentFinal_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Convert 
//cout << "TimePointClockCurrentFinal_time_as_count: " << TimePointClockCurrentFinal_time_as_count << endl;

requestWhileWaitAux.tv_sec=(int)(TimePointClockCurrentFinal_time_as_count/((long)1000000000));
requestWhileWaitAux.tv_nsec=(long)(TimePointClockCurrentFinal_time_as_count%(long)1000000000);
return requestWhileWaitAux;
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
clock_nanosleep(CLOCK_TAI, 0, &ts, NULL); //

return 0; // All ok
}

int CKPD::DisablePRUs(){
// Disable PRU and close memory mappings
prussdrv_pru_disable(PRU_ClockPhys_NUM);
prussdrv_pru_disable(PRU_HandlerSynch_NUM);

return 0;
}

CKPD::~CKPD() {
//	this->unexportGPIO();
	this->DisablePRUs();
	//fclose(outfile); 
	prussdrv_exit();
	//munmap(ddrMem, 0x0FFFFFFF);
	//close(mem_fd); // Device
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
 	 CKPDagent.AdjCountsFreq=stoul(argv[1]);
	 CKPDagent.RatioAverageFactorClockHalfPeriod=stod(argv[2]);
	 CKPDagent.RatioFreqAdjustment=stod(argv[3]);
	 CKPDagent.PlotPIDHAndlerInfo=(strcmp(argv[4], "true") == 0);
	 // Recompute some values:
	 CKPDagent.MinNumClocksHalfPeriodPRUclock=(unsigned int)((1.0-CKPDagent.RatioFreqAdjustment)*(double)(CKPDagent.NumClocksHalfPeriodPRUclock));
	 CKPDagent.MaxNumClocksHalfPeriodPRUclock=(unsigned int)((1.0+CKPDagent.RatioFreqAdjustment)*(double)(CKPDagent.NumClocksHalfPeriodPRUclock));
	 //cout << "CKPDagent.AdjCountsFreq: " << CKPDagent.AdjCountsFreq << endl;
	 //cout << "CKPDagent.RatioAverageFactorClockHalfPeriod: " << CKPDagent.RatioAverageFactorClockHalfPeriod << endl;
	 //cout << "CKPDagent.RatioFreqAdjustment: " << CKPDagent.RatioFreqAdjustment << endl;
	 //cout << "CKPDagent.PlotPIDHAndlerInfo: " << CKPDagent.PlotPIDHAndlerInfo << endl;
	 if (CKPDagent.PlotPIDHAndlerInfo==true){
	 	cout << "Attention, when outputing PID values, the synch performance decreases because of the uncontrolled/large time offset between periodic timer computing." << endl;
	 }
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
 
 CKPDagent.GenerateSynchClockPRU();// Launch the generation of the clock
 cout << "Starting to actively adjust clock output..." << endl;
 while(isValidWhileLoop){ 
   try{
 	try {
    	// Code that might throw an exception 
 	// Check if there are need messages or actions to be done by the node
 	//CKPDagent.acquire();
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
        //CKPDagent.release();
	if (signalReceivedFlag.load()){CKPDagent.~CKPD();}// Destroy the instance
        CKPDagent.RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop));// Wait a few microseconds for other processes to enter
    }
    catch (const std::exception& e) {
	// Handle the exception
    	cout << "Exception: " << e.what() << endl;
  	}
  } // upper try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
    }
    } // while
  cout << "Exiting the BBBclockKernelPhysicalDaemon" << endl;
  
  
 return 0; // Everything Ok
}
