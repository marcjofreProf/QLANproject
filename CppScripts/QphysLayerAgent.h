/* Author: Marc Jofre

Header declaration file for Quantum physical Layer Agent

*/
#ifndef QphysLayerAgent_H_
#define QphysLayerAgent_H_

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQphysLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QPLA {
//private:
	int numberLinks, *EmitLinkNumberArray, *ReceiveLinkNumberArray;

public:
	QPLA(int numberLinks, int* EmitLinkNumberArray, int* ReceiveLinkNumberArray); //constructor

	// General Input and Output functions
	virtual int emitQuBit();
	virtual int receiveQuBit();

	virtual ~QPLA();  //destructor will unexport the pin

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQphysLayerAgent */

#endif /* QphysLayerAgent_H_ */
