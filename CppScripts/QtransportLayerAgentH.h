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
public: // Variables/Objects
	int numberSessions=0;
private: // Variables/Objects	
	char IPaddressesSockets[2][15]; // IP address of the client/server host/node in the control/operation networks
	char* SCmode; // Variable to know if the host instance is working as server or client
	int socket_fdArray[2]; // socket descriptor, an integer (like a file-handle)
	int new_socketArray[2]; // socket between client and server, an integer. Created by the server.

public: // Functions
	QTLAH(int numberSessions); //constructor
	int InitAgent(char* ParamsDescendingCharArray,char* ParamsAscendingCharArray); // Passing parameters from the upper Agent to initialize the current agent
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
};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
