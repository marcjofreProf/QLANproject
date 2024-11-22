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
// Timer management
#include <sys/timerfd.h>
#include <sys/select.h>

#define PRUdataPATH1 "./PRUdata/"
#define PRUdataPATH2 "../PRUdata/"
// Clock adjustment
#define ClockPeriodNanoseconds			1000000000// 1pps //1000000// 1KHz//1000000000// 1pps 31250// 32Khz. If this is touched, then the below WaitTimeAfterMainWhileLoop has to be adjusted so that there is enough time after the sleep. For 1pps WaitTimeAfterMainWhileLoop=990000000. For 1 KHz WaitTimeAfterMainWhileLoop=100000 but a lot of averaging needed at it is suffering and drifting. Maybe it is better to use 1pps and discount ticks in the arguments passed -99900000.0
#define WaitTimeAfterMainWhileLoop 990000000 //nanoseconds. Maximum 999999999. Adjusted to have 10ms time slot to activate the busy wait
#define PRUclockStepPeriodNanoseconds		5.00000//4.99999 // Very critical parameter experimentally assessed. PRU clock cycle time in nanoseconds. Specs says 5ns, but maybe more realistic is the 24 MHz clock is a bit higher and then multiplied by 8
#define WaitCyclesBeforeAveraging	10 // To go into steady state in the initialization
#define MaxMedianFilterArraySize	100
#define FilterMode 1 // 0: averaging; 1: median; 2: mean window. The error jumps between two values, then maybe it is better to use mean window
#define	CyclesAvgErrorApplied	1

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
	double RatioAverageFactorClockQuarterPeriodHolder=0.0; // The lower the more aggresive taking the new computed values.
	double RatioAverageFactorClockQuarterPeriod=0.9999; // The lower the more aggresive taking the new computed values. Whe using mean filter.
	unsigned long long int MedianFilterFactor=1; // When using median filter
	unsigned long long int MeanFilterFactor=1; // When using mean filter
	unsigned long long int AppliedMeanFilterFactor=CyclesAvgErrorApplied; // When using mean filter
	bool PlotPIDHAndlerInfo=false; // plot information of interest (it can be done with the launching script)
	double NumClocksQuarterPeriodPRUclock=0.25*(static_cast<double>(ClockPeriodNanoseconds))/(static_cast<double>(PRUclockStepPeriodNanoseconds));// set the number of clocks that defines the Quarter period of the clock.
	double NumClocksQuarterPeriodPRUclockOld=0.0;
	double NumClocksQuarterPeriodPRUclockUpdated=0.0;

