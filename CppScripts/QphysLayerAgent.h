/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum physical Layer Agent

*/
#ifndef QphysLayerAgent_H_
#define QphysLayerAgent_H_

#include "./BBBhw/GPIO.h"

#define LinkNumberMAX 2
// Payload messages
#define NumBytesPayloadBuffer 1000
#define IPcharArrayLengthMAX 15
#define NumHostConnection 5
// ICP connections
#define NumBytesBufferICPMAX 4096 // Oversized to make sure that sockets do not get full
//Qubits
#define NumQubitsMemoryBuffer 1964// In multiples of NumQuBitsPerRun (e.g., 1964, 3928, ...). Equivalent to received MTU (Maximum Transmission Unit) - should be in link layer - could be named received Quantum MTU
#define NumQuBitsPerRun 1964 // Really defined in GPIO.h. Max 1964 for 12 input pins. 2048 for 8 input pins. Given the shared PRU memory size (discounting a 0x200 offset)
// Synchronization
#define HistPeriodicityAux 4096 //Period in PRU counts of the synch period/histogram
#define NumCalcCenterMass 3 // Number of centers of mass to measure to compute the synchronization
#define NumRunsPerCenterMass 6 // Minimum 2. In order to compute the difference. Better and even number because the computation is done between differences and a median so effectively using odd number of measurements
// String operations
#include<string>
#include<fstream>
// Threading
#include <thread>
// Semaphore
#include <atomic>
// Time/synchronization management
#include <chrono>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

//using namespace exploringBB; // API to easily use GPIO in c++. No because it will confuse variables (like semaphore acquire)

namespace nsQphysLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QPLA {
private: //Variables/Instances		
	int numberLinks=0;// Number of full duplex links directly connected to this physical quantum node
	unsigned long long int RawTimeTaggs[NumQubitsMemoryBuffer]={0}; // Timetaggs of the detections raw
	unsigned short RawChannelTags[NumQubitsMemoryBuffer]={0}; // Detection channels of the timetaggs raw
	unsigned long long int TimeTaggs[NumQubitsMemoryBuffer]={0}; // Timetaggs of the detections
	unsigned short ChannelTags[NumQubitsMemoryBuffer]={0}; // Detection channels of the timetaggs
        //int EmitLinkNumberArray[LinkNumberMAX]={60}; // Array indicating the GPIO numbers identifying the emit pins
        //int ReceiveLinkNumberArray[LinkNumberMAX]={48}; // Array indicating the GPIO numbers identifying the receive pins
        float QuBitsPerSecondVelocity[LinkNumberMAX]={1000000.0}; // Array indicating the qubits per second velocity of each emit/receive pair 
        //int QuBitsNanoSecPeriodInt[LinkNumberMAX]={1000000};
        //int QuBitsNanoSecHalfPeriodInt[LinkNumberMAX]={500000};
        //int QuBitsNanoSecQuarterPeriodInt[LinkNumberMAX]={250000};// Not exacte quarter period since it should be 2.5
        // Semaphore
	std::atomic<bool> valueSemaphore=true;// Start as 1  (open or acquireable)
	// Payload messages
	char PayloadReadBuffer[NumBytesPayloadBuffer]={0}; //Buffer to read payload messages
	char PayloadSendBuffer[NumBytesPayloadBuffer]={0}; //Buffer to send payload messages
	// GPIO
	//GPIO* PRUGPIO; // Object for handling PRU
	//GPIO* inGPIO; // Slow (not used) Object for reading Qubits
	//GPIO* outGPIO; // Slow (not used) Object for sending Qubits
	// Time/synchronization management
	struct my_clock
	{
	    using duration   = std::chrono::nanoseconds;
	    using rep        = duration::rep;
	    using period     = duration::period;
	    using time_point = std::chrono::time_point<my_clock>;
	    static constexpr bool is_steady = false;// true, false

