/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2024
Created: 2024

Agent script for Quantum transport Layer Node
*/
#include "QtransportLayerAgentN.h"
#include<iostream>
#include<unistd.h> //for usleep
#include "QnetworkLayerAgent.h"
// InterCommunication Protocols - Sockets - Common to Server and Client
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#define PORT 8010
#define NumSocketsMax 2
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
#define WaitTimeAfterMainWhileLoop 100
#include <thread>
// Semaphore
#include <atomic>
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20

using namespace std;

namespace nsQtransportLayerAgentN {

QTLAN::QTLAN(int numberSessions) { // Constructor 
 this->numberSessions = numberSessions; // Number of sessions of different services
 strcpy(this->SCmode[0],"server"); // to know if this host instance is client or server

}
///////////////////////////////////////////////////
void QTLAN::acquire() {
while(valueSemaphore==0);
this->valueSemaphore=0; // Make sure it stays at 0
}
 
void QTLAN::release() {
this->valueSemaphore=1; // Make sure it stays at 1
}

static void SignalPIPEHandler(int s) {
cout << "Caught SIGPIPE" << endl;
}
///////////////////////////////////////////////////////
int QTLAN::InitiateBelowAgentsObjects(){// Some objects of the below layers that have to be initialized
strcpy(this->QNLAagent.SCmode[0],this->SCmode[1]);
strcpy(this->QNLAagent.QLLAagent.SCmode[0],this->SCmode[1]);
strcpy(this->QNLAagent.QLLAagent.QPLAagent.SCmode[0],this->SCmode[1]);

return 0; // All OK
}

int QTLAN::InitParametersAgent(){// Client node have some parameters to adjust to the server node

memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer

return 0; //All OK
}

int QTLAN::SendParametersAgent(){// The upper layer gets the information to be send
//this->acquire(); Does not need it since it is within the while loop

char ParamsCharArray[NumBytesBufferICPMAX] = {0};
strcpy(ParamsCharArray,"Trans;"); // Initiates the ParamsCharArray, so use strcpy
if (string(this->PayloadSendBuffer)!=string("")){
	strcat(ParamsCharArray,this->PayloadSendBuffer);
}
else{
	strcat(ParamsCharArray,"none_none_");
}
strcat(ParamsCharArray,";");
QNLAagent.SendParametersAgent(ParamsCharArray);// Below Agent Method
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer
// Mount the information to send the message
//cout << "ParamsCharArray: " << ParamsCharArray << endl;
if (string(ParamsCharArray)!=string("Trans;none_none_;Net;none_none_;Link;none_none_;Phys;none_none_;")){
	 // Generate the message	 
	memset(this->SendBuffer, 0, sizeof(this->SendBuffer));
	strcpy(this->SendBuffer,this->IPaddressesSockets[1]);
	strcat(this->SendBuffer,",");
	strcat(this->SendBuffer,this->IPaddressesSockets[2]);
	strcat(this->SendBuffer,",");
	strcat(this->SendBuffer,"Control");
	strcat(this->SendBuffer,",");
	strcat(this->SendBuffer,"InfoProcess");
	strcat(this->SendBuffer,",");
	strcat(this->SendBuffer,ParamsCharArray);
	strcat(this->SendBuffer,",");// Very important to end the message
	//cout << "this->SendBuffer: " << this->SendBuffer << endl;
	int socket_fd_conn;
	if (string(SOCKtype)=="SOCK_DGRAM"){
		socket_fd_conn=this->socket_fdArray[0];// UDP works with socket descriptor (not connection)
	}
	else{
		socket_fd_conn=this->new_socketArray[0];   // The first point probably to the host
	}
	this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[1]); // send message to node
}
//this->release();Does not need it since it is within the while loop

return 0; // All OK

}

int QTLAN::SetSendParametersAgent(char* ParamsCharArray){// Node accumulates parameters for the other node

strcpy(this->PayloadSendBuffer,"none_none_");

return 0; //All OK
}

