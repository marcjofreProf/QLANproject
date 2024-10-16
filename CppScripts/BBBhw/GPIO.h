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
#define MaxNumQuBitsPerRun 1964 // Really defined in GPIO.h. Max 1964 for 12 input pins. 2048 for 8 input pins. Given the shared PRU memory size (discounting a 0x200 offset)
#define MaxNumQuBitsMemStored 1*MaxNumQuBitsPerRun // Maximum size of the array for memory storing qubits (timetaggs and channels)
#define MaxNumPulses	8192	// Used in the averaging of time synchronization arrays
#define PRUclockStepPeriodNanoseconds		5.00000//4.99999 // Very critical parameter experimentally assessed. PRU clock cycle time in nanoseconds. Specs says 5ns, but maybe more realistic is the 24 MHz clock is a bit higher and then multiplied by 8
#define PulseFreq	1000 // Hz// Not used. Meant for external synchronization pulses (which it is what is wanted to avoid up to some extend)
#define QuadNumChGroups 3 // There are three quad groups of emission channels and detection channels (which are treated independetly)

namespace exploringBB {

	typedef int (*CallbackType)(int);
	enum GPIO_DIRECTION{ INPUT, OUTPUT };
	enum GPIO_VALUE{ LOW=0, HIGH=1 };
	enum GPIO_EDGE{ NONE, RISING, FALLING, BOTH };

