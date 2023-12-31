/* Author: Marc Jofre
Agent script for Quantum transport Layer Host
*/
#include "QtransportLayerAgentH.h"
#include<iostream>
#include<unistd.h> //for usleep
// InterCommunication Protocols - Sockets - Common to Server and Client
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#define PORT 8010
// InterCommunicaton Protocols - Sockets - Server
#include <netinet/in.h>
#include <stdlib.h>
// InterCommunicaton Protocols - Sockets - Client
#include <arpa/inet.h>


using namespace std;

namespace nsQtransportLayerAgentH {

QTLAH::QTLAH(int numberSessions) { // Constructor
 this->numberSessions = numberSessions; // Number of sessions of different services
}

int QTLAH::InitAgent(char* ParamsDescendingCharArray,char* ParamsAscendingCharArray) { // Passing parameters from the upper Agent to initialize the current agent
 	//cout << "The value of the input is: "<< ParamsDescendingCharArray << endl;
 	// Parse the ParamsDescendingCharArray
 	strcpy(this->IPaddressesSockets[0],strtok(ParamsDescendingCharArray,","));
	strcpy(this->IPaddressesSockets[1],strtok(NULL,","));//Null indicates we are using the same pointer as the last strtok
	this->SCmode=strtok(NULL,","); // to know if this host instance is client or server
	
	//cout << "IPaddressesSockets[0]: "<< this->IPaddressesSockets[0] << endl;
	//cout << "IPaddressesSockets[1]: "<< this->IPaddressesSockets[1] << endl;
	//cout << "IPaddressesSockets[2]: "<< this->IPaddressesSockets[2] << endl;
	//cout << "IPaddressesSockets[3]: "<< this->IPaddressesSockets[3] << endl;
	cout << "SCmode: "<< this->SCmode << endl;
	
	// One of the firsts things to do for a host is to initialize ICP socket connection with it host or with its attached nodes.
	this->InitiateICPconnections();	 	
	
	return 0; //All Ok
}

int QTLAH::InitiateICPconnections() {
	// This agent applies to hosts. So, regarding sockets, different situations apply
	 // Host is a client initiating the service, so:
	 //	- host will be client to its own node
	 //	- host will be client to another host (for classical communication exchange)
	 //	- initiate the instance by QapplicationServerLayer.ipynb, and use IPattachedNodeConfiguration IPserverHostOperation
	 // Host is server listening for service provision, so:	
	 //	- host will be client to attached node
	 //	- host will be server to client host
	 //	- initiate the instance by QapplicationClientLayer.ipynb, and use IPattachedNodeConfiguration
	 // since the paradigm is always to establish first node connections and then hosts connections, apparently there are no conflics confusing how is connecting to or from.
	
	// First connect ot the attached node
	this->ICPmanagementOpenClient(this->socket_fdArray[0],this->IPaddressesSockets[0]); // Connect as client to own node
	// Then either connect to the server host (acting as client) or open server listening (acting as server)
	if (strcmp(this->SCmode,"client")){		
		this->ICPmanagementOpenClient(this->socket_fdArray[1],this->IPaddressesSockets[1]); // Connect as client to destination host
	}
	else{// server
		this->ICPmanagementOpenServer(this->socket_fdArray[1],this->new_socketArray[1]); // Open port as listen as server
	}
	this->numberSessions=1;
	return 0; // All OK
}

int QTLAH::StopICPconnections() {
	// First stop client or server host connection
	if (strcmp(this->SCmode,"client")){ // client
		ICPmanagementCloseClient(this->socket_fdArray[1]);
	}
	else{// server
		ICPmanagementCloseServer(this->socket_fdArray[1],this->new_socketArray[1]);
	}
	
	// then stop client connection to attached node
	ICPmanagementCloseClient(this->socket_fdArray[0]);
	
	// Update indicators
	this->numberSessions=0;
	return 0; // All OK
}

int QTLAH::ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets) {
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
    cout << "Client connected to: "<< IPaddressesSockets << endl;
    return 0; // All Ok
}

int QTLAH::ICPmanagementOpenServer(int& socket_fd,int& new_socket) {// Node listening for connection from attached host
    
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
    cout << "Node starting socket server listening to host/node" << endl;
    return 0; // All Ok
}

int QTLAH::ICPmanagementRead(int socket_fd) {
    int valread;
    char buffer[1024] = { 0 };
    valread = read(socket_fd, buffer, 1024 - 1); // subtract 1 for the null terminator at the end
    printf("%s\n", buffer);

    return 0; // All OK
}

int QTLAH::ICPmanagementSend(int new_socket) {
    const char* hello = "Hello from ICP client";
    send(new_socket, hello, strlen(hello), 0);
    printf("Hello message sent\n");
    
    return 0; // All OK

}

int QTLAH::ICPmanagementCloseClient(int socket_fd) {
    // closing the connected socket
    close(socket_fd);
 
    return 0; // All OK
}

int QTLAH::ICPmanagementCloseServer(int socket_fd,int new_socket) {
    // closing the connected socket
    close(new_socket);
    // closing the listening socket
    close(socket_fd);
    
    return 0; // All OK
}


QTLAH::~QTLAH() {
	// destructor
	this->StopICPconnections();
}

} /* namespace nsQnetworkLayerAgentH */


// For testing

using namespace nsQtransportLayerAgentH;

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
 
 
 
 return 0;
}