int QTLAN::ReadParametersAgent(char* ParamsCharArray){// Node checks parameters from the other node
//cout << "QTLAN::ReadParametersAgent: " << ParamsCharArray << endl;
char DiscardBuffer[NumBytesPayloadBuffer]={0};
strcpy(DiscardBuffer,strtok(ParamsCharArray,";"));
strcpy(this->PayloadReadBuffer,strtok(NULL,";"));
//cout << "this->PayloadReadBuffer: " << this->PayloadReadBuffer << endl;
char ParamsCharArrayAux[NumBytesPayloadBuffer]={0};
strcpy(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
strcat(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
strcat(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
strcat(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
strcat(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
strcat(ParamsCharArrayAux,strtok(NULL,";"));
strcat(ParamsCharArrayAux,";");
//cout << "ParamsCharArrayAux: " << ParamsCharArrayAux << endl;
QNLAagent.SetReadParametersAgent(ParamsCharArrayAux); // Send respective information to the below layer agent

if (string(this->PayloadReadBuffer)!=string("") and string(this->PayloadReadBuffer)==string("none_none_")){
	this->ProcessNewParameters();
}

memset(this->PayloadReadBuffer, 0, sizeof(this->PayloadReadBuffer));// Reset buffer
return 0; // All OK
}

int QTLAN::SetReadParametersAgent(char* ParamsCharArray){// The upper layer sets information to be read
this->acquire();
strcat(this->PayloadReadBuffer,ParamsCharArray);
this->release();
return 0; // All OK
}
////////////////////////////////////////////////////
int QTLAN::countQintupleComas(char* ParamsCharArray) {
  int comasCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ',') {
      comasCount++;
    }
  }

  return comasCount/5;
}

int QTLAN::countDoubleColons(char* ParamsCharArray) {
  int colonCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == ':') {
      colonCount++;
    }
  }

  return colonCount/2;
}

int QTLAN::countDoubleUnderscores(char* ParamsCharArray) {
  int underscoreCount = 0;

  for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
    if (ParamsCharArray[i] == '_') {
      underscoreCount++;
    }
  }

  return underscoreCount/2;
}

int QTLAN::ProcessNewParameters(){
char ParamsCharArray[NumBytesPayloadBuffer]={0};
char HeaderCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
char ValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
char TokenValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};

strcpy(ParamsCharArray,this->PayloadReadBuffer);

int NumDoubleUnderscores = this->countDoubleUnderscores(ParamsCharArray);

for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
	if (iHeaders==0){
		strcpy(HeaderCharArray[iHeaders],strtok(ParamsCharArray,"_"));		
	}
	else{
		strcpy(HeaderCharArray[iHeaders],strtok(NULL,"_"));
	}
	strcpy(ValuesCharArray[iHeaders],strtok(NULL,"_"));
}

for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
// Missing to develop if there are different values

}

return 0; // All OK
}
////////////////////////////////////////////////////////
int QTLAN::InitAgentProcess(){
	// Then, regularly check for next job/action without blocking		  	
	// Not used void* params;
	// Not used this->threadRef=std::thread(&QTLAH::AgentProcessStaticEntryPoint,params);
	this->threadRef=std::thread(&QTLAN::AgentProcessRequestsPetitions,this);
	  //if (ret) {
	    // Handle the error
	  //} 
	return 0; //All OK
}

int QTLAN::SocketCheckForceShutDown(int socket_fd){
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

int QTLAN::InitiateICPconnections(int argc){
int RetValue = 0;
if (string(SOCKtype)=="SOCK_DGRAM"){
	RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[0],this->new_socketArray[0],this->IPaddressesSockets[0],this->IPaddressesSockets[2],this->IPSocketsList[0]);// Listen to the port
	
	if (RetValue>-1){RetValue=this->ICPmanagementOpenClient(this->socket_SendUDPfdArray[0],this->IPaddressesSockets[0],this->IPaddressesSockets[2],this->IPSocketsList[0]);}// In order to send datagrams
}
else{// TCP
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
	
	RetValue=this->ICPmanagementOpenServer(this->socket_fdArray[0],this->new_socketArray[0],this->IPaddressesSockets[0],this->IPaddressesSockets[2],this->IPSocketsList[0]);
	// Not needed because provided in the initialization strcpy(this->IPaddressesSockets[0],this->IPSocketsList[0]);
	// Eventually, if it is an intermediate node
	if (argc > 3){ // Establish client connection with next node
	// First parse the parama passed IP address

	//QTLANagent.ICPmanagementOpenClient(QTLANagent.socket_fdArray[1],char* IPaddressesSockets)
	}
}
	if (RetValue==-1){this->m_exit();} // Exit application
	else{this->numberSessions=1;} // Update indicators
	
	return 0; // All OK
}

