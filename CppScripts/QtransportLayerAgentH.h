/* Author: Marc Jofre

Header declaration file for Quantum transport Layer Agent Host

*/
#ifndef QtransportLayerAgentH_H_
#define QtransportLayerAgentH_H_
// Threading
#include <thread>

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
	char IPaddressesSockets[2][15]; // IP address of the client/server host/node in the control/operation networks
	char* SCmode; // Variable to know if the host instance is working as server or client
	int socket_fdArray[2]; // socket descriptor, an integer (like a file-handle)
	int new_socketArray[2]; // socket between client and server, an integer. Created by the server.
	char ReadBuffer[1024] = { 0 };// Buffer to read ICP messages
	char SendBuffer[1024] = { 0 };// Buffer to send ICP messages	
	std::thread threadRef; // Process thread that executes requests/petitions without blocking

public: // Functions
	QTLAH(int numberSessions); //constructor
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
	int InitAgent(char* ParamsDescendingCharArray,char* ParamsAscendingCharArray); // Passing parameters from the upper Agent to initialize the current agent
	// Communication messages
	int SendMessageAgent(char* ParamsDescendingCharArray); // Passing message from the upper Agent to send message to specific host/node
	~QTLAH();  //destructor

private: //Functions
        // Management functions as client
	int ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets); // Open ICP socket 
	int ICPmanagementCloseClient(int socket_fd); // Close ICP socket
	// As server
	int ICPmanagementOpenServer(int& socket_fd,int& new_socket);
	int ICPmanagementCloseServer(int socket_fd,int new_socket);
	// As server or client
	int ICPmanagementRead(int socket_fd); // Read ICP socket
	int ICPmanagementSend(int new_socket); // Send ICP socket
	int InitiateICPconnections(); // Initiating sockets
        int StopICPconnections(); // Closing sockets
//	friend void* threadedPoll(void *value);
	static void* AgentProcessStaticEntryPoint(void* c); // Not used
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
	int ICPConnectionsCheckNewMessages(); // Check for new messages
};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
