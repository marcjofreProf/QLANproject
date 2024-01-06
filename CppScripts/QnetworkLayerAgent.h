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
// Threading
#include <thread>
// Semaphore
#include <atomic>
#include "QlinkLayerAgent.h"
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQnetworkLayerAgent {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QNLA {
private:// Variables/Instances
	int numberHops=0;
	// Semaphore
	std::atomic<int> valueSemaphore=1;// Start as 1 (open or acquireable)	

public: // Variables/Instances
	nsQlinkLayerAgent::QLLA QLLAagent;// Instance of the below agent

public: // Functions/Methods
	QNLA(); //constructor
	int InitAgentProcess(); // Initializer of the thread
	~QNLA();  //destructor

private://Functions/Methods
	// Thread management
	std::thread threadRef; // Process thread that executes requests/petitions without blocking
	// Sempahore
	void acquire();
	void release();
	void AgentProcessRequestsPetitions(); // Process thread that manages requests and petitions
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQnetworkLayerAgent */

#endif /* QnetworkLayerAgent_H_ */
