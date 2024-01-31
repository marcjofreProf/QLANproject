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
#define NumSocketsMax 2
#define NumBytesBufferICPMAX 4096 // Oversized to make sure that sockets do not get full
#define IPcharArrayLengthMAX 15

// Payload messages
#define NumBytesPayloadBuffer 1000

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
	char IPaddressesSockets[NumSocketsMax+2][IPcharArrayLengthMAX]; // IP address of the client/server host/node in the control/operation networks
	// IPaddressesSockets[0]: IP host attached ConNet
	// IPaddressesSockets[1]: IP host other OpNet
	// IPaddressesSockets[2]: IP node attached ConNet
	// IPaddressesSockets[3]: IP host attached OpNet
	char IPSocketsList[NumSocketsMax][IPcharArrayLengthMAX]; // IP address where the socket descriptors are pointing to	
	// IPSocketsList[0]: IP host attached ConNet
	// IPSocketsList[1]: Not used/IP node intermediate OpNet
	char SCmode[NumSocketsMax][NumBytesBufferICPMAX] = {0}; // Variable to know if the node instance is working as server or client to the other node

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
	std::atomic<int> valueSemaphore=1;// Start as 1 (open or acquireable)
	// Payload messages
	char PayloadReadBuffer[NumBytesPayloadBuffer]={0}; //Buffer to read payload messages
	char PayloadSendBuffer[NumBytesPayloadBuffer]={0}; //Buffer to send payload messages
	// Status info
	bool OtherNodeThereFlag=false; // To check if the other node is there
	bool InfoIPaddressesSocketsFlag=false;// To check if there is information for IPAddressesSockets
	
public: // Functions/Methods
	int InitAgentProcess(); // Initializer of the thread
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
	// Payload information parameters
	int SendParametersAgent();// The upper layer gets the information to be send
        int SetReadParametersAgent(char* ParamsCharArray);// The upper layer sets information from the other node
        int RetrieveIPSocketsHosts();
	int NegotiateInitialParamsNode();
	~QTLAN();  //destructor

private: // Functions/Methods
	static void SignalPIPEHandler(int s); // Handler for socket SIGPIPE signal error
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
	// Sempahore
	void acquire();
	void release();
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
	int countQintupleComas(char* ParamsCharArray);
	
};


} /* namespace nsQtransportLayerAgentN */

#endif /* QtransportLayerAgentN_H_ */
