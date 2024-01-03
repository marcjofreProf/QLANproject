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
#define NumBytesBufferICPMAX 1024
#define IPcharArrayLengthMAX 15

#include<string>
#include<fstream>
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
	int ParamArgc=0; // Number of passed parameters
	int numberSessions=0;

private: // Variables/Objects	
	// Member Variables Such As Window Handle, Time Etc.,
	ApplicationState m_state;	
	char IPaddressesSockets[NumSocketsMax][IPcharArrayLengthMAX]; // IP address of the client/server host/node in the control/operation networks
	char IPSocketsList[NumSocketsMax][IPcharArrayLengthMAX]; // IP address where the socket descriptors are pointing to
	char SCmode[NumSocketsMax][NumBytesBufferICPMAX] = { 0 }; // Variable to know if the host instance is working as server or client
	int socket_fdArray[NumSocketsMax]; // socket descriptor, an integer (like a file-handle)
	int new_socketArray[NumSocketsMax]; // socket between client and server. Created by the server
	char ReadBuffer[NumBytesBufferICPMAX] = { 0 };// Buffer to read ICP messages
	char SendBuffer[NumBytesBufferICPMAX] = { 0 };// Buffer to send ICP messages
	int socketReadIter = 0; // Variable to read each time a different socket
	
public: // Functions
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
        int ICPConnectionsCheckNewMessages(); // Check for new messages
        int UpdateSocketsInformation(); // Update information to where the sockets are pointing to
	~QTLAN();  //destructor

private: // Functions
	// Managing ICP connections with sockets
	// Typically the Node will act as server to the upper host. If the node is in between, then it will act as server of the origin client node. Net 192.168.X.X or Net 10.0.0.X
	// With nodes in between hosts, the origin node will also act as client to the next node acting as server. Net 10.0.0.X	
	int ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPSocketsList); // Open ICP socket 
	int ICPmanagementCloseClient(int socket_fd); // Close ICP socket 
	// As server
	int ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPSocketsList);
	int ICPmanagementCloseServer(int socket_fd,int new_socket);
	// As server or cleint
	int ICPmanagementRead(int socket_fd_conn);
	int ICPmanagementSend(int socket_fd_conn);
	int SendMessageAgent(char* ParamsDescendingCharArray); // Passing message from the Agent to send message to specific host/node
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentN */

#endif /* QtransportLayerAgentN_H_ */