int QTLAN::ICPmanagementOpenClient(int& socket_fd,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList) {    
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
    
    cout << "Client connected to server: "<< IPaddressesSockets << endl;
    return 0; // All Ok
}

int QTLAN::ICPmanagementOpenServer(int& socket_fd,int& new_socket,char* IPaddressesSockets,char* IPaddressesSocketsLocal,char* IPSocketsList) {// Node listening for connection from attached host    
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
    
    //cout << "IPSocketsList: "<< IPSocketsList << endl;
    
    cout << "Node starting socket server to host/node: " << IPSocketsList << endl;
    return 0; // All Ok
}

int QTLAN::ICPmanagementRead(int socket_fd_conn,int SockListenTimeusec) {
  // Check for new messages
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(socket_fd_conn, &fds);

  // Set the timeout
  struct timeval timeout;
  // The tv_usec member is rarely used, but it can be used to specify a more precise timeout. For example, if you want to wait for a message to arrive on the socket for up to 1.5 seconds, you could set tv_sec to 1 and tv_usec to 500,000.
  timeout.tv_sec = 0;
  timeout.tv_usec = SockListenTimeusec;
  
  int nfds = socket_fd_conn + 1;
  int ret = 0;
  if (string(SOCKtype)=="SOCK_DGRAM"){ret = select(nfds, &fds, NULL, NULL, &timeout);}
  else{ret = select(nfds, &fds, NULL, NULL, &timeout);}

  if (ret < 0) {
    //cout << "Node select no new messages" << endl;this->m_exit();
    return -1;}
   else if (ret==0){
   //cout << "Node no new messages" << endl;
   return -1;}
  else {// There is at least one new message
    if (FD_ISSET(socket_fd_conn, &fds)) {// is a macro that checks whether a specified file descriptor is set in a specified file descriptor set.
      // Read the message from the socket
        int valread=0;
	if (this->ReadFlagWait){
		if (string(SOCKtype)=="SOCK_DGRAM"){
		struct sockaddr_in orgaddr; 
		    memset(&orgaddr, 0, sizeof(orgaddr));		       
		    // Filling information 
		    orgaddr.sin_family    = AF_INET; // IPv4
		    for (int i=0; i<(NumSocketsMax); i++){
		    	//cout << "IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
		    	if (socket_fd_conn==socket_fdArray[i]){
		    		orgaddr.sin_addr.s_addr = inet_addr(this->IPaddressesSockets[i]);//INADDR_ANY; 
		    		orgaddr.sin_port = htons(PORT);
		    unsigned int addrLen;
			addrLen = sizeof(orgaddr);
		valread=recvfrom(socket_fd_conn,this->ReadBuffer,NumBytesBufferICPMAX,0,(struct sockaddr *) &orgaddr,&addrLen);
		    	}
		    }
		    
		}
    		else{valread = recv(socket_fd_conn, this->ReadBuffer,NumBytesBufferICPMAX,0);}
		}
	else{
		if (string(SOCKtype)=="SOCK_DGRAM"){
		struct sockaddr_in orgaddr; 
		    memset(&orgaddr, 0, sizeof(orgaddr));		       
		    // Filling information 
		    orgaddr.sin_family    = AF_INET; // IPv4 
		    for (int i=0; i<(NumSocketsMax); i++){
		    	//cout << "IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
		    	if (socket_fd_conn==socket_fdArray[i]){
		    		orgaddr.sin_addr.s_addr = inet_addr(this->IPaddressesSockets[i]);//INADDR_ANY; 
		    		orgaddr.sin_port = htons(PORT);
		    unsigned int addrLen;
			addrLen = sizeof(orgaddr);
		valread=recvfrom(socket_fd_conn,this->ReadBuffer,NumBytesBufferICPMAX,0,(struct sockaddr *) &orgaddr,&addrLen);//MSG_WAITALL (needs a NULL in the timer in select
		    	}
		    }
		    
		}
    		else{valread = recv(socket_fd_conn, this->ReadBuffer,NumBytesBufferICPMAX,MSG_DONTWAIT);}
	}
        //cout << "Node message received: " << this->ReadBuffer << endl;
      if (valread <= 0) {
	if (valread<0){
		cout << strerror(errno) << endl;
		cout << "Node error reading new messages" << endl;
		// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
		memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
		this->m_exit();
		return -1;
	}
	else{
		//cout << strerror(errno) << endl;
		//cout << "Node agent message of 0 Bytes" << endl;
		// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
		memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
		//this->m_exit();
		return 0;
	}
	
      }
      // Process the message
      else{// (n>0){
      	//cout << "Received message: " << this->ReadBuffer << endl;
      	return valread; //
      }
    }
    else{this->m_exit();cout << "Node error FD_ISSET new messages" << endl;return -1;}
  }    
}

