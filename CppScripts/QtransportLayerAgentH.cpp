/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum transport Layer Host
*/
#include "QtransportLayerAgentH.h"
#include <iostream>
#include <unistd.h> //for usleep
// InterCommunication Protocols - Sockets - Common to Server and Client
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>
#define PORT 8010
#define NumBytesBufferICPMAX 4096 // Oversized to make sure that sockets do not get full
#define IPcharArrayLengthMAX 15
#define SockListenTimeusecStandard 50
#define NumProcessMessagesConcurrentMax 20
// InterCommunicaton Protocols - Sockets - Server
#include <netinet/in.h>
#include <stdlib.h>
#define SOCKtype "SOCK_DGRAM" //"SOCK_STREAM": tcp; "SOCK_DGRAM": udp
#define SOCKkeepaliveTime 2000000 // WaitTimeAfterMainWhileLoop
// InterCommunicaton Protocols - Sockets - Client
#include <arpa/inet.h>
// Threading
#define WaitTimeAfterMainWhileLoop 10000000 // nanoseconds
#include <thread>
// Semaphore
#include <atomic>
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20
// Synchronizaton network
#define usSynchProcIterRunsTimePoint 10000000 // Time to wait (seconds) between iterations of the synch mechanisms to allow time to send and receive the necessary qubits

using namespace std;

namespace nsQtransportLayerAgentH {

QTLAH::QTLAH(int numberSessions,char* ParamsDescendingCharArray,char* ParamsAscendingCharArray) { // Constructor

/// Errors handling - Actually handled in Python
// signal(SIGINT, SignalINTHandler);// Interruption signal
// signal(SIGPIPE, SignalPIPEHandler);// Error trying to write/read to a socket
// signal(SIGSEGV, SignalSegmentationFaultHandler);// Segmentation fault
 
 this->numberSessions = numberSessions; // Number of sessions of different services
 
 //cout << "The value of the input is: "<< ParamsDescendingCharArray << endl;
// Parse the ParamsDescendingCharArray
strcpy(this->SCmode[0],"client"); // to know if this host instance is client or server to the node. It is always client
strcpy(this->SCmode[1],strtok(ParamsDescendingCharArray,",")); // to know if this host instance is client or server

strcpy(this->IPaddressesSockets[0],strtok(NULL,","));//Null indicates we are using the same pointer as the last strtok
strcpy(this->IPaddressesSockets[1],strtok(NULL,","));//Null indicates we are using the same pointer as the last strtok
strcpy(this->IPaddressesSockets[2],strtok(NULL,","));
strcpy(this->IPaddressesSockets[3],strtok(NULL,","));
strcpy(this->IPaddressesSockets[4],strtok(NULL,","));

//cout << "IPaddressesSockets[0]: "<< this->IPaddressesSockets[0] << endl;
//cout << "IPaddressesSockets[1]: "<< this->IPaddressesSockets[1] << endl;
//cout << "SCmode[0]: "<< this->SCmode[0] << endl;
//cout << "SCmode[1]: "<< this->SCmode[1] << endl;

}

/*
void* QTLAH::AgentProcessStaticEntryPoint(void* c){// Not really used
  try {
      ((QTLAH*) c)->AgentProcessRequestsPetitions();
  } catch (...) {
      throw;
  }
  return NULL;
}
*/
//////////////// Sempahore /////////////////////////////*


void QTLAH::acquire() {
/*
bool CheckAquire = !valueSemaphore.compare_exchange_strong(this->valueSemaphoreExpected, 0,std::memory_order_acquire);
while (CheckAquire);
this->valueSemaphoreExpected=1; // Make sure it stays at 1
this->valueSemaphore=0; // Make sure it stays at 0

// Notify any waiting threads
std::atomic_thread_fence(std::memory_order_release);
std::this_thread::yield();
*/
/*
while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
*/
// https://stackoverflow.com/questions/61493121/when-can-memory-order-acquire-or-memory-order-release-be-safely-removed-from-com
// https://medium.com/@pauljlucas/advanced-thread-safety-in-c-4cbab821356e
//int oldCount;
bool valueSemaphoreExpected=true;
while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
	if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
	break;
	}
}
}
 
void QTLAH::release() {
/*
bool CheckRelease = valueSemaphore.fetch_add(1, std::memory_order_acquire);
    if (CheckRelease) {
      // Notify any waiting threads
      std::atomic_thread_fence(std::memory_order_release);
      std::this_thread::yield();
      this->valueSemaphoreExpected=1; // Make sure it stays at 1
      this->valueSemaphore=1; // Make sure it stays at 1
    }*/
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}
/*
/// Errors handling
std::atomic<bool> signalReceivedFlag{false};
static void SignalINTHandler(int s) {
signalReceivedFlag.store(true);
cout << "Caught SIGINT" << endl;
}

static void SignalPIPEHandler(int s) {
signalReceivedFlag.store(true);
cout << "Caught SIGPIPE" << endl;
}

static void SignalSegmentationFaultHandler(int s) {
signalReceivedFlag.store(true);
cout << "Caught SIGSEGV" << endl;
}
*/
/////////////////////////////////////////////////////////
int QTLAH::countQintupleComas(char* ParamsCharArray) {
  int comasCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ',') {
      comasCount++;
    }
  }
 //cout << "comasCount: " << comasCount << endl;
  return comasCount/5;
}

int QTLAH::countColons(char* ParamsCharArray) {
  int colonCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ':') {
      colonCount++;
    }
  }

  return colonCount;
}

int QTLAH::countDoubleColons(char* ParamsCharArray) {
  int colonCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ':') {
      colonCount++;
    }
  }

  return colonCount/2;
}

int QTLAH::countDoubleUnderscores(char* ParamsCharArray) {
  int underscoreCount = 0;
  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == '_') {
      underscoreCount++;
    }
  }
  return underscoreCount/2;
}

int QTLAH::countQuadrupleUnderscores(char* ParamsCharArray) {
  int underscoreCount = 0;
  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == '_') {
      underscoreCount++;
    }
  }
  return underscoreCount/4;
}
////////////////////////////////////////////////////////
int QTLAH::InitAgentProcess(){
	// Then, regularly check for next job/action without blocking		  	
	// Not used void* params;
	// Not used this->threadRef=std::thread(&QTLAH::AgentProcessStaticEntryPoint,params);
	this->threadRef=std::thread(&QTLAH::AgentProcessRequestsPetitions,this);
	  //if (ret) {
	    // Handle the error
	  //} 
	return 0; //All OK
}

int QTLAH::InitiateICPconnections() {
int RetValue=0;
if (string(SOCKtype)=="SOCK_DGRAM"){
	// UDP philosophy is different since it is not connection oriented. Actually, we are telling to listen to a port, and if we want also specifically to an IP (which we might want to do to keep better track of things)
	// The sockets for receiving
	RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[0],this->new_socketArray[0],this->IPaddressesSockets[0],this->IPaddressesSockets[1],this->IPSocketsList[0]); // Listen to the port
	
	if (RetValue>-1){
	RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[1],this->new_socketArray[1],this->IPaddressesSockets[3],this->IPaddressesSockets[2],this->IPSocketsList[1]);} // Open port and listen as server
	
	if (RetValue>-1){RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[2],this->new_socketArray[2],this->IPaddressesSockets[4],this->IPaddressesSockets[2],this->IPSocketsList[2]);} // Open port and listen as server
	// The sockets for sending	
	if (RetValue>-1){RetValue=this->ICPmanagementOpenClient(this->socket_SendUDPfdArray[0],this->IPaddressesSockets[0],this->IPaddressesSockets[1],this->IPSocketsList[0]);}// In order to send datagrams
	
	if (RetValue>-1){RetValue=this->ICPmanagementOpenClient(this->socket_SendUDPfdArray[1],this->IPaddressesSockets[3],this->IPaddressesSockets[2],this->IPSocketsList[1]);}// In order to send datagrams
	if (RetValue>-1){RetValue=this->ICPmanagementOpenClient(this->socket_SendUDPfdArray[2],this->IPaddressesSockets[4],this->IPaddressesSockets[2],this->IPSocketsList[2]);} // Open port and listen as server

}
else{// TCP
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
	
	// First connect to the attached node
	RetValue=this->ICPmanagementOpenClient(this->socket_fdArray[0],this->IPaddressesSockets[0],this->IPaddressesSockets[1],this->IPSocketsList[0]); // Connect as client to own node
	if (RetValue==-1){this->m_exit();} // Exit application
	// Then either connect to the server host (acting as client) or open server listening (acting as server)
	//cout << "Check - SCmode[1]: " << this->SCmode[1] << endl;
	if (string(this->SCmode[1])==string("client")){
		//cout << "Check - Generating connection as client" << endl;	
		RetValue=this->ICPmanagementOpenClient(this->socket_fdArray[1],this->IPaddressesSockets[3],this->IPaddressesSockets[2],this->IPSocketsList[1]); // Connect as client to destination host
	}
	else{// server. suppossedly in both situations being server or delear, it will become server to the client (the client will conntect to this host)
		//cout << "Check - Generating connection as server" << endl;
		RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[1],this->new_socketArray[1],this->IPaddressesSockets[3],this->IPaddressesSockets[2],this->IPSocketsList[1]); // Open port and listen as server
		
	}
	if (RetValue==-1){this->m_exit();} // Exit application
	
	if (string(this->SCmode[1])==string("dealer")){
		RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[2],this->new_socketArray[2],this->IPaddressesSockets[4],this->IPaddressesSockets[2],this->IPSocketsList[2]); // Open port and listen as server
	}
	else{
		RetValue=this->ICPmanagementOpenClient(this->socket_fdArray[2],this->IPaddressesSockets[4],this->IPaddressesSockets[2],this->IPSocketsList[2]); // Connect as client to destination host
	}
}
	if (RetValue==-1){this->m_exit();} // Exit application
	this->numberSessions=1;
	this->NumSockets=3;
		
	return 0; // All OK
}

