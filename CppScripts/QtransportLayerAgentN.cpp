/* Author: Marc Jofre
Agent script for Quantum transport Layer Node
*/
#include "QtransportLayerAgentN.h"
#include<iostream>
#include<unistd.h> //for usleep
#include "QnetworkLayerAgent.h"
// InterCommunication Protocols - Sockets - Common to Server and Client
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#define PORT 8010
#define NumSocketsMax 2
#define NumBytesBufferICPMAX 1024
// InterCommunicaton Protocols - Sockets - Server
#include <netinet/in.h>
#include <stdlib.h>
// InterCommunicaton Protocols - Sockets - Client
#include <arpa/inet.h>

using namespace std;

namespace nsQtransportLayerAgentN {

QTLAN::QTLAN(int numberSessions) { // Constructor
 this->numberSessions = numberSessions; // Number of sessions of different services
}

int QTLAN::ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPSocketsList) {
    int status;
    struct sockaddr_in serv_addr;    
    
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Client Socket creation error" << endl;
        return -1;
    }
 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, IPaddressesSockets, &serv_addr.sin_addr)<= 0) {
        cout << "Invalid address / Address not supported" << endl;
        return -1;
    }
 
    if ((status= connect(socket_fd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)))< 0) {
        cout << "Client Connection Failed" << endl;
        return -1;
    }
    
    struct sockaddr_in Address;
    socklen_t len = sizeof (Address);
    memset(&Address, 42, len);
    if (getsockname(socket_fd, (struct sockaddr*)&Address, &len) == -1) {
      cout << "Client failed to get socket name" << endl;
      return -1;
    }
    IPSocketsList=inet_ntoa(Address.sin_addr);
    //cout << "IPSocketsList: "<< IPSocketsList << endl;
    
    cout << "Client connected to: "<< IPaddressesSockets << endl;
    return 0; // All Ok
}

int QTLAN::ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPSocketsList) {// Node listening for connection from attached host
    
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);       
 
    // Creating socket file descriptor
    // AF_INET: (domain) communicating between processes on different hosts connected by IPV4
    // type: SOCK_STREAM: TCP(reliable, connection oriented)
    // Protocol value for Internet Protocol(IP), which is 0
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout << "Server socket failed" << endl;
        return -1;
    }
 
    // Forcefully attaching socket to the port
    if (setsockopt(socket_fd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
        cout << "Server attaching socket to port failed" << endl;
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
 
    // Forcefully attaching socket to the port
    if (bind(socket_fd, (struct sockaddr*)&address,sizeof(address))< 0) {
        cout << "Server socket bind failed" << endl;
        return -1;
    }
    if (listen(socket_fd, 3) < 0) {
        cout << "Server socket listen failed" << endl;
        return -1;
    }
    if ((new_socket= accept(socket_fd, (struct sockaddr*)&address,&addrlen))< 0) {
        cout << "Server socket accept failed" << endl;
        return -1;
    }
    
    struct sockaddr_in Address;
    socklen_t len = sizeof (Address);
    memset(&Address, 42, len);
    if (getsockname(socket_fd, (struct sockaddr*)&Address, &len) == -1) {
      cout << "Server failed to get socket name" << endl;
      return -1;
    }
    IPSocketsList=inet_ntoa(Address.sin_addr);
    //cout << "IPSocketsList: "<< IPSocketsList << endl;
    
    cout << "Node starting socket server to host/node: " << IPSocketsList << endl;
    return 0; // All Ok
}

int QTLAN::ICPmanagementRead(int socket_fd) {
    int valread = read(socket_fd, this->ReadBuffer,NumBytesBufferICPMAX - 1); // subtract 1 for the null
    // terminator at the end
    //cout << "Node message received: " << this->ReadBuffer << endl;
    
    return valread; 
}

int QTLAN::ICPmanagementSend(int new_socket) {
    const char* SendBufferAux = this->SendBuffer;
    send(new_socket, SendBufferAux, strlen(SendBufferAux), 0);
    
    return 0; // All OK
}

int QTLAN::ICPmanagementCloseClient(int socket_fd) {
    // closing the connected socket
    close(socket_fd);
 
    return 0; // All OK
}

int QTLAN::ICPmanagementCloseServer(int socket_fd,int new_socket) {
    // closing the connected socket
    close(new_socket);
    // closing the listening socket
    close(socket_fd);
    
    return 0; // All OK
}

int QTLAN::InitiateICPconnections(int argc){
	// This agent applies to nodes. So, regarding sockets, different situations apply
	// Node is from a host initiating the service, so:
	//	- node will be server to its own host
	//	- initiate the instance by ./QtransportLayerAgentN
	// Node is an intermediate node, so:
	//	- node will be server to the origin node
	//	- node will be client to the destination node
	//	- initiate the instance by ./QtransportLayerAgentN IPnextNodeOperation
	// since the paradigm is always to establish first server connection and then, evenually, client connection, apparently there are no conflics confusing how is connecting to or from. 

	// First, opening server listening socket 
	int RetValue = 0;
	RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[0],this->new_socketArray[0],this->IPSocketsList[0]);
	// Eventually, if it is an intermediate node
	if (argc > 1){ // Establish client connection with next node
	// First parse the parama passed IP address

	//QTLANagent.ICPmanagementOpenClient(QTLANagent.socket_fdArray[1],char* IPaddressesSockets)
	}
	if (RetValue==-1){this->m_pause();} // Exit application
	else{this->numberSessions=1;} // Update indicators
	
	return 0; // All OK
}