int QTLAN::ICPmanagementSend(int socket_fd_conn,char* IPaddressesSockets) {
	cout << "Node SendBuffer: " << this->SendBuffer << endl;
	cout << "Node SendBuffer IPaddressesSockets: " << IPaddressesSockets << endl;
    const char* SendBufferAux = this->SendBuffer;
    int BytesSent=0;
    if (string(SOCKtype)=="SOCK_DGRAM"){    
    	    int socket_fd=0;
	    
	    struct sockaddr_in destaddr;   
	    memset(&destaddr, 0, sizeof(destaddr)); 
	       
	    // Filling server information 
	    destaddr.sin_family = AF_INET; 
	    destaddr.sin_port = htons(PORT); 
	    destaddr.sin_addr.s_addr = inet_addr(IPaddressesSockets); 
	    
	    for (int i=0; i<(NumSocketsMax); i++){
	    	//cout << "IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
	    	if (socket_fd_conn==socket_fdArray[i]){
	    		socket_fd=socket_SendUDPfdArray[i];
	    		BytesSent=sendto(socket_fd,SendBufferAux,strlen(SendBufferAux),MSG_CONFIRM,(const struct sockaddr *) &destaddr,sizeof(destaddr));
	    	}
	    }
    }
    else{BytesSent=send(socket_fd_conn, SendBufferAux, strlen(SendBufferAux),MSG_DONTWAIT);}
    
    if (BytesSent<0){
    	perror("send");
    	cout << "ICPmanagementSend: Errors sending Bytes" << endl;
    }
    return 0; // All OK
}

int QTLAN::ICPmanagementCloseClient(int socket_fd) {
    // closing the connected socket
    close(socket_fd);
 
    return 0; // All OK
}

int QTLAN::ICPmanagementCloseServer(int socket_fd,int new_socket) {
if (string(SOCKtype)=="SOCK_STREAM"){
    // closing the connected socket
    close(new_socket);
}
    // closing the listening socket
    close(socket_fd);
    
    return 0; // All OK
}

