/*
 * Marc Jofre
 * Technical University of Catalonia
 * 2024
 */

#ifndef GPIO_H_
#define GPIO_H_
#include<string>
#include<fstream>
// Threading
#include <thread>
// Semaphore
#include <atomic>
// Time/synchronization management
#include <chrono>

using namespace std;

using std::string;
using std::ofstream;
using std::ifstream;
using std::fstream;

#define GPIO_PATH "/sys/class/gpio/"
#define PRUdataPATH1 "./PRUdata/"
#define PRUdataPATH2 "../PRUdata/"

#define MaxNumPulses	8192	// Used in the averging of time synchronization arrays
#define PRUclockStepPeriodNanoseconds		4.999775 // Very critical parameter experimentally assessed. PRU clock cycle time in nanoseconds. Specs says 5ns, but maybe more realistic is the 24 MHz clock is a bit higher and then multiplied by 8
#define PulseFreq	1000 // Hz// Not used. Meant for external synchronization pulses (which it is what is wanted to avoid up to some extend)

namespace exploringBB {

typedef int (*CallbackType)(int);
enum GPIO_DIRECTION{ INPUT, OUTPUT };
enum GPIO_VALUE{ LOW=0, HIGH=1 };
enum GPIO_EDGE{ NONE, RISING, FALLING, BOTH };

class GPIO {
//public: //Variables

private:// Variables
	bool ResetPeriodicallyTimerPRU1=false;// Disaster when used, due to all the interrupts handling time uncertainty
	// Semaphore
	std::atomic<bool> valueSemaphore=true;// Start as 1  (open or acquireable)
	std::atomic<bool> ManualSemaphore=false;
	std::atomic<bool> ManualSemaphoreExtra=false;
	std::thread threadRefSynch; // Process thread that executes requests/petitions without blocking
	long long int LostCounts=5; // For stoping and changing IEP counter. It has to do with jitter??? If not ajusted correctly, more jitter
	int NumSynchMeasAvgAux=251;//251//121//31; // Num averages to compute the time error. Better to be odd number.
	int ExtraNumSynchMeasAvgAux=301;//301//181; // More averaging for computing interrupts access time
	unsigned int NextSynchPRUcommand=5;// set initially to NextSynchPRUcorrection=0
	unsigned int NextSynchPRUcorrection=0;// Correction or sequence signal value
	double PRUoffsetDriftError=0;
	double PRUoffsetDriftErrorArray[MaxNumPulses]={0};
	double PRUoffsetDriftErrorAvg=0.0;
	double PRUoffsetDriftErrorLast=0;
	double PRUoffsetDriftErrorIntegral=0;
	double PRUoffsetDriftErrorDerivative=0;
	double PRUoffsetDriftErrorApplied=0;
	double PRUoffsetDriftErrorAppliedRaw=0;
	double PRUcurrentTimerVal=0;
	double PRUcurrentTimerValWrap=0;
	double PRUcurrentTimerValOldWrap=0;
	double PRUcurrentTimerValOld=0;
	double PRUoffsetDriftErrorAppliedOldRaw=0;
	unsigned long long int iIterPRUcurrentTimerVal=0;
	unsigned long long int iIterPRUcurrentTimerValSynch=0;// Account for rounds entered
	unsigned long long int iIterPRUcurrentTimerValPass=1;// Account for rounds that has no tentered
	unsigned long long int iIterPRUcurrentTimerValLast=0;
	double EstimateSynch=1.0;
	double EstimateSynchAvg=1.0;
	double EstimateSynchArray[MaxNumPulses]={EstimateSynch};// They are not all set to the value, only the first one (a function in the declarator should be used to fill them in.
	double EstimateSynchDirection=0.0;
	double EstimateSynchDirectionAvg=0.0;
	double EstimateSynchDirectionArray[MaxNumPulses]={0.0};
	// PID error correction
	//double SynchAdjconstant=1.0;// 
	//double PIDconstant=1.0; // Maybe a little bit over 1.0 to be more aggresive
	//double PIDintegral=0.0;// Not used
	//double PIDderiv=0.0;	// Not used
	// Time/synchronization management
	struct my_clock
	{
	    using duration   = std::chrono::nanoseconds;
	    using rep        = duration::rep;
	    using period     = duration::period;
	    using time_point = std::chrono::time_point<my_clock>;
	    static constexpr bool is_steady = false;// true, false. With false, probably the corrections are abrupt, but less slots affected by misadjustments in time. True, the corrections are smooth and probably more slots affected by misadjustments in time.