int QTLAN::StopICPconnections(int argc){
	// First, eventually close the client socket	
	// Eventually, if it is an intermediate node
	if (argc > 1){ // Establish client connection with next node
	// First parse the parama passed IP address

	//QTLANagent.ICPmanagementCloseClient(QTLANagent.socket_fdArray[1],char* IPaddressesSockets)
	}
	// then stop the server socket
	this->ICPmanagementCloseServer(this->socket_fdArray[0],this->new_socketArray[0]);
	// Update indicators
	this->numberSessions=0;
	return 0; // All OK
}

int QTLAN::ICPConnectionsCheckNewMessages(){
  for (int i=0;i<NumSocketsMax;++i){
     try{
	  int socket_fd=this->socket_fdArray[i]; // To be improved if several sockets
	  // Check for new messages
	  fd_set fds;
	  FD_ZERO(&fds);
	  FD_SET(socket_fd, &fds);

	  // Set the timeout to 1 second
	  struct timeval timeout;
	  // The tv_usec member is rarely used, but it can be used to specify a more precise timeout. For example, if you want to wait for a message to arrive on the socket for up to 1.5 seconds, you could set tv_sec to 1 and tv_usec to 500,000.
	  timeout.tv_sec = 1;
	  timeout.tv_usec = 0;
	  
	  int nfds = socket_fd + 1;
	  int ret = select(nfds, &fds, NULL, NULL, &timeout);

	  if (ret < 0) {
	    cout << "Node error select to check new messages" << endl;
	    this->m_exit();
	    return -1;
	  } else if (ret == 0) {
	    //cout << "Node agent no new messages" << endl;
	    return 0; // All OK;
	  } else {
	    // There is at least one new message
	    if (FD_ISSET(socket_fd, &fds)) {
	      // Read the message from the socket
	      int n = this->ICPmanagementRead(socket_fd);
	      if (n < 0) {
		cout << "Node error reading new messages" << endl;
		this->m_exit();
		return -1;
	      }

	      // Process the message
	      cout << "Received message: " << this->ReadBuffer << endl;
	    }
	  }
  } // try
    catch (const std::exception& e) {
	// Handle the exception
    	cout << "Exception: " << e.what() << endl;
  	}
  } // for
  return 0; // All OK
}

