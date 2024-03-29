/*
 * Marc Jofre
 * Technical University of Catalonia
 * 2024
 */

#ifndef BBBclockKernelPhysicalDaemon_H_
#define BBBclockKernelPhysicalDaemon_H_
#include<string>
#include<cmath>
// Semaphore
#include <atomic>
// Time/synchronization management
#include <chrono>

#define PRUdataPATH1 "./PRUdata/"
#define PRUdataPATH2 "../PRUdata/"
// Clock adjustment
#define ClockPeriodNanoseconds			31250// 32Khz
#define PRUclockStepPeriodNanoseconds		5 // PRU clock cycle time in nanoseconds
#define ClockCyclePeriodAdjustment		2048 // It has to be power of 2 and adjusted in assembler code for faster operations. Multiply this value to the ClockPeriodNanoseconds, the value has to be larger than the WaitTimeAfterMainWhileLoop

namespace exploringBBBCKPD {

class CKPD {
public: //Variables
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };

private:// Variables
	ApplicationState m_state;
	// Semaphore
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)
	// Time/synchronization management
	using Clock = std::chrono::system_clock;// system_clock;steady_clock;high_resolution_clock
	using TimePoint = std::chrono::time_point<Clock>;
	struct timespec requestWhileWait;
	// PRU
	static int mem_fd;
	static void *ddrMem, *sharedMem, *pru0dataMem, *pru1dataMem;
	static void *pru_int;       // Points to start of PRU memory.
	//static int chunk;
	static unsigned int *sharedMem_int,*pru0dataMem_int,*pru1dataMem_int;
	// Time keeping
	unsigned long long int TimeAdjPeriod=(unsigned long long int)(ClockCyclePeriodAdjustment*ClockPeriodNanoseconds); // Period at which the clock is adjusted
	TimePoint TimePointClockCurrentInitial=std::chrono::time_point<Clock>(); // Initial updated value of the clock (updated in each iteration)
	// PRU clock handling
	unsigned int NumClocksHalfPeriodPRUclock=(unsigned int)(0.5*((double)(ClockPeriodNanoseconds))/((double)(PRUclockStepPeriodNanoseconds)));// set the number of clocks that defines the half period of the clock. For 32Khz, with a PRU clock of 5ns is 6250
	float RatioFreqAdjustment=0.8;
	unsigned int MinNumClocksHalfPeriodPRUclock=(unsigned int)((1.0-RatioFreqAdjustment)*(float)(NumClocksHalfPeriodPRUclock));
	unsigned int MaxNumClocksHalfPeriodPRUclock=(unsigned int)((1.0+RatioFreqAdjustment)*(float)(NumClocksHalfPeriodPRUclock));
	int retInterruptsPRU0;
	int WaitTimeInterruptPRU0=(int)(ClockCyclePeriodAdjustment*ClockPeriodNanoseconds/1000); // In microseconds
	// PRU clock generation
	float RatioAverageFactorClockHalfPeriod=0.05;
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=2000000; // In microseconds

public:	// Functions/Methods
	CKPD(); //constructor	
	// PRU
	int GenerateSynchClockPRU();//  PRU0
	int HandleInterruptSynchPRU();// PRU1
	// Managing status of this Agent
        ApplicationState getState() const { return m_state; }	
        bool m_start() { m_state = APPLICATION_RUNNING; return true; }
        bool m_pause() { m_state = APPLICATION_PAUSED; return true; } 
        // resume may keep track of time if the application uses a timer.
        // This is what makes it different than start() where the timer
        // in start() would be initialized to 0. And the last time before
        // paused was trigger would be saved, and then reset as new starting
        // time for your timer or counter. 
        bool m_resume() { m_state = APPLICATION_RUNNING; return true; }      
        bool m_exit() { m_state = APPLICATION_EXIT;  return false; }
        int RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep);
        // Sempahore
	void acquire();
	void release();
	~CKPD();  //destructor

private: // Functions/Methods
	static void SignalINTHandler(int s); // Handler for socket SIGPIPE signal error
	static void SignalPIPEHandler(int s); // Handler for socket SIGPIPE signal error	
	//static void SignalSegmentationFaultHandler(int s); // Handler for segmentation error
	// Clock synch
	struct timespec SetFutureTimePoint();
	// PRU
	int LOCAL_DDMinit();
	int DisablePRUs();	
};


} /* namespace exploringBBBCKPD */

#endif /* BBBclockKernelPhysicalDaemon_H_ */
