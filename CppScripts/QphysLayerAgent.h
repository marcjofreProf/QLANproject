/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum physical Layer Agent

*/
#ifndef QphysLayerAgent_H_
#define QphysLayerAgent_H_

#define LinkNumberMAX 2

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQphysLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QPLA {
private: //Variables/Instances
	int numberLinks=0;// Number of full duplex links directly connected to this physical quantum node
        int EmitLinkNumberArray[LinkNumberMAX]={0}; // Array indicating the GPIO numbers identifying the emit pins
        int ReceiveLinkNumberArray[LinkNumberMAX]={0}; // Array indicating the GPIO numbers identifying the receive pins
        
public: // Variables/Instances
	int NumStoredQubitsNode[LinkNumberMAX]={10}; // Array indicating the number of stored qubits

public: // Methods
	QPLA(); //constructor

	// General Input and Output functions
	int emitQuBit();
	int receiveQuBit();
	~QPLA();  //destructor

//private: // Methods
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQphysLayerAgent */

#endif /* QphysLayerAgent_H_ */
