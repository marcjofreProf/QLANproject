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
#define PRUclockStepPeriodNanoseconds		4.9997375 // Very critical parameter experimentally assessed. PRU clock cycle time in nanoseconds. Specs says 5ns, but maybe more realistic is the 24 MHz clock is a bit higher and then multiplied by 8
#define PulseFreq	1000 // Hz// 

namespace exploringBB {

typedef int (*CallbackType)(int);
enum GPIO_DIRECTION{ INPUT, OUTPUT };
enum GPIO_VALUE{ LOW=0, HIGH=1 };
enum GPIO_EDGE{ NONE, RISING, FALLING, BOTH };

class GPIO {
//public: //Variables

private:// Variables
	bool ResetPeriodicallyTimerPRU1=true;// Disaster when used, due to all the interrupts handling time uncertainty
	// Semaphore
	std::atomic<bool> valueSemaphore=true;// Start as 1  (open or acquireable)
	std::atomic<bool> ManualSemaphore=false;
	std::thread threadRefSynch; // Process thread that executes requests/petitions without blocking
	long long int LostCounts=9; // For stoping and changing IEP counter. It has to do with jitter??? If not ajusted correctly, more jitter
	int NumSynchMeasAvgAux=61; // Num averages to compute the time error. Better to be odd number.
	double PRUoffsetDriftError=0;
	double PRUoffsetDriftErrorArray[MaxNumPulses]={0};
	double PRUoffsetDriftErrorAvg=0.0;
	double PRUoffsetDriftErrorLast=0;
	double PRUoffsetDriftErrorIntegral=0;
	double PRUoffsetDriftErrorIntegralOld=0;
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
	double SynchAdjconstant=1.0;// 
	double PIDconstantAdvancing=0.775;// Too close to 1.0 makes it unstable and too much correction
	double PIDconstantDelaying=PIDconstantAdvancing;// Too close to 1.0 makes it unstable and too much correction
	double PIDintegral=0.0;
	double PIDderiv=0.0;	
	// Time/synchronization management
	using Clock = std::chrono::system_clock;// Since we use a time sleep, it might make sense a system_clock//tai_clock, system_clock or steady_clock;
	using TimePoint = std::chrono::time_point<Clock>;
	unsigned long long int TimePRU1synchPeriod=800000000;// The faster the more corrections, and less time passed isnce last correction, but more averaging needed. Also, there is a limit on the lower limit to procees and handle interrupts.
	struct timespec requestWhileWait;
	TimePoint TimePointClockCurrentSynchPRU1future=std::chrono::time_point<Clock>();// For synch purposes
	TimePoint TimePointClockSendCommandFinal=std::chrono::time_point<Clock>();// For synch purposes
	TimePoint TimePointClockSendCommandInitial=std::chrono::time_point<Clock>();// For synch purposes
	unsigned long long int TimeClockMarging=100000;// In nanoseconds
	unsigned long long int TimeClockMargingExtra=50*TimeClockMarging;// In nanoseconds
	unsigned long long int TimeElpasedNow_time_as_count=0;
	// PRU
	static int mem_fd;
	static void *ddrMem, *sharedMem, *pru0dataMem, *pru1dataMem;
	static void *pru_int;       // Points to start of PRU memory.
	//static int chunk;
	static unsigned int *sharedMem_int,*pru0dataMem_int,*pru1dataMem_int;
	unsigned long long int valCarryOnCycleCountPRU=0; // 64 bits
	// PRU timetagger
	int retInterruptsPRU0;
	int WaitTimeInterruptPRU0=2500000; // In microseconds. 2.5 seconds because the clock can go up to 5 seconds, but has an active range of 2.5 seconds, then discounting some clock misadjustments. 2.5s is safe.
	//int WaitTimeToFutureTimePointPRU0=1000; // The internal PRU counter (as it is all programmed) can hold around 5s before overflowing. Hence, accounting for sending the command, it is reasonable to say that the timer should last 5s.
	//TimePoint TimePointClockNowPRU0;
	//unsigned long long int TimeNow_time_as_countPRU0;	
	//TimePoint FutureTimePointPRU0;
	//unsigned long long int TimePointFuture_time_as_countPRU0;
	//bool CheckTimeFlagPRU0;
	//bool finPRU0;
	// PRU Signal
	unsigned int NumberRepetitionsSignal=4096;// Sets the equivalent MTU (Maximum Transmission Unit) for quantum (together with the clock time) - it could be named Quantum MTU
	int retInterruptsPRU1;
	int WaitTimeInterruptPRU1=2500000; // In microseconds
	//int WaitTimeToFutureTimePointPRU1=1000;// The internal PRU counter (as it is all programmed) can hold around 5s before overflowing. Hence, accounting for sending the command, it is reasonable to say that the timer should last 5s.
	//TimePoint TimePointClockNowPRU1;
	//unsigned long long int TimeNow_time_as_countPRU1;	
	//TimePoint FutureTimePointPRU1;
	//unsigned long long int TimePointFuture_time_as_countPRU1;
	//bool CheckTimeFlagPRU1;
	//bool finPRU1;
	// SHARED RAM to file dump
	int iIterDump;
	unsigned int NumRecords=256; //2048; //Number of records per run. Max 2048. It is also defined in PRUassTaggDetScript.p and QphysLayerAgent.cpp
	unsigned int NumSynchPulses=0;
	unsigned char* valpHolder;
	unsigned char* valpAuxHolder;
	unsigned int* synchpHolder; // 32 bits
	unsigned char* valp; // 8 bits
	unsigned char* valpAux; // 8 bits
	unsigned int* synchp; // 32 bits
	unsigned int valCycleCountPRU=0; // 32 bits // Made relative to each acquition run
	unsigned int valOverflowCycleCountPRU=0; // 32 bits
	unsigned int valOverflowCycleCountPRUold=0; // 32 bits
	//unsigned int valIEPtimerFinalCounts; // 32 bits
	unsigned long long int extendedCounterPRU=0; // 64 bits
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
	// Pulses compensation
	int NumSynchPulsesRed=0;
	unsigned long long int SynchPulsesTags[MaxNumPulses]={0};
	unsigned long long int SynchPulsesTagsUsed[MaxNumPulses]={0};
	double PeriodCountsPulseAdj=(((1.0/(double)(PulseFreq))*1e9)/((double)(PRUclockStepPeriodNanoseconds)));
	double AdjPulseSynchCoeff=1.0;
	double OldLastAdjPulseSynchCoeff=1.0;
	double AdjPulseSynchCoeffAverage=1.0;
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
	int SendTriggerSignals(); // Uses output pins to clock subsystems physically generating qubits or entangled qubits
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

	virtual ~GPIO();  //destructor will unexport the pin

private: // Functions/Methods
	// Sempahore
	void acquire();
	void release();
	// PRU synchronization
	struct timespec SetWhileWait();
	int PRUsignalTimerSynch(); // Periodic synchronizaton of the timer to control the generated signals
	int PIDcontrolerTime();
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
	double DoubleMeanFilterSubArray(double* ArrayHolderAux,int MeanFilterFactor);
	int DoubleBubbleSort(double* arr,int MedianFilterFactor);
};

void* threadedPoll(void *value);
void* threadedToggle(void *value);

} /* namespace exploringBB */

#endif /* GPIO_H_ */
