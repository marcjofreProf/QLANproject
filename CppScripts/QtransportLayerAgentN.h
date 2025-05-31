/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum transport Layer Agent Node

*/
#ifndef QtransportLayerAgentN_H_
#define QtransportLayerAgentN_H_

// ICP connections
#define NumSocketsMax 1
#define NumBytesBufferICPMAX 4096 // Oversized to make sure that sockets do not get full
#define IPcharArrayLengthMAX 15
// Synchronization
#define NumCalcCenterMass 1//1//3 // Number of centers of mass to measure to compute the synchronization// With 3, it also computes hardware calibration of the detunnings

// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumHostConnection 5
//
#include<string>
#include<fstream>
// Threading
#include <thread>
// Semaphore
#include <atomic>
#include "QnetworkLayerAgent.h"
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQtransportLayerAgentN {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

	class QTLAN {
public: // Variables/Objects
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	};
	nsQnetworkLayerAgent::QNLA QNLAagent; // Instance of the below agent
	int ParamArgc=0; // Number of passed parameters
	int numberSessions=0;
	char IPaddressesSockets[2][IPcharArrayLengthMAX]; // IP address of the client/server host/node in the control/operation networks	
	// IPaddressesSockets[0]: IP host attached ConNet
	// IPaddressesSockets[1]: IP node ConNet
	char IPSocketsList[1][IPcharArrayLengthMAX]; // IP address where the socket descriptors are pointing to	
	// IPSocketsList[0]: IP host attached ConNet
	char SCmode[2][NumBytesBufferICPMAX] = {0}; // Variable to know if the node instance is working as server or client to the other node

private: // Variables/Objects		
	// Member Variables Such As Window Handle, Time Etc.,
	ApplicationState m_state;		
	int socket_fdArray[NumSocketsMax]; // socket descriptor, an integer (like a file-handle)
	int socket_SendUDPfdArray[NumSocketsMax]; // socket descriptor, an integer (like a file-handle), for sending in UDP
	int new_socketArray[NumSocketsMax]; // socket between client and server. Created by the server
	char ReadBuffer[NumBytesBufferICPMAX] = {0};// Buffer to read ICP messages
	char SendBuffer[NumBytesBufferICPMAX] = {0};// Buffer to send ICP messages
	bool ReadFlagWait=false;
	int socketReadIter = 0; // Variable to read each time a different socket
	// Semaphore
	unsigned long long int UnTrapSemaphoreValueMaxCounter=10000000;//MAx counter trying to acquire semaphore, then force release
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)
	// Payload messages
	char PayloadReadBuffer[NumBytesPayloadBuffer]={0}; //Buffer to read payload messages
	char PayloadSendBuffer[NumBytesPayloadBuffer]={0}; //Buffer to send payload messages
	// Status info
	bool BusyNode=false;
	bool OtherNodeThereFlag=false; // To check if the other node is there
	bool InfoIPaddressesSocketsFlag=false;// To check if there is information for IPAddressesSockets
	bool QPLASimulateEmitQuBitFlag=false;
	bool QPLASimulateReceiveQuBitFlag=false;
	bool GetSimulateNumStoredQubitsNodeFlag=false;// Flag to check if another process is already trying to retrieve the number of qubits
	char IPorgAux[IPcharArrayLengthMAX]={0};
	char IPdestAux[IPcharArrayLengthMAX]={0};
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
		if (clock_gettime(CLOCK_REALTIME, &ts))// CLOCK_REALTIME//CLOCK_TAI
			throw 1;
		using sec = std::chrono::seconds;
		return time_point{sec{ts.tv_sec}+duration{ts.tv_nsec}};
	}
};
	using Clock = my_clock;//
	// Variables to pass to below agents
	// QLLA agent
	char QLLAModeActivePassive[NumBytesPayloadBuffer] = {0};// "Active" or "Passive"
	char QPLLACurrentEmitReceiveIP[NumBytesBufferICPMAX]= {0}; // Specific current IP (maybe more than one) that will emit/receive now
	char QLLAIPaddresses[NumBytesBufferICPMAX] = {0}; // IP addresses to send TimePoint barrier; separated by "_"
	int QLLAnumReqQuBits=0;
	double QLLAFineSynchAdjVal[2]={0};// Adjust synch trig offset and frequency
	// Synchronization test frequencies and others
	int QLLANumRunsPerCenterMass=0;
	double QLLAFreqSynchNormValuesArray[NumCalcCenterMass]={0.0};//,-0.25,0.25}; // Normalized values of frequency testing// Relative frequency difference normalized
	bool GPIOnodeHardwareSynched=false;// Indicates if the node is hardware PRU synchronized.
	unsigned long long int iIterPeriodicTimerVal=0; // Variable to keep track of number of passes thorugh periodic checks
	unsigned long long int MaxiIterPeriodicTimerVal=2000; // Max number of passes so that it enters the periodic checks
	int QLLAQuadEmitDetecSelec=7; // Identifies the quad group channels to emitt or detect
	
