/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2025
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
#define MaxNumQuBitsPerRun 1964 // Really defined in GPIO.h. Max 1964 for 12 input pins. 2048 for 8 input pins. Given the shared PRU memory size (discounting a 0x200 offset)
#define NumQubitsMemoryBuffer 5*MaxNumQuBitsPerRun // Really defined in GPIO.h.// In multiples of NumQuBitsPerRun (e.g., 1964, 3928, ...). Maximum buffer size
// Synchronization
#define NumCalcCenterMass 1 // 1 // 3 // Number of centers of mass to measure to compute the synchronization. // With 3, it also computes hardware calibration of the detunnings
#define NumRunsPerCenterMass 4 // Minimum 2. In order to compute the difference. Better and even number because the computation is done between differences and a median so effectively using odd number of measurements
#define QuadNumChGroups 3 // There are three quad groups of emission channels and detection channels (which are treated independetly)
#define NumSmallOffsetDriftAux 3 // 5 // Better odd number Length of samples to filter the small time offset continuous correction for the PID. Integration length of the PID controller for correction

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
	int NumberRepetitionsSignal=32768;//8192// Sets the equivalent MTU (Maximum Transmission Unit) for quantum (together with the clock time) - it could be named Quantum MTU. The larger, the more stable the hardware clocks to not lose the periodic synchronization while emitting.
	int NumQuBitsPerRun=1964; // Really defined in GPIO.h. Max 1964 for 12 input pins. 2048 for 8 input pins. Given the shared PRU memory size (discounting a 0x200 offset)	
	long long int CoincidenceWindowPRU=16;//Size of the coincidence window for evaluating coincidences in PRU time, related to jitter and the fact that there are inter corrections for each quad channel
	int numberLinks=0;// Number of full duplex links directly connected to this physical quantum node
	unsigned long long int RawLastTimeTaggRef[1]={0}; // Timetaggs of the start of the detection in units of Time (not PRU time)
	unsigned long long int RawTimeTaggs[QuadNumChGroups][NumQubitsMemoryBuffer]={0}; // Timetaggs of the detections raw
	unsigned short int RawChannelTags[QuadNumChGroups][NumQubitsMemoryBuffer]={0}; // Detection channels of the timetaggs raw
	unsigned long long int TimeTaggs[QuadNumChGroups][NumQubitsMemoryBuffer]={0}; // Timetaggs of the detections
	unsigned short int ChannelTags[QuadNumChGroups][NumQubitsMemoryBuffer]={0}; // Detection channels of the timetaggs
	unsigned int RawTotalCurrentNumRecordsQuadCh[QuadNumChGroups]={0}; // Number of detections for quad channels groups
    //int EmitLinkNumberArray[LinkNumberMAX]={60}; // Array indicating the GPIO numbers identifying the emit pins
    //int ReceiveLinkNumberArray[LinkNumberMAX]={48}; // Array indicating the GPIO numbers identifying the receive pins
    float QuBitsPerSecondVelocity[LinkNumberMAX]={1000000.0}; // Array indicating the qubits per second velocity of each emit/receive pair 
    //int QuBitsNanoSecPeriodInt[LinkNumberMAX]={1000000};
    //int QuBitsNanoSecHalfPeriodInt[LinkNumberMAX]={500000};
    //int QuBitsNanoSecQuarterPeriodInt[LinkNumberMAX]={250000};// Not exacte quarter period since it should be 2.5
    // Semaphore
    unsigned long long int UnTrapSemaphoreValueMaxCounter=1000;//Max counter trying to acquire semaphore, then force release
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
		if (clock_gettime(CLOCK_REALTIME, &ts))// CLOCK_REALTIME//CLOCK_TAI
			throw 1;
		using sec = std::chrono::seconds;
		return time_point{sec{ts.tv_sec}+duration{ts.tv_nsec}};
	}
};
	using Clock = my_clock;//using Clock = std::chrono::system_clock;//system_clock;steady_clock;high_resolution_clock. We need a watch to wall synchronize starting of signal and measurement, that is why we use system_clock (if we wanted a chrono we would use steady_clock)
	using TimePoint = std::chrono::time_point<Clock>;
	TimePoint FutureTimePoint=std::chrono::time_point<Clock>();// could be milliseconds, microseconds or others, but it has to be consistent everywhere
	TimePoint OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// could be milliseconds, microseconds or others, but it has to be consistent everywhere
	unsigned long long int TimeClockMarging=0;//100;// In nanoseconds. So that we can do a busy wait to be more accurate and responsive. If too large, it disastabilizes the timming performance
	// Private threads
	bool RunThreadSimulateEmitQuBitFlag=true;
	bool RunThreadSimulateReceiveQuBitFlag=true;
	bool RunThreadAcquireSimulateNumStoredQubitsNode=true;
	bool FirstQPLACalcStats=true; // Data analytics about timetags are referenced to common tag for easiness to transfer the data to the application layer. How it is done, different nodes are comparable
	long long int FirstQPLAtimeTagNorm=0;// Normalization value of time taggs for the Stats
	// Periodic signal histogram analysis
	unsigned long long int OldLastTimeTagg=0;
	// Above agents passed values to this agent
	char ModeActivePassive[NumBytesPayloadBuffer] = {0};// "Active" or "Passive"
	char CurrentHostIP[NumBytesPayloadBuffer]={0};// Identifies the host IP of the current node
	char IPaddressesTimePointBarrier[NumBytesBufferICPMAX] = {0};// List of IP addresses separated by "_" to send the Time Point Barrier to
	double FineSynchAdjVal[2]={0.0};// Adjust synch trig offset and frequency
	// Automatic calculation of synchronization
	bool FlagTestSynch=false;
	double MultFactorEffSynchPeriodQPLA=4.0; // When using 4 channels histogram, this value is 4.0; when using real signals this value should be 1.0 (also in GPIO.h)
	double HistPeriodicityAux=4096.0/4.0; //Value indicated by upper agents (and uploaded to GPIO)//Period in PRU counts of the synch period/histogram
	int CurrentSpecificLink=-1; // Identifies th eindex of the other emiter/receiver
	int CurrentSpecificLinkMultiple=-1; // Identifies the index of the other emiter/receiver links when multiple present
	char ListCombinationSpecificLink[2*((1LL<<LinkNumberMAX)-1)][NumBytesPayloadBuffer]; // Stores the list of detected links and combinations in an ordered way
	int numSpecificLinkmatches=0; // Identifies if multiple links used currently
	int CurrentSpecificLinkMultipleIndices[2*((1LL<<LinkNumberMAX)-1)]={0}; // Array containing the indices CurrentSpecificLink when involving multiple links. The different combinaion of links is 2*(2**(NumberLinks)-1); the minus 1 since the not using any link is not contemplated, and then all of it multiplied by 2 because sometimes we will further identify the link with the actual emitter/sender
	long long int SynchTimeTaggRef[NumCalcCenterMass][NumRunsPerCenterMass]; // Holder to store the starts of the detections
	long long int SynchFirstTagsArrayAux[NumQubitsMemoryBuffer]={0}; // Holder to perform median computing
	long long int SynchFirstTagsArray[NumCalcCenterMass][NumRunsPerCenterMass]; // To store the first tags (averaged if needed for all the tags in the run
	long long int SynchFirstTagsArrayOffsetCalc[NumRunsPerCenterMass]; // To momentally store the first iteration of the synch which has no extra relative frequency edifferencew
	double SynchHistCenterMassArray[NumCalcCenterMass]={0.0}; // Array containing the needed center of mass for the histograms of the synchronization
	double SynchCalcValuesArray[3]={0.0,0.0,0.0}; // Computed values for achieving synchronization protocol
	double SynchAdjRelFreqCalcValuesArray[2*((1LL<<LinkNumberMAX)-1)][3]={1.0}; // Computed hardware values for adjusting the application of the computed rel.freq. correction. The order is, 0 index is 0 rel. freq. correction (so 1.0), adjustment value for negative correction for index 1 and adjustment value for positive correction for index 2.
	double FreqSynchNormValuesArray[NumCalcCenterMass]={0.0};//,-0.25,0.25}; // Updated from QPTLH agent. Normalized values of frequency testing which are applied for calibration
	double SynchNetAdj[2*((1LL<<LinkNumberMAX)-1)]={1.0};
	double SynchNetworkParamsLink[LinkNumberMAX][3]={0.0}; // Stores the synchronizatoin parameters corrections to apply depending on the node to whom receive or send. Zerod at the begining
	double originalSynchNetworkParamsLink[LinkNumberMAX][3]={0.0}; // Stores the synchronizatoin parameters corrections to apply depending on the node to whom receive or send. Zerod at the begining
	int QuadChannelParamsLink[LinkNumberMAX]={0};// Identifies the independently associated quad cahnnel number
	double SynchNetworkParamsLinkOther[LinkNumberMAX][3]={0.0}; // Stores the synchronizatoin parameters corrections to apply depending on the node to whom receive or send from the other nodes. Zeroed at the begining
	double CurrentSynchNetworkParamsLink[3]={0.0}; //Stores currently the network synch values of interest given the link in use for reception
	double CurrentExtraSynchNetworkParamsLink[3]={0.0}; //Stores currently the network synch values of interest given the link in use for emission
	double GPIOHardwareSynched=false; // Variable to monitor the hardware synch status of the GPIO process
	char CurrentEmitReceiveIP[NumBytesBufferICPMAX]={0}; // Current IP (maybe more than one) identifier who will emit (for receiver function) or receive (for emit function) qubits
	int CurrentNumIdentifiedEmitReceiveIP=0; // Variable to keep track of the number of identified IPs emitting/receiving to this node
	int CurrentNumIdentifiedMultipleIP=0; // Variable to keep track of the number of identified multiple links to this node
	char LinkIdentificationArray[LinkNumberMAX][IPcharArrayLengthMAX]={0}; // To track details of each specific link
	bool ApplyProcQubitsSmallTimeOffsetContinuousCorrection=true; // Since we know that (after correcting for relative frequency difference and time offset) the tags should coincide with the initial value of the periodicity where the signals are sent
	long long int SmallOffsetDriftAuxArray[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)][NumSmallOffsetDriftAux]={0}; // Array to filter the SmallOffsetDriftAux
	int IterSmallOffsetDriftAuxArray[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)]={0}; // Array storing the index of the new value
	// the following arrays are initialized to zero in the Agent creator. PID system develop
	bool ApplyPIDOffsetContinuousCorrection=true; // Correct at the transmitter the instantaneous offset retrieved
	double SplitEmitReceiverSmallOffsetDriftPerLink=0.25; // Splitting ratio between the effort of the emitter and receiver of constantly updating the synch values. The closer to 0 the more aggresive (more correction from the transmitter)
	double EffectiveSplitEmitReceiverSmallOffsetDriftPerLink=SplitEmitReceiverSmallOffsetDriftPerLink; // Effective (if correction above certain threshold) Splitting ratio between the effort of the emitter and receiver of constantly updating the synch values. The closer to 0 the more aggresive (more correction from the transmitter)
	double SplitEmitReceiverSmallOffsetDriftPerLinkP=0.65; // Proportional values for the PID
	double SplitEmitReceiverSmallOffsetDriftPerLinkI=0.05; // Integral values for the PID
	double SplitEmitReceiverSmallOffsetDriftPerLinkD=0.01; // Derivative value for the PID
	long long int SmallOffsetDriftPerLink[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)]={0}; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
	long long int SmallOffsetDriftPerLinkError[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)]={0}; // Integral value of PID. Idnetified each link and the accumulated error for the PID
	long long int oldSmallOffsetDriftPerLink[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)]={0}; // Old valuesIdentified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
	long long int ReferencePointSmallOffsetDriftPerLink[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)]={0}; // Identified by each link, annotate the first time offset that all other acquisitions should match to, so an offset with respect the SignalPeriod histogram
	// Filtering qubits
	bool NonInitialReferencePointSmallOffsetDriftPerLink[QuadNumChGroups][2*((1LL<<LinkNumberMAX)-1)]={false}; // Identified by each link, annotate if the first capture has been done and hence the initial ReferencePoint has been stored
	// Filtering qubits
	bool ApplyRawQubitFilteringFlag=false;// Variable to select (true) or unselect (false) the filtering of raw qubits thorugh LinearRegressionQuBitFilter function. For the time being, it is not applied in the synchronization procedure.
	long long int FilteringAcceptWindowSize=64; // Better power of 2!  Equivalent to around 3 times the time jitter (when physically synch). In PRU time. It should be dependent on the period of the signal
	double SynchCalcValuesFreqThresh=5e-7; //Threshold value to not apply relative frequency difference
	bool UseAllTagsForEstimation=true; // When false, use only the first tag (not resilent because it could be a remaining noise tag), when true it uses all tags of the run
	//int iCenterMassAuxiliarTest=0;
	//int iNumRunsPerCenterMassAuxiliarTest=0;
	// Selector of mission or detection quadruples or group. 0=000b no channel selected (actually it is not a valid option), 1=001b emission or detection of the first lower group of 4 channels, 2=010b emission or detection of the second lower group of 4 channels, 3=011b emission or detection of the first and second group of lower group of 4 channels....7=111b emission or detection of the third, second and first lower groups of 4 channels (all channels). This value is updated thorugh upper layer agents for each emission or detection
	int QuadEmitDetecSelec=7; // Initialization to all channels
	int SpecificQuadChEmt=-1; //[0,2]; -1 indicates none in particular selected. Identifies the current single quad group channel emitter (e.g., for synchronization purposes)
	int SpecificQuadChDet=-1; //[0,2]; -1 indicates none in particular selected. Identifies the current single quad group channel detection (e.g., for synchronization purposes)

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
    int SimulateEmitQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux, int QuadEmitDetecSelecAux);
    int SimulateEmitSynchQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass, int QuadEmitDetecSelecAux);
    int SimulateReceiveQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux, int QuadEmitDetecSelecAux);
    int SimulateReceiveSynchQuBit(char* ModeActivePassiveAux,char* CurrentReceiveHostIPaux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass, int QuadEmitDetecSelecAux);
    int GetSimulateNumStoredQubitsNode(double* TimeTaggsDetAnalytics);
    int GetSimulateSynchParamsNode(double* TimeTaggsDetSynchParams);
    bool GetGPIOHardwareSynchedNode();
	~QPLA();  //destructor