int QTLAN::StopICPconnections(int argc){
	if (string(SOCKtype)=="SOCK_DGRAM"){ICPmanagementCloseClient(this->socket_SendUDPfdArray[0]);}
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

int QTLAN::ICPConnectionsCheckNewMessages(int SockListenTimeusec){ 
int socket_fd_conn=0;

if (string(this->SCmode[this->socketReadIter])==string("client")){// Client sends on the file descriptor
	socket_fd_conn=this->socket_fdArray[this->socketReadIter];
}
else{// server checks on the socket connection
	socket_fd_conn=this->new_socketArray[this->socketReadIter];
}

if(this->ICPmanagementRead(socket_fd_conn,SockListenTimeusec)>0){this->m_start();} // Process the message

// Update the socketReadIter
this->socketReadIter++; // Variable to read each time a different socket
this->socketReadIter=this->socketReadIter % NumSocketsMax;
return 0; // All OK	  
}

int QTLAN::ICPdiscoverSend(char* ParamsCharArray){
     strcpy(this->SendBuffer,ParamsCharArray);//strtok(NULL,","));
    //cout << "SendBuffer: " << this->SendBuffer << endl;	
    // Parse the message information
    char IPaddressesSocketsAux[IPcharArrayLengthMAX];
    strcpy(IPaddressesSocketsAux,strtok(ParamsCharArray,","));//Null indicates we are using the same pointer as the last strtok
    //cout << "IPaddressesSocketsAux: " << IPaddressesSocketsAux << endl;
    // Understand which socket descriptor has to be used
    int socket_fd_conn;
    for (int i=0; i<(NumSocketsMax); ++i){
    	//cout << "IPSocketsList[i]: " << this->IPSocketsList[i] << endl;
    	if (string(this->IPSocketsList[i])==string(IPaddressesSocketsAux)){
    	//cout << "Found socket file descriptor//connection to send" << endl;
    	if (string(this->SCmode[i])==string("client") or string(SOCKtype)=="SOCK_DGRAM"){// Client sends on the file descriptor
    		socket_fd_conn=this->socket_fdArray[i];
    	}
    	else{// server sends on the socket connection
    		//cout << "socket_fd_conn" << socket_fd_conn << endl;
    		socket_fd_conn=this->new_socketArray[i];
    	}
    	}
    }  
    this->ICPmanagementSend(socket_fd_conn,IPaddressesSocketsAux);  
return 0; // All Ok
}
///////////////////////////////////////////////////////////////////////
int QTLAN::SendMessageAgent(char* ParamsDescendingCharArray){// Probably not use for this class
    memset(this->SendBuffer, 0, sizeof(this->SendBuffer));
    this->ICPdiscoverSend(ParamsDescendingCharArray);   

    return 0; //All OK
}

int QTLAN::UpdateSocketsInformation(){
	// First resolve the IPs the sockets are pointing to:
	for (int i=0;i<NumSocketsMax;++i){
	    struct sockaddr_in Address;
	    socklen_t len = sizeof (Address);
	    memset(&Address, 42, len);
	    if (getsockname(this->socket_fdArray[i], (struct sockaddr*)&Address, &len) == -1) {
	      //cout << "Failed to get socket name" << endl;
	      //return -1;
	    }
	    strcpy(IPSocketsList[i],inet_ntoa(Address.sin_addr));
	    cout << "inet_ntoa(Address.sin_addr): "<< inet_ntoa(Address.sin_addr) << endl;
	    cout << "IPSocketsList: "<< this->IPSocketsList[i] << endl;
	 }
	 return 0; // All ok
}

int QTLAN::ProcessNewMessage(){
cout << "Node ReadBuffer: " << this->ReadBuffer << endl;

// Parse the message information
char ReadBufferAuxOriginal[NumBytesBufferICPMAX] = {0};
strcpy(ReadBufferAuxOriginal,this->ReadBuffer); // Otherwise the strtok puts the pointer at the end and then ReadBuffer is empty

int NumQintupleComas=this->countQintupleComas(ReadBufferAuxOriginal);
//NumQintupleComas=1;
if (NumQintupleComas>20){NumQintupleComas=NumProcessMessagesConcurrentMax;}// Limit the total number that can be proceessed, avoiding cascades
//cout << "NumQintupleComas: " << NumQintupleComas << endl;
if (NumQintupleComas>20){NumQintupleComas=20;}// Limit the total number that can be proceessed
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

	//cout << "IPdest: " << IPdest << endl;
	//cout << "IPorg: " << IPorg << endl;
	//cout << "Type: " << Type << endl;
	//cout << "Command: " << Command << endl;
	//cout << "Payload: " << Payload << endl;
	//cout << "this->ReadBuffer: " << this->ReadBuffer << endl;
	// Identify what to do and execute it
	if (string(Type)==string("Operation")){// Operation message. Forward to the host (there should not be messages of this type in the QtransportLayerAgent. So not develop
		// Do not do anything
	}
	else if(string(Type)==string("Control")){//Control message	
		if (string(Command)==string("InfoRequest")){ // Request to provide information
			if (string(Payload)==string("NumStoredQubitsNode")){
			  int NumStoredQubitsNode=this->QNLAagent.QLLAagent.QPLAagent.GetNumStoredQubitsNode();// to be developed for more than one link
			  // Generate the message
			char ParamsCharArray[NumBytesBufferICPMAX] = {0};
			strcpy(ParamsCharArray,IPorg);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,IPdest);
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"Operation");
			strcat(ParamsCharArray,",");
			strcat(ParamsCharArray,"NumStoredQubitsNode");
			strcat(ParamsCharArray,",");
			char charNum[NumBytesBufferICPMAX] = {0};
			sprintf(charNum, "%d", NumStoredQubitsNode);
			strcat(ParamsCharArray,charNum);
			strcat(ParamsCharArray,",");// Very important to end the message
			//cout << "ParamsCharArray: " << ParamsCharArray << endl;
			  // reply immediately with a message to requester		  
			  this->ICPdiscoverSend(ParamsCharArray); 
			}
			else if (string(Payload)==string("NodeAreYouThere?")){
			// Mount message and send it to attached node
			 // Generate the message		
			memset(this->SendBuffer, 0, sizeof(this->SendBuffer));
			strcpy(this->SendBuffer,this->IPaddressesSockets[1]); //IP attached host to the other node
			//cout << "this->IPaddressesSockets[1]: " << this->IPaddressesSockets[1] << endl;
			strcat(this->SendBuffer,",");
			strcat(this->SendBuffer,this->IPaddressesSockets[2]); // This attached host so that the other node can reply
			//cout << "this->IPaddressesSockets[2]: " << this->IPaddressesSockets[2] << endl;
			strcat(this->SendBuffer,",");
			strcat(this->SendBuffer,"Control");
			strcat(this->SendBuffer,",");
			strcat(this->SendBuffer,"InfoRequest");
			strcat(this->SendBuffer,",");
			strcat(this->SendBuffer,"YesIamHere");
			strcat(this->SendBuffer,",");// Very important to end the message
			//cout << "this->SendBuffer: " << this->SendBuffer << endl;
			//cout << "Node responding that I am here" << endl;	
			int socket_fd_conn;	
			if (string(SOCKtype)=="SOCK_DGRAM"){
				socket_fd_conn=this->socket_fdArray[0]; // UDP works with socket descriptor
			}
			else{
				socket_fd_conn=this->new_socketArray[0];   // The first point probably to the host
			}
			this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[1]); // send message to node
			}
			else if (string(Payload)==string("YesIamHere")){this->OtherNodeThereFlag=true;}
			else{
			//discard
			cout << "Node does not have this information "<< Payload << endl;
			}
		}
		else if (string(Command)==string("IPaddressesSockets")){
			strcpy(this->IPaddressesSockets[1],strtok(Payload,":"));
			strcpy(this->IPaddressesSockets[3],strtok(NULL,":"));
			InfoIPaddressesSocketsFlag=true;
		}		
		else if (string(Command)==string("InfoProcess")){// Params messages
			//cout << "New Message: "<< Payload << endl;
			this->ReadParametersAgent(Payload);
		}
		else if (string(Command)==string("SendQubits")){// Send qubits to the requesting host
			this->QNLAagent.QLLAagent.QPLAagent.emitQuBit();
		}
		else if (string(Command)==string("ReceiveQubits")){// Read qubits to the attached node
			 this->QNLAagent.QLLAagent.QPLAagent.receiveQuBit();
		}
		else if (string(Command)==string("print")){
			cout << "New Message: "<< Payload << endl;
		}
		else{//Default
		// Do not do anything
		}
	}
	else{// Info message; Default
		if (string(Command)==string("print")){
			cout << "New Message: "<< Payload << endl;
		}
		else{//Default
		// Do not do anything
		}
	}  
}// for
// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer

