/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum transport Layer Host
*/
#include "QtransportLayerAgentH.h"
#include<iostream>
#include<unistd.h> //for usleep
// InterCommunication Protocols - Sockets - Common to Server and Client
#include <stdio.h>
#include <string.h>
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
// InterCommunicaton Protocols - Sockets - Client
#include <arpa/inet.h>
// Threading
#define WaitTimeAfterMainWhileLoop 1000
#include <thread>
// Semaphore
#include <atomic>

using namespace std;

namespace nsQtransportLayerAgentH {

QTLAH::QTLAH(int numberSessions,char* ParamsDescendingCharArray,char* ParamsAscendingCharArray) { // Constructor

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
	
	RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[2],this->new_socketArray[2],this->IPaddressesSockets[4],this->IPaddressesSockets[2],this->IPSocketsList[2]); // Open port and listen as server
}
	if (RetValue==-1){this->m_exit();} // Exit application
	this->numberSessions=1;
	this->NumSockets=3;
		
	return 0; // All OK
}

int QTLAH::StopICPconnections() {
	// First stop client or server host connection
	if (string(SOCKtype)=="SOCK_DGRAM"){ICPmanagementCloseClient(this->socket_SendUDPfdArray[0]);ICPmanagementCloseClient(this->socket_SendUDPfdArray[1]);ICPmanagementCloseServer(this->socket_fdArray[0],this->new_socketArray[0]);ICPmanagementCloseServer(this->socket_fdArray[1],this->new_socketArray[1]);}
	else{
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
			//usleep(SockListenTimeusec);		
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
			//usleep(SockListenTimeusec);
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

int QTLAH::ICPmanagementSend(int socket_fd_conn,char* IPaddressesSockets) {
//cout << "Host SendBuffer: " << this->SendBuffer << endl;
//cout << "Host SendBuffer IPaddressesSockets: " << IPaddressesSockets << endl;
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
        this->release(); // Release the semaphore 
        //cout << "(int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)): " << (int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)) << endl;
        usleep((int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
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
						this->TimeTaggsDetAnalytics[i-1]=stof(SubPayload);
						//cout << "stof(SubPayload): " << stof(SubPayload) << endl;
					}
				}
								
				this->InfoSimulateNumStoredQubitsNodeFlag=true;
				//cout << "SimulateNumStoredQubitsNode finished parsing values" << endl;		
			}
			else if (string(Command)==string("print")){
				cout << "New Message: "<< Payload << endl;
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
			cout << "Operation Message forward ParamsCharArray: " << ParamsCharArray << endl;	
			this->ICPdiscoverSend(ParamsCharArray);
		}				
		else{//Default
			cout << "Operational message to host not handled: "<< Payload << endl;
		}
	}
	else if(string(Type)==string("Control")){//Control message are not meant for host, so forward it accordingly
		if (string(IPorg)==string(this->IPaddressesSockets[0])){ // If it comes from its attached node and is a control message then it is not for this host, forward it to other hosts' nodes
		// The node of a host is always identified in the Array in position 0			    
		    int socket_fd_conn;
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
		    if (string(this->SCmode[1])==string("client") or string(SOCKtype)=="SOCK_DGRAM"){//host acts as client
			    socket_fd_conn=this->socket_fdArray[1];   // host acts as client to the other host, so it needs the socket descriptor (it applies both to TCP and UDP) 
			    this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[3]);
		    }
		    else{ //host acts as server		    
			    socket_fd_conn=this->new_socketArray[1];  // host acts as server to the other host, so it needs the socket connection   
			    this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[3]);
		    }
		    // Mount message
		    char ParamsCharArrayAux[NumBytesBufferICPMAX] = {0};
			strcpy(ParamsCharArrayAux,IPdest);
			strcat(ParamsCharArrayAux,",");
			strcat(ParamsCharArrayAux,IPorg);
			strcat(ParamsCharArrayAux,",");
			strcat(ParamsCharArrayAux,Type);
			strcat(ParamsCharArrayAux,",");
			strcat(ParamsCharArrayAux,Command);
			strcat(ParamsCharArrayAux,",");
			strcat(ParamsCharArrayAux,Payload);
			strcat(ParamsCharArrayAux,",");// Very important to end the message
			//cout << "ParamsCharArray: " << ParamsCharArray << endl;
		    strcpy(this->SendBuffer,ParamsCharArrayAux);			
		    if (string(this->SCmode[1])==string("client") or string(SOCKtype)=="SOCK_DGRAM"){//host acts as client
			    socket_fd_conn=this->socket_fdArray[2];   // host acts as client to the other host, so it needs the socket descriptor (it applies both to TCP and UDP) 
			    this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[4]);
		    }
		    else{ //host acts as server		    
			    socket_fd_conn=this->new_socketArray[2];  // host acts as server to the other host, so it needs the socket connection   
			    this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[4]);
		    }
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
		    int socket_fd_conn=this->socket_fdArray[0];  // the host always acts as client to the node, so it needs the socket descriptor   (it applies both to TCP and UDP)
		    this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[0]);
		}  
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

int QTLAH::SimulateRetrieveNumStoredQubitsNode(char* IPhostReply,char* IPhostRequest, int* ParamsIntArray,int nIntarray,float* ParamsFloatArray,int nFloatarray){ // Send to the upper layer agent how many qubits are stored
usleep((int)(2000*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait initially because this method does not need to send/receive message compared ot others like send or receive qubits, and then it happens that it executes first sometimes
this->acquire();
// It is a "blocking" communication between host and node, because it is many read trials for reading
while(this->SimulateRetrieveNumStoredQubitsNodeFlag==true){//Wait, only one asking
this->release();
usleep((int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));
this->acquire();
}
this->SimulateRetrieveNumStoredQubitsNodeFlag=true;
int isValidWhileLoopCount = 10; // Number of tries If it needs more than one trial is because the sockets are not working correctly. It is best to reboot nodes
this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
while(isValidWhileLoopCount>0){
	if (isValidWhileLoopCount % 10 ==0){// Only try to resend the message once every 10 times
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
	usleep((int)(500*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give some time to have the chance to receive the response
	this->acquire();
	if (this->InfoSimulateNumStoredQubitsNodeFlag==true){
		//cout << "We received info for SimulateRetrieveNumStoredQubitsNode" << endl;
		ParamsIntArray[0]=this->SimulateNumStoredQubitsNodeParamsIntArray[0];
		ParamsFloatArray[0]=this->TimeTaggsDetAnalytics[0];
		ParamsFloatArray[1]=this->TimeTaggsDetAnalytics[1];
		ParamsFloatArray[2]=this->TimeTaggsDetAnalytics[2];
		ParamsFloatArray[3]=this->TimeTaggsDetAnalytics[3];
		ParamsFloatArray[4]=this->TimeTaggsDetAnalytics[4];
		ParamsFloatArray[5]=this->TimeTaggsDetAnalytics[5];
		ParamsFloatArray[6]=this->TimeTaggsDetAnalytics[6];
		ParamsFloatArray[7]=this->TimeTaggsDetAnalytics[7];
		
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
		if (isValidWhileLoopCount<=0){
			cout << "Host did not achieve to RetrieveNumStoredQubitsNode" << endl;
			this->InfoSimulateNumStoredQubitsNodeFlag=false; // Reset the flag
			this->SimulateRetrieveNumStoredQubitsNodeFlag=false;
			this->release();
		}
	}
}//while
usleep((int)(2000*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Give time to clear lost replies that might confuse the read
return 0; // All OK
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