	class GPIO {
//public: //Variables

private:// Variables
	// For frequency synchronization Then, the interrogation time has to be made very large (seconds)
	int SynchCorrectionTimeFreqNoneFlag=2; //0: No correction; 1: frequency correction; 2: Time correction; 3: time and frequency correction
	bool SlowMemoryPermanentStorageFlag=false; // Variable when true they are stored in a file (slower due to writting and reading) ; otherwise it uses array memory to store qubits (much faster)
	bool ResetPeriodicallyTimerPRU1=true;// Avoiding interrupts
	// Semaphore
	unsigned long long int UnTrapSemaphoreValueMaxCounter=10000000;//MAx counter trying to acquire semaphore, then force release
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)
	std::atomic<bool> ManualSemaphore=false;
	std::atomic<bool> ManualSemaphoreExtra=false;
	std::thread threadRefSynch; // Process thread that executes requests/petitions without blocking
	long long int LostCounts=4; // For stoping and changing IEP counter. It has to do with jitter??? If not ajusted correctly, more jitter
	int ApproxInterruptTime=5000; // Typical time of interrupt time duration
	int NumSynchMeasAvgAux=51;//51; // Num averages to compute the time error. Better to be odd number.
	int ExtraNumSynchMeasAvgAux=31; // Averaging for computing current absolute time offset
	unsigned int NextSynchPRUcommand=11;// set initially to NextSynchPRUcorrection=0
	unsigned int NextSynchPRUcorrection=0;// Correction or sequence signal value
	//unsigned int OffsetSynchPRUBaseCorrection=262144;// Base value from where the synch offset is added or discounted to achieve periodic offset correction
	double PRUoffsetDriftErrorAbsAvgAux=0.0;
	//bool IEPtimerPRUreset=false;	
	// Relative error
	double PRUoffsetDriftError=0;
	double PRUoffsetDriftErrorArray[MaxNumPulses]={0};
	double PRUoffsetDriftErrorAvg=0.0;
	// Absolute corrected error
	double PRUoffsetDriftErrorAbs=0;
	double PRUoffsetDriftErrorAbsArray[MaxNumPulses]={0};
	double PRUoffsetDriftErrorAbsAvg=0.0;
	double PRUoffsetDriftErrorAbsAvgMax=65536.0; // Maximum absolute offset to correct for. It can be decreases if the handling interrupt time is improved (decreased). The longer the more correction but the longer it will wait to initiate sequence
	// Others
	double PRUoffsetDriftErrorIntegral=0;
	double PRUoffsetDriftErrorDerivative=0;
	double PRUoffsetDriftErrorApplied=0;
	double PRUoffsetDriftErrorAppliedRaw=0;
	double PRUcurrentTimerVal=0;
	double PRUcurrentTimerValLong=0;
	double PRUcurrentTimerValWrap=0;
	double PRUcurrentTimerValWrapLong=0;
	double PRUcurrentTimerValOldWrap=0;
	double PRUcurrentTimerValOldWrapLong=0;
	unsigned long long int iIterPRUcurrentTimerVal=0;
	unsigned long long int iIterPRUcurrentTimerValSynch=0;// Account for rounds entered
	unsigned long long int iIterPRUcurrentTimerValSynchLong=0;// Account for long rounds entered
	unsigned long long int iIterPRUcurrentTimerValPass=1;// Account for rounds that has not entered
	unsigned long long int iIterPRUcurrentTimerValPassLong=1;// Account for rounds that has not entered
	double EstimateSynch=1.0;
	double EstimateSynchAvg=1.0;
	double EstimateSynchArray[MaxNumPulses]={EstimateSynch};// They are not all set to the value, only the first one (a function in the declarator should be used to fill them in.
	// Time/synchronization management
	struct my_clock
	{
		using duration   = std::chrono::nanoseconds;
		using rep        = duration::rep;
		using period     = duration::period;
		using time_point = std::chrono::time_point<my_clock>;
	    static constexpr bool is_steady = false;// true, false.

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
	double SynchTrigPeriod=4096.0; //For slotted analysis. It has to match to the histogram analysis
	double MultFactorEffSynchPeriod=4.0; // When using 4 channels histogram, this value is 4.0; when using real signals this value should be 1.0 (also in QphysLayerAgent.h)
	unsigned long long int TimePRU1synchPeriod=100000000; // In nanoseconds// The faster the more corrections, and less time passed since last correction, but more averaging needed. Also, there is a limit on the lower limit to procees and handle interrupts. Also, the sorter the more error in the correct estimation, since there has not elapsed enough time to compute a tendency (it also happens with PRUdetCorrRelFreq() method whre a separation TagsSeparationDetRelFreq is inserted). The limit might be the error at each iteration, if the error becomes too small, then it cannot be corrected. Anyway, with a better hardware clock (more stable) the correctioons can be done more separated in time).
	unsigned long long int DistTimePRU1synchPeriod=10; // Number of passes with respect TimePRU1synchPeriod, in order to compute both the absolute time difference and the relative frequency difference
	unsigned long long int iepPRUtimerRange32bits=4294967296; //32 bits
	struct timespec requestWhileWait;
	struct timespec requestCoincidenceWhileWait;
	TimePoint TimePointClockCurrentSynchPRU1future=std::chrono::time_point<Clock>();// For synch purposes
	TimePoint TimePointClockSendCommandFinal=std::chrono::time_point<Clock>();// For synch purposes
	//TimePoint TimePointClockSendCommandInitial=std::chrono::time_point<Clock>();// For synch purposes
	//TimePoint TimePointClockPRUinitial=std::chrono::time_point<Clock>();// For absolute drift purposes. Not used
	//TimePoint TimePointClockSynchPRUinitial=std::chrono::time_point<Clock>();// For absolute drift purposes. Not used
	TimePoint TimePointClockSynchPRUfinal=std::chrono::time_point<Clock>();// For absolute drift purposes
	//TimePoint TimePointClockTagPRUinitial=std::chrono::time_point<Clock>();// For absolute drift purposes
	long double ldTimePointClockTagPRUDiff=0.0;
	long double ldTimePointClockTagPRUinitial=0.0;
	//TimePoint TimePointClockTagPRUfinal=std::chrono::time_point<Clock>();// For absolute drift purposes
	TimePoint QPLAFutureTimePoint=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld1=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld2=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld3=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld4=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld5=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld6=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointOld7=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	TimePoint QPLAFutureTimePointSleep=std::chrono::time_point<Clock>();// For matching trigger signals and timetagging
	//TimePoint TimePointClockTagPRUinitialOld=std::chrono::time_point<Clock>();// For absolute drift purposes. Not used
	//int duration_FinalInitialDriftAux=0;// For absolute drift purposes	
	//int duration_FinalInitialDriftAuxArray[MaxNumPulses]={0};// For absolute drift purposes
	//int duration_FinalInitialDriftAuxArrayAvg=0;// For absolute drift purposes
	double duration_FinalInitialCountAux=0.0;
	double duration_FinalInitialCountAuxArrayAvg=0.0;
	double duration_FinalInitialCountAuxArrayAvgInitial=0.0;
	//int duration_FinalInitialMeasTrigAux=0;
	int duration_FinalInitialMeasTrigAuxArray[MaxNumPulses]={0};
	int duration_FinalInitialMeasTrigAuxAvg=0;
	unsigned long long int TrigAuxIterCount=0;
	// Trigger Signal and Timetagging methods
	int SynchRem=0;
	long long int SignAuxInstantCorr=0;
	long long int InstantCorr=0.0;
	long long int ContCorr=0.0;	
	unsigned long TimeClockMarging=100;// In nanoseconds. If too large, it disastabilizes the timming performance. It has to be smaller than the SynchTrigPeriod
	unsigned long long int TimeClockMargingExtra=10*TimeClockMarging;// In nanoseconds
	unsigned long TimePRUcommandDelay=250000;// In nanoseconds. If too large, it disastabilizes the timming performance. Very important parameter!!! When duration_FinalInitialMeasTrigAuxAvg properly set then is around 4000
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
	int WaitTimeInterruptPRU0=7500000; //up to 20000000 with Simple TTG. In microseconds. Although the longer the more innacurraccy in the synch routine
	//TimePoint TimePointClockNowPRU0;
	//unsigned long long int TimeNow_time_as_countPRU0;	
	//TimePoint FutureTimePointPRU0;
	//unsigned long long int TimePointFuture_time_as_countPRU0;
	//bool CheckTimeFlagPRU0;
	//bool finPRU0;
	// PRU Signal
	unsigned int NumberRepetitionsSignal=65536;//8192// Sets the equivalent MTU (Maximum Transmission Unit) for quantum (together with the clock time) - it could be named Quantum MTU. The larger, the more stable the hardware clocks to not lose the periodic synchronization while emitting.
	unsigned int NumQuBitsPerRun=1964; // Really defined in GPIO.h. Max 1964 for 12 input pins. 2048 for 8 input pins. Given the shared PRU memory size (discounting a 0x200 offset)
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=7500000; // In microseconds. Signal generation
	//int WaitTimeToFutureTimePointPRU1=1000;// The internal PRU counter (as it is all programmed) can hold around 5s before overflowing. Hence, accounting for sending the command, it is reasonable to say that the timer should last 5s, not more otherwise the synch calculation error overflows as well and things go bad.
	//TimePoint TimePointClockNowPRU1;
	//unsigned long long int TimeNow_time_as_countPRU1;	
	//TimePoint FutureTimePointPRU1;
	//unsigned long long int TimePointFuture_time_as_countPRU1;
	//bool CheckTimeFlagPRU1;
	//bool finPRU1;
	// SHARED RAM to file dump
	int iIterDump;
	unsigned int NumSynchPulses=0;
	unsigned short* valpHolder;
	unsigned short* valpAuxHolder;
	unsigned int* CalpHolder; // 32 bits
	unsigned int* synchpHolder; // 32 bits
	unsigned short* valp; // 16 bits
	unsigned short* valpAux; // 16 bits
	unsigned int* synchp; // 32 bits
	unsigned int valCycleCountPRU=0; // 32 bits // Made relative to each acquisition run
	unsigned int valOverflowCycleCountPRU=0; // 32 bits
	unsigned int valOverflowCycleCountPRUold=0; // 32 bits
	int AboveThresoldCycleCountPRUCompValue=8;//Value adjusted experimentally when PRU clock goes aboe 0x80000000. Not used
	//unsigned int valIEPtimerFinalCounts; // 32 bits
	unsigned long long int extendedCounterPRU=0; // 64 bits
	unsigned long long int extendedCounterPRUholder=0; // 64 bits.
	unsigned long long int extendedCounterPRUholderOld=0; // 64 bits
	unsigned long long int extendedCounterPRUaux=0; // 64 bits
	//unsigned char val; // 8 bits
	unsigned short valBitsInterest=0; // 16 bits, 2 bytes
	unsigned int valSkewCounts=0;
	unsigned int valThresholdResetCounts=0;
	unsigned long long int auxUnskewingFactorResetCycle=0;
	unsigned int AfterCountsThreshold=0;
	// Memory storage
	int TotalCurrentNumRecords=0; ////Variable to hold the number of currently stored records in memory
	unsigned long long int TimeTaggsStored[MaxNumQuBitsMemStored]={0};
	unsigned short ChannelTagsStored[MaxNumQuBitsMemStored]={0};
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
	unsigned long long int ULLIEpochReOffset=344810000000000000;// Amount to remove to timetaggs so that thier numbers are not so high and difficult to handle by other agents (value adjusted August 2024)
	unsigned long long int OldLastTimeTagg=0;
	unsigned long long int TimeTaggsLast=0;
	unsigned long long int TimeTaggsLastStored=0;
	unsigned long long int TimeTaggsInit=0;
	unsigned long long int LastTimeTaggRef[1]={0};
	// Pulses compensation
	int NumSynchPulsesRed=0;
	unsigned long long int SynchPulsesTags[MaxNumPulses]={0};
	unsigned long long int SynchPulsesTagsUsed[MaxNumPulses]={0};
	double PeriodCountsPulseAdj=(((1.0/(double)(PulseFreq))*1e9)/((double)(PRUclockStepPeriodNanoseconds)));// Not used
	double AdjPulseSynchCoeff=1.0;
	double AdjPulseSynchCoeffAverage=1.0;
	double AccumulatedErrorDrift=0.0; // For retrieved relative frequency difference from protocol
	double AccumulatedErrorDriftAux=0.0;// For retrieved relative offset difference from protocol
	double AdjPulseSynchCoeffArray[MaxNumPulses]={0.0};
	bool QPLAFlagTestSynch=false;
	// Correct Qubits relative frequency difference due to the sender
	int TagsSeparationDetRelFreq=10; // Number of index separation to compute the slope of disadjustment in order to have accuraccy
	double SlopeDetTagsAuxArray[MaxNumQuBitsMemStored]={0.0}; // Array in order to do the computations
	// Information and status
	bool HardwareSynchStatus=false; // Turn to true when hardware synchronized with the PRU clock
	// Specific emission or detection quad group of channels
	int QuadEmitDetecSelecGPIO=7; // Initialization to all channels

public:	// Functions/Methods
	GPIO(int number); //constructor will export the pin	
	// PRU
	GPIO(); // initializates PRU operation
	int InitAgentProcess();
	int LOCAL_DDMinit();
	int DDRdumpdata(int iIterRunsAux);
	int DisablePRUs();
	int ReadTimeStamps(int iIterRunsAux,int QuadEmitDetecSelecAux, double SynchTrigPeriodAux,unsigned int NumQuBitsPerRunAux,double* FineSynchAdjValAux, unsigned long long int QPLAFutureTimePointNumber, bool FlagTestSynchAux);// Read the detected timestaps in four channels
	int SendTriggerSignals(int QuadEmitDetecSelecAux, double SynchTrigPeriodAux,unsigned int NumberRepetitionsSignalAux,double* FineSynchAdjValAux,unsigned long long int QPLAFutureTimePointNumber, bool FlagTestSynchAux); // Uses output pins to clock subsystems physically generating qubits or entangled qubits
	int SendTriggerSignalsSelfTest();//
	int SendEmulateQubits(); // Emulates sending 2 entangled qubits through the 8 output pins (each qubits needs 4 pins)
	int RetrieveNumStoredQuBits(unsigned long long int* LastTimeTaggRefAux, unsigned int* TotalCurrentNumRecordsQuadCh, unsigned long long int TimeTaggs[QuadNumChGroups][MaxNumQuBitsMemStored], unsigned short int ChannelTags[QuadNumChGroups][MaxNumQuBitsMemStored]); // Reads the fstream file to retrieve number of stored timetagged qubits
	int ClearStoredQuBits(); // Send the writting pointer back to the beggining - effectively clearing stored QuBits
	// Synchronization related
	int SetSynchDriftParams(double* AccumulatedErrorDriftParamsAux);// Method to update (or reset with 0s) the synchronization parameters of the long time drift (maybe updated periodically)
	bool GetHardwareSynchStatus();// To provide information to the above agent when asked
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
	struct timespec CoincidenceSetWhileWait();
	int PRUsignalTimerSynchJitterLessInterrupt();// Tries to avoid interrupt jitter (might not be completely absolute time// Periodic synchronizaton of the timer to control the generated signals
	int PIDcontrolerTimeJiterlessInterrupt();
	int PRUdetCorrRelFreq(unsigned int* TotalCurrentNumRecordsQuadCh, unsigned long long int TimeTaggs[QuadNumChGroups][MaxNumQuBitsMemStored], unsigned short int ChannelTags[QuadNumChGroups][MaxNumQuBitsMemStored]);// Correct the detections relative frequency difference of the sender as well as separate by quad channel groups
	// Data processing
	unsigned short packBits(unsigned short value);
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
