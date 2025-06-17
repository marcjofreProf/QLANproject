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
#define WaitTimeAfterMainWhileLoop 5000000 // nanoseconds
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
// Hard coded order of quad groups channels for automatic periodic synchronization
char ParamsCharArrayEmt[NumBytesBufferICPMAX] = {0};
char ParamsCharArrayDet[NumBytesBufferICPMAX] = {0};
strcpy(ParamsCharArrayEmt,strtok(NULL,","));
strcpy(ParamsCharArrayDet,strtok(NULL,","));
for (int iNumConnectedHosts=0;iNumConnectedHosts<NumConnectedHosts;iNumConnectedHosts++){
	if (iNumConnectedHosts==0){
		this->QTLAHParamsTableDetAutoSynchQuadChEmt[iNumConnectedHosts]=atoi(strtok(ParamsCharArrayEmt,";"));
	}
	else{
		this->QTLAHParamsTableDetAutoSynchQuadChEmt[iNumConnectedHosts]=atoi(strtok(NULL,";"));
	}	
}
for (int iNumConnectedHosts=0;iNumConnectedHosts<NumConnectedHosts;iNumConnectedHosts++){
	if (iNumConnectedHosts==0){
		this->QTLAHParamsTableDetAutoSynchQuadChDet[iNumConnectedHosts]=atoi(strtok(ParamsCharArrayDet,";"));
	}
	else{
		this->QTLAHParamsTableDetAutoSynchQuadChDet[iNumConnectedHosts]=atoi(strtok(NULL,";"));
	}	
}

//cout << "IPaddressesSockets[0]: "<< this->IPaddressesSockets[0] << endl;
//cout << "IPaddressesSockets[1]: "<< this->IPaddressesSockets[1] << endl;
//cout << "SCmode[0]: "<< this->SCmode[0] << endl;
//cout << "SCmode[1]: "<< this->SCmode[1] << endl;
// Initialize some values
for (int i=0;i<(NumConnectedHosts+1);i++){
	HostsActiveActionsFree[i]=true; // Indicate if the hosts are currently free to perform active actions. Index 0 is the host itself, the other indexes are the other remote hosts in the order of IPaddressesSockets starting from position 2	
}
strcpy(InfoRemoteHostActiveActions[0],"\0");
strcpy(InfoRemoteHostActiveActions[1],"\0");
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
	//unsigned long long int ProtectionSemaphoreTrap=0;
	bool valueSemaphoreExpected=true;
	while(true){
	//oldCount = this->valueSemaphore.load(std::memory_order_acquire);
	//if (oldCount > 0 && this->valueSemaphore.compare_exchange_strong(oldCount,oldCount-1,std::memory_order_acquire)){
		//ProtectionSemaphoreTrap++;
		//if (ProtectionSemaphoreTrap>UnTrapSemaphoreValueMaxCounter){this->release();cout << "QTLAH::Releasing semaphore!!!" << endl;}// Avoid trapping situations
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

  int QTLAH::RelativeNanoSleepWait(unsigned long long int TimeNanoSecondsSleep){
  	struct timespec ts;
  	ts.tv_sec=(int)(TimeNanoSecondsSleep/((long)1000000000));
  	ts.tv_nsec=(long)(TimeNanoSecondsSleep%(long)1000000000);
clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL); //

return 0; // All ok
}

