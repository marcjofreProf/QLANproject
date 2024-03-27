/*
 * Marc Jofre
 * Technical University of Catalonia
 * 2024
 */

#ifndef BBBclockKernelPhysicalDaemon_H_
#define BBBclockKernelPhysicalDaemon_H_
#include<string>
// Semaphore
#include <atomic>
// Time/synchronization management
#include <chrono>

#define PRUdataPATH1 "./PRUdata/"
#define PRUdataPATH2 "../PRUdata/"

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
	using Clock = std::chrono::steady_clock;// Not needing the system_clock (to mucnh accurate and already in use by the PRUs) //system_clock;steady_clock;high_resolution_clock
	using TimePoint = std::chrono::time_point<Clock>;
	TimePoint OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// could be milliseconds, microseconds or others, but it has to be consistent everywhere
	// PRU
	static int mem_fd;
	static void *ddrMem, *sharedMem, *pru0dataMem, *pru1dataMem;
	static void *pru_int;       // Points to start of PRU memory.
	//static int chunk;
	static unsigned int *sharedMem_int,*pru0dataMem_int,*pru1dataMem_int;
	// PRU timetagger
	int retInterruptsPRU0;
	int WaitTimeInterruptPRU0=2000000; // In microseconds
	//int WaitTimeToFutureTimePointPRU0=1000; // The internal PRU counter (as it is all programmed) can hold around 5s before overflowing. Hence, accounting for sending the command, it is reasonable to say that the timer should last 5s.
	//TimePoint TimePointClockNowPRU0;
	//unsigned long long int TimeNow_time_as_countPRU0;	
	//TimePoint FutureTimePointPRU0;
	//unsigned long long int TimePointFuture_time_as_countPRU0;
	//bool CheckTimeFlagPRU0;
	//bool finPRU0;
	// PRU Signal
	unsigned int NumberRepetitionsSignal=4194304;// Sets the equivalent MTU (Maximum Transmission Unit) for quantum (together with the clock time) - it could be named Quantum MTU
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=2000000; // In microseconds
	//int WaitTimeToFutureTimePointPRU1=1000;// The internal PRU counter (as it is all programmed) can hold around 5s before overflowing. Hence, accounting for sending the command, it is reasonable to say that the timer should last 5s.
	//TimePoint TimePointClockNowPRU1;
	//unsigned long long int TimeNow_time_as_countPRU1;	
	//TimePoint FutureTimePointPRU1;
	//unsigned long long int TimePointFuture_time_as_countPRU1;
	//bool CheckTimeFlagPRU1;
	//bool finPRU1;

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
	// PRU
	int LOCAL_DDMinit();
	int DisablePRUs();	
};


} /* namespace exploringBBBCKPD */

#endif /* BBBclockKernelPhysicalDaemon_H_ */