int QTLAH::StopICPconnections() {
	// First stop client or server host connection
	if (string(SOCKtype)=="SOCK_DGRAM"){
		ICPmanagementCloseClient(this->socket_SendUDPfdArray[0]);
		ICPmanagementCloseClient(this->socket_SendUDPfdArray[1]);
		ICPmanagementCloseClient(this->socket_SendUDPfdArray[2]);
		ICPmanagementCloseServer(this->socket_fdArray[0],this->new_socketArray[0]);
		ICPmanagementCloseServer(this->socket_fdArray[1],this->new_socketArray[1]);
		ICPmanagementCloseServer(this->socket_fdArray[1],this->new_socketArray[2]);
	}
	else{
		if (string(this->SCmode[1])==string("dealer")){
			ICPmanagementCloseServer(this->socket_fdArray[2],this->new_socketArray[2]);
		}
		else{
			ICPmanagementCloseClient(this->socket_fdArray[2]);
		}
		
		if (string(this->SCmode[1])==string("client")){ // client
			ICPmanagementCloseClient(this->socket_fdArray[1]);
		}
		else{// server
			ICPmanagementCloseServer(this->socket_fdArray[1],this->new_socketArray[1]);
		}

		// then stop client connection to attached node
		ICPmanagementCloseClient(this->socket_fdArray[0]);
	}
	// Update indicators
	this->numberSessions=0;
	return 0; // All OK
}

int QTLAH::SocketCheckForceShutDown(int socket_fd){
//The close() function terminates the entire connection and releases all resources associated with the socket, including the file descriptor. This means that the socket can no longer be used for any purpose, and it will be closed immediately.

//The shutdown() function, on the other hand, allows you to terminate a connection in a more controlled way. You can specify whether to shut down the read, write, or both ends of the connection. This can be useful if you want to gracefully terminate the connection, such as when the server is shutting down or when a client needs to send a final message.

//if (shutdown(socket_fd, SHUT_RDWR) < 0) {
//perror("shutdown");
//close(socket_fd);
//cout << "Not capable to shutdown Socket!!! Restart the application!" << endl;
//return -1;
//}
return 0; // All Ok
}

int QTLAH::ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList) {    
    struct sockaddr_in address;
    int opt = 1;
    // Creating socket file descriptor
    // AF_INET: (domain) communicating between processes on different hosts connected by IPV4
    // type: SOCK_STREAM: TCP(reliable, connection oriented) // ( SOCK_STREAM for TCP / SOCK_DGRAM for UDP ) 
    // Protocol value for Internet Protocol(IP), which is 0 
    if (string(SOCKtype)=="SOCK_DGRAM"){socket_fd = socket(AF_INET, SOCK_DGRAM, 0);}
    else {socket_fd = socket(AF_INET, SOCK_STREAM, 0);}
    if (socket_fd < 0) {
        cout << "Client Socket creation error" << endl;
        return -1;
    }
    
    //cout << "Client Socket file descriptor: " << socket_fd << endl;
    
    // Check status of a previously initiated socket to reduce misconnections
    //this->SocketCheckForceShutDown(socket_fd); Not used
    
    // Specifying some options to the port
    if (setsockopt(socket_fd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
	cout << "Server attaching socket options failed" << endl;
	return -1;
    }
    
    address.sin_family = AF_INET;
    if (string(SOCKtype)=="SOCK_DGRAM"){
    	address.sin_addr.s_addr = inet_addr(IPaddressesSocketsLocal);// Since we have the info, it is better to specify, instead of INADDR_ANY;
    	address.sin_port = htons(0);// 0 any avaliable
    }
    else{address.sin_addr.s_addr = inet_addr(IPaddressesSocketsLocal);address.sin_port = htons(PORT);}// Since we have the info, it is better to specify, instead of INADDR_ANY;
        
    // Forcefully attaching socket to the port
    if (bind(socket_fd, (struct sockaddr*)&address,sizeof(address))< 0) {
	cout << "Client socket bind failed" << endl;
	return -1;
    }
    
    // Connect is for TCP
    if (string(SOCKtype)=="SOCK_STREAM"){	 
	    // Convert IPv4 and IPv6 addresses from text to binary form
	    if (inet_pton(AF_INET, IPaddressesSockets, &address.sin_addr)<= 0) {
		cout << "Invalid address / Address not supported" << endl;
		return -1;
	    }
	    
    
	    int status= connect(socket_fd, (struct sockaddr*)&address,sizeof(address));
	    if (status< 0) {
		cout << "Client Connection Failed" << endl;
		return -1;
	    }
    }
    
    strcpy(IPSocketsList,IPaddressesSockets);
    //cout << "IPSocketsList: "<< IPSocketsList << endl;
    
    cout << "Host client connected to server: "<< IPaddressesSockets << endl;
    return 0; // All Ok
}

int QTLAH::ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList) {// Node listening for connection from attached host
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);       
 
    // Creating socket file descriptor
    // AF_INET: (domain) communicating between processes on different hosts connected by IPV4
    // type: SOCK_STREAM: TCP(reliable, connection oriented) // ( SOCK_STREAM for TCP / SOCK_DGRAM for UDP ) 
    // Protocol value for Internet Protocol(IP), which is 0 
    if (string(SOCKtype)=="SOCK_DGRAM"){socket_fd = socket(AF_INET, SOCK_DGRAM, 0);}
    else {socket_fd = socket(AF_INET, SOCK_STREAM, 0);}
    if (socket_fd < 0) {
        cout << "Server socket failed" << endl;
        return -1;
    }
    
    // Check status of a previously initiated socket to reduce misconnections
    //this->SocketCheckForceShutDown(socket_fd); Not used
    
    // Specifying some options to the port
    if (setsockopt(socket_fd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
	cout << "Server attaching socket options failed" << endl;
	return -1;
    }
    
    address.sin_family = AF_INET;
    if (string(SOCKtype)=="SOCK_DGRAM"){
    	address.sin_addr.s_addr = inet_addr(IPaddressesSocketsLocal);// Since we have the info, it is better to specify, instead of INADDR_ANY;
    }
    else{address.sin_addr.s_addr = inet_addr(IPaddressesSocketsLocal);}// Since we have the info, it is better to specify, instead of INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Forcefully attaching socket to the port
    if (bind(socket_fd, (struct sockaddr*)&address,sizeof(address))< 0) {
    	//cout << "Server bind IPaddressesSocketsLocal: " << IPaddressesSocketsLocal << endl;
    	cout << strerror(errno) << endl;
        cout << "Server socket bind failed" << endl;
        return -1;
    }
    
    // listen and accept are particular of TCP
    if (string(SOCKtype)=="SOCK_STREAM"){
	    if (listen(socket_fd, 3) < 0) {
		cout << "Server socket listen failed" << endl;
		return -1;
	    }
	    
	    new_socket= accept(socket_fd, (struct sockaddr*)&address,&addrlen);
	    if (new_socket< 0) {
		cout << "Server socket accept failed" << endl;
		return -1;
	    }
	// Retrive IP address client
    strcpy(IPSocketsList,inet_ntoa(address.sin_addr));
    }
    else{
    // Retrive IP address client
    strcpy(IPSocketsList,IPaddressesSockets);
    }
    
    //cout << " Server socket_fd: " << socket_fd << endl;
    //cout << " Server new_socket: " << new_socket << endl;
    
    
    //cout << "IPSocketsList: "<< IPSocketsList << endl;
    
    cout << "Host starting socket server to host/node: " << IPSocketsList << endl;
    return 0; // All Ok
}