	    static time_point now()
	    {
		timespec ts;
		if (clock_gettime(CLOCK_TAI, &ts))// CLOCK_REALTIME//CLOCK_TAI
		    throw 1;
		using sec = std::chrono::seconds;
		return time_point{sec{ts.tv_sec}+duration{ts.tv_nsec}};
	    }
	};
	using Clock = my_clock;//Clock = std::chrono::system_clock;// Since we use a time sleep, it might make sense a system_clock//tai_clock, system_clock or steady_clock;
	using TimePoint = std::chrono::time_point<Clock>;
	double SynchTrigPeriod=65536; //For histogram analysis this is 8*65536
	int FineSynchAdjOffVal=0;// Value provided by user space to adjust the triggering of the signals - offset
	int FineSynchAdjFreqVal=0;// Value provided by user space to adjust the triggering of the signals - frequency
	unsigned long long int TimePRU1synchPeriod=500000000;// In nanoseconds// The faster the more corrections, and less time passed since last correction, but more averaging needed. Also, there is a limit on the lower limit to procees and handle interrupts. The limit might be the error at each iteration, if the error becomes too small, then it cannot be corrected. Anyway, with a better hardware clock (more stable) the correctioons can be done more separated in time).
	unsigned long long int iepPRUtimerRange32bits=4294967296;
	struct timespec requestWhileWait;
	TimePoint TimePointClockCurrentSynchPRU1future=std::chrono::time_point<Clock>();// For synch purposes
	TimePoint TimePointClockSendCommandFinal=std::chrono::time_point<Clock>();// For synch purposes
	//TimePoint TimePointClockSendCommandInitial=std::chrono::time_point<Clock>();// For synch purposes
	//TimePoint TimePointClockPRUinitial=std::chrono::time_point<Clock>();// For absolute drift purposes. Not used
	//TimePoint TimePointClockSynchPRUinitial=std::chrono::time_point<Clock>();// For absolute drift purposes. Not used
	TimePoint TimePointClockSynchPRUfinal=std::chrono::time_point<Clock>();// For absolute drift purposes
	TimePoint TimePointClockTagPRUinitial=std::chrono::time_point<Clock>();// For absolute drift purposes
	TimePoint TimePointClockTagPRUfinal=std::chrono::time_point<Clock>();// For absolute drift purposes
	//TimePoint TimePointClockTagPRUinitialOld=std::chrono::time_point<Clock>();// For absolute drift purposes. Not used
	//int duration_FinalInitialDriftAux=0;// For absolute drift purposes	
	//int duration_FinalInitialDriftAuxArray[MaxNumPulses]={0};// For absolute drift purposes
	//int duration_FinalInitialDriftAuxArrayAvg=0;// For absolute drift purposes
	double duration_FinalInitialCountAux=0.0;
	double duration_FinalInitialCountAuxArray[MaxNumPulses]={0.0};
	double duration_FinalInitialCountAuxArrayAvg=0.0;
	//int duration_FinalInitialMeasTrigAux=0;
	int duration_FinalInitialMeasTrigAuxArray[MaxNumPulses]={0};
	int duration_FinalInitialMeasTrigAuxAvg=0;
	unsigned long long int TrigAuxIterCount=0;	
	unsigned long TimeClockMarging=500000;// In nanoseconds
	unsigned long long int TimeClockMargingExtra=10*TimeClockMarging;// In nanoseconds
	unsigned long long int TimeElpasedNow_time_as_count=0;
	// PRU
	static int mem_fd;
	static void *ddrMem, *sharedMem, *pru0dataMem, *pru1dataMem;
	static void *pru_int;       // Points to start of PRU memory.
	//static int chunk;
	static unsigned int *sharedMem_int,*pru0dataMem_int,*pru1dataMem_int;
	long long int valCarryOnCycleCountPRU=0; // 64 bits
	// PRU timetagger
	int retInterruptsPRU0;
	int WaitTimeInterruptPRU0=15000000; //up to 20000000 with Simple TTG. In microseconds. Although the longer the more innacurraccy in the synch routine
	//TimePoint TimePointClockNowPRU0;
	//unsigned long long int TimeNow_time_as_countPRU0;	
	//TimePoint FutureTimePointPRU0;
	//unsigned long long int TimePointFuture_time_as_countPRU0;
	//bool CheckTimeFlagPRU0;
	//bool finPRU0;
	// PRU Signal
	unsigned int NumberRepetitionsSignal=32768;//8192// Sets the equivalent MTU (Maximum Transmission Unit) for quantum (together with the clock time) - it could be named Quantum MTU. The larger, the more stable the hardware clocks to not lose the periodic synchronization while emitting.
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=15000000; // In microseconds
	//int WaitTimeToFutureTimePointPRU1=1000;// The internal PRU counter (as it is all programmed) can hold around 5s before overflowing. Hence, accounting for sending the command, it is reasonable to say that the timer should last 5s, not more otherwise the synch calculation error overflows as well and things go bad.
	//TimePoint TimePointClockNowPRU1;
	//unsigned long long int TimeNow_time_as_countPRU1;	
	//TimePoint FutureTimePointPRU1;
	//unsigned long long int TimePointFuture_time_as_countPRU1;
	//bool CheckTimeFlagPRU1;
	//bool finPRU1;
	// SHARED RAM to file dump
	int iIterDump;
	unsigned int NumRecords=2048; //2048; //Number of records per run. Max 2048. It is also defined in PRUassTaggDetScript.p and QphysLayerAgent.cpp
	unsigned int NumSynchPulses=0;
	unsigned char* valpHolder;
	unsigned char* valpAuxHolder;
	unsigned int* CalpHolder; // 32 bits
	unsigned int* synchpHolder; // 32 bits
	unsigned char* valp; // 8 bits
	unsigned char* valpAux; // 8 bits
	unsigned int* synchp; // 32 bits
	unsigned int valCycleCountPRU=0; // 32 bits // Made relative to each acquition run
	unsigned int valOverflowCycleCountPRU=0; // 32 bits
	unsigned int valOverflowCycleCountPRUold=0; // 32 bits
	int AboveThresoldCycleCountPRUCompValue=8;//Value adjusted experimentally when PRU clock goes aboe 0x80000000. Not used
	//unsigned int valIEPtimerFinalCounts; // 32 bits
	unsigned long long int extendedCounterPRU=0; // 64 bits
	unsigned long long int extendedCounterPRUholder=0; // 64 bits
	unsigned long long int extendedCounterPRUaux=0; // 64 bits
	//unsigned char val; // 8 bits
	unsigned char valBitsInterest=0; // 8 bits
	unsigned int valSkewCounts=0;
	unsigned int valThresholdResetCounts=0;
	unsigned long long int auxUnskewingFactorResetCycle=0;
	unsigned int AfterCountsThreshold=0;
	//FILE* outfile;
	fstream streamDDRpru;
	fstream streamSynchpru;
	bool FirstTimeDDRdumpdata=true; // First time the Threshold reset counts of the timetagg is not well computed, hence estimated as the common value
	// Non PRU
	int number, debounceTime;
	string name, path;
	ofstream streamOut;
	ifstream streamIn;
	pthread_t thread;
	CallbackType callbackFunction;
	bool threadRunning;
	int togglePeriod;  //default 100ms
	int toggleNumber;  //default -1 (infinite)
	// Testing with periodic histogram signal
	unsigned long long int OldLastTimeTagg=0;
	unsigned long long int TimeTaggsLast=0;
	unsigned long long int TimeTaggsInit=0;
	// Pulses compensation
	int NumSynchPulsesRed=0;
	unsigned long long int SynchPulsesTags[MaxNumPulses]={0};
	unsigned long long int SynchPulsesTagsUsed[MaxNumPulses]={0};
	double PeriodCountsPulseAdj=(((1.0/(double)(PulseFreq))*1e9)/((double)(PRUclockStepPeriodNanoseconds)));// Not used
	double AdjPulseSynchCoeff=1.0;
	double AdjPulseSynchCoeffAverage=1.0;
	double AdjPulseSynchPeriodicCorrectionCoeffAverage=1.0;
	double AdjPulseSynchCoeffArray[MaxNumPulses]={0.0};

public:	// Functions/Methods
	GPIO(int number); //constructor will export the pin	
	// PRU
	GPIO(); // initializates PRU operation
	int InitAgentProcess();
	int LOCAL_DDMinit();
	int DDRdumpdata();
	int DisablePRUs();
	int ReadTimeStamps();// Read the detected timestaps in four channels
	int SendTriggerSignals(int* FineSynchAdjValAux); // Uses output pins to clock subsystems physically generating qubits or entangled qubits
	int SendTriggerSignalsSelfTest();//
	int SendEmulateQubits(); // Emulates sending 2 entangled qubits through the 8 output pins (each qubits needs 4 pins)
	int RetrieveNumStoredQuBits(unsigned long long int* TimeTaggs, unsigned char* ChannelTags); // Reads the fstream file to retrieve number of stored timetagged qubits
	int ClearStoredQuBits(); // Send the writting pointer back to the beggining - effectively clearing stored QuBits
	// Non PRU
	virtual int getNumber() { return number; }
	// General Input and Output Settings
	virtual int setDirection(GPIO_DIRECTION);
	virtual GPIO_DIRECTION getDirection();
	virtual int setValue(GPIO_VALUE);
	virtual int toggleOutput();
	virtual GPIO_VALUE getValue();
	virtual int setActiveLow(bool isLow=true);  //low=1, high=0
	virtual int setActiveHigh(); //default
	//software debounce input (ms) - default 0
	virtual void setDebounceTime(int time) { this->debounceTime = time; }

