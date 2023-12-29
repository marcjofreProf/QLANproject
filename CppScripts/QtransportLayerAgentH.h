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
	int clientHN_fd;

public: // Functions
	QTLAH(int numberSessions); //constructor
	int InitAgent(char* ParamsDescendingCharArray,char* ParamsAscendingCharArray); // Passign parameters from the upper Agent to initialize the current agent
	~QTLAH();  //destructor

private: //Functions
	int ICPmanagementOpenClientHN(); // Open ICP socket // host will act as client to the attached node. Net 192.168.X.X
	int ICPmanagementReadClientHN(); // Read ICP socket // host will act as client to the attached node. Net 192.168.X.X
	int ICPmanagementSendClientHN(); // Send ICP socket // host will act as client to the attached node. Net 192.168.X.X
	int ICPmanagementCloseClientHN(); // Close ICP socket // host will act as client to the attached node. Net 192.168.X.X
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