int QTLAH::ICPmanagementRead(int socket_fd_conn,int SockListenTimeusec) {
    // Check for new messages
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(socket_fd_conn, &fds);

  // Set the timeout
  struct timeval timeout;
  // The tv_usec member is rarely used, but it can be used to specify a more precise timeout. For example, if you want to wait for a message to arrive on the socket for up to 1.5 seconds, you could set tv_sec to 1 and tv_usec to 500,000.
  timeout.tv_sec = 0;
  timeout.tv_usec = SockListenTimeusec;
  
  int nfds = socket_fd_conn + 1; //The nfds argument specifies the range of file descriptors to be tested. The select() function tests file descriptors in the range of 0 to nfds-1.
  int ret = 0;
  if (string(SOCKtype)=="SOCK_DGRAM"){ret = select(nfds, &fds, NULL, NULL, &timeout);}
  else{ret = select(nfds, &fds, NULL, NULL, &timeout);}

if (ret < 0){
//cout << "Host select no new messages" << endl;this->m_exit();
return -1;}
else if (ret==0){//cout << "No new messages" << endl;
return -1;
}
else {// There might be at least one new message
	if (FD_ISSET(socket_fd_conn, &fds)){// is a macro that checks whether a specified file descriptor is set in a specified file descriptor set.
		// Read the message from the socket
		int valread=0;
		if (this->ReadFlagWait){			
			if (string(SOCKtype)=="SOCK_DGRAM"){
				struct sockaddr_in orgaddr; 
			    memset(&orgaddr, 0, sizeof(orgaddr));		       
			    // Filling information 
			    orgaddr.sin_family    = AF_INET; // IPv4 
			    
			    for (int i=0; i<(NumSockets); i++){
				//cout << "socket_fd_conn: " << socket_fd_conn << endl;
			    	//cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
			    	if (socket_fd_conn==socket_fdArray[i]){
			    		//cout << "this->IPaddressesSockets[i]: " << this->IPaddressesSockets[i] << endl;
			    		orgaddr.sin_addr.s_addr = inet_addr(this->IPaddressesSockets[i]);//INADDR_ANY;
			    		orgaddr.sin_port = htons(PORT);
					    unsigned int addrLen;
						addrLen = sizeof(orgaddr);
					valread=recvfrom(socket_fd_conn,this->ReadBuffer,NumBytesBufferICPMAX,0,(struct sockaddr *) &orgaddr,&addrLen);
			    	}
			    }
			    
			//cout << "valread: " << valread << endl;
			//for (int i=0; i<(NumSockets); i++){
			//	cout << "socket_fd_conn: " << socket_fd_conn << endl;
			//    	cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
			//    	if (socket_fd_conn==socket_fdArray[i]){
			//cout << "Host " << string(this->SCmode[i]) << " this->ReadBuffer: " << this->ReadBuffer << endl;
			//}
			//}
			}
    			else{valread = recv(socket_fd_conn, this->ReadBuffer,NumBytesBufferICPMAX,0);}
			}
		else{			
			if (string(SOCKtype)=="SOCK_DGRAM"){
				struct sockaddr_in orgaddr; 
			    memset(&orgaddr, 0, sizeof(orgaddr));		       
			    // Filling information 
			    orgaddr.sin_family    = AF_INET; // IPv4
			    for (int i=0; i<(NumSockets); i++){
				//cout << "socket_fd_conn: " << socket_fd_conn << endl;
			    	//cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
			    	if (socket_fd_conn==socket_fdArray[i]){
			    		//cout << "this->IPaddressesSockets[i]: " << this->IPaddressesSockets[i] << endl;
			    		orgaddr.sin_addr.s_addr = inet_addr(this->IPaddressesSockets[i]);//INADDR_ANY; 
			    		orgaddr.sin_port = htons(PORT);
			    unsigned int addrLen;
				addrLen = sizeof(orgaddr);
			valread=recvfrom(socket_fd_conn,this->ReadBuffer,NumBytesBufferICPMAX,0,(struct sockaddr *) &orgaddr,&addrLen);//MSG_WAITALL
			    	}
			    }
			    
			//cout << "valread: " << valread << endl;
			//for (int i=0; i<(NumSockets); i++){
			//	cout << "socket_fd_conn: " << socket_fd_conn << endl;
			//    	cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
			//    	if (socket_fd_conn==socket_fdArray[i]){
			//cout << "Host " << string(this->SCmode[i]) << " this->ReadBuffer: " << this->ReadBuffer << endl;
			//}
			//}
			}
    			else{valread = recv(socket_fd_conn, this->ReadBuffer,NumBytesBufferICPMAX,MSG_DONTWAIT);}
			}
		//cout << "valread: " << valread << endl;
		//cout << "Host message received: " << this->ReadBuffer << endl;
		if (valread <= 0){
			if (valread<0){
				cout << strerror(errno) << endl;
				for (int i=0; i<(NumSockets); i++){
				//cout << "socket_fd_conn: " << socket_fd_conn << endl;
			    	//cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
			    	if (socket_fd_conn==socket_fdArray[i]){
				cout << "Host " << string(this->SCmode[i]) << " error reading new messages" << endl;
				}
				}
				
			}
			else{
				cout << strerror(errno) << endl;
				cout << "Host agent message of 0 Bytes" << endl;				
			}
			// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
			memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
			this->m_exit();
			return -1;			
		}
		// Process the message
		else{// (valread>0){
			//cout << "Host Received message: " << this->ReadBuffer << endl;
			//cout << "valread: " << valread << endl;
			return valread;
		}
	}
	else{cout << "Host FD_ISSET error new messages" << endl;this->m_exit();return -1;}
   }

}

bool QTLAH::isSocketWritable(int sock) {
if (string(SOCKtype)=="SOCK_DGRAM"){ 
for (int i=0; i<(NumSockets); i++){
    	//cout << "IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
    	if (sock==socket_fdArray[i]){
    		//cout << "socket_fd_conn: " << socket_fd_conn << endl;
    		//cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
    		//cout << "IPaddressesSockets: " << IPaddressesSockets << endl;
    		sock=socket_SendUDPfdArray[i];
 	}
 }
 }
    
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    // Set timeout to zero for non-blocking check
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // Check if the socket is writable
    int select_res = select(sock + 1, nullptr, &write_fds, nullptr, &timeout);

    // select_res > 0 means the socket is writable
    return select_res > 0;
}

int QTLAH::ICPmanagementSend(int socket_fd_conn,char* IPaddressesSockets) {
//cout << "Host SendBuffer: " << this->SendBuffer << endl;
//cout << "Host SendBuffer IPaddressesSockets: " << IPaddressesSockets << endl;
	
	//if (!this->isSocketWritable(socket_fd_conn)){//Reconnect socket
	//	cout << "Host socket ICPmanagementSend not writable!" << endl;
	//}
    const char* SendBufferAux = this->SendBuffer;
    //cout << "SendBufferAux: " << SendBufferAux << endl;
    int BytesSent=0;
    if (string(SOCKtype)=="SOCK_DGRAM"){
	int socket_fd = 0;
	    
	    struct sockaddr_in destaddr;   
	    memset(&destaddr, 0, sizeof(destaddr)); 
	       
	    // Filling server information 
	    destaddr.sin_family = AF_INET; 
	    destaddr.sin_port = htons(PORT); 
	    destaddr.sin_addr.s_addr = inet_addr(IPaddressesSockets); 
	    
	    for (int i=0; i<(NumSockets); i++){
	    	//cout << "IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
	    	if (socket_fd_conn==socket_fdArray[i]){
	    		//cout << "socket_fd_conn: " << socket_fd_conn << endl;
	    		//cout << "socket_fdArray[i]: " << socket_fdArray[i] << endl;
	    		//cout << "IPaddressesSockets: " << IPaddressesSockets << endl;
	    		socket_fd=socket_SendUDPfdArray[i];
	    		BytesSent=sendto(socket_fd,SendBufferAux,strlen(SendBufferAux),MSG_DONTWAIT,(const struct sockaddr *) &destaddr,sizeof(destaddr));//MSG_CONFIRM
	    	}
	    }	    
    }
    else{BytesSent=send(socket_fd_conn, SendBufferAux, strlen(SendBufferAux),MSG_DONTWAIT);}
    
    if (BytesSent<0){
    	perror("send");
    	cout << "ICPmanagementSend: Errors sending Bytes" << endl;
    }
    /*
    // Check if file descriptor is only readable
    	int flags = fcntl(socket_fd, F_GETFL);

	if (flags & O_RDONLY) {
	  printf("File descriptor is read-only\n");
	} else {
	  printf("File descriptor is not read-only\n");
	}
	*/

    return 0; // All OK
}

int QTLAH::ICPmanagementCloseClient(int socket_fd) {
    // closing the connected socket
    close(socket_fd);
 
    return 0; // All OK
}

int QTLAH::ICPmanagementCloseServer(int socket_fd,int new_socket) {
if (string(SOCKtype)=="SOCK_STREAM"){
    // closing the connected socket
    close(new_socket);
}
    // closing the listening socket
    close(socket_fd);
    
    return 0; // All OK
}

int QTLAH::RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep){
struct timespec ts;
ts.tv_sec=(int)(TimeNanoSecondsSleep/((long)1000000000));
ts.tv_nsec=(long)(TimeNanoSecondsSleep%(long)1000000000);
clock_nanosleep(CLOCK_TAI, 0, &ts, NULL); //

return 0; // All ok
}