private: // Functions/Methods
	int RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep);
	long long int BitPositionChannelTags(unsigned long long int ChannelTagsPosAux);
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
	int PurgeExtraordinaryTimePointsNodes();
	int makeEvenInt(int xAux);
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	// Particular process threads
	bool isPotentialIpAddressStructure(const char* ipAddress);
	int ThreadSimulateEmitQuBit();
	int ThreadSimulateReceiveQubit();	
	struct timespec SetFutureTimePointOtherNode();
	struct timespec GetFutureTimePointOtherNode();
	// Synchronization primitives
	unsigned long long int IPtoNumber(char* IPaddressAux);
	int SetSynchParamsOtherNode();// Tell the other nodes about the synchronization information calculated
	int RetrieveOtherEmiterReceiverMethod();// Stores and retrieves the current other emiter receiver
	int HistCalcPeriodTimeTags(char* CurrentReceiveHostIPaux, int iCenterMass,int iNumRunsPerCenterMass); // Calculate the histogram center given a period and a list of timetaggs
	int SmallDriftContinuousCorrection(char* CurrentEmitReceiveHostIPaux);// Methods to keep track of the small offset correction at each measurement (but not in the network synch)
	double DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor);
	double DoubleMeanFilterSubArray(double* ArrayHolderAux,int MeanFilterFactor);
	int DoubleBubbleSort(double* arr,int MedianFilterFactor);
	unsigned long long int ULLIMedianFilterSubArray(unsigned long long int* ArrayHolderAux,int MedianFilterFactor);
	int ULLIBubbleSort(unsigned long long int* arr,int MedianFilterFactor);
	long long int LLIMeanFilterSubArray(long long int* ArrayHolderAux,int MeanFilterFactor);
	long long int LLIMedianFilterSubArray(long long int* ArrayHolderAux,int MedianFilterFactor);
	int LLIBubbleSort(long long int* arr,int MedianFilterFactor);
	// QuBits Filtering methods
	int LinearRegressionQuBitFilter();// Try to filter signal from dark counts (noise) using a linear estimation
};


} /* namespace nsQphysLayerAgent */

#endif /* QphysLayerAgent_H_ */
