/* Author: Marc Jofre

Header declaration file for Quantum transport Layer Agent Host

*/
#ifndef QtransportLayerAgentH_H_
#define QtransportLayerAgentH_H_

//#include<string>
//#include<fstream>
//using std::string;
//using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQtransportLayerAgentH {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QTLAH {
private: // Variables/Objects
	int numberSessions;
	char* IPhostConNet; // IP address of the client/server host in the control/configuration network
	char* IPhostOpNet; // IP address of the client/server host in the operation network
	char* IPnodeConNet; // IP address of the client/server node (connected to the client host) in the control/configuration network
	char* IPnodeOpNet; // IP address of the client/server node in the operation network
	int socket_fdArray; // socket descriptor, an integer (like a file-handle)
	int new_socketArray; // socket between client and server, an integer. Created by the server.

public: // Functions
	QTLAH(int numberSessions); //constructor
	int InitAgent(char* ParamsDescendingCharArray,char* ParamsAscendingCharArray); // Passing parameters from the upper Agent to initialize the current agent
	~QTLAH();  //destructor

private: //Functions
        // Management functions as client
	int ICPmanagementOpenClient(); // Open ICP socket 
	int ICPmanagementCloseClient(); // Close ICP socket
	// As server
	int ICPmanagementOpenServer();
	int ICPmanagementCloseServer();
	// As server or client
	int ICPmanagementRead(); // Read ICP socket
	int ICPmanagementSend(); // Send ICP socket 
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