return 0; // All OK
}

QTLAN::~QTLAN() {
// destructor
	this->StopICPconnections(this->ParamArgc);
	this->threadRef.join();// Terminate the process thread
	this->QNLAagent.~QNLA(); // Destructor of the agent below
}

void QTLAN::AgentProcessRequestsPetitions(){// Check next thing to do
 bool isValidWhileLoop = true;
 while(isValidWhileLoop){
 try{
   try {
   	this->acquire();// Wait semaphore until it can proceed
    	
        this->release(); // Release the semaphore 
        usleep(WaitTimeAfterMainWhileLoop);// Wait a few microseconds for other processes to enter
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

int QTLAN::RetrieveIPSocketsHosts(){ // Ask the host about the other host IP

try{
// It is a "blocking" communication between host and node, because it is many read trials for reading
int socket_fd_conn;
if (string(SOCKtype)=="SOCK_DGRAM"){
	socket_fd_conn=this->socket_fdArray[0]; // UDP works with socket descriptor
}
else{
	socket_fd_conn=this->new_socketArray[0];   // The first point probably to the host
}

int SockListenTimeusec=9999999; // Negative means infinite
this->InfoIPaddressesSocketsFlag=false;// Reset flag indicating information
int isValidWhileLoopCount = 5; // Number of tries

while(isValidWhileLoopCount>0){
memset(this->SendBuffer, 0, sizeof(this->SendBuffer));
strcpy(this->SendBuffer,this->IPaddressesSockets[0]); //IP attached host
//cout << "this->IPaddressesSockets[0]: " << this->IPaddressesSockets[0] << endl;
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,this->IPaddressesSockets[2]);
//cout << "this->IPaddressesSockets[2]: " << this->IPaddressesSockets[2] << endl;
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,"Operation");
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,"InfoRequest");
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,"IPaddressesSockets");
strcat(this->SendBuffer,",");// Very important to end the message
usleep(99999);
this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[0]); // send message to node
//usleep(999999);
this->ReadFlagWait=true;
if (string(SOCKtype)=="SOCK_DGRAM"){usleep(SockListenTimeusec);}
int ReadBytes=this->ICPmanagementRead(socket_fd_conn,SockListenTimeusec);
this->ReadFlagWait=false;
//cout << "ReadBytes: " << ReadBytes << endl;
if (ReadBytes>0){// Read block	
	this->ProcessNewMessage();		
}
if (this->InfoIPaddressesSocketsFlag==true){isValidWhileLoopCount=0;}
else{
	// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
	memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
	isValidWhileLoopCount--;
	if (isValidWhileLoopCount==0){
	cout << "Exiting QtransportLayerAgentN since no initial IP addresses retrieved" << endl;
	this->m_exit(); // Exit the application
	}
	else{
	this->release();// Non-block during sleeping
	usleep(9999);
	this->acquire(); // Re-acquire semaphore
	}
}
}//while

} // try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
    }

