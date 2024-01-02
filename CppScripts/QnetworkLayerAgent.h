/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum network Layer Agent

*/
#ifndef QnetworkLayerAgent_H_
#define QnetworkLayerAgent_H_

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQnetworkLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QNLA {
private:
	int numberConnections;

public:
	QNLA(int numberConnections); //constructor
	
	~QNLA();  //destructor

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQnetworkLayerAgent */

#endif /* QnetworkLayerAgent_H_ */