int QTLAH::RegularCheckToPerform(){
if (iIterPeriodicTimerVal>MaxiIterPeriodicTimerVal){
	// First thing to do is to know if the node below is PRU hardware synch
	if (GPIOnodeHardwareSynched==false){// Ask the node
		char ParamsCharArray[NumBytesBufferICPMAX] = {0};
		strcpy(ParamsCharArray,this->IPaddressesSockets[0]);
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,this->IPaddressesSockets[1]);
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"Control");
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"HardwareSynchNode");
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"none");
		strcat(ParamsCharArray,",");// Very important to end the message
		//cout << "Host sent HardwareSynchNode" << endl;
		this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
	}

	// Second thing to do is to network synchronize the below node, when at least the node is PRU hardware synch
	//cout << "GPIOnodeHardwareSynched: " << GPIOnodeHardwareSynched << endl;
	//cout << "GPIOnodeNetworkSynched: " << GPIOnodeNetworkSynched << endl;
	//cout << "HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
	if (GPIOnodeHardwareSynched==true and GPIOnodeNetworkSynched==false and HostsActiveActionsFree[0]==true){
		cout << "Host synching node to the network!" << endl;

		char argsPayloadAux[NumBytesBufferICPMAX] = {0};
		for (int iConnHostsNodes=0;iConnHostsNodes<NumConnectedHosts;iConnHostsNodes++){
			if (iConnHostsNodes==0){strcpy(argsPayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);}
			else{strcat(argsPayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);}
			strcat(argsPayloadAux,",");
		}
		//cout << "argsPayloadAux: " << argsPayloadAux << endl;
		this->WaitUntilActiveActionFree(argsPayloadAux,NumConnectedHosts);
		if (AchievedAttentionParticularHosts==true){
			this->PeriodicRequestSynchsHost();
			
			if (InitialNetworkSynchPass<1){//the very first time, two rounds are needed to achieve a reasonable network synchronization
				GPIOnodeNetworkSynched=false;// Do not Update value as synched
				InitialNetworkSynchPass=InitialNetworkSynchPass+1;
			}
			else{
				GPIOnodeNetworkSynched=true;// Update value as synched
			}
			
			this->UnBlockActiveActionFree(argsPayloadAux,NumConnectedHosts);
			iIterNetworkSynchcurrentTimerVal=0;// Reset value
			cout << "Host synched node to the network!" << endl;
		}
	}
	else{
		iIterNetworkSynchcurrentTimerVal=iIterNetworkSynchcurrentTimerVal+1; // Update value
	}

	if (iIterNetworkSynchcurrentTimerVal>MaxiIterNetworkSynchcurrentTimerVal){// Every some iterations re-synch the node thorugh the network
		GPIOnodeHardwareSynched=false;// Update value as not synched
		GPIOnodeNetworkSynched=false;// Update value as not synched
		iIterNetworkSynchcurrentTimerVal=0;// Reset value
		cout << "Host will re-synch node to the network!" << endl;
	}

	// Other task to perform at some point or regularly
	
	iIterPeriodicTimerVal=0;// Reset variable
}
iIterPeriodicTimerVal++;
return 0; // all ok
}

void QTLAH::AgentProcessRequestsPetitions(){// Check next thing to do
 // One of the firsts things to do for a host is to initialize ICP socket connection with it host or with its attached nodes.
 this->InitiateICPconnections(); // Very important that they work. Otherwise the rest go wrong
 // Then negotiate some parameters
 
 /*
 cout << "this->SCmode[1]: " << this->SCmode[1] << endl;
 cout << "this->IPaddressesSockets[0]: " << this->IPaddressesSockets[0] << endl;
 cout << "this->IPaddressesSockets[1]: " << this->IPaddressesSockets[1] << endl;
 cout << "this->IPaddressesSockets[2]: " << this->IPaddressesSockets[2] << endl;
 cout << "this->IPaddressesSockets[3]: " << this->IPaddressesSockets[3] << endl;
 cout << "this->IPSocketsList[0]: " << this->IPSocketsList[0] << endl;
 cout << "this->IPSocketsList[1]: " << this->IPSocketsList[1] << endl;
 */
 //
 this->m_pause(); // Initiate in paused state.
 cout << "Starting in pause state the QtransportLayerAgentH" << endl;
 int sockKeepAlivecounter=0;// To send periodic heart beats through sockets to keep them alive
 bool isValidWhileLoop = true;
 //unsigned int CheckCounter=0;
 while(isValidWhileLoop){
 //cout << "CheckCounter: " << CheckCounter << endl;
 //CheckCounter++;
 try{
   try {
   	this->acquire();// Wait semaphore until it can proceed
    	// Code that might throw an exception
 	// Check if there are need messages or actions to be done by the node 	
 	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
 	this->RegularCheckToPerform();// Every now and then some checks have to happen
       switch(this->getState()) {
           case QTLAH::APPLICATION_RUNNING: {               
               // Do Some Work
               this->ProcessNewMessage();
               //while(this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard)>0);// Make sure to remove all pending mesages in the socket
               this->m_pause(); // After procesing the request, pass to paused state
               break;
           }
           case QTLAH::APPLICATION_PAUSED: {
               // Wait Till You Have Focus Or Continues
               // Maybe do some checks if necessary   
               break;
           }
           case QTLAH::APPLICATION_EXIT: {    
               cout << "Exiting the QtransportLayerAgentH" << endl;
               isValidWhileLoop=false;//break;
           }
           default: {
               // ErrorHandling Throw An Exception Etc.
           }

        } // switch
        //if (sockKeepAlivecounter>=SOCKkeepaliveTime){sockKeepAlivecounter=0;this->SendKeepAliveHeartBeatsSockets();}
        //else{sockKeepAlivecounter++;}
        this->release(); // Release the semaphore
        //if (signalReceivedFlag.load()){this->~QTLAH();}// Destroy the instance
        //cout << "(int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)): " << (int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)) << endl;
        //usleep((int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
        this->RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few nanoseconds for other processes to enter
    }
    catch (const std::exception& e) {
	// Handle the exception
    	cout << "Exception: " << e.what() << endl;
    }
    } // upper try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
    }
    } // while
   
}

int QTLAH::ICPConnectionsCheckNewMessages(int SockListenTimeusec){// Read one message at a time and from the different sockets
int socket_fd_conn=0;
if (string(this->SCmode[this->socketReadIter])==string("client") or string(SOCKtype)=="SOCK_DGRAM"){// Client sends on the file descriptor (applies also to UDP)
	socket_fd_conn=this->socket_fdArray[this->socketReadIter];
}
else{// server checks on the socket connection
	socket_fd_conn=this->new_socketArray[this->socketReadIter];
}

if(this->ICPmanagementRead(socket_fd_conn,SockListenTimeusec)>0){this->m_start();} // Process the message

// Update the socketReadIter
this->socketReadIter++; // Variable to read each time a different socket
this->socketReadIter=this->socketReadIter % NumSockets;

return 0; // All OK
}

