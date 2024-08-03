/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum transport Layer Agent Host

*/
#ifndef QtransportLayerAgentH_H_
#define QtransportLayerAgentH_H_
// ICP connections
#define NumBytesBufferICPMAX 4096 // Oversized to make sure that sockets do not get full
#define IPcharArrayLengthMAX 15
// Network hosts
#define NumConnectedHosts 2// Number of connected hosts (not counting this one)
// Synchronization
#define NumCalcCenterMass 3 // Number of centers of mass to measure to compute the synchronization
#define NumRunsPerCenterMass 6 // Minimum 2. In order to compute the difference. Better and even number because the computation is done between differences and a median so effectively using odd number of measurements
// Threading
#include <thread>
// Semaphore
#include <atomic>

//#include<string>
//#include<fstream>
//using std::string;
//using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQtransportLayerAgentH {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QTLAH {
public: // Variables/Objects
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };	    
	int numberSessions=0;
private: // Variables/Objects	
	ApplicationState m_state;
	char IPaddressesSockets[5][IPcharArrayLengthMAX]; // IP address of the client/server host/node in the control/operation networks
	// IPaddressesSockets[0]: IP node attached ConNet
	// IPaddressesSockets[1]: IP host attached ConNet
	// IPaddressesSockets[2]: IP host attached OpNet
	// IPaddressesSockets[3]: IP host other OpNet
	// IPaddressesSockets[4]: IP host other OpNet
	char SCmode[2][NumBytesBufferICPMAX] = {0}; // Variable to know if the host instance is working as server or client
	int socket_fdArray[3]; // socket descriptor, an integer (like a file-handle)
	int socket_SendUDPfdArray[3]; // socket descriptor, an integer (like a file-handle), for sending in UDP
	int new_socketArray[3]; // socket between client and server, an integer. Created by the server.
	char IPSocketsList[3][IPcharArrayLengthMAX]; // IP address where the socket descriptors are pointing to
	// IPSocketsList[0]: IP node attached ConNet
	// IPSocketsList[1]: IP host other OpNet
	char ReadBuffer[NumBytesBufferICPMAX] = {0};// Buffer to read ICP messages
	char SendBuffer[NumBytesBufferICPMAX] = {0};// Buffer to send ICP messages
	bool ReadFlagWait=false;	
	int socketReadIter = 0; // Variable to read each time a different socket
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	// Semaphore
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)	
	int valueSemaphoreExpected = 1;
	// Status
	bool InfoSimulateNumStoredQubitsNodeFlag=false;// Flag to account that there is informaiton on number Qubits in node
	int SimulateNumStoredQubitsNodeParamsIntArray[1]={0};// Array storing the Number Qubits stored in the node
	double TimeTaggsDetAnalytics[8]={0.0};// Array containing the timetaggs detections analytics (proceesses by the nodes)
	double TimeTaggsDetSynchParams[3]={0.0};// Array containing the retrieved synch params (proceesses by the nodes)
	int NumSockets=0;
	bool SimulateRetrieveNumStoredQubitsNodeFlag=false; // Flag to only allow one process for ask to retrieve QuBits info
	// Time synchronization
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
	using Clock = my_clock;//
	// Synchronization parameters
	int InitialNetworkSynchPass=0; // Variable to control that at least two rounds of network synchronization are performed the very first time
	int numHolderOtherNodesSynchNetwork=0; // Variable to keep track if the other connected nodes have iterated thorugh the network synch
	bool CycleSynchNetworkDone=false; // Variable to keep track if this host has been network synchronized in this cycle round of network sincronizxations
	int numHolderOtherNodesSendSynchQubits=0; // Variable to count number of consecutive SendSynchQubits request
	unsigned long long int iIterNetworkSynchcurrentTimerVal=0;// Variable to count how many time has passed since last network synchronization
	unsigned long long int MaxiIterNetworkSynchcurrentTimerVal=3600; // Counter value to reset network synchronization, which is actually multiplied by the variable MaxiIterPeriodicTimerVal
	bool GPIOnodeHardwareSynched=false;// VAriable to know the hardware synch status of the node below. Actually, do not let many operations and controls to happen until this variable is set to true.
	bool GPIOnodeNetworkSynched=false;// VAriable to know the network synch status of the node below. Periodically turn to false, to proceed again with network synchronization
	double QTLAHFreqSynchNormValuesArray[NumCalcCenterMass]={0.0,0.35,0.70}; // Normalized values of frequency testing// Relative frequency difference normalized
	// Scheduler status
	unsigned long long int iIterPeriodicTimerVal=0; // Variable to keep track of number of passes thorugh periodic checks
	unsigned long long int MaxiIterPeriodicTimerVal=1000; // Max number of passes so that it enters the periodic checks
	int IterHostsActiveActionsFreeStatus=0;// 0: Not asked this question; 1: Question asked; 2: All questions received; -1: Abort and reset all
	int ReWaitsAnswersHostsActiveActionsFree=0;// Counter to know how many times waited to have all the responses
	int MaxReWaitsAnswersHostsActiveActionsFree=30; // Maximum number of times to wait to collect all answers
	bool HostsActiveActionsFree[1+NumConnectedHosts]={true}; // Indicate if the hosts are currently free to perform active actions. Index 0 is the host itself, the other indexes are the other remote hosts in the order of IPaddressesSockets starting from position 2
	int NumAnswersOtherHostsActiveActionsFree=0;// Counter of the number of answers from other hosts proceessed
	char InfoRemoteHostActiveActions[2][IPcharArrayLengthMAX]={"\0","\0"}; // Two parameters indicating current active blocking host and status
	bool AchievedAttentionParticularHosts=false;// Indicates that we have got the attenation of the hosts