	    static time_point now()
	    {
		timespec ts;
		if (clock_gettime(CLOCK_TAI, &ts))// CLOCK_REALTIME//CLOCK_TAI
		    throw 1;
		using sec = std::chrono::seconds;
		return time_point{sec{ts.tv_sec}+duration{ts.tv_nsec}};
	    }
	};
	using Clock = my_clock;//using Clock = std::chrono::system_clock;//system_clock;steady_clock;high_resolution_clock. We need a wathc to wall synchronize starting of signal and measurement, that is why we use system_clock (if we wanted a chrono we would use steady_clock)
	using TimePoint = std::chrono::time_point<Clock>;
	TimePoint FutureTimePoint=std::chrono::time_point<Clock>();// could be milliseconds, microseconds or others, but it has to be consistent everywhere
	TimePoint OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// could be milliseconds, microseconds or others, but it has to be consistent everywhere
	unsigned long long int TimeClockMarging=0;//100;// In nanoseconds. So that we can do a busy wait to be more accurate and responsive. If too large, it disastabilizes the timming performance
	unsigned long long int TimePointMarginGPIOTrigTagQubits=500000; // Margin so that Trig signals and timestamps in GPI always coincide
	// Private threads
	bool RunThreadSimulateEmitQuBitFlag=true;
	bool RunThreadSimulateReceiveQuBitFlag=true;
	bool RunThreadAcquireSimulateNumStoredQubitsNode=true;
	// Periodic signal histogram analysis
	unsigned long long int OldLastTimeTagg=0;
	// Above agents passed values to this agent
	char ModeActivePassive[NumBytesPayloadBuffer] = {0};// "Active" or "Passive"
	char IPaddressesTimePointBarrier[NumBytesBufferICPMAX] = {0};// List of IP addresses separated by "_" to send the Time Point BArrier to
	int numReqQuBits=0;
	double FineSynchAdjVal[2]={0};// Adjust synch trig offset and frequency
	// Automatic calculation of synchronization
	unsigned long long int SynchFirstTagsArray[NumCalcCenterMass][NumRunsPerCenterMass]; // To store the first tags (averaged if needed for all the tags in the run
	double SynchHistCenterMassArray[NumCalcCenterMass]={0.0,0.0,0.0}; // Array containing the needed center of mass for the histograms of the synchronization
	double SynchCalcValuesArray[NumCalcCenterMass]={0.0,0.0,0.0}; // Computed values for achieving synchronization protocol
	double SynchCalcValuesAbsArray[NumCalcCenterMass]={0.0,0.0,0.0}; // Computed absolute values for achieving synchronization protocol, informative
	double FreqSynchNormValuesArray[NumCalcCenterMass]={0.0,0.35,0.70}; // Normalized values of frequency testing
	double adjFreqSynchNormRatiosArray[NumCalcCenterMass]={1.0,1.0,1.0}; // adjusting Normalized ratios of frequency testing
	double GPIOHardwareSynched=false; // Variable to monitor the hardware synch status of the GPIO process
	char CurrentEmitIP[NumBytesBufferICPMAX]={0}; // Current IP (maybe more than one) identifier who will emit qubits
	int CurrentNumIdentifiedEmitIP=0; // Variable to keep track of the number of identified IPs emitting to this node
	char LinkIdentificationArray[LinkNumberMAX][IPcharArrayLengthMAX]={0}; // To track details of each specific link
	bool ApplyProcQubitsSmallTimeOffsetContinuousCorrection=true; // Since we know that (after correcting for relative frequency difference and time offset) the tags should coincide with the initial value of the periodicity where the signals are sent
	unsigned long long int SmallOffsetDriftPerLink[LinkNumberMAX]={0,0}; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
	// Filtering qubits
	bool ApplyRawQubitFilteringFlag=false;// Variable to select or unselect the filtering of raw qubits
	long long int FilteringAcceptWindowSize=200; // Equivalent to around 3 times the time jitter
        
public: // Variables/Instances
	exploringBB::GPIO PRUGPIO;
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };
	ApplicationState m_state;
	int SimulateNumStoredQubitsNode[LinkNumberMAX]={0}; // Array indicating the number of stored qubits
	int SimulateQuBitValueArray[NumQubitsMemoryBuffer]={0};
	char SCmode[1][NumBytesPayloadBuffer] = {0}; // Variable to know if the node instance is working as server or client to the other node

public: // Functions/Methods
	QPLA(); //constructor
	int InitAgentProcess(); // Initializer of the thread
	// Payload information parameters
	int SendParametersAgent(char* ParamsCharArray);// The upper layer gets the information to be send
        int SetReadParametersAgent(char* ParamsCharArray);// The upper layer sets information from the other node
        // General Input and Output functions
	int SimulateEmitQuBit(char* ModeActivePassiveAux,char* IPaddressesAux,int numReqQuBitsAux,double* FineSynchAdjValAux);
	int SimulateEmitSynchQuBit(char* ModeActivePassiveAux,char* IPaddressesAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass);
	int SimulateReceiveQuBit(char* ModeActivePassiveAux,char* CurrentEmitIPAux,char* IPaddressesAux,int numReqQuBitsAux);
	int SimulateReceiveSynchQuBit(char* ModeActivePassiveAux,char* CurrentEmitIPAux,char* IPaddressesAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,int iCenterMass,int iNumRunsPerCenterMass);
	int GetSimulateNumStoredQubitsNode(double* TimeTaggsDetAnalytics);
	int GetSimulateSynchParamsNode(double* TimeTaggsDetSynchParams);
	bool GetGPIOHardwareSynchedNode();
	~QPLA();  //destructor

private: // Functions/Methods
	int RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep);
	// Sempahore
	void acquire();
	void release();
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
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
        // Payload information parameters
        int InitParametersAgent();// Client node have some parameters to adjust to the server node
        int RegularCheckToPerform(); // Some check to do every now and then     
	int SetSendParametersAgent(char* ParamsCharArray);// Node accumulates parameters for the other node
	int ReadParametersAgent();// Node checks parameters from the other node
	int NegotiateInitialParamsNode();
	int ProcessNewParameters();// Process the parameters
	int countDoubleColons(char* ParamsCharArray);
	int countDoubleUnderscores(char* ParamsCharArray);
	int countUnderscores(char* ParamsCharArray);
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	// Particular process threads
	bool isPotentialIpAddressStructure(const char* ipAddress);
	int ThreadSimulateEmitQuBit();
	int ThreadSimulateReceiveQubit();	
	struct timespec SetFutureTimePointOtherNode();
	struct timespec GetFutureTimePointOtherNode();
	// Synchronization primitives
	int HistCalcPeriodTimeTags(int iCenterMass,int iNumRunsPerCenterMass); // Calculate the histogram center given a period and a list of timetaggs
	double DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor);
	int DoubleBubbleSort(double* arr,int MedianFilterFactor);
	// QuBits Filtering methods
	int LinearRegressionQuBitFilter();// Try to filter signal from dark counts (noise) using a linear estimation
};


} /* namespace nsQphysLayerAgent */

#endif /* QphysLayerAgent_H_ */