int QTLAN::SendMessageAgent(char* ParamsDescendingCharArray){
	try {
    	// Code that might throw an exception 

	    // Parse the message information
	    strcpy(this->SendBuffer,strtok(ParamsDescendingCharArray,","));
	    char* IPaddressesSockets;
	    strcpy(IPaddressesSockets,strtok(NULL,","));//Null indicates we are using the same pointer as the last strtok
	    // Understand which socket descriptor has to be used
	    int new_socket;
	    for (int i=0; i<NumSocketsMax; ++i){
	    	if (string(this->IPSocketsList[i])==string(IPaddressesSockets)){new_socket=this->new_socketArray[i];}
	    }  
	    this->ICPmanagementSend(new_socket);
    
    } // try
    catch (const std::exception& e) {
	// Handle the exception
    	cout << "Exception: " << e.what() << endl;
  	}
    return 0; //All OK
}

QTLAN::~QTLAN() {
// destructor
	this->StopICPconnections(this->ParamArgc);
}

} /* namespace nsQnetworkLayerAgentN */


// For running in the quantum node

using namespace nsQtransportLayerAgentN;

int main(int argc, char const * argv[]){
 // Basically used for testing, since the declarations of the other functions will be used by other Agents as primitives 
 // argv and argc are how command line arguments are passed to main() in C and C++.

 // argc will be the number of strings pointed to by argv. This will (in practice) be 1 plus the number of arguments, as virtually all implementations will prepend the name of the program to the array.

 // The variables are named argc (argument count) and argv (argument vector) by convention, but they can be given any valid identifier: int main(int num_args, char** arg_strings) is equally valid.

 // They can also be omitted entirely, yielding int main(), if you do not intend to process command line arguments.
 
 //printf( "argc:     %d\n", argc );
 //printf( "argv[0]:  %s\n", argv[0] );
 
 //if ( argc == 1 ) {
 // printf( "No arguments were passed.\n" );
 //}
 //else{
 // printf( "Arguments:\n" );
 // for (int i = 1; i < argc; ++i ) {
 //  printf( "  %d. %s\n", i, argv[i] );
 // }
 //}
 
 
 QTLAN QTLANagent(0); // Initiate the instance with 0 sessions connected. A session is composed of one server sockets descriptor active.
 // Save some given parameters to the instance of the object
 QTLANagent.ParamArgc=argc;
 // One of the firsts things to do for a node is to initialize listening ICP socket connection with it host or with its adjacent nodes.
 QTLANagent.InitiateICPconnections(QTLANagent.ParamArgc);
   
 // Then await for next actions
 QTLANagent.m_pause(); // Initiate in paused state.
 cout << "Starting in pause state the QtransportLayerAgentN" << endl;
 bool isValidWhileLoop = true;
 
 while(isValidWhileLoop){ 
 	try {
    	// Code that might throw an exception 
 
 	// Check if there are need messages or actions to be done by the node
 	QTLANagent.ICPConnectionsCheckNewMessages(); // This function has some time out (so will not consume resources of the node)
       switch(QTLANagent.getState()) {
           case QTLAN::APPLICATION_RUNNING: {
               
               // Do Some Work
               QTLANagent.m_pause(); // After procesing the request, pass to paused state
               break;
           }
           case QTLAN::APPLICATION_PAUSED: {
               // Wait Till You Have Focus Or Continues
               if (QTLANagent.numberSessions>0 and true){
               	QTLANagent.m_start();
               }
               else{               
	        QTLANagent.m_pause(); // Keep paused state
               }
               break;
           }
           case QTLAN::APPLICATION_EXIT: {    
               cout << "Exiting the QtransportLayerAgentN" << endl;
               QTLANagent.StopICPconnections(QTLANagent.ParamArgc);
               isValidWhileLoop=false;//break;
           }
           default: {
               // ErrorHandling Throw An Exception Etc.
           }

        } // switch
    }
    catch (const std::exception& e) {
	// Handle the exception
    	cout << "Exception: " << e.what() << endl;
  	}
    } // while
   
 return 0; // Everything Ok
}

