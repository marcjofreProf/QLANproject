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
#define ClockPeriodNanoseconds			1000000000// 1pps //1000000// 1KHz//1000000000// 1pps 31250// 32Khz. If this is touched, then the below WaitTimeAfterMainWhileLoop has to be adjusted so that there is enough time after the sleep. For 1pps WaitTimeAfterMainWhileLoop=990000000. For 1 KHz WaitTimeAfterMainWhileLoop=100000 but a lot of averaging needed at it is suffering and drifting. Maybe it is better to use 1pps and discount ticks in the arguments passed -99900000.0
#define WaitTimeAfterMainWhileLoop 990000000 //nanoseconds. Maximum 999999999. Adjusted to have 10ms time slot to activate the busy wait
#define PRUclockStepPeriodNanoseconds		5 // PRU clock cycle time in nanoseconds. Specs says 5ns, but maybe more realistic is 
#define ClockCyclePeriodAdjustment		1// pps// 65536 32 KHz // Very important parameter. The larger the better, since the interrupts time jitter do not paly a role, as long as the PRU counter does not overexceed (the turn down is that ht eupdate time is larger)
#define WaitCyclesBeforeAveraging	20 // To go into steady state in the initialization
#define MaxMedianFilterArraySize	50
#define FilterMode 1 // 0: averaging; 1: median

namespace exploringBBBCKPD {

class CKPD {
public: //Variables
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };
	// Variables adjusted by passing values to the main function
	double AdjCountsFreq=0.0; // Number of clock ticks to adjust to the required frequency (e.g., 32 KHz) to account for having some idle time when resetting DWT_CNT in PRU
	double AdjCountsFreqHolder=0.0;
	double RatioAverageFactorClockHalfPeriodHolder=0.0; // The lower the more aggresive taking the new computed values
	double RatioAverageFactorClockHalfPeriod=0.9999; // The lower the more aggresive taking the new computed values. Whe using mean filter
	unsigned long long int MedianFilterFactor=1; // When using median filter
	bool PlotPIDHAndlerInfo=false;
	double FactorTimerAdj=0.5; 
	double NumClocksHalfPeriodPRUclock=0.5*(static_cast<double>(ClockPeriodNanoseconds))/(static_cast<double>(PRUclockStepPeriodNanoseconds));// set the number of clocks that defines the half period of the clock.
	double NumClocksHalfPeriodPRUclockOld=0.0;
	double NumClocksHalfPeriodPRUclockUpdated=0.0;

private:// Variables
	ApplicationState m_state;
	// Semaphore
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)
	// Time/synchronization management
	unsigned long long int CounterHandleInterruptSynchPRU=0;
	using ClockChrono = std::chrono::system_clock;// steady_clock;steady_clock;high_resolution_clock// Actually inverted with respect the theoretical explanation that is comming. Might seem that for measuring cycles (like a chronometer) steady_clock is better, system_clock is much better than seady_clock aimed at measuring absolute time (like a watch)
	using ClockWatch = std::chrono::system_clock;// system_clock;steady_clock;high_resolution_clock//Actually inverted with respect the theoretical explanation that is comming.  Might seem that for measuring cycles (like a chronometer) steady_clock is better, system_clock is much better than steady_clock aimed at measuring absolute time (like a watch)
	using TimePointChrono = std::chrono::time_point<ClockChrono>;
	using TimePointWatch = std::chrono::time_point<ClockWatch>;
	unsigned long long int TimePointClockCurrentFinalInitialAdj_time_as_count=1000000000; // Initial value to 1 s
	struct timespec requestWhileWait;
	// PRU
	static int mem_fd;
	static void *ddrMem, *sharedMem, *pru0dataMem, *pru1dataMem;
	static void *pru_int;       // Points to start of PRU memory.
	//static int chunk;
	static unsigned int *sharedMem_int,*pru0dataMem_int,*pru1dataMem_int;
	// Time keeping
	unsigned long long int TimeAdjPeriod=static_cast<unsigned long long int>(ClockCyclePeriodAdjustment*ClockPeriodNanoseconds); // Period at which the clock is adjusted. VEry important parameter
	int TimePointClockCurrentAdjError=0;
	TimePointWatch TimePointClockCurrentInitial=std::chrono::time_point<ClockWatch>(); // Initial updated value of the clock (updated in each iteration)
	TimePointWatch TimePointClockCurrentFinal=std::chrono::time_point<ClockWatch>(); // Initial updated value of the clock (updated in each iteration)
	TimePointChrono TimePointClockCurrentInitialAdj=std::chrono::time_point<ClockChrono>(); // Initial updated value of the clock (updated in each iteration)
	TimePointChrono TimePointClockCurrentFinalAdj=std::chrono::time_point<ClockChrono>(); // Initial updated value of the clock (updated in each iteration)
	// PRU clock handling	
	unsigned long long int iIterPlotPIDHAndlerInfo=0;		
	int retInterruptsPRU0;
	int WaitTimeInterruptPRU0=static_cast<int>(ClockCyclePeriodAdjustment*ClockPeriodNanoseconds/2000); // In microseconds
	// PRU clock generation	
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=2000000; // In microseconds
	// Median filter implementation
	unsigned long long int TimePointClockCurrentFinalInitialAdj_time_as_countArray[MaxMedianFilterArraySize]={1000000000};
	double NumClocksHalfPeriodPRUclockArray[MaxMedianFilterArraySize]={NumClocksHalfPeriodPRUclock};
	int TimePointClockCurrentAdjErrorArray[MaxMedianFilterArraySize]={0};

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
	int SetFutureTimePoint();
	struct timespec SetWhileWait();
	// PRU
	int LOCAL_DDMinit();
	int DisablePRUs();
	// Median filter
	double DoubleMedianFilterSubArray(double* ArrayHolderAux);
	int DoubleBubbleSort(double* arr);
	unsigned long long int ULLIMedianFilterSubArray(unsigned long long int* ArrayHolderAux);
	int ULLIBubbleSort(unsigned long long int* arr);
	int IMedianFilterSubArray(int* ArrayHolderAux);
	int IBubbleSort(int* arr);
};


} /* namespace exploringBBBCKPD */

#endif /* BBBclockKernelPhysicalDaemon_H_ */
