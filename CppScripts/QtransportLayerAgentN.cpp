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
#define PORT 8080
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

int QTLAN::ICPmanagementOpenServerN() {// Node listening for connection from attached host
    
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);       
 
    // Creating socket file descriptor
    if ((this->serverHN_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        //exit(EXIT_FAILURE);
    }
 
    // Forcefully attaching socket to the port 8080
    if (setsockopt(this->serverHN_fd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
        perror("setsockopt");
        //exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
 
    // Forcefully attaching socket to the port 8080
    if (bind(this->serverHN_fd, (struct sockaddr*)&address,sizeof(address))< 0) {
        perror("bind failed");
        //exit(EXIT_FAILURE);
    }
    if (listen(this->serverHN_fd, 3) < 0) {
        perror("listen");
        //exit(EXIT_FAILURE);
    }
    if ((this->newHN_socket= accept(this->serverHN_fd, (struct sockaddr*)&address,&addrlen))< 0) {
        perror("accept");
        //exit(EXIT_FAILURE);
    }

    return 0; // All Ok
}

int QTLAN::ICPmanagementReadServerN() {
    ssize_t valread;
    char buffer[1024] = { 0 };
    valread = read(this->newHN_socket, buffer,1024 - 1); // subtract 1 for the null
    // terminator at the end
    printf("%s\n", buffer);
    
    return 0; // All OK
}

int QTLAN::ICPmanagementSendServerN() {
    const char* hello = "Hello from ICP server";
    send(this->newHN_socket, hello, strlen(hello), 0);
    printf("Hello message sent\n");
    
    return 0; // All OK
}

int QTLAN::ICPmanagementCloseServerN() {
    // closing the connected socket
    close(this->newHN_socket);
    // closing the listening socket
    close(this->serverHN_fd);
    
    return 0; // All OK
}


QTLAN::~QTLAN() {
// destructor
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
 
 QTLAN QTLANagent(0); // Initiate the instance with 0 sessions connected.
 
 // One of the firsts things to do for a node is to initialize ICP socket connection with it host or with its adjacent nodes.
 
 // Then await for next actions
 QTLANagent.pause(); // Initiate in paused state.
 cout << "Starting in pause state the QtransportLayerAgentN" << endl;
 bool isValidWhileLoop = true;
 
 while(isValidWhileLoop){
       switch(QTLANagent.getState()) {
           case QTLAN::APPLICATION_RUNNING: {
               
               // Do Some Work
               QTLANagent.pause(); // After procesing the request, pass to paused state
               break;
           }
           case QTLAN::APPLICATION_PAUSED: {
               // Wait Till You Have Focus Or Continues
               if (true){
               	QTLANagent.start();
               }
               else if (false){
               	QTLANagent.exit();// Change The Application State to exit
               }
               else{               
	        QTLANagent.pause(); // Keep paused state
	        usleep(1000); // Wait 1ms, waiting for other jobs to process
               }
               break;
           }
           case QTLAN::APPLICATION_EXIT: {    
               cout << "Exiting the QtransportLayerAgentN" << endl;
               isValidWhileLoop=false;//break;
           }
           default: {
               // ErrorHandling Throw An Exception Etc.
           }

        } // switch
        
    }
   
 return 0; // Everything Ok
}