private:// Variables
	ApplicationState m_state;
	// Semaphore
	unsigned long long int UnTrapSemaphoreValueMaxCounter=10000000;//MAx counter trying to acquire semaphore, then force release
	int ApproxInterruptTime=6000;
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)
	// Priority values
	int PriorityValRegular=40; // Regular priority during most of the operation
	int PriorityValTop=85; // Top priority for critical operations
	// Time/synchronization management
	unsigned long long int CounterHandleInterruptSynchPRU=0;
	unsigned long long int CounterHandleInterruptSynchPRUlast=0;
	struct my_clock
	{
	    using duration   = std::chrono::nanoseconds;
	    using rep        = duration::rep;
	    using period     = duration::period;
	    using time_point = std::chrono::time_point<my_clock>;
	    static constexpr bool is_steady = false;//false, true. 

	    static time_point now()
	    {
		timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts))// CLOCK_REALTIME//CLOCK_TAI
		    throw 1;
		using sec = std::chrono::seconds;
		return time_point{sec{ts.tv_sec}+duration{ts.tv_nsec}};
	    }
	};
	using ClockWatch = my_clock;//std::chrono::system_clock;// Since we do not need time sleep, it might make sense to use steady_clock;//system_clock; //system_clock;steady_clock;high_resolution_clock// Might seem that for measuring cycles (like a chronometer) steady_clock is better, system_clock is much better than steady_clock aimed at measuring absolute time (like a watch)
	//using ClockChrono = std::chrono::steady_clock;//Probably is also better to also measure with system_clock. system_clock;steady_clock;high_resolution_clock// Might seem that for measuring cycles (like a chronometer) steady_clock is better, system_clock is much better than steady_clock aimed at measuring absolute time (like a watch)	
	//int tfd; // Timer. Attention: // close the time descriptor in the destructor
	//fd_set rfds;
	//int TimerTFDretval;
	//struct timeval TimerTimeout;
	using TimePointWatch = std::chrono::time_point<ClockWatch>;
	//using TimePointChrono = std::chrono::time_point<ClockChrono>;
	unsigned long long int TimePointClockCurrentFinalInitialAdj_time_as_count=ClockPeriodNanoseconds; // Initial value to 1 s
	struct timespec requestWhileWait;
	// PRU
	static int mem_fd;
	static void *ddrMem, *sharedMem, *pru0dataMem, *pru1dataMem;
	static void *pru_int;       // Points to start of PRU memory.
	//static int chunk;
	static unsigned int *sharedMem_int,*pru0dataMem_int,*pru1dataMem_int;
	// Time keeping
	unsigned long long int TimeClockMarging=1000000;// In nanoseconds. It has to be sufficiently large so that after the sleep the system gets aggresive enough with the busy waiting
	unsigned long long int TimeAdjPeriod=static_cast<unsigned long long int>(ClockPeriodNanoseconds); // Period at which the clock is adjusted. VEry important parameter
	double TimePointClockCurrentAdjError=0;
	double TimePointClockCurrentAdjFilError=0;
	double TimePointClockCurrentAdjFilErrorIntegral=0;
	double TimePointClockCurrentAdjFilErrorDerivative=0;
	double TimePointClockCurrentAdjFilErrorLast=0;
	double TimePointClockCurrentAdjFilErrorApplied=0;
	TimePointWatch TimePointClockCurrentInitial=std::chrono::time_point<ClockWatch>(); // Initial updated value of the clock (updated in each iteration)
	TimePointWatch TimePointClockCurrentFinal=std::chrono::time_point<ClockWatch>(); // Initial updated value of the clock (updated in each iteration)
	TimePointWatch TimePointClockCurrentInitialMeas=std::chrono::time_point<ClockWatch>(); // Initial updated value of the clock (updated in each iteration)
	TimePointWatch TimePointClockCurrentFinalMeas=std::chrono::time_point<ClockWatch>(); // Initial updated value of the clock (updated in each iteration)
	//TimePointChrono TimePointClockCurrentInitialAdj=std::chrono::time_point<ClockChrono>(); // Initial updated value of the clock (updated in each iteration)
	//TimePointChrono TimePointClockCurrentFinalAdj=std::chrono::time_point<ClockChrono>(); // Initial updated value of the clock (updated in each iteration)
	//TimePointWatch TimePointClockPRUinitial=std::chrono::time_point<ClockWatch>();// For absolute drift purposes
	int duration_FinalInitialDriftAux=0;// For absolute drift purposes
	int duration_FinalInitialDriftAuxArray[MaxMedianFilterArraySize]={0};
	int duration_FinalInitialDriftAuxArrayAvg=0;
	// PRU clock handling			
	int retInterruptsPRU0;
	// PRU clock generation
	unsigned int PRU1QuarterClocksAux=static_cast<unsigned int>(this->NumClocksQuarterPeriodPRUclock);
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=1500000; // In microseconds
	unsigned int MinNumPeriodColcksPRUnoHalt=1000;// Protection againts very low numbers
	unsigned int MaxNumPeriodColcksPRUnoHalt=200000000;// Protecion agains very large numbers
	// Median filter implementation
	unsigned long long int TimePointClockCurrentFinalInitialAdj_time_as_countArray[MaxMedianFilterArraySize]={1000000000};
	double NumClocksQuarterPeriodPRUclockArray[MaxMedianFilterArraySize]={NumClocksQuarterPeriodPRUclock};
	double TimePointClockCurrentAdjFilErrorArray[MaxMedianFilterArraySize]={0.0};
	double TimePointClockCurrentAdjFilErrorAppliedArray[MaxMedianFilterArraySize]={0.0};
	// PID error correction
	double PIDconstant=1.0; // The larger than 1 the more aggressive correction. Below 1.0 is not aggressively enough to correct fully, eventhought it will try. This value times the maxium value set in MaxTimePointClockCurrentAdjError, has not ot exceed the period wanted. It has to be larger than the jitter of the hardware clocks
	double PIDintegral=0.0;
	double PIDderiv=0.0;	
	// Maximum values
	double MaxAdjCountsFreq=1000000000;
	double MinAdjCountsFreq=-1000000000+MinNumPeriodColcksPRUnoHalt;
	double MaxNumClocksQuarterPeriodPRUclockUpdated=10.0*NumClocksQuarterPeriodPRUclock;
	double MinNumClocksQuarterPeriodPRUclockUpdated=0.01*NumClocksQuarterPeriodPRUclock;
	unsigned long long int MaxTimePointClockCurrentFinalInitialAdj_time_as_count=2*ClockPeriodNanoseconds; // Not used
	unsigned long long int MinTimePointClockCurrentFinalInitialAdj_time_as_count=ClockPeriodNanoseconds/100; // Not used
	double MaxTimePointClockCurrentAdjFilError=4.0*5.0*static_cast<double>(NumClocksQuarterPeriodPRUclock)/100.0;
	double MinTimePointClockCurrentAdjFilError=-4.0*5.0*static_cast<double>(NumClocksQuarterPeriodPRUclock)/100.0;

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
	// Task manager priority
	bool setMaxRrPriority(int PriorityValAux);
	// Handling errors
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
	double DoubleMeanFilterSubArray(double* ArrayHolderAux,unsigned long long int FilterFactor);
	int DoubleBubbleSort(double* arr);
	unsigned long long int ULLIMedianFilterSubArray(unsigned long long int* ArrayHolderAux);
	unsigned long long int ULLIMeanFilterSubArray(unsigned long long int* ArrayHolderAux);
	int ULLIBubbleSort(unsigned long long int* arr);
	int IMedianFilterSubArray(int* ArrayHolderAux);
	int IMeanFilterSubArray(int* ArrayHolderAux);
	int IBubbleSort(int* arr);
	// PID controller
	int PIDcontrolerTime();
};


} /* namespace exploringBBBCKPD */

