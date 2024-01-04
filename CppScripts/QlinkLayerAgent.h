/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum link Layer Agent

*/
#ifndef QlinkLayerAgent_H_
#define QlinkLayerAgent_H_

#include<string>
#include<fstream>
#include "QphysLayerAgent.h"
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQlinkLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QLLA {
private: // Variables
	int numberHops;	

public: // Variables/Instances
	nsQphysLayerAgent::QPLA QPLAagent;
	
public: // Functions
	QLLA(); //constructor
	
	~QLLA();  //destructor

private: // Functions
//	int write(string path, string filename, string value);
};


} /* namespace nsQlinkLayerAgent */

#endif /* QlinkLayerAgent_H_ */
