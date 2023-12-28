/* Author: Marc Jofre

Header declaration file for Quantum transport Layer Agent

*/
#ifndef QtransportLayerAgent_H_
#define QtransportLayerAgent_H_

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQtransportLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QTLA {
private:
	int numberSessions;

public:
	QTLA(int numberSessions); //constructor
	
	virtual ~QTLA();  //destructor

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgent */

#endif /* QtransportLayerAgent_H_ */