#endif /* BBBclockKernelPhysicalDaemon_H_ */

/*We're using std::chrono::steady_clock for most of our internal queues and relative timings. However, we're on a POSIX platform and steady_clock is implemented by using clock_gettime(CLOCK_MONOTONIC, ....

The CLOCK_MONOTONIC clock is not affected by discontinuous jumps in the system time (e.g., if the system administrator manually changes the clock), but is affected by the incremental adjustments performed by adjtime(3) and NTP. This clock does not count time that the system is suspended. All CLOCK_MONOTONIC variants guarantee that the time returned by consecutive calls will not go backwards, but successive calls may—depending on the architecture—return identical (not-increased) time values.

Since steady_clock is affected by certain clock adjustments, in some parts of our codebase clock_gettime(CLOCK_MONOTONIC_RAW, ... is therefore used directly instead, which is ...

Similar to CLOCK_MONOTONIC, but provides access to a raw hardware-based time that is not subject to NTP adjustments or the incremental adjustments performed by adjtime(3). This clock does not count time that the system is suspended.

pragma once       // or a classic portable header guard

#include <chrono>
#include <ctime>

namespace foo {

struct monotonic_raw_clock {
    using duration = std::chrono::nanoseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<monotonic_raw_clock, duration>;

    static constexpr bool is_steady = true;

    static inline time_point now() noexcept {
        std::timespec tp;
        // The return value from clock_gettime is ignored in the gcc steady_clock
        // implementation too:
        static_cast<void>(clock_gettime(CLOCK_MONOTONIC_RAW, &tp));

        return time_point(duration(std::chrono::seconds(tp.tv_sec) +
                                   std::chrono::nanoseconds(tp.tv_nsec)));
    }
};

}  // namespace foo
*/
