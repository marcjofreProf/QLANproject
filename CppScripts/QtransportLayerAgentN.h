/* Author: Marc Jofre

Header declaration file for Quantum transport Layer Agent Node

*/
#ifndef QtransportLayerAgentN_H_
#define QtransportLayerAgentN_H_

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

private: // Variables/Objects
	int numberSessions;
	// Member Variables Such As Window Handle, Time Etc.,
	ApplicationState m_state;	
	 
	int socket_fdArray; // socket descriptor, an integer (like a file-handle)
	int new_socketArray; // socket between client and server
	
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
        
	~QTLAN();  //destructor

private: // Functions
	// Managing ICP connections with sockets
	// Typically the Node will act as server to the upper host. If the node is in between, then it will act as server of the origin client node. Net 192.168.X.X or Net 10.0.0.X
	// With nodes in between hosts, the origin node will also act as client to the next node acting as server. Net 10.0.0.X	
	int ICPmanagementOpenClient(); // Open ICP socket // host will act as client to the attached node. Net 192.168.X.X
	int ICPmanagementCloseClient(); // Close ICP socket // host will act as client to the attached node. Net 192.168.X.X
	// As server
	int ICPmanagementOpenServer();
	int ICPmanagementCloseServer();
	// As server or cleint
	int ICPmanagementRead();
	int ICPmanagementSend();
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentN */

#endif /* QtransportLayerAgentN_H_ */