public: // Functions
	// Management
	QTLAH(int numberSessions,char* ParamsDescendingCharArray,char* ParamsAscendingCharArray); //constructor
	int SendKeepAliveHeartBeatsSockets();
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
	// Process Requests and Petitions
	int InitAgentProcess(); // Initializer of the thread
	// Requests. They have to be in semaphore structure to avoid collisions between main and thread
	int SendMessageAgent(char* ParamsDescendingCharArray); // Passing message from the upper Agent to send message to specific host/node	
	int SimulateRetrieveNumStoredQubitsNode(char* IPhostReplyOpNet,char* IPhostRequestOpNet, int* ParamsIntArray,int nIntarray,double* ParamsDoubleArray,int nDoublearray); // Send to the upper layer agent how many qubits are stored, and some statistics of the detections
	int SimulateRetrieveSynchParamsNode(char* IPhostReplyOpNet,char* IPhostRequestOpNet,double* ParamsDoubleArray,int nDoublearray); // Send to the upper layer agent how synch retrieved parameters stored in the below node
	// Task scheduler
	int WaitUntilActiveActionFreePreLock(char* ParamsCharArray, int nChararray);
	int UnBlockActiveActionFreePreLock(char* ParamsCharArray, int nChararray);
	// Destructor
	~QTLAH();  //destructor

private: //Functions//Methods
	//static void SignalINTHandler(int s); // Handler for socket SIGPIPE signal error
	//static void SignalPIPEHandler(int s); // Handler for socket SIGPIPE signal error
	//static void SignalSegmentationFaultHandler(int s); // Handler for segmentation error
	int RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep);
	// Sempahore
	void acquire();
	void release();
	// Management of ICP communications
	int SocketCheckForceShutDown(int socket_fd);// Force the socket to shutdown (whatever status but closed)
        // Management functions as client
	int ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList); // Open ICP socket 
	int ICPmanagementCloseClient(int socket_fd); // Close ICP socket
	// As server
	int ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList);
	int ICPmanagementCloseServer(int socket_fd,int new_socket);
	// As server or client
	int ICPmanagementRead(int socket_fd_conn,int SockListenTimeusec); // Read ICP socket
	int ICPmanagementSend(int socket_fd_conn,char* IPaddressesSockets); // Send ICP socket
	int ICPdiscoverSend(char* ParamsCharArray); // Discover the socket and send the message
	int InitiateICPconnections(); // Initiating sockets
        int StopICPconnections(); // Closing sockets
        bool isSocketWritable(int sock);
//	friend void* threadedPoll(void *value);
	//static void* AgentProcessStaticEntryPoint(void* c); // Not used
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
	int ICPConnectionsCheckNewMessages(int SockListenTimeusec); // Check for new messages
	// Process and execute requests
	int ProcessNewMessage(); // main function perferming the required operatons to process the task
	int countQintupleComas(char* ParamsCharArray);
	int countColons(char* ParamsCharArray);
	int countDoubleColons(char* ParamsCharArray);
	int countDoubleUnderscores(char* ParamsCharArray);
	int countQuadrupleUnderscores(char* ParamsCharArray);
	// Synchronization network related
	int RegularCheckToPerform(); // Sort of a scheduler
	int PeriodicRequestSynchsHost();// Executes when commanded the mechanisms for synchronizing the network
	// Host scheduler
	int WaitUntilActiveActionFree(char* ParamsCharArray, int nChararray);
	int UnBlockActiveActionFree(char* ParamsCharArray, int nChararray);
	int SequencerAreYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray); // Sequencer of the different steps to ask for availability to other hosts
	int SendAreYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray);// Ask all other hosts if free, while setting this host as not free
	int AcumulateAnswersYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray);// Accumulates the answers from the requested hosts
	bool CheckReceivedAnswersYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray);// If all host of interest free, return true, otherwise return false
	int UnBlockYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray);// IUnblock the previously block hosts

};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