	// Advanced OUTPUT: Faster write by keeping the stream alive (~20X)
	virtual int streamInOpen();
	virtual int streamOutOpen();
	virtual int streamOutWrite(GPIO_VALUE);
	virtual int streamInRead();
	virtual int streamInClose();
	virtual int streamOutClose();

	virtual int toggleOutput(int time); //threaded invert output every X ms.
	virtual int toggleOutput(int numberOfTimes, int time);
	virtual void changeToggleTime(int time) { this->togglePeriod = time; }
	virtual void toggleCancel() { this->threadRunning = false; }

	// Advanced INPUT: Detect input edges; threaded and non-threaded
	virtual int setEdgeType(GPIO_EDGE);
	virtual GPIO_EDGE getEdgeType();
	virtual int waitForEdge(); // waits until button is pressed
	virtual int waitForEdge(CallbackType callback); // threaded with callback
	virtual void waitForEdgeCancel() { this->threadRunning = false; }

	~GPIO();  //destructor

private: // Functions/Methods
	// Task manager priority
	bool setMaxRrPriority();
	// Sempahore
	void acquire();
	void release();
	// PRU synchronization
	struct timespec SetWhileWait();
	int PRUsignalTimerSynch(); // Periodic synchronizaton of the timer to control the generated signals
	int PRUsignalTimerSynchJitterLessInterrupt();// Tries to avoid interrupt jitter (might not be completely absolute time
	int PIDcontrolerTimeJiterlessInterrupt();
	// Data processing
	unsigned char packBits(unsigned char value);
	// Non-PRU
	int write(string path, string filename, string value);
	int write(string path, string filename, int value);
	string read(string path, string filename);
	//int exportGPIO(); Not currently used - legacy
	//int unexportGPIO(); Not currently used - legacy
	friend void* threadedPoll(void *value);
	friend void* threadedToggle(void *value);
	// Median filter
	double DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor);
	int IntMedianFilterSubArray(int* ArrayHolderAux,int MedianFilterFactor);
	double DoubleMeanFilterSubArray(double* ArrayHolderAux,int MeanFilterFactor);
	int DoubleBubbleSort(double* arr,int MedianFilterFactor);
	int IntBubbleSort(int* arr,int MedianFilterFactor);
};

void* threadedPoll(void *value);
void* threadedToggle(void *value);

} /* namespace exploringBB */

#endif /* GPIO_H_ */
