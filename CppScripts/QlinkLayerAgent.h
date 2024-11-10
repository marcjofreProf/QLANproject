/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Header declaration file for Quantum link Layer Agent

*/
#ifndef QlinkLayerAgent_H_
#define QlinkLayerAgent_H_

// Payload messages
#define NumBytesPayloadBuffer 1000

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
	unsigned long long int UnTrapSemaphoreValueMaxCounter=10000000;//MAx counter trying to acquire semaphore, then force release
	std::atomic<bool> valueSemaphore=true;// Start as 1 (open or acquireable)	
	// Payload messages
	char PayloadReadBuffer[NumBytesPayloadBuffer]={0}; //Buffer to read payload messages
	char PayloadSendBuffer[NumBytesPayloadBuffer]={0}; //Buffer to send payload messages
	// Time synchronization
	struct my_clock
	{
	    using duration   = std::chrono::nanoseconds;
	    using rep        = duration::rep;
	    using period     = duration::period;
	    using time_point = std::chrono::time_point<my_clock>;
	    static constexpr bool is_steady = false;// true, false

	    static time_point now()
	    {
		timespec ts;
		if (clock_gettime(CLOCK_REALTIME, &ts))// CLOCK_REALTIME//CLOCK_TAI
		    throw 1;
		using sec = std::chrono::seconds;
		return time_point{sec{ts.tv_sec}+duration{ts.tv_nsec}};
	    }
	};
	using Clock = my_clock;//

public: // Variables/Instances
	enum ApplicationState { // State of the agent sequences
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };
	ApplicationState m_state;
	nsQphysLayerAgent::QPLA QPLAagent;
	char SCmode[1][NumBytesPayloadBuffer] = {0}; // Variable to know if the node instance is working as server or client to the other node
	
public: // Functions/Methods
	QLLA(); //constructor
	int InitAgentProcess(); // Initializer of the thread
	// Payload information parameters
	int SendParametersAgent(char* ParamsCharArray);// The upper layer gets the information to be send
        int SetReadParametersAgent(char* ParamsCharArray);// The upper layer sets information from the other node
	~QLLA();  //destructor

private: // Functions//Methods
	int RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep);
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
        // Payload information parameters
        int InitParametersAgent();// Client node have some parameters to adjust to the server node
        int SetSendParametersAgent(char* ParamsCharArray);// Node accumulates parameters for the other node
	int ReadParametersAgent();// Node checks parameters from the other node
	int NegotiateInitialParamsNode();
	int ProcessNewParameters();// Process the parameters 
	int countDoubleColons(char* ParamsCharArray);
	int countDoubleUnderscores(char* ParamsCharArray);
};


} /* namespace nsQlinkLayerAgent */

#endif /* QlinkLayerAgent_H_ */