public: // Functions/Methods
	int RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep);
	// Sempahore
	void acquire();
	void release();
	int SendKeepAliveHeartBeatsSockets();
	int InitAgentProcess(); // Initializer of the thread. Not used
	int InitiateBelowAgentsObjects();
	QTLAN(int numberSessions); //constructor
	// virtual ~Application(); // Default Okay - Use Virtual If Using Inheritance
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
        int InitiateICPconnections(int argc); // Initiating sockets
        int StopICPconnections(int argc); // Closing sockets
        int ICPConnectionsCheckNewMessages(int SockListenTimeusec); // Check for new messages
        int UpdateSocketsInformation(); // Update information to where the sockets are pointing to
        // Process and execute requests
        int ProcessNewMessage();
        int RegularCheckToPerform();
	// Payload information parameters
	int SendParametersAgent();// The upper layer gets the information to be send
        int SetReadParametersAgent(char* ParamsCharArray);// The upper layer sets information from the other node
	~QTLAN();  //destructor

private: // Functions/Methods
	static void SignalINTHandler(int s); // Handler for socket SIGPIPE signal error
	static void SignalPIPEHandler(int s); // Handler for socket SIGPIPE signal error	
	//static void SignalSegmentationFaultHandler(int s); // Handler for segmentation error
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions	
	// Managing ICP connections with sockets
	// Typically the Node will act as server to the upper host. If the node is in between, then it will act as server of the origin client node. Net 192.168.X.X or Net 10.0.0.X
	// With nodes in between hosts, the origin node will also act as client to the next node acting as server. Net 10.0.0.X	
	int SocketCheckForceShutDown(int socket_fd);// Force the socket to shutdown (whatever status but closed)
	int ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList); // Open ICP socket 
	int ICPmanagementCloseClient(int socket_fd); // Close ICP socket 
	// As server
	int ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList);
	int ICPmanagementCloseServer(int socket_fd,int new_socket);
	// As server or cleint
	int ICPmanagementRead(int socket_fd_conn,int SockListenTimeusec);
	int ICPmanagementSend(int socket_fd_conn,char* IPaddressesSockets);
	int ICPdiscoverSend(char* ParamsCharArray); // Discover the socket and send the message
	bool isSocketWritable(int sock);
	// REquests	
	int SendMessageAgent(char* ParamsDescendingCharArray); // Passing message from the Agent to send message to specific host/node
//	friend void* threadedPoll(void *value);
	// Payload information parameters
	int InitParametersAgent();// Client node have some parameters to adjust to the server node
	int SetSendParametersAgent(char* ParamsCharArray);// Node accumulates parameters for the other node
	int ReadParametersAgent(char* ParamsCharArray);// Node checks parameters from the other node
	int ProcessNewParameters();// Process the parameters received
	int countDoubleColons(char* ParamsCharArray);
	int countDoubleUnderscores(char* ParamsCharArray);
	int countUnderscores(char* ParamsCharArray);
	int countQintupleComas(char* ParamsCharArray);
	//
	int QPLASimulateEmitQuBit(double HistPeriodicityAuxAux);
	int QPLASimulateEmitSynchQuBit(double HistPeriodicityAuxAux,int iCenterMass,int iNumRunsPerCenterMass);
	int QPLASimulateReceiveQuBit(double HistPeriodicityAuxAux);
	int QPLASimulateReceiveSynchQuBit(char* CurrentReceiveHostIPaux, double HistPeriodicityAuxAux,int iCenterMass,int iNumRunsPerCenterMass);
	int GetSimulateNumStoredQubitsNode();
	int GetSimulateSynchParamsNode();
	
};


} /* namespace nsQtransportLayerAgentN */

#endif /* QtransportLayerAgentN_H_ */
