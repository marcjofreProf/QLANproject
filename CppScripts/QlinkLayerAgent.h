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
// Threading
#include <thread>
// Semaphore
#include <atomic>
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
	// Semaphore
	std::atomic<int> valueSemaphore=1;// Start as 1 (open or acquireable)	

public: // Variables/Instances
	nsQphysLayerAgent::QPLA QPLAagent;
	
public: // Functions/Methods
	QLLA(); //constructor
	int InitAgentProcess(); // Initializer of the thread
	~QLLA();  //destructor

private: // Functions//Methods
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	// Sempahore
	void acquire();
	void release();
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
//	int write(string path, string filename, string value);
};


} /* namespace nsQlinkLayerAgent */

#endif /* QlinkLayerAgent_H_ */
