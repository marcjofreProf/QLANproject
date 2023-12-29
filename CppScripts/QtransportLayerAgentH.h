/* Author: Marc Jofre

Header declaration file for Quantum transport Layer Agent Host

*/
#ifndef QtransportLayerAgentH_H_
#define QtransportLayerAgentH_H_

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQtransportLayerAgentH {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QTLAH {
private:
	int numberSessions;

public:
	QTLAH(int numberSessions); //constructor
	
	virtual ~QTLAH();  //destructor

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentH */

#endif /* QtransportLayerAgentH_H_ */