return 0; // All OK
}

int QTLAN::NegotiateInitialParamsNode(){

 try{
 
if (string(this->SCmode[1])==string("client")){
 // First check that the other node is with established connection
 
this->OtherNodeThereFlag=false;// Reset the value
// It is a "blocking" communication between host and node, because it is many read trials for reading

int socket_fd_conn;
if (string(SOCKtype)=="SOCK_DGRAM"){
	socket_fd_conn=this->socket_fdArray[0]; // UDP works with socket descriptor
}
else{
	socket_fd_conn=this->new_socketArray[0];   // The first point probably to the host
}

int SockListenTimeusec=9999999; // Negative means infinite

int isValidWhileLoopCount = 15; // Number of tries

while(isValidWhileLoopCount>0){
memset(this->SendBuffer, 0, sizeof(this->SendBuffer));
strcpy(this->SendBuffer,this->IPaddressesSockets[1]); //IP attached host to the other node
//cout << "this->IPaddressesSockets[1]: " << this->IPaddressesSockets[1] << endl;
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,this->IPaddressesSockets[2]); // This attached host so that the other node can reply
//cout << "this->IPaddressesSockets[2]: " << this->IPaddressesSockets[2] << endl;
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,"Control");
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,"InfoRequest");
strcat(this->SendBuffer,",");
strcat(this->SendBuffer,"NodeAreYouThere?");
strcat(this->SendBuffer,",");// Very important to end the message
//usleep(999999);
this->ICPmanagementSend(socket_fd_conn,this->IPaddressesSockets[0]); // send message to node
//usleep(999999);
this->ReadFlagWait=true;
if (string(SOCKtype)=="SOCK_DGRAM"){usleep(SockListenTimeusec);}
int ReadBytes=this->ICPmanagementRead(socket_fd_conn,SockListenTimeusec);
this->ReadFlagWait=false;
//cout << "ReadBytes: " << ReadBytes << endl;
if (ReadBytes>0){// Read block	
	this->ProcessNewMessage();	
}
if (this->OtherNodeThereFlag==true){isValidWhileLoopCount=0;}
else{
	// Never memset this->ReadBuffer!!! Important, otherwise the are kernel failures
	memset(this->ReadBuffer, 0, sizeof(this->ReadBuffer));// Reset buffer
	isValidWhileLoopCount--;
	if (isValidWhileLoopCount==0){
	cout << "Exiting QtransportLayerAgentN since no initial parameters negotiation achieved" << endl;
	this->m_exit(); // Exit the application
	}
	else{
	this->release();// Non-block during sleeping
	usleep(9999);
	this->acquire(); // Re-acquire semaphore
	}
}
}//while