int QTLAH::ProcessNewMessage(){
//cout << "Host ReadBuffer: " << this->ReadBuffer << endl;
// Parse the message information
char ReadBufferAuxOriginal[NumBytesBufferICPMAX] = {0};
strcpy(ReadBufferAuxOriginal,this->ReadBuffer); // Otherwise the strtok puts the pointer at the end and then ReadBuffer is empty

int NumQintupleComas=this->countQintupleComas(ReadBufferAuxOriginal);
if (NumQintupleComas>20){NumQintupleComas=NumProcessMessagesConcurrentMax;}// Limit the total number that can be proceessed, avoiding cascades
//cout << "NumQintupleComas: " << NumQintupleComas << endl;
//NumQintupleComas=1;
for (int iIterMessages=0;iIterMessages<NumQintupleComas;iIterMessages++){
	char IPdest[NumBytesBufferICPMAX] = {0};
	char IPorg[NumBytesBufferICPMAX] = {0};
	char Type[NumBytesBufferICPMAX] = {0};
	char Command[NumBytesBufferICPMAX] = {0};
	char Payload[NumBytesBufferICPMAX] = {0};
	char ReadBufferAux[NumBytesBufferICPMAX] = {0};
	strcpy(ReadBufferAux,ReadBufferAuxOriginal); // Otherwise the strtok puts the pointer at the end and then ReadBuffer is empty
	for (int iIterDump=0;iIterDump<(5*iIterMessages);iIterDump++){
		if (iIterDump==0){strtok(ReadBufferAux,",");}
		else{strtok(NULL,",");}
	}
	if (iIterMessages==0){strcpy(IPdest,strtok(ReadBufferAux,","));}
	else{strcpy(IPdest,strtok(NULL,","));}
	strcpy(IPorg,strtok(NULL,","));
	strcpy(Type,strtok(NULL,","));
	strcpy(Command,strtok(NULL,","));
	strcpy(Payload,strtok(NULL,","));
	
	//cout << "NumQintupleComas: " << NumQintupleComas << endl;
	//cout << "iIterMessages: " << iIterMessages << endl;
	//cout << "IPdest: " << IPdest << endl;
	//cout << "IPorg: " << IPorg << endl;
	//cout << "Type: " << Type << endl;
	//cout << "Command: " << Command << endl;
	//cout << "Payload: " << Payload << endl;

	// Identify what to do and execute it
	if (string(Type)==string("Operation")){// Operation message. 
		//cout << "this->ReadBuffer: " << this->ReadBuffer << endl;
		if(string(IPdest)==string(this->IPaddressesSockets[1]) or string(IPdest)==string(this->IPaddressesSockets[2])){// Information for this host
			if (string(Command)==string("SimulateNumStoredQubitsNode")){// Reply message. Expected/awaiting message
				//cout << "We are here NumStoredQubitsNode" << endl;
				//cout << "Payload: " << Payload << endl;
				int NumSubPayloads=this->countColons(Payload);
				//cout << "NumSubPayloads: " << NumSubPayloads << endl;
				char SubPayload[NumBytesBufferICPMAX] = {0};						
				for (int i=0;i<NumSubPayloads;i++){					
					if (i==0){						
						strcpy(SubPayload,strtok(Payload,":"));
						this->SimulateNumStoredQubitsNodeParamsIntArray[0]=atoi(SubPayload);
						//cout << "atoi(SubPayload): " << atoi(SubPayload) << endl;
					}
					else{
						strcpy(SubPayload,strtok(NULL,":"));
						this->TimeTaggsDetAnalytics[i-1]=stod(SubPayload);
						//cout << "stof(SubPayload): " << stod(SubPayload) << endl;
					}
				}
								
				this->InfoSimulateNumStoredQubitsNodeFlag=true;
				//cout << "SimulateNumStoredQubitsNode finished parsing values" << endl;		
			}
			else if (string(Command)==string("SimulateSynchParamsNode")){// Reply message. Expected/awaiting message
				//cout << "We are here SimulateSynchParamsNode" << endl;
				//cout << "Payload: " << Payload << endl;
				int NumSubPayloads=this->countColons(Payload);
				//cout << "NumSubPayloads: " << NumSubPayloads << endl;
				char SubPayload[NumBytesBufferICPMAX] = {0};						
				for (int i=0;i<(NumSubPayloads-1);i++){				
					if (i==0){						
						strcpy(SubPayload,strtok(Payload,":"));
					}
					else{
						strcpy(SubPayload,strtok(NULL,":"));
					}
					this->TimeTaggsDetSynchParams[i]=stod(SubPayload);
					//cout << "stod(SubPayload): " << stod(SubPayload) << endl;
				}
								
				this->InfoSimulateNumStoredQubitsNodeFlag=true;
				//cout << "SimulateSynchParamsNode finished parsing values" << endl;		
			}
			else if (string(Command)==string("HardwareSynchNode")){
				if (string(Payload)==string("true")){
					// If received is because node is hardware synched
					GPIOnodeHardwareSynched=true;
					cout << "Node hardware synched. Proceed with the network synchronization..." << endl;
				}
				else{
					// If received is because node is NOT hardware synched
					GPIOnodeHardwareSynched=false;
					cout << "Node hardware NOT synched..." << endl;
				}
			}
			else if (string(Command)==string("HostAreYouFree")){// Operations regarding availability request by other hosts
				// Different operation with respect the host that has sent the request				
				if (string(InfoRemoteHostActiveActions[0])==string(IPorg)){// Is the message from the current active host?
					if (string(Payload)==string("Block")){// Confirm block
						strcpy(InfoRemoteHostActiveActions[1],"Block");// Set status to Block
					}
					else{// UnBlock
						strcpy(InfoRemoteHostActiveActions[0],"\0");// Clear active host
						strcpy(InfoRemoteHostActiveActions[1],"\0");// Clear status
						HostsActiveActionsFree[0]=true; // Set the host as free
					}
				}
				else{// Either the message is not from the current active hosts or there is no active host, or it is a response for this host
					if (string(Payload)==string("true") or string(Payload)==string("false")){// Response from another host
						if (string(Payload)==string("true")){					HostsActiveActionsFree[1+NumAnswersOtherHostsActiveActionsFree]="true";}
						else{HostsActiveActionsFree[1+NumAnswersOtherHostsActiveActionsFree]="false";}
						NumAnswersOtherHostsActiveActionsFree=NumAnswersOtherHostsActiveActionsFree+1;// Update value
						//cout << "Response HostAreYouFree: " << IPorg << ", " << Payload << endl;
					}
					else if (HostsActiveActionsFree[0]==true and string(Payload)==string("Preventive") and GPIOnodeHardwareSynched==true){// Preventive block
						strcpy(InfoRemoteHostActiveActions[0],IPorg);// Copy the identification of the host
						strcpy(InfoRemoteHostActiveActions[1],"Preventive");// Set status to Preventive
						HostsActiveActionsFree[0]=false; // Set the host as not free
						// Respond with message saying that available
						char ParamsCharArray[NumBytesBufferICPMAX] = {0};
						strcpy(ParamsCharArray,IPorg);// Send to what was the origin
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,IPdest);// From what was the origin
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,Type);
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,Command);
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,"true");
						strcat(ParamsCharArray,",");// Very important to end the message
						//cout << "Operation HostAreYouFree Preventive" << endl;	
						this->ICPdiscoverSend(ParamsCharArray);
					}
					else if (HostsActiveActionsFree[0]==false and string(Payload)==string("Preventive")){// Not free. Respond that not free
						// Respond with message saying that not available
						// Respond with message saying that available
						char ParamsCharArray[NumBytesBufferICPMAX] = {0};
						strcpy(ParamsCharArray,IPorg);// Send to what was the origin
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,IPdest);// From what was the origin
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,Type);
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,Command);
						strcat(ParamsCharArray,",");
						strcat(ParamsCharArray,"false");
						strcat(ParamsCharArray,",");// Very important to end the message
						//cout << "Operation Message forward ParamsCharArray: " << ParamsCharArray << endl;	
						this->ICPdiscoverSend(ParamsCharArray);
					}
					else if (string(Payload)==string("UnBlock")){// Apparently not needed, but just in case
						strcpy(InfoRemoteHostActiveActions[0],"\0");// Clear active host
						strcpy(InfoRemoteHostActiveActions[1],"\0");// Clear status
						HostsActiveActionsFree[0]=true; // Set the host as free
					}
					else{// Non of the above, which is a malfunction
						cout << "Host HostAreYouFree not handled!" << endl;
					}
				}
			
			}
			else if (string(Command)==string("print")){
				cout << "Host New Message: "<< Payload << endl;
			}				
			else{
				cout << "Host does not have this Operational message: "<< Command << endl;
			}			
		}
		else if (string(IPdest)!=string(this->IPaddressesSockets[1]) and string(IPdest)!=string(this->IPaddressesSockets[2])){// Message not for this host, forward it			
			char ParamsCharArray[NumBytesBufferICPMAX] = {0};
			strcpy(ParamsCharArray,IPdest);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,IPorg);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,Type);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,Command);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,Payload);
			strcat(ParamsCharArray,",");// Very important to end the message
			//cout << "Operation Message forward ParamsCharArray: " << ParamsCharArray << endl;	
			this->ICPdiscoverSend(ParamsCharArray);
		}				
		else{//Default
			cout << "Operational message to host not handled: "<< Payload << endl;
		}
	}
	else if(string(Type)==string("Control")){//Control message are not meant for host, so forward it accordingly
		if (string(IPorg)==string(this->IPaddressesSockets[0])){ // If it comes from its attached node and is a control message then it is not for this host, forward it to other hosts' nodes
		// The node of a host is always identified in the Array in position 0
		    //cout << "Host retransmit params from node: " << Payload << endl;			    
		    int socket_fd_conn;
		    // Mount message
		    // Notice that nodes Control (and only Control messages, Operational messages do not have this) messages meant to other nodes through their respective hosts have a composed payload, where the first part has the other host's node to whom the message has to be forwarded (at least up to its host - the host of the node sending the message). Hence, the Payload has to be processed a bit.
		    for (int iIterOpHost=0;iIterOpHost<2;iIterOpHost++){// Manually set - This should be programmed in order to scale
		    	    char PayLoadReassembled[NumBytesPayloadBuffer] = {0};
		    	    char PayLoadProc1[NumBytesPayloadBuffer] = {0};// To process the "_" level
		    	    char PayLoadProc1Aux[NumBytesPayloadBuffer] = {0};// To process the "_" level Aux
		    	    char PayloadAux[NumBytesPayloadBuffer] = {0};
			    // Process the payload first
			    char IPaddressesSocketsAux[IPcharArrayLengthMAX] = {0};
			    strcpy(IPaddressesSocketsAux,this->IPaddressesSockets[iIterOpHost+3]);
			    bool NonEmptyPayloadAux=false;
			    // Analyze if there are messages for host this->IPaddressesSockets[iIterOpHost+3]
			    // Payload message is like: Trans;Header_Payload1:Payload2:_Header_Payload_;Header_Payload_Header_Payload_;Net;none_none_;Link;none_none_;Phys;none_none_; 
			    
			    for (int iIterNodeAgents=0;iIterNodeAgents<4;iIterNodeAgents++){// Discard the first Layer Name = Trans
			        strcpy(PayloadAux,Payload);// Make a copy of the original Payload
			    	for (int iIterPayloadDump=0;iIterPayloadDump<(2*iIterNodeAgents);iIterPayloadDump++){// Reposition the pointer of strtok
					if (iIterPayloadDump==0){strtok(PayloadAux,";");}
					else{strtok(NULL,";");}
				}
			    	if (iIterNodeAgents==0){
			    		strcpy(PayLoadReassembled,strtok(PayloadAux,";"));
			    	}
			    	else{// Discard the following Layer Names = Net; Link; Phys
			    		strcat(PayLoadReassembled,strtok(NULL,";"));			    		
			    	}
			    	strcat(PayLoadReassembled,";");// Finish with semicolon
			    	strcpy(PayLoadProc1,strtok(NULL,";"));// To process the "_" level
			    	if (string(PayLoadProc1)!=string("") and string(PayLoadProc1)!=string("none_none_")){// There is data to process
			    		int NumSubPayloads=countQuadrupleUnderscores(PayLoadProc1);//
			    		//cout << "NumSubPayloads: " << NumSubPayloads << endl;
			    		//cout << "PayLoadProc1: " << PayLoadProc1 << endl;
			    		int NumInterestSubPayloads=0;
			    		for (int iIterSubPayloads=0;iIterSubPayloads<NumSubPayloads;iIterSubPayloads++){
			    			if (iIterSubPayloads==0){strtok(PayLoadProc1,"_");}// Discard because it should be IPdest
			    			else{strtok(NULL,"_");}// Discard because it should be IPdest
			    			strcpy(PayLoadProc1Aux,strtok(NULL,"_"));// Indicates the IP	
			    			//cout << "PayLoadProc1Aux: " << PayLoadProc1Aux << endl;			    						    				    			
			    			if (string(IPaddressesSocketsAux)==string(PayLoadProc1Aux)){// Param message meant for the host's node of current interest
			    				strcat(PayLoadReassembled,strtok(NULL,"_"));
			    				strcat(PayLoadReassembled,"_");	// Finish with underscore
			    				strcat(PayLoadReassembled,strtok(NULL,"_"));
			    				strcat(PayLoadReassembled,"_");	// Finish with underscore
			    				NumInterestSubPayloads++;
			    				NonEmptyPayloadAux=true;
				    		}
				    		else{// Discard params because not meant for the IP of interest
				    			strtok(NULL,"_");
				    			strtok(NULL,"_");
				    		}
			    		}
			    		if (NumInterestSubPayloads==0){// Just put "none_none_" if empty; none of the parameters was for the IP checked
			    			strcat(PayLoadReassembled,"none_none_");
			    		}
			    		
			    	}
			    	else{// Just copy "none_none_"
			    		strcat(PayLoadReassembled,PayLoadProc1);			    		
			    	}
			    	strcat(PayLoadReassembled,";");	// Finish with semicolon	    			    				    
			    }
			    //cout << "Host's node Control Message original: " << Payload << endl;
			    //cout << "Host's node Control Message reassembled: " << PayLoadReassembled << endl;
			    
			    // Actually mount the reassembled message
			    char ParamsCharArray[NumBytesBufferICPMAX] = {0};
				strcpy(ParamsCharArray,IPaddressesSocketsAux);//IPdest);
				strcat(ParamsCharArray,",");
				strcat(ParamsCharArray,IPorg);
				strcat(ParamsCharArray,",");
				strcat(ParamsCharArray,Type);
				strcat(ParamsCharArray,",");
				strcat(ParamsCharArray,Command);
				strcat(ParamsCharArray,",");
				strcat(ParamsCharArray,PayLoadReassembled);//Payload);
				strcat(ParamsCharArray,",");// Very important to end the message
				//cout << "Node message to redirect at host ParamsCharArray: " << ParamsCharArray << endl;
				//cout << "IPaddressesSocketsAux: " << IPaddressesSocketsAux << endl;
			if(NonEmptyPayloadAux){// Non-empty, hence send
			    strcpy(this->SendBuffer,ParamsCharArray);			
			    if (string(this->SCmode[1])==string("client") or string(SOCKtype)=="SOCK_DGRAM"){//host acts as client
				    socket_fd_conn=this->socket_fdArray[1];   // host acts as client to the other host, so it needs the socket descriptor (it applies both to TCP and UDP) 
				    this->ICPmanagementSend(socket_fd_conn,IPaddressesSocketsAux);//this->IPaddressesSockets[3]);
			    }
			    else{ //host acts as server		    
				    socket_fd_conn=this->new_socketArray[1];  // host acts as server to the other host, so it needs the socket connection   
				    this->ICPmanagementSend(socket_fd_conn,IPaddressesSocketsAux);//this->IPaddressesSockets[3]);
			    }
			    }
		     } // end for	    
		}	
		else{// It does not come from its node and it is a control message, so it has to forward to its node
		   // The node of a host is always identified in the Array in position 0	
		    //cout << "SendBuffer: " << this->SendBuffer << endl;
		    // Mount message
		    char ParamsCharArray[NumBytesBufferICPMAX] = {0};
			strcpy(ParamsCharArray,IPdest);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,IPorg);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,Type);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,Command);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,Payload);
			strcat(ParamsCharArray,",");// Very important to end the message
			//cout << "ParamsCharArray: " << ParamsCharArray << endl;
		    strcpy(this->SendBuffer,ParamsCharArray);
		    int socket_fd_conn=this->socket_fdArray[0];  // the host always acts as client to the node, so it needs the socket descriptor (it applies both to TCP and UDP)
		    this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[0]);
		}  
	}
	else if(string(Type)==string("KeepAlive")){
		//cout << "Message to Host HeartBeat: "<< Payload << endl;
	}
	else{// 
		cout << "Message not handled by host: "<< Payload << endl;
	}  
}// for
// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
return 0; // All OK
}

