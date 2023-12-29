/* Author: Marc Jofre

Header declaration file for Quantum transport Layer Agent Node

*/
#ifndef QtransportLayerAgentN_H_
#define QtransportLayerAgentN_H_

#include<string>
#include<fstream>
using std::string;
using std::ofstream;

// #define GPIO_PATH "/sys/class/gpio/"

namespace nsQtransportLayerAgentN {

// typedef int (*CallbackType)(int);
// enum GPIO_DIRECTION{ INPUT, OUTPUT };

class QTLAN {
public:
	enum ApplicationState {
		APPLICATION_RUNNING = 0,
		APPLICATION_PAUSED = 1,  // Out of Focus or Paused If In A Timed Situation
		APPLICATION_EXIT = -1,
	    };

private:
	int numberSessions;
	// Member Variables Such As Window Handle, Time Etc.,
	ApplicationState m_state;
	
public:
	QTLAN(int numberSessions); //constructor
	// virtual ~Application(); // Default Okay - Use Virtual If Using Inheritance
        ApplicationState getState() const { return m_state; }

        bool start() { m_state = APPLICATION_RUNNING; return true; }
        bool pause() { m_state = APPLICATION_PAUSED; return true; } 
        // resume may keep track of time if the application uses a timer.
        // This is what makes it different than start() where the timer
        // in start() would be initialized to 0. And the last time before
        // paused was trigger would be saved, and then reset as new starting
        // time for your timer or counter. 
        bool resume() { m_state = APPLICATION_RUNNING; return true; }      
        bool exit() { m_state = APPLICATION_EXIT;  return false; }
	virtual ~QTLAN();  //destructor

//private:
//	int write(string path, string filename, string value);
//	friend void* threadedPoll(void *value);
};


} /* namespace nsQtransportLayerAgentN */

#endif /* QtransportLayerAgentN_H_ */
