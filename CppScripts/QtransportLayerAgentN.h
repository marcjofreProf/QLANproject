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
private:
	int numberSessions;

public:
	QTLAN(int numberSessions); //constructor
	
	virtual ~QTLAN();  //destructor

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentN */

#endif /* QtransportLayerAgentN_H_ */
