/* Author: Marc Jofre

Header declaration file for Quantum link Layer Agent

*/
#ifndef QlinkLayerAgent_H_
#define QlinkLayerAgent_H_

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQlinkLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QLLA {
private:
	int numberHops;

public:
	QLLA(int numberHops); //constructor
	
	~QLLA();  //destructor

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQlinkLayerAgent */

#endif /* QlinkLayerAgent_H_ */
