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
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };
	ApplicationState m_state;
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
	// Managing status of this Agent
	ApplicationState getState() const { return m_state; }	
        bool m_start() { m_state = APPLICATION_RUNNING; return true; }
        bool m_pause() { m_state = APPLICATION_PAUSED; return true; } 
        // resume may keep track of time if the application uses a timer.
        // This is what makes it different than start() where the timer
        // in start() would be initialized to 0. And the last time before
        // paused was trigger would be saved, and then reset as new starting
        // time for your timer or counter. 
        bool m_resume() { m_state = APPLICATION_RUNNING; return true; }      
        bool m_exit() { m_state = APPLICATION_EXIT;  return false; }
};


} /* namespace nsQlinkLayerAgent */

#endif /* QlinkLayerAgent_H_ */