int QTLAH::ICPdiscoverSend(char* ParamsCharArray){
    memset(this->SendBuffer, 0, sizeof(this->SendBuffer));
    strcpy(this->SendBuffer,ParamsCharArray);//strtok(NULL,","));
    //cout << "SendBuffer: " << this->SendBuffer << endl;	
    // Parse the message information
    char IPaddressesSocketsAux[IPcharArrayLengthMAX];
    strcpy(IPaddressesSocketsAux,strtok(ParamsCharArray,","));//Null indicates we are using the same pointer as the last strtok
    //cout << "IPaddressesSocketsAux: " << IPaddressesSocketsAux << endl;
    // Understand which socket descriptor has to be used
    int socket_fd_conn;
    for (int i=0; i<NumSockets; i++){
    	if (string(this->IPSocketsList[i])==string(IPaddressesSocketsAux)){
    	//cout << "IPaddressesSocketsAux: " << IPaddressesSocketsAux << endl;
    	//cout << "this->IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
    	//cout << "Found socket file descriptor//connection to send" << endl;
    	if (string(this->SCmode[i])==string("client") or string(SOCKtype)=="SOCK_DGRAM"){// Client sends on the file descriptor
    		socket_fd_conn=this->socket_fdArray[i];// it applies both to TCP and UDP
    	}
    	else{// server sends on the socket connection    		
    		//cout << "socket_fd_conn" << socket_fd_conn << endl;
    		socket_fd_conn=this->new_socketArray[i];
    	}
    	}
    }  
    this->ICPmanagementSend(socket_fd_conn,IPaddressesSocketsAux);   
return 0; //All OK
}
///////////////////////////////////////////////////////////////////
// Request methods
// The blocking requests from host to the node are ok because the node is not to sent request to the host (following the OSI pile methodology). At most, there will be request of retransmission to the other host's node, which have to be let pass
int QTLAH::SendMessageAgent(char* ParamsDescendingCharArray){
// Code that might throw an exception
    //cout << "Before acquire valueSemaphore: " << valueSemaphore << endl;
    //cout << "Before acquire valueSemaphoreExpected: " << valueSemaphoreExpected << endl;
    this->acquire();// Wait semaphore until it can proceed
    //cout << "After acquire valueSemaphore: " << valueSemaphore << endl;
    //cout << "After acquire valueSemaphoreExpected: " << valueSemaphoreExpected << endl;
    try{
    this->ICPdiscoverSend(ParamsDescendingCharArray);
    } // try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
    }
    this->release(); // Release the semaphore 
    return 0; //All OK
}