//cout << "SendParametersAgent()" << endl;
 this->SendParametersAgent();
}
else{//server
// Expect to receive some information
}

} // try
  catch (...) { // Catches any exception
  cout << "Exception caught" << endl;
    }

return 0;// All OK
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
 
 signal(SIGPIPE, SignalPIPEHandler);
 QTLAN QTLANagent(0); // Initiate the instance with 0 sessions connected. A session is composed of one server sockets descriptor active.
 QTLANagent.m_pause(); // Initiate in paused state.
 //cout << "Starting in pause state the QtransportLayerAgentN" << endl;
 // Save some given parameters to the instance of the object
 QTLANagent.ParamArgc=argc;
 strcpy(QTLANagent.SCmode[1],argv[1]); // to know if this host instance is client or server
 cout << "QTLANagent.SCmode[1]: " << QTLANagent.SCmode[1] << endl;
 strcpy(QTLANagent.IPaddressesSockets[2],argv[2]); // To know its own IP in the control network
 cout << "QTLANagent.IPaddressesSockets[2]: " << QTLANagent.IPaddressesSockets[2] << endl;
 strcpy(QTLANagent.IPaddressesSockets[0],argv[3]); // To know the other host IP in the control network
 cout << "QTLANagent.IPaddressesSockets[0]: " << QTLANagent.IPaddressesSockets[0] << endl;
 // Then the sub agents threads can be started
 QTLANagent.QNLAagent.InitAgentProcess();
 QTLANagent.InitiateBelowAgentsObjects();
 
 // One of the firsts things to do for a node is to initialize listening ICP socket connection with it host or with its adjacent nodes.
 QTLANagent.InitiateICPconnections(QTLANagent.ParamArgc);
 // Discover some IP addresses of interest 
 QTLANagent.RetrieveIPSocketsHosts();
 /*
 cout << "QTLANagent.IPaddressesSockets[0]: " << QTLANagent.IPaddressesSockets[0] << endl;
 cout << "QTLANagent.IPaddressesSockets[1]: " << QTLANagent.IPaddressesSockets[1] << endl;
 cout << "QTLANagent.IPaddressesSockets[2]: " << QTLANagent.IPaddressesSockets[2] << endl;
 cout << "QTLANagent.IPaddressesSockets[3]: " << QTLANagent.IPaddressesSockets[3] << endl;
 cout << "QTLANagent.IPSocketsList[0]: " << QTLANagent.IPSocketsList[0] << endl;
 cout << "QTLANagent.IPSocketsList[1]: " << QTLANagent.IPSocketsList[1] << endl;
 */
 // Then negotiate some parameters
 QTLANagent.NegotiateInitialParamsNode(); 
 
 // Then await for next actions
 
 bool isValidWhileLoop = true;
 
 while(isValidWhileLoop){ 
   try{
 	try {
    	// Code that might throw an exception 
 	// Check if there are need messages or actions to be done by the node
 	QTLANagent.ICPConnectionsCheckNewMessages(SockListenTimeusecStandard); // This function has some time out (so will not consume resources of the node)
 	QTLANagent.SendParametersAgent(); // Send Parameters information stored
       switch(QTLANagent.getState()) {
           case QTLAN::APPLICATION_RUNNING: {               
               // Do Some Work
               QTLANagent.ProcessNewMessage();
               while(QTLANagent.ICPConnectionsCheckNewMessages(SockListenTimeusecStandard)>0);// Make sure to remove all pending mesages in the socket
               QTLANagent.m_pause(); // After procesing the request, pass to paused state
               break;
           }
           case QTLAN::APPLICATION_PAUSED: {
               // Maybe do some checks if necessary 
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
        usleep(WaitTimeAfterMainWhileLoop);// Wait a few microseconds for other processes to enter
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
   
 return 0; // Everything Ok
}

