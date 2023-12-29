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
#define PORT 8080
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
 	this->IPhostConNet=strtok(ParamsDescendingCharArray,",");
	this->IPhostOpNet=strtok(NULL,",");//Null indicates we are using the same pointer as the last strtok
	this->IPnodeConNet=strtok(NULL,",");
	this->IPnodeOpNet=strtok(NULL,",");
	
	//cout << "IPhostConNet: "<< this->IPhostConNet << endl;
	//cout << "IPhostOpNet: "<< this->IPhostOpNet << endl;
	//cout << "IPnodeConNet: "<< this->IPnodeConNet << endl;
	//cout << "IPnodeOpNet: "<< this->IPnodeOpNet << endl;
	
	return 0; //All Ok
}

int QTLAH::ICPmanagementOpenClient() {// Host connecting to the attached node    
    int status;
    struct sockaddr_in serv_addr;    
    
    if ((this->clientHN_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, this->IPnodeConNet, &serv_addr.sin_addr)<= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
 
    if ((status= connect(this->clientHN_fd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)))< 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    return 0; // All Ok
}

int QTLAH::ICPmanagementReadClient() {
    int valread;
    char buffer[1024] = { 0 };
    valread = read(this->clientHN_fd, buffer, 1024 - 1); // subtract 1 for the null terminator at the end
    printf("%s\n", buffer);
    
    return 0; // All OK
}

int QTLAH::ICPmanagementSendClient() {
    const char* hello = "Hello from ICP client";
    send(this->clientHN_fd, hello, strlen(hello), 0);
    printf("Hello message sent\n");
    
    return 0; // All OK
}

int QTLAH::ICPmanagementCloseClient() {
    // closing the connected socket
    close(this->clientHN_fd);
    
    return 0; // All OK
}


QTLAH::~QTLAH() {
// destructor
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