int QTLAH::RegularCheckToPerform(){
	if (iIterPeriodicTimerVal>(int)(MaxiIterPeriodicTimerVal*(1.0+(float)rand()/(float)RAND_MAX))){ // Put some randomness here
		// First thing to do is to know if the node below is PRU hardware synch
		if (GPIOnodeHardwareSynched==false and HostsActiveActionsFree[0]==true){// Ask the node
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

		if (GPIOnodeHardwareSynched==true){//} and HostsActiveActionsFree[0]==true){// Ask the node if busy
			char ParamsCharArray[NumBytesBufferICPMAX] = {0};
			strcpy(ParamsCharArray,this->IPaddressesSockets[0]);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,this->IPaddressesSockets[1]);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"Control");
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"BusyNode");
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
		//cout << "Host " << this->IPaddressesSockets[2] << " numHolderOtherNodesSynchNetwork: " << numHolderOtherNodesSynchNetwork << endl;
		if (GPIOnodeHardwareSynched==true and GPIOnodeNetworkSynched==false and HostsActiveActionsFree[0]==true and CycleSynchNetworkDone==false and BusyAttachedNode==false){
			char argsPayloadAux[NumBytesBufferICPMAX] = {0};
				// Try to block all connected nodes
				for (int iConnHostsNodes=0;iConnHostsNodes<NumConnectedHosts;iConnHostsNodes++){
					if (iConnHostsNodes==0){strcpy(argsPayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);}
					else{strcat(argsPayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);}
					strcat(argsPayloadAux,",");
				}
			if (FastInitialFakeSkipNetworkSynchFlag==true){// Skip network synchronization and thus blocking all other involved hosts
				AchievedAttentionParticularHosts=true;
			}
			else{			
				this->WaitUntilActiveActionFree(argsPayloadAux,NumConnectedHosts);
			}
			
			// Block only the two participating nodes in each iteration
			//strcpy(argsPayloadAux,this->IPaddressesSockets[3+iIterNetworkSynchScan]);
			//strcat(argsPayloadAux,",");
			//this->WaitUntilActiveActionFree(argsPayloadAux,1);
			
			//cout << "argsPayloadAux: " << argsPayloadAux << endl;		
			if (AchievedAttentionParticularHosts==true){
				cout << "Host " << this->IPaddressesSockets[2] << " synching node " << this->IPaddressesSockets[0] << " to the network!" << endl;
				if (FastInitialFakeSkipNetworkSynchFlag==true){ // Fake the initial synchronization step
					numHolderOtherNodesSynchNetwork=NumConnectedHosts+1;
					cout << "Host " << this->IPaddressesSockets[2] << " skipping network synchronization!!! to be deactivated..." << endl;
				}
				else{
					this->PeriodicRequestSynchsHost();
					numHolderOtherNodesSynchNetwork++; // Update value
				}

				CycleSynchNetworkDone=true;
				
				if (numHolderOtherNodesSynchNetwork==(NumConnectedHosts+1)){// All connected nodes and this host's node have been network synch, so we can reset the synch cycle 
					CycleSynchNetworkDone=false;
					numHolderOtherNodesSynchNetwork=0;// reset value
				}
				
				if (InitialNetworkSynchPass<1){//the very first time, two rounds are needed to achieve a reasonable network synchronization
					GPIOnodeNetworkSynched=false;// Do not Update value as synched
					InitialNetworkSynchPass++;
					cout << "Host " << this->IPaddressesSockets[2] << " first initial synch process completed...executing second process..." << endl; 
				}
				else{
					GPIOnodeNetworkSynched=true;// Update value as synched
				}
				if (FastInitialFakeSkipNetworkSynchFlag==true){
					AchievedAttentionParticularHosts=false;					
				}
				else{
					this->UnBlockActiveActionFree(argsPayloadAux,NumConnectedHosts);
				}
				iIterNetworkSynchcurrentTimerVal=0;// Reset value
				cout << "Host " << this->IPaddressesSockets[2] << " synched node " << this->IPaddressesSockets[0] << " to the network!" << endl;
			}
		}
		else{
			iIterNetworkSynchcurrentTimerVal++; // Update value
		}

		if (iIterNetworkSynchcurrentTimerVal>MaxiIterNetworkSynchcurrentTimerVal and HostsActiveActionsFree[0]==true and string(InfoRemoteHostActiveActions[1])!=string("Block")){// Every some iterations re-synch the node through the network
			GPIOnodeHardwareSynched=false;// Update value as not synched
			GPIOnodeNetworkSynched=false;// Update value as not synched
			iIterNetworkSynchcurrentTimerVal=0;// Reset value
			CycleSynchNetworkDone=false;// Reset value
			cout << "Host " << this->IPaddressesSockets[2] << " will re-synch node to the network!" << endl;
			// Some information of interest
			// InfoRemoteHostActiveActions should never contain information when HostsActiveActionsFree[0] is true (or 1)
			//cout << "Host " << this->IPaddressesSockets[2] << " HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
			//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[0]: " << InfoRemoteHostActiveActions[0] << endl;
			//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[1]: " << InfoRemoteHostActiveActions[1] << endl;
		}

		// Other task to perform at some point or regularly
		//cout << "Host " << this->IPaddressesSockets[2] << " HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
		//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[0]: " << InfoRemoteHostActiveActions[0] << endl;
		//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[1]: " << InfoRemoteHostActiveActions[1] << endl;
		
		iIterPeriodicTimerVal=0;// Reset variable
	}
	/////////////////////////////////////////////////////////////////////////////
	// Check if there is a malfunction with the blocks
	int OtherHostsActiveActionsFreeAux=0;
	for (int i=0;i<NumConnectedHosts;i++){
		if (HostsActiveActionsFree[1+i]==false){
			OtherHostsActiveActionsFreeAux++;
		}
	}
	/*
	if (HostsActiveActionsFree[0]==true and (string(InfoRemoteHostActiveActions[0])!=string("\0") or string(InfoRemoteHostActiveActions[1])!=string("\0") or AchievedAttentionParticularHosts==true or OtherHostsActiveActionsFreeAux>0)){
		cout << "Host " << this->IPaddressesSockets[2] << " scheduler blocking malfunction!!!...trying to correct it..." << endl;
		cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[0]: " << InfoRemoteHostActiveActions[0] << endl;
		cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[1]: " << InfoRemoteHostActiveActions[1] << endl;
		strcpy(InfoRemoteHostActiveActions[0],"\0");
		strcpy(InfoRemoteHostActiveActions[1],"\0");
		cout << "Host " << this->IPaddressesSockets[2] << " AchievedAttentionParticularHosts: " << AchievedAttentionParticularHosts << endl;
		AchievedAttentionParticularHosts=false;
		for (int i=0;i<NumConnectedHosts;i++){
			cout << "Host " << this->IPaddressesSockets[2] << " HostsActiveActionsFree[1+i]: " << HostsActiveActionsFree[1+i] << endl;
			HostsActiveActionsFree[1+i]=true;
		}
		////////////////////
		// Send unblock signals
		cout << "Host " << this->IPaddressesSockets[2] << " will unblock itself due to block malfunction" << endl;
		int nChararray=NumConnectedHosts;
		char ParamsCharArrayArg[NumBytesBufferICPMAX];
		for (int i=0;i<NumConnectedHosts;i++){
			if (i==0){strcpy(ParamsCharArrayArg,this->IPaddressesSockets[3+i]);}
			else{strcat(ParamsCharArrayArg,this->IPaddressesSockets[3+i]);}
			strcat(ParamsCharArrayArg,","); // IP separator
		}
		this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray); // Try to unblock others if needed	
		iIterPeriodicBlockTimer=0; // Reset value
		//////////////////////
	}
	*/
	////////////////////////////////////////////////////////////////////////////
	// Check if there is a permanent Block at this node
	if (string(InfoRemoteHostActiveActions[1])==string("Block") or HostsActiveActionsFree[0]==false){
		iIterPeriodicBlockTimer++; // Counter to acknowledge how much time it has been consecutively blocked
	}
	else
	{
		iIterPeriodicBlockTimer=0; // Reset value
	}
	if (iIterPeriodicBlockTimer>MaxiIterPeriodicBlockTimer and HostsActiveActionsFree[0]==false){// Try to unblock itself
		// Send unblock signals
		cout << "Host " << this->IPaddressesSockets[2] << " will unblock itself since to much time blocked" << endl;
		int nChararray=NumConnectedHosts;
		char ParamsCharArrayArg[NumBytesBufferICPMAX];
		for (int i=0;i<NumConnectedHosts;i++){
			if (i==0){strcpy(ParamsCharArrayArg,this->IPaddressesSockets[3+i]);}
			else{strcat(ParamsCharArrayArg,this->IPaddressesSockets[3+i]);}
			strcat(ParamsCharArrayArg,","); // IP separator
		}
		this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray); // Try to unblock others if needed	
		iIterPeriodicBlockTimer=0; // Reset value
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
 	this->acquire();// Wait semaphore until it can proceed
 	try{
 		try {
   	
    	// Code that might throw an exception
 	// Check if there are need messages or actions to be done by the node 	
 	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
 	this->RegularCheckToPerform();// Every now and then some checks have to happen.
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
      }
      catch (const std::exception& e) {
	// Handle the exception
      	cout << "Exception: " << e.what() << endl;
      }
    } // upper try
  catch (...) { // Catches any exception
  	cout << "Exception caught" << endl;
  }
  	this->release(); // Release the semaphore
  	//if (signalReceivedFlag.load()){this->~QTLAH();}// Destroy the instance
    //cout << "(int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)): " << (int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)) << endl;
    //usleep((int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
    this->RelativeNanoSleepWait((unsigned long long int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few nanoseconds for other processes to enter
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
try{
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
				else if (string(Command)==string("BusyNode")){ // Have knowledge if the node attached is busy doing things
					if (string(Payload)==string("true")){
						// If received is because node is hardware synched
						BusyAttachedNode=true;
						//cout << "Node " << this->IPaddressesSockets[0] << " busy attached node ..." << endl;
					}
					else{
						// If received is because node is NOT hardware synched
						BusyAttachedNode=false;
						//cout << "Node " << this->IPaddressesSockets[0] << " Non busy attached node ..." << endl;
					}
				}
				else if (string(Command)==string("HardwareSynchNode")){
					if (string(Payload)==string("true")){
						// If received is because node is hardware synched
						GPIOnodeHardwareSynched=true;
						cout << "Node " << this->IPaddressesSockets[0] << " hardware synched. Proceed with the network synchronization..." << endl;
					}
					else{
						// If received is because node is NOT hardware synched
						GPIOnodeHardwareSynched=false;
						cout << "Node " << this->IPaddressesSockets[0] << " hardware NOT synched..." << endl;
					}
				}
				else if (string(Command)==string("HostAreYouFree")){// Operations regarding availability request by other hosts
					// Different operation with respect the host that has sent the request				
					if (string(InfoRemoteHostActiveActions[0])==string(IPorg)){// Is the message from the current active host?
						if (string(Payload)==string("UnBlock")){// UnBlock
							strcpy(InfoRemoteHostActiveActions[0],"\0");// Clear active host
							strcpy(InfoRemoteHostActiveActions[1],"\0");// Clear status
							HostsActiveActionsFree[0]=true; // Set the host as free
							BusyAttachedNode=false; // Set the node already as not busy
						}
					}
					else{// Either the message is not from the current active hosts or there is no active host, or it is a response for this host
						if (string(Payload)==string("true") or string(Payload)==string("false")){// Response from another host
							if (string(Payload)==string("true")){HostsActiveActionsFree[1+NumAnswersOtherHostsActiveActionsFree]=false;}// Mark it as Block
							else{HostsActiveActionsFree[1+NumAnswersOtherHostsActiveActionsFree]=true;}
							NumAnswersOtherHostsActiveActionsFree++;// Update value
							//cout << "Response HostAreYouFree IPorg " << IPorg << " to IPdest " << IPdest <<": " << Payload << endl;
						}
						else if (HostsActiveActionsFree[0]==true and string(Payload)==string("Block") and GPIOnodeHardwareSynched==true and BusyAttachedNode==false){// Block
							strcpy(InfoRemoteHostActiveActions[0],IPorg);// Copy the identification of the host
							strcpy(InfoRemoteHostActiveActions[1],"Block");// Set status to Preventive
							HostsActiveActionsFree[0]=false; // Set the host as not free
							BusyAttachedNode=true; // Set the node already as busy
							// Respond with message saying that available
							char ParamsCharArray[NumBytesBufferICPMAX] = {0};
							strcpy(ParamsCharArray,IPorg);// Send to what was the origin
							strcat(ParamsCharArray,",");
							strcat(ParamsCharArray,IPdest);// From what was the origin
							strcat(ParamsCharArray,",");
							strcat(ParamsCharArray,"Operation");
							strcat(ParamsCharArray,",");
							strcat(ParamsCharArray,Command);
							strcat(ParamsCharArray,",");
							strcat(ParamsCharArray,"true");
							strcat(ParamsCharArray,",");// Very important to end the message
							//cout << "Operation HostAreYouFree Preventive" << endl;	
							this->ICPdiscoverSend(ParamsCharArray);
						}
						else if ((HostsActiveActionsFree[0]==false or GPIOnodeHardwareSynched==false or BusyAttachedNode==true) and string(Payload)==string("Block")){// Not free. Respond that not free (either because not free, or not finalized with synchronizations)
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
							//Do not act on UnBlock petitions from other hosts since another active petition has been already proceesses
							
						}
						else{// Non of the above, which is a malfunction						
							cout << "Host HostAreYouFree not handled!" << endl;
							cout << "IPdest: " << IPdest << ", IPorg: " << IPorg << ", Type: " << Type << ", Command: " << Command << " , Payload: " << Payload << endl;
						}
					}

				}
				else if (string(Command)==string("print")){
					cout << "Host " << this->IPaddressesSockets[2] << " New Message: "<< Payload << endl;
				}				
				else{
					cout << "Host " << this->IPaddressesSockets[2] << " does not have this Operational message: "<< Command << endl;
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
			  //cout << "Host " << this->IPaddressesSockets[2] << " retransmit params from node " << this->IPaddressesSockets[0] <<": " << Payload << endl;			    
				int socket_fd_conn;
			    // Mount message
			    // Notice that nodes Control (and only Control messages, Operational messages do not have this) messages meant to other nodes through their respective hosts have a composed payload, where the first part has the other host's node to whom the message has to be forwarded (at least up to its host - the host of the node sending the message). Hence, the Payload has to be processed a bit.
			    for (int iIterOpHost=0;iIterOpHost<NumConnectedHosts;iIterOpHost++){// Manually set - This should be programmed in order to scale
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
				    	//cout << "QTLAH::PayLoadReassembled: " << PayLoadReassembled << endl;
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
				    //cout << "Host's node Control Message original Payload: " << Payload << endl;
				    //cout << "Host's node Control Message reassembled Payload: " << PayLoadReassembled << endl;
				    
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
			    // Just to keep track of things
			    if (string(Command)==string("SimulateSendSynchQubits")){// Count how many order of synch network from other hosts received
			    	// First way of counting
			    	numHolderOtherNodesSendSynchQubits++;
			    	//cout << "Another node " << IPorg << " requesting synch qubits! " << "numHolderOtherNodesSendSynchQubits: " << numHolderOtherNodesSendSynchQubits << endl;
			    	// Second way of counting for superior resilence and avoid being blocked
			    	// PArtially decoding the Payload with the information of the current iterators of the network synching process
			    	char PayloadAux[NumBytesBufferICPMAX] = {0}; // Copy the Payload to not correct it when reading with strtok
			    	strcpy(PayloadAux,Payload);	
			    	strtok(PayloadAux,";");// skip content//strcpy(this->QLLAModeActivePassive,strtok(Payload,";"));
				//char PayloadAuxAux[NumBytesPayloadBuffer]={0};
				strtok(NULL,";");// skip content//strcpy(PayloadAux,strtok(NULL,";"));
				strtok(NULL,";");// skip content //this->QLLANumRunsPerCenterMass=atoi(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				int iCenterMassAux=atoi(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				int iNumRunsPerCenterMassAux=atoi(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				//this->QLLAFreqSynchNormValuesArray[0]=atof(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				//this->QLLAFreqSynchNormValuesArray[1]=atof(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				//this->QLLAFreqSynchNormValuesArray[2]=atof(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				//this->QLLAFineSynchAdjVal[0]=atof(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				//this->QLLAFineSynchAdjVal[1]=atof(strtok(NULL,";"));// Copy this first to not lose strtok pointer
				if (numHolderOtherNodesSendSynchQubits>=(NumCalcCenterMass*NumRunsPerCenterMass) or (iCenterMassAux==(NumCalcCenterMass-1) and iNumRunsPerCenterMassAux==(NumRunsPerCenterMass-1))){
			    		numHolderOtherNodesSendSynchQubits=0;// reset value
			    		numHolderOtherNodesSynchNetwork++;// Count the number of other nodes that run network synch
			    		cout << "Another pair of nodes " << IPorg << " and " << IPdest << " synch iteration completed!" << endl;
			    		if (numHolderOtherNodesSynchNetwork==(NumConnectedHosts+1)){// All connected nodes and this host's node have been netwrok synch, so we can reset the synch cycle 
			    			CycleSynchNetworkDone=false;
						numHolderOtherNodesSynchNetwork=0;// reset value
					}
				}
			}		    
		}  
	}
	else if(string(Type)==string("KeepAlive")){
			//cout << "Message to Host HeartBeat: "<< Payload << endl;
	}
		else{// 
			cout << "Message not handled by host: "<< Payload << endl;
		}  
	}// for
}
catch (const std::exception& e) {
// Handle the exception
	cout << "QTLAH::ProcessNewMessage Exception: " << e.what() << endl;
}
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
this->RelativeNanoSleepWait((unsigned long long int)(150*(unsigned long long int)(WaitTimeAfterMainWhileLoop)));//usleep((int)(5000*WaitTimeAfterMainWhileLoop));// Wait initially because this method does not need to send/receive message compared ot others like send or receive qubits, and then it happens that it executes first sometimes. This can be improved by sending messages to the specific node, and this node replying that has received the detection command, then this could start
this->acquire();
// It is a "blocking" communication between host and node, because it is many read trials for reading
while(this->SimulateRetrieveNumStoredQubitsNodeFlag==true){//Wait, only one asking
	this->release();
	this->RelativeNanoSleepWait((unsigned long long int)(25*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//usleep((int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));
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
	this->RelativeNanoSleepWait((unsigned long long int)(1000*(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));//usleep((int)(500*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give some time to have the chance to receive the response
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
		ParamsDoubleArray[8]=this->TimeTaggsDetAnalytics[8];
		ParamsDoubleArray[9]=this->TimeTaggsDetAnalytics[9];
		ParamsDoubleArray[10]=this->TimeTaggsDetAnalytics[10];
		
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
this->RelativeNanoSleepWait((unsigned long long int)(100*(unsigned long long int)(WaitTimeAfterMainWhileLoop)));//usleep((int)(5000*WaitTimeAfterMainWhileLoop));// Wait initially because this method does not need to send/receive message compared ot others like send or receive qubits, and then it happens that it executes first sometimes. This can be improved by sending messages to the specific node, and this node replying that has received the detection command, then this could start
this->acquire();
// It is a "blocking" communication between host and node, because it is many read trials for reading
while(this->SimulateRetrieveNumStoredQubitsNodeFlag==true){//Wait, only one asking
	this->release();
this->RelativeNanoSleepWait((unsigned long long int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//usleep((int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));
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
	this->RelativeNanoSleepWait((unsigned long long int)(1000*(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));//usleep((int)(500*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give some time to have the chance to receive the response
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
	try{
		//cout << "Host " << this->IPaddressesSockets[2] << " Initiated WaitUntilActiveActionFreePreLock" << endl;
		this->RelativeNanoSleepWait((unsigned long long int)(1500*(unsigned long long int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));
		this->acquire();
		//cout << "Host " << this->IPaddressesSockets[2] << " Entered acquire 1" << endl;
		bool FirstPassAux=true;
		int MaxNumPassesCheckBlockAux=0;
		int NumPassesCheckBlockAux=0;
		while((AchievedAttentionParticularHosts==false or FirstPassAux==true) and NumPassesCheckBlockAux<MaxNumPassesCheckBlockAux){
			NumPassesCheckBlockAux++;
			if (NumPassesCheckBlockAux>=MaxNumPassesCheckBlockAux){// Not the best solution, but avoid for ever block
				cout << "QTLAH::WaitUntilActiveActionFreePreLock Malfunction. Host " << this->IPaddressesSockets[2] << " will proceed eventhough Blocking not achieved!" << endl;
			}
			//cout << "Host " << this->IPaddressesSockets[2] << " Entered While 1" << endl;
			if (FirstPassAux==false){
				cout << "Host " << this->IPaddressesSockets[2] << " trying to get attention from other involved hosts!" << endl;
				//cout << "Host " << this->IPaddressesSockets[2] << " HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
				//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[0]: " << InfoRemoteHostActiveActions[0] << endl;
				//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[1]: " << InfoRemoteHostActiveActions[1] << endl;
				//cout << "Host " << this->IPaddressesSockets[2] << " GPIOnodeHardwareSynched: " << GPIOnodeHardwareSynched << endl;
				//cout << "Host " << this->IPaddressesSockets[2] << " GPIOnodeNetworkSynched: " << GPIOnodeNetworkSynched << endl;
			}
			FirstPassAux=false;// First pass is compulsory, since it might be true AchievedAttentionParticularHosts, but because of another process
			while (BusyAttachedNode==true or HostsActiveActionsFree[0]==false or GPIOnodeHardwareSynched==false or GPIOnodeNetworkSynched==false){// Wait here// No other thread checking this info
				//cout << "Host " << this->IPaddressesSockets[2] << " Entered While 2" << endl;
				this->release();
				//cout << "Host " << this->IPaddressesSockets[2] << " Exited release 1" << endl;
				//cout << "HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
				//cout << "GPIOnodeHardwareSynched: " << GPIOnodeHardwareSynched << endl;
				//cout << "GPIOnodeNetworkSynched: " << GPIOnodeNetworkSynched << endl;
				cout << "Host " << this->IPaddressesSockets[2] << " waiting network & hardware synchronization and availability of other hosts to proceed with the request!" << endl;
				this->RelativeNanoSleepWait((unsigned long long int)(1500*(unsigned long long int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));
				this->acquire();
				//cout << "Host " << this->IPaddressesSockets[2] << " Entered acquire 2" << endl;
			}
			//int numForstEquivalentToSleep=200;//100: Equivalent to 1 seconds# give time to other hosts to enter
			//for (int i=0;i<numForstEquivalentToSleep;i++){
			//	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
			//	//cout << "this->getState(): " << this->getState() << endl;
			//	if(this->getState()==0) {
			//		this->ProcessNewMessage();
			//		this->m_pause(); // After procesing the request, pass to paused state
			//	}
			//	this->RelativeNanoSleepWait((unsigned long long int)(WaitTimeAfterMainWhileLoop));// Wait a few nanoseconds for other processes to enter
			//}
			//cout << "Host " << this->IPaddressesSockets[2] << " Exited While 2" << endl;
			if (HostsActiveActionsFree[0]==true and GPIOnodeHardwareSynched==true and GPIOnodeNetworkSynched==true and BusyAttachedNode==false){//string(InfoRemoteHostActiveActions[0])==string(this->IPaddressesSockets[2]) or string(InfoRemoteHostActiveActions[0])==string("\0")){
				//cout << "Host " << this->IPaddressesSockets[2] << " Entering WaitUntilActiveActionFree" << endl;
				this->WaitUntilActiveActionFree(ParamsCharArrayArg,nChararray);
			}
			else{
				//cout << "Host " << this->IPaddressesSockets[2] << " NOT Entering WaitUntilActiveActionFree" << endl;
				AchievedAttentionParticularHosts=false;// reset value
			}
			//if (AchievedAttentionParticularHosts==false and string(InfoRemoteHostActiveActions[1])==string("Block") and string(InfoRemoteHostActiveActions[0])==string(this->IPaddressesSockets[2]) and GPIOnodeHardwareSynched==true and GPIOnodeNetworkSynched==true){// Autoblocked
			//	this->UnBlockActiveActionFree(ParamsCharArrayArg,nChararray);// Unblock
			//}
		}
	}
    catch (const std::exception& e) {
		// Handle the exception
    	cout << "QTLAH::WaitUntilActiveActionFreePreLock Exception: " << e.what() << endl;
    }
	this->release();
	//cout << "Host " << this->IPaddressesSockets[2] << " Exited release 2" << endl;
	//cout << "Host " << this->IPaddressesSockets[2] << " Finished WaitUntilActiveActionFreePreLock" << endl;

return 0; // all ok;

}

int QTLAH::WaitUntilActiveActionFree(char* ParamsCharArrayArg, int nChararray){
	// First block the current host
	//HostsActiveActionsFree[0]=false;// This host blocked
	//BusyAttachedNode=true; // Set it already as busy
	AchievedAttentionParticularHosts=false;// reset value
	//cout << "Host " << this->IPaddressesSockets[2] << " Initiated WaitUntilActiveActionFree" << endl;
	//cout << "Host " << this->IPaddressesSockets[2] << " IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
		if(this->getState()==0){
			this->ProcessNewMessage();
			this->m_pause(); // After procesing the request, pass to paused state
			//cout << "IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
		}

	this->SequencerAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
	//cout << "Host " << this->IPaddressesSockets[2] << " IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
	while(IterHostsActiveActionsFreeStatus!=0){
		//cout << "Host " << this->IPaddressesSockets[2] << " WaitUntilActiveActionFree While 1" << endl;
		//cout << "Host " << this->IPaddressesSockets[2] << " IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
		this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
		if(this->getState()==0){
			this->ProcessNewMessage();
			this->m_pause(); // After procesing the request, pass to paused state
			//cout << "IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
		}
		this->SequencerAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);		
	}
//cout << "Host " << this->IPaddressesSockets[2] << " Finished WaitUntilActiveActionFree" << endl;
return 0; // All ok
}

int QTLAH::UnBlockActiveActionFreePreLock(char* ParamsCharArrayArg, int nChararray){	
	try{
		//cout << "Host " << this->IPaddressesSockets[2] << " Initiated UnBlockActiveActionFreePreLock" << endl;
		this->RelativeNanoSleepWait((unsigned long long int)(1500*(unsigned long long int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));
		this->acquire();
		this->UnBlockActiveActionFree(ParamsCharArrayArg,nChararray);
	}
    catch (const std::exception& e) {
		// Handle the exception
    	cout << "QTLAH::UnBlockActiveActionFreePreLock Exception: " << e.what() << endl;
    }
	this->release();
//cout << "Host " << this->IPaddressesSockets[2] << " Finished UnBlockActiveActionFreePreLock" << endl;
return 0; // All ok
}

int QTLAH::UnBlockActiveActionFree(char* ParamsCharArrayArg, int nChararray){

	this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);

return 0; // All ok
}

int QTLAH::SequencerAreYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
	if (IterHostsActiveActionsFreeStatus==0){
			if (HostsActiveActionsFree[0]==true and GPIOnodeHardwareSynched==true and BusyAttachedNode==false){
				this->SendAreYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
				return 0; // All Ok
			}			
			// Process other messages if available
			if (GPIOnodeHardwareSynched==true and BusyAttachedNode==true){//} and HostsActiveActionsFree[0]==true){// Ask the node if busy
					char ParamsCharArray[NumBytesBufferICPMAX] = {0};
					strcpy(ParamsCharArray,this->IPaddressesSockets[0]);
					strcat(ParamsCharArray,",");
					strcat(ParamsCharArray,this->IPaddressesSockets[1]);
					strcat(ParamsCharArray,",");
					strcat(ParamsCharArray,"Control");
					strcat(ParamsCharArray,",");
					strcat(ParamsCharArray,"BusyNode");
					strcat(ParamsCharArray,",");
					strcat(ParamsCharArray,"none");
					strcat(ParamsCharArray,",");// Very important to end the message
					//cout << "Host sent HardwareSynchNode" << endl;
					this->ICPdiscoverSend(ParamsCharArray); // send mesage to dest
				}
			
			//this->release();
			this->RelativeNanoSleepWait((unsigned long long int)(2*(unsigned long long int)(WaitTimeAfterMainWhileLoop*(1.0+10.0*(float)rand()/(float)RAND_MAX))));
			//this->acquire();

			this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
				if(this->getState()==0){
					this->ProcessNewMessage();
					this->m_pause(); // After procesing the request, pass to paused state
					//cout << "IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
				}
	}
	else if (IterHostsActiveActionsFreeStatus==1){
		this->AcumulateAnswersYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
	}
	else{// if (IterHostsActiveActionsFreeStatus==2){
		AchievedAttentionParticularHosts=this->CheckReceivedAnswersYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
	}

return 0; // All Ok
}

int QTLAH::SendAreYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
// Three-step handshake
// First block the current host
HostsActiveActionsFree[0]=false;// This host blocked
BusyAttachedNode=true; // Set it already as busy
strcpy(InfoRemoteHostActiveActions[0],this->IPaddressesSockets[2]);// Clear active host
strcpy(InfoRemoteHostActiveActions[1],"Block");// Clear status

NumAnswersOtherHostsActiveActionsFree=0;// Reset the number of answers received
ReWaitsAnswersHostsActiveActionsFree=0; // Reset the counter

int NumInterestIPaddressesAux=nChararray;

for (int i=0;i<NumInterestIPaddressesAux;i++){// Reset values
	HostsActiveActionsFree[1+i]=true;
}

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
	strcat(ParamsCharArray,"Block");
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
	ReWaitsAnswersHostsActiveActionsFree++;// Update the counter
}
else{// Too many rounds, kill the process of blocking other hosts
	cout << "Host " << this->IPaddressesSockets[2] << " HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
	cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[0]: " << InfoRemoteHostActiveActions[0] << endl;
	cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[1]: " << InfoRemoteHostActiveActions[1] << endl;
	cout << "Host " << this->IPaddressesSockets[2] << " GPIOnodeHardwareSynched: " << GPIOnodeHardwareSynched << endl;
	cout << "Host " << this->IPaddressesSockets[2] << " GPIOnodeNetworkSynched: " << GPIOnodeNetworkSynched << endl;
	cout << "Host " << this->IPaddressesSockets[2] << " too many iterations waiting for Block requests answers from other nodes...aborting block request!" << endl;
	
	this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
	//cout << "Host " << this->IPaddressesSockets[2] << " HostsActiveActionsFree[0]: " << HostsActiveActionsFree[0] << endl;
	//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[0]: " << InfoRemoteHostActiveActions[0] << endl;
	//cout << "Host " << this->IPaddressesSockets[2] << " InfoRemoteHostActiveActions[1]: " << InfoRemoteHostActiveActions[1] << endl;
	//cout << "Host " << this->IPaddressesSockets[2] << " GPIOnodeHardwareSynched: " << GPIOnodeHardwareSynched << endl;
	//cout << "Host " << this->IPaddressesSockets[2] << " GPIOnodeNetworkSynched: " << GPIOnodeNetworkSynched << endl;
	
	IterHostsActiveActionsFreeStatus=0;
	ReWaitsAnswersHostsActiveActionsFree=0;// Reset the counter

	for (int i=0;i<NumInterestIPaddressesAux;i++){// Reset values
		HostsActiveActionsFree[1+i]=true;
	}	
}

//cout << "ReWaitsAnswersHostsActiveActionsFree: " << ReWaitsAnswersHostsActiveActionsFree << endl;
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
		if (HostsActiveActionsFree[1+i]==false){
			SumCheckAllOthersFree++;
		}
	}
//cout << "SumCheckAllOthersFree: " << SumCheckAllOthersFree << endl;

if (SumCheckAllOthersFree>=NumInterestIPaddressesAux){CheckAllOthersFreeAux=true;}

for (int i=0;i<NumInterestIPaddressesAux;i++){// Reset values
	HostsActiveActionsFree[1+i]=true;
}

// If all available, notify that attention achieved
if (CheckAllOthersFreeAux==true){
	IterHostsActiveActionsFreeStatus=0;// reset process
	//BusyAttachedNode=true; // Set it already as busy
	return true;
}
else{ // If some are unavailable, unblock them all
	cout << "Host " << this->IPaddressesSockets[2] << " did not obtain Block attention of all requested Hosts...aborting block" << endl;
	this->UnBlockYouFreeRequestToParticularHosts(ParamsCharArrayArg,nChararray);
	IterHostsActiveActionsFreeStatus=0;// reset process	
	return false;
}

return false; // all ok
}

int QTLAH::UnBlockYouFreeRequestToParticularHosts(char* ParamsCharArrayArg, int nChararray){
if ((HostsActiveActionsFree[0]==false and string(InfoRemoteHostActiveActions[0])==string(this->IPaddressesSockets[2])) or iIterPeriodicBlockTimer>MaxiIterPeriodicBlockTimer){// This is the blocking host so proceed to unblock
	//int numForstEquivalentToSleep=500;//100: Equivalent to 1 seconds# give time to other hosts to enter
	//for (int i=0;i<numForstEquivalentToSleep;i++){
	//	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
	//	//cout << "this->getState(): " << this->getState() << endl;
	//	if(this->getState()==0) {
	//		this->ProcessNewMessage();
	//		this->m_pause(); // After procesing the request, pass to paused state
	//	}
	//	this->RelativeNanoSleepWait((unsigned long long int)(WaitTimeAfterMainWhileLoop));// Wait a few nanoseconds for other processes to enter
	// }
	strcpy(InfoRemoteHostActiveActions[0],"\0");// Clear active host
	strcpy(InfoRemoteHostActiveActions[1],"\0");// Clear status
	//BusyAttachedNode=false;
	HostsActiveActionsFree[0]=true;// This host unblocked
	IterHostsActiveActionsFreeStatus=0;// reset process
	AchievedAttentionParticularHosts=false;// Indicates that we have got NOT the attention of the hosts

	int NumInterestIPaddressesAux=nChararray;
	for (int i=0;i<NumConnectedHosts;i++){//NumInterestIPaddressesAux;i++){// Reset values
		HostsActiveActionsFree[1+i]=true;
	}

	// Only host who care will take action with the UnBlock message below
	//int NumInterestIPaddressesAux=nChararray;
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
	/*
	int numForstEquivalentToSleep=500;//100: Equivalent to 1 seconds# give time to other hosts to enter
	for (int i=0;i<numForstEquivalentToSleep;i++){
		this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
		//cout << "this->getState(): " << this->getState() << endl;
		if(this->getState()==0) {
			this->ProcessNewMessage();
			this->m_pause(); // After procesing the request, pass to paused state
		}
		this->RelativeNanoSleepWait((unsigned long long int)(WaitTimeAfterMainWhileLoop));// Wait a few nanoseconds for other processes to enter
	}*/
	this->RelativeNanoSleepWait((unsigned long long int)(10*(unsigned long long int)(WaitTimeAfterMainWhileLoop*(1.0+10.0*(float)rand()/(float)RAND_MAX))));
	this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
		if(this->getState()==0){
			this->ProcessNewMessage();
			this->m_pause(); // After procesing the request, pass to paused state
			//cout << "IterHostsActiveActionsFreeStatus: " << IterHostsActiveActionsFreeStatus << endl;
		}
}
return 0; // All ok
}

int QTLAH::PeriodicRequestSynchsHost(){// Execute automatically the network synchronization from this host node to all other connected hosts nodes
	char charNumAux[NumBytesBufferICPMAX] = {0};
	char messagePayloadAux[NumBytesBufferICPMAX] = {0};
	char ParamsCharArray[NumBytesBufferICPMAX] = {0};
for (int iConnHostsNodes=0;iConnHostsNodes<NumConnectedHosts;iConnHostsNodes++){// For each connected node to be synch with
	for (int iCenterMass=0;iCenterMass<NumCalcCenterMass;iCenterMass++){
		for (int iNumRunsPerCenterMass=0;iNumRunsPerCenterMass<NumRunsPerCenterMass;iNumRunsPerCenterMass++){			
			// First message - Receive Qubits
			strcpy(messagePayloadAux,"Active");
			strcat(messagePayloadAux,";");
			strcat(messagePayloadAux,this->IPaddressesSockets[2]);// Ip OPnet of the receiver
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");
			strcat(messagePayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);// Ip OPnet of the emitter
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");				
			strcat(messagePayloadAux,this->IPaddressesSockets[3+iConnHostsNodes]);
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", this->NumQuBitsPerRunAux);
			strcat(messagePayloadAux,charNumAux);
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
			for (int i=0;i<NumCalcCenterMass;i++){
				sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[i]);
				strcat(messagePayloadAux,charNumAux);
				strcat(messagePayloadAux,";");
			}
			sprintf(charNumAux, "%4f", this->HistPeriodicityAuxAux);// Histogram period for network synchronziation
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", 0.0);// Zero added offset since we are not testing
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", 0.0);// Zero added relative frequency difference offset since we are not testing
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", QTLAHParamsTableDetAutoSynchQuadChDet[iConnHostsNodes]);
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
			
			// Second message - Send QuBits
			strcpy(messagePayloadAux,"Passive");
			strcat(messagePayloadAux,";");
			strcat(messagePayloadAux,this->IPaddressesSockets[2]);// IP of the receiver
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");
			strcat(messagePayloadAux,this->IPaddressesSockets[2]);
			strcat(messagePayloadAux,"_");
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", this->NumberRepetitionsSignalAux);
			strcat(messagePayloadAux,charNumAux);
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
			for (int i=0;i<NumCalcCenterMass;i++){
				sprintf(charNumAux, "%4f", QTLAHFreqSynchNormValuesArray[i]);
				strcat(messagePayloadAux,charNumAux);
				strcat(messagePayloadAux,";");
			}
			sprintf(charNumAux, "%4f", this->HistPeriodicityAuxAux);// Histogram period for network synchronziation
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", 0.0);// Zero added offset since we are not testing
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%4f", 0.0);// Zero added relative frequency difference offset since we are not testing
			strcat(messagePayloadAux,charNumAux);
			strcat(messagePayloadAux,";");
			sprintf(charNumAux, "%d", QTLAHParamsTableDetAutoSynchQuadChEmt[iConnHostsNodes]);
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
			int numForstEquivalentToSleep=2500;//100: Equivalent to 1 seconds#(usSynchProcIterRunsTimePoint*1000)/WaitTimeAfterMainWhileLoop;
			for (int i=0;i<numForstEquivalentToSleep;i++){
				this->ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
				//cout << "this->getState(): " << this->getState() << endl;
				if(this->getState()==0) {
					this->ProcessNewMessage();
					this->m_pause(); // After procesing the request, pass to paused state
				}
				this->RelativeNanoSleepWait((unsigned long long int)(WaitTimeAfterMainWhileLoop));// Wait a few nanoseconds for other processes to enter
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