int QTLAH::SimulateRetrieveNumStoredQubitsNode(char* IPhostReply,char* IPhostRequest, int* ParamsIntArray,int nIntarray,double* ParamsDoubleArray,int nDoublearray){ // Send to the upper layer agent how many qubits are stored
this->RelativeNanoSleepWait((unsigned int)((unsigned int)1000*(unsigned int)WaitTimeAfterMainWhileLoop));//usleep((int)(5000*WaitTimeAfterMainWhileLoop));// Wait initially because this method does not need to send/receive message compared ot others like send or receive qubits, and then it happens that it executes first sometimes. This can be improved by sending messages to the specific node, and this node replying that has received the detection command, then this could start
this->acquire();
// It is a "blocking" communication between host and node, because it is many read trials for reading
while(this->SimulateRetrieveNumStoredQubitsNodeFlag==true){//Wait, only one asking
this->release();
this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//usleep((int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));
this->acquire();
}
this->SimulateRetrieveNumStoredQubitsNodeFlag=true;
int isValidWhileLoopCount = 10; // Number of tries If it needs more than one trial is because the sockets are not working correctly. It is best to check for open sockets and kill the processes taking hold of them
this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
while(isValidWhileLoopCount>0){
	if (isValidWhileLoopCount % 10==0){// Only try to resend the message once every 10 times
	char ParamsCharArray[NumBytesBufferICPMAX] = {0};
	strcpy(ParamsCharArray,IPhostReply);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,IPhostRequest);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"Control");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"InfoRequest");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"SimulateNumStoredQubitsNode");
	strcat(ParamsCharArray,",");// Very important to end the message
	//cout << "SimulateRetrieveNumStoredQubitsNode ParamsCharArray: " << ParamsCharArray << endl;
	this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
	}
	this->release();
	this->RelativeNanoSleepWait((unsigned int)(1000*(unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));//usleep((int)(500*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give some time to have the chance to receive the response
	this->acquire();
	if (this->InfoSimulateNumStoredQubitsNodeFlag==true){
		//cout << "We received info for SimulateRetrieveNumStoredQubitsNode" << endl;
		ParamsIntArray[0]=this->SimulateNumStoredQubitsNodeParamsIntArray[0];
		ParamsDoubleArray[0]=this->TimeTaggsDetAnalytics[0];
		ParamsDoubleArray[1]=this->TimeTaggsDetAnalytics[1];
		ParamsDoubleArray[2]=this->TimeTaggsDetAnalytics[2];
		ParamsDoubleArray[3]=this->TimeTaggsDetAnalytics[3];
		ParamsDoubleArray[4]=this->TimeTaggsDetAnalytics[4];
		ParamsDoubleArray[5]=this->TimeTaggsDetAnalytics[5];
		ParamsDoubleArray[6]=this->TimeTaggsDetAnalytics[6];
		ParamsDoubleArray[7]=this->TimeTaggsDetAnalytics[7];
		
		this->SimulateRetrieveNumStoredQubitsNodeFlag=false;
		this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
		this->release();			
		isValidWhileLoopCount=0;
	}
	else{
		// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
		//memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
		ParamsIntArray[0]=-1;
		isValidWhileLoopCount--;
		if (isValidWhileLoopCount<=1){// Finish at 1, so that a query message is ot send again without handling the eventual answer
			cout << "Host did not achieve to RetrieveNumStoredQubitsNode" << endl;
			this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
			this->SimulateRetrieveNumStoredQubitsNodeFlag=false;
			this->release();
		}
	}
}//while

return 0; // All OK
}

int QTLAH::SimulateRetrieveSynchParamsNode(char* IPhostReply,char* IPhostRequest,double* ParamsDoubleArray,int nDoublearray){ // Send to the upper layer agent how many qubits are stored
this->RelativeNanoSleepWait((unsigned int)((unsigned int)1000*(unsigned int)WaitTimeAfterMainWhileLoop));//usleep((int)(5000*WaitTimeAfterMainWhileLoop));// Wait initially because this method does not need to send/receive message compared ot others like send or receive qubits, and then it happens that it executes first sometimes. This can be improved by sending messages to the specific node, and this node replying that has received the detection command, then this could start
this->acquire();
// It is a "blocking" communication between host and node, because it is many read trials for reading
while(this->SimulateRetrieveNumStoredQubitsNodeFlag==true){//Wait, only one asking
this->release();
this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//usleep((int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));
this->acquire();
}
this->SimulateRetrieveNumStoredQubitsNodeFlag=true;
int isValidWhileLoopCount = 10; // Number of tries If it needs more than one trial is because the sockets are not working correctly. It is best to check for open sockets and kill the processes taking hold of them
this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
while(isValidWhileLoopCount>0){
	if (isValidWhileLoopCount % 10==0){// Only try to resend the message once every 10 times
	char ParamsCharArray[NumBytesBufferICPMAX] = {0};
	strcpy(ParamsCharArray,IPhostReply);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,IPhostRequest);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"Control");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"InfoRequest");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"SimulateRetrieveSynchParamsNode");
	strcat(ParamsCharArray,",");// Very important to end the message
	//cout << "SimulateRetrieveSynchParamsNode ParamsCharArray: " << ParamsCharArray << endl;
	this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
	}
	this->release();
	this->RelativeNanoSleepWait((unsigned int)(1000*(unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));//usleep((int)(500*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give some time to have the chance to receive the response
	this->acquire();
	if (this->InfoSimulateNumStoredQubitsNodeFlag==true){
		//cout << "We received info for SimulateRetrieveNumStoredQubitsNode" << endl;
		ParamsDoubleArray[0]=this->TimeTaggsDetSynchParams[0];
		ParamsDoubleArray[1]=this->TimeTaggsDetSynchParams[1];
		ParamsDoubleArray[2]=this->TimeTaggsDetSynchParams[2];
		
		this->SimulateRetrieveNumStoredQubitsNodeFlag=false;
		this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
		this->release();			
		isValidWhileLoopCount=0;
	}
	else{
		// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
		//memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
		ParamsDoubleArray[0]=0.0;
		ParamsDoubleArray[1]=0.0;
		ParamsDoubleArray[2]=0.0;
		isValidWhileLoopCount--;
		if (isValidWhileLoopCount<=1){// Finish at 1, so that a query message is ot send again without handling the eventual answer
			cout << "Host did not achieve to RetrieveSynchParamsNode" << endl;
			this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
			this->SimulateRetrieveNumStoredQubitsNodeFlag=false;
			this->release();
		}
	}
}//while

return 0; // All OK
}

int QTLAH::SendKeepAliveHeartBeatsSockets(){
for (int i=0;i<NumSockets;i++){
	char ParamsCharArray[NumBytesBufferICPMAX] = {0};
	strcpy(ParamsCharArray,this->IPSocketsList[i]);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,this->IPaddressesSockets[1]);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"KeepAlive");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"HeartBeat");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"none");
	strcat(ParamsCharArray,",");// Very important to end the message
	//cout << "SendKeepAliveHeartBeatsSockets ParamsCharArray: " << ParamsCharArray << endl;
	this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
}

return 0; // all ok
}

int QTLAH::WaitUntilActiveActionFreePreLock(char* ParamsCharArrayArg, int nChararray){
this->acquire();
while (HostsActiveActionsFree[0]==false and GPIOnodeHardwareSynched==false){// Wait here// No other thread checking this info
this->release();this->RelativeNanoSleepWait((unsigned int)(150*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();
}
while(AchievedAttentionParticularHosts==false){
	this->WaitUntilActiveActionFree(ParamsCharArrayArg,nChararray);
}
this->release();
return 0; // all ok;

}

int QTLAH::WaitUntilActiveActionFree(char* ParamsCharArrayArg, int nChararray){

this->SequencerAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);

while(IterHostsActiveActionsFreeStatus!=0){
	//cout << "IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
	//cout << "this->getState(): " << this->getState() << endl;
	if(this->getState()==0 and IterHostsActiveActionsFreeStatus==1) {
		this->ProcessNewMessage();
		this->m_pause(); // After procesing the request, pass to paused state
		this->SequencerAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
		//cout << "IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
	}
	else{
		this->SequencerAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
	}
	this->RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few nanoseconds for other processes to enter
}
//cout << "Finished WaitUntilActiveActionFree" << endl;
return 0; // All ok
}

int QTLAH::UnBlockActiveActionFreePreLock(char* ParamsCharArrayArg, int nChararray){
this->acquire();
this->UnBlockActiveActionFree(ParamsCharArrayArg,nChararray);
this->release();
return 0; // All ok
}

int QTLAH::UnBlockActiveActionFree(char* ParamsCharArrayArg, int nChararray){

this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);

return 0; // All ok
}

int QTLAH::SequencerAreYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
if (IterHostsActiveActionsFreeStatus==0){
this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);// To reset, just in case
this->SendAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
}
else if (IterHostsActiveActionsFreeStatus==1){
this->AcumulateAnswersYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
}
else if (IterHostsActiveActionsFreeStatus==2){
AchievedAttentionParticularHosts=this->CheckReceivedAnswersYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
}
else{
this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
}


return 0; // All Ok
}

int QTLAH::SendAreYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
// Three-step handshake
// First block the current host
HostsActiveActionsFree[0]=false;// This host blocked
NumAnswersOtherHostsActiveActionsFree=0;// Reset the number of answers received
ReWaitsAnswersHostsActiveActionsFree=0; // Reset the counter

int NumInterestIPaddressesAux=nChararray;

char interestIPaddressesSocketsAux[static_cast<const int>(nChararray)][IPcharArrayLengthMAX];
char ParamsCharArrayArgAux[NumBytesBufferICPMAX] = {0};
strcpy(ParamsCharArrayArgAux,ParamsCharArrayArg);
for (int i=0;i<NumInterestIPaddressesAux;i++){
	if (i==0){strcpy(interestIPaddressesSocketsAux[i],strtok(ParamsCharArrayArgAux,","));}
	else{strcpy(interestIPaddressesSocketsAux[i],strtok(NULL,","));}
}

char ParamsCharArray[NumBytesBufferICPMAX] = {0};
// Send requests
for (int i=0;i<NumInterestIPaddressesAux;i++){	
	strcpy(ParamsCharArray,interestIPaddressesSocketsAux[i]);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,this->IPaddressesSockets[2]);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"Operation");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"HostAreYouFree");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"Preventive");
	strcat(ParamsCharArray,",");// Very important to end the message
	//cout << "HostAreYouFree Preventive ParamsCharArray: " << ParamsCharArray << endl;
	this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
}
IterHostsActiveActionsFreeStatus=1;
return 0; // All ok
}

