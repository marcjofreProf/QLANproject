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
#define NumSocketsMax 2
#define NumBytesBufferICPMAX 1024
#define IPcharArrayLengthMAX 15
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
	char IPaddressesSockets[NumSocketsMax+2][IPcharArrayLengthMAX]; // IP address of the client/server host/node in the control/operation networks
	char SCmode[NumSocketsMax][NumBytesBufferICPMAX] = {0}; // Variable to know if the host instance is working as server or client
	int socket_fdArray[NumSocketsMax]; // socket descriptor, an integer (like a file-handle)
	int new_socketArray[NumSocketsMax]; // socket between client and server, an integer. Created by the server.
	char IPSocketsList[NumSocketsMax][IPcharArrayLengthMAX]; // IP address where the socket descriptors are pointing to
	char ReadBuffer[NumBytesBufferICPMAX] = {0};// Buffer to read ICP messages
	char SendBuffer[NumBytesBufferICPMAX] = {0};// Buffer to send ICP messages	
	int socketReadIter = 0; // Variable to read each time a different socket
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	// Semaphore
	std::atomic<int> valueSemaphore=1;// Start as 1 (open or acquireable)	
	int valueSemaphoreExpected = 1;

public: // Functions
	// Management
	QTLAH(int numberSessions,char* ParamsDescendingCharArray,char* ParamsAscendingCharArray); //constructor
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
	int RetrieveNumStoredQubitsNode(int* ParamsIntArray,int nIntarray); // Send to the upper layer agent how many qubits are stored
	~QTLAH();  //destructor

private: //Functions//Methods
	void SignalPIPEHandler(int s); // Handler for socket SIGPIPE signal error
	// Sempahore
	void acquire();
	void release();
	// Management of ICP communications
	int SocketCheckForceShutDown(int socket_fd);// Force the socket to shutdown (whatever status but closed)
        // Management functions as client
	int ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPSocketsList); // Open ICP socket 
	int ICPmanagementCloseClient(int socket_fd); // Close ICP socket
	// As server
	int ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPSocketsList);
	int ICPmanagementCloseServer(int socket_fd,int new_socket);
	// As server or client
	int ICPmanagementRead(int socket_fd_conn,int SockListenTimeusec); // Read ICP socket
	int ICPmanagementSend(int socket_fd_conn); // Send ICP socket
	int ICPdiscoverSend(char* ParamsCharArray); // Discover the socket and send the message
	int InitiateICPconnections(); // Initiating sockets
        int StopICPconnections(); // Closing sockets
//	friend void* threadedPoll(void *value);
	//static void* AgentProcessStaticEntryPoint(void* c); // Not used
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
	int ICPConnectionsCheckNewMessages(int SockListenTimeusec); // Check for new messages
	// Process and execute requests
	int ProcessNewMessage(); // main function perferming the required operatons to process the task
};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