int QTLAH::AcumulateAnswersYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
int NumInterestIPaddressesAux=nChararray;
char interestIPaddressesSocketsAux[static_cast<const int>(nChararray)][IPcharArrayLengthMAX];
char ParamsCharArrayArgAux[NumBytesBufferICPMAX] = {0};
strcpy(ParamsCharArrayArgAux,ParamsCharArrayArg);
for (int i=0;i<NumInterestIPaddressesAux;i++){
	if (i==0){strcpy(interestIPaddressesSocketsAux[i],strtok(ParamsCharArrayArgAux,","));}
	else{strcpy(interestIPaddressesSocketsAux[i],strtok(NULL,","));}
}

//IterHostsActiveActionsFreeStatus 0: Not asked this question; 1: Question asked; 2: All questions received; -1: Abort and reset all
if (NumAnswersOtherHostsActiveActionsFree>=NumInterestIPaddressesAux){
	IterHostsActiveActionsFreeStatus=2;
	ReWaitsAnswersHostsActiveActionsFree=0;// Reset the counter
}
else if (ReWaitsAnswersHostsActiveActionsFree<MaxReWaitsAnswersHostsActiveActionsFree){// Increment the wait counter
	ReWaitsAnswersHostsActiveActionsFree=ReWaitsAnswersHostsActiveActionsFree+1;// Update the counter
}
else{// Too many rounds, kill the process of blocking other hosts
	IterHostsActiveActionsFreeStatus=-1;
	ReWaitsAnswersHostsActiveActionsFree=0;// Reset the counter
}

return 0; // All ok
}

bool QTLAH::CheckReceivedAnswersYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
// Check result of the request to all other hosts
bool CheckAllOthersFreeAux=false;
int SumCheckAllOthersFree=0;

int NumInterestIPaddressesAux=nChararray;
char interestIPaddressesSocketsAux[static_cast<const int>(nChararray)][IPcharArrayLengthMAX];
char ParamsCharArrayArgAux[NumBytesBufferICPMAX] = {0};
strcpy(ParamsCharArrayArgAux,ParamsCharArrayArg);
for (int i=0;i<NumInterestIPaddressesAux;i++){
	if (i==0){strcpy(interestIPaddressesSocketsAux[i],strtok(ParamsCharArrayArgAux,","));}
	else{strcpy(interestIPaddressesSocketsAux[i],strtok(NULL,","));}
}

for (int i=0;i<NumInterestIPaddressesAux;i++){
	if (HostsActiveActionsFree[1+i]==true){
		SumCheckAllOthersFree=SumCheckAllOthersFree+1;
	}
}
//cout << "SumCheckAllOthersFree: " << SumCheckAllOthersFree << endl;

if (SumCheckAllOthersFree>=NumInterestIPaddressesAux){CheckAllOthersFreeAux=true;}
char ParamsCharArray[NumBytesBufferICPMAX] = {0};
// If all available, block them all
if (CheckAllOthersFreeAux==true){
	for (int i=0;i<NumInterestIPaddressesAux;i++){
		strcpy(ParamsCharArray,interestIPaddressesSocketsAux[i]);
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,this->IPaddressesSockets[2]);
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"Operation");
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"HostAreYouFree");
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"Block");
		strcat(ParamsCharArray,",");// Very important to end the message
		//cout << "HostAreYouFree ParamsCharArray: " << ParamsCharArray << endl;
		this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
	}
	IterHostsActiveActionsFreeStatus=0;// reset process
	return true;
}
else{ // If some are unavailable, unblock them all
	for (int i=0;i<NumInterestIPaddressesAux;i++){
		strcpy(ParamsCharArray,interestIPaddressesSocketsAux[i]);
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,this->IPaddressesSockets[2]);
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"Operation");
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"HostAreYouFree");
		strcat(ParamsCharArray,",");
		strcat(ParamsCharArray,"UnBlock");
		strcat(ParamsCharArray,",");// Very important to end the message
		//cout << "HostAreYouFree ParamsCharArray: " << ParamsCharArray << endl;
		this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
	}
	HostsActiveActionsFree[0]=true;// This host unblocked
	IterHostsActiveActionsFreeStatus=0;// reset process
	return false;
}

return false; // all ok
}

int QTLAH::UnBlockYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
int NumInterestIPaddressesAux=nChararray;
char interestIPaddressesSocketsAux[static_cast<const int>(nChararray)][IPcharArrayLengthMAX];
char ParamsCharArrayArgAux[NumBytesBufferICPMAX] = {0};
strcpy(ParamsCharArrayArgAux,ParamsCharArrayArg);
for (int i=0;i<NumInterestIPaddressesAux;i++){
	if (i==0){strcpy(interestIPaddressesSocketsAux[i],strtok(ParamsCharArrayArgAux,","));}
	else{strcpy(interestIPaddressesSocketsAux[i],strtok(NULL,","));}
}

char ParamsCharArray[NumBytesBufferICPMAX] = {0};
for (int i=0;i<NumInterestIPaddressesAux;i++){
	strcpy(ParamsCharArray,interestIPaddressesSocketsAux[i]);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,this->IPaddressesSockets[2]);
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"Operation");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"HostAreYouFree");
	strcat(ParamsCharArray,",");
	strcat(ParamsCharArray,"UnBlock");
	strcat(ParamsCharArray,",");// Very important to end the message
	//cout << "HostAreYouFree UnBlock ParamsCharArray: " << ParamsCharArray << endl;
	this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
}
HostsActiveActionsFree[0]=true;// This host unblocked
IterHostsActiveActionsFreeStatus=0;// reset process
AchievedAttentionParticularHosts=false;// Indicates that we have got the attenation of the hosts
return 0; // All ok
}

int QTLAH::PeriodicRequestSynchsHost(){

char charNumAux[NumBytesBufferICPMAX] = {0};
char messagePayloadAux[NumBytesBufferICPMAX] = {0};
char ParamsCharArray[NumBytesBufferICPMAX] = {0};
for (int iConnHostsNodes=0;iConnHostsNodes<NumConnectedHosts;iConnHostsNodes++){// For each connected node to be synch with
	for (int iCenterMass=0;iCenterMass<NumCalcCenterMass;iCenterMass++){
		for (int iNumRunsPerCenterMass=0;iNumRunsPerCenterMass<NumRunsPerCenterMass;iNumRunsPerCenterMass++){			
			// First message
			strcpy(messagePayloadAux,"Active");
			strcat(messagePayloadAux,";");
			strcat(messagePayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", NumRunsPerCenterMass);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", iCenterMass);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", iNumRunsPerCenterMass);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[0]);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[1]);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[2]);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			//cout << "PeriodicRequestSynchsHost messagePayloadAux: " << messagePayloadAux << endl;	
			
			strcpy(ParamsCharArray,this->IPaddressesSockets[0]);// Destination, the attached node ConNet
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,this->IPaddressesSockets[1]);// Origin, this host ConNet
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"Control");// Because is for the nodes
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"SimulateReceiveSynchQubits");
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,messagePayloadAux);
			strcat(ParamsCharArray,",");// Very important to end the message		
			this->ICPdiscoverSend(ParamsCharArray);// Without acquire/release
			//this->SendMessageAgent(ParamsCharArray);// With acquire/release
			//cout << "PeriodicRequestSynchsHost ParamsCharArray: " << ParamsCharArray << endl;
			
			// Second message
			strcpy(messagePayloadAux,"Passive");
			strcat(messagePayloadAux,";");
			strcat(messagePayloadAux,this->IPaddressesSockets[2]);
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", NumRunsPerCenterMass);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", iCenterMass);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", iNumRunsPerCenterMass);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", 0.0);// Zero added offset since we are not testing
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", 0.0);// Zero added relative frequency difference offset since we are not testing
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[0]);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[1]);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[2]);
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			
			strcpy(ParamsCharArray,this->IPaddressesSockets[3+iConnHostsNodes]);// Destination, the other host OpNet
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,this->IPaddressesSockets[2]);// Origin, this host OpNet
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"Control");// Because is for the nodes
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"SimulateSendSynchQubits");
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,messagePayloadAux);
			strcat(ParamsCharArray,",");// Very important to end the message		
			this->ICPdiscoverSend(ParamsCharArray);// Without acquire/release
			//this->SendMessageAgent(ParamsCharArray);// With acquire/release
			//cout << "PeriodicRequestSynchsHost ParamsCharArray: " << ParamsCharArray << endl;
			
			// Instead of a simple sleep, which would block the operation (specially processing new messages)
			// usleep(usSynchProcIterRunsTimePoint);// Give time between iterations to send and receive qubits
			int numForstEquivalentToSleep=1000;//1000: Equivalent to 10 seconds#(usSynchProcIterRunsTimePoint*1000)/WaitTimeAfterMainWhileLoop;
			for (int i=0;i<numForstEquivalentToSleep;i++){
				this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
				//cout << "this->getState(): " << this->getState() << endl;
				if(this->getState()==0) {
					this->ProcessNewMessage();
					this->m_pause(); // After procesing the request, pass to paused state
				}
				this->RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop));// Wait a few nanoseconds for other processes to enter
			}
		}
	}
}
return 0; // all ok
}			
				
///////////////////////////////////////////////////////////////////
QTLAH::~QTLAH() {
	// destructor
	this->StopICPconnections();
	this->threadRef.join();// Terminate the process thread
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
