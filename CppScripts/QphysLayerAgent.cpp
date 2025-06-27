/* Author: Prof. Marc Jofre
Dept. Network Engineering
Universitat Politècnica de Catalunya - Technical University of Catalonia

Modified: 2025
Created: 2024

Agent script for Quantum Physical Layer
*/
#include "QphysLayerAgent.h"
#include<iostream>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<bitset>
#include <algorithm> // For std::nth_element
// Time/synchronization management
#include <chrono>

#define LinkNumberMAX 2
#include "./BBBhw/GPIO.h"
#include <stdlib.h>
// Threading
#define WaitTimeAfterMainWhileLoop 10000000 // nanoseconds
// Payload messages
#define NumBytesPayloadBuffer 1000
#define NumParamMessagesMax 20
#define IPcharArrayLengthMAX 15
// Threads
#include <thread>
// Semaphore
#include <atomic>
// time points
#define WaitTimeToFutureTimePoint 399000000 // Max 999999999. It is the time barrier to try to achieve synchronization. Considered nanoseconds (it can be changed on the transformation used)
#define UTCoffsetBarrierErrorThreshold 37000000000 // Some BBB when synch with linuxPTP have an error on the UTC offset with respect TAI. Remove this sistemic offset and announce it!
// Mathematical calculations
#include <cmath>

//using namespace exploringBB; // API to easily use GPIO in c++. No because it will confuse variables (like semaphore acquire)
/* A Simple GPIO application
* Written by Derek Molloy for the book "Exploring BeagleBone: Tools and
* Techniques for Building with Embedded Linux" by John Wiley & Sons, 2018
* ISBN 9781119533160. Please see the file README.md in the repository root
* directory for copyright and GNU GPLv3 license information.*/
using namespace std;

namespace nsQphysLayerAgent {
QPLA::QPLA() {// Constructor
	PRUGPIO.InitAgentProcess();// Initialize thread of the agent below
	//PRUGPIO=new GPIO();// Initiates custom PRU code in BBB
	//PRUGPIO->InitAgentProcess(); // Launch the periodic synchronization. Done in the class itself
	 //Very slow GPIO BBB not used anymore
	/*// The above pins initializatoins (and also in the destructor will not be needed in the future since it is done with PRU
	inGPIO=new GPIO(this->ReceiveLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
	inGPIO->setDirection(INPUT);
	inGPIO->setEdgeType(NONE);
	inGPIO->streamInOpen();
	outGPIO=new GPIO(this->EmitLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
	outGPIO->setDirection(OUTPUT);
	outGPIO->streamOutOpen();
	outGPIO->streamOutWrite(LOW);//outGPIO.setValue(LOW);*/
	// Synchronized "slotted" emission
	// Initialize some arrays at the beggining
	int CombinationLinksNumAux=static_cast<int>(2*((1LL<<LinkNumberMAX)-1));
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		for (int i=0;i<CombinationLinksNumAux;i++){
		  SmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  SmallOffsetDriftPerLinkError[iQuadChIter][i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  oldSmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Old Values, Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  ReferencePointSmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Identified by each link, annotate the first time offset that all other acquisitions should match to, so an offset with respect the SignalPeriod histogram
		  // Filtering qubits
		  NonInitialReferencePointSmallOffsetDriftPerLink[iQuadChIter][i]=false; // Identified by each link, annotate if the first capture has been done and hence the initial ReferencePoint has been stored
		  for (int j=0;j<NumSmallOffsetDriftAux;j++){
		  	SmallOffsetDriftAuxArray[iQuadChIter][i][j]=0;
		  }
		  IterSmallOffsetDriftAuxArray[iQuadChIter][i]=0;
		}
	}
	for (int i=0;i<LinkNumberMAX;i++){
		QuadChannelParamsLink[i]=-1;// Reset values
		for (int j=0;j<3;j++){
			SynchNetworkParamsLink[i][j]=0.0; // Stores the synchronizatoin parameters corrections to apply depending on the node to whom receive or send. Zerod at the begining
			SynchNetworkParamsLinkOther[i][j]=0.0; // Stores the synchronizatoin parameters corrections to apply depending on the node to whom receive or send from the other nodes. Zeroed at the begining
		}
	}
}

////////////////////////////////////////////////////////
void QPLA::acquire() {
/*while(valueSemaphore==0);
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
		//if (ProtectionSemaphoreTrap>UnTrapSemaphoreValueMaxCounter){this->release();cout << "QPLA::Releasing semaphore!!!" << endl;}// Avoid trapping situations
		if (this->valueSemaphore.compare_exchange_strong(valueSemaphoreExpected,false,std::memory_order_acquire)){	
			break;
		}
	}
}

void QPLA::release() {
this->valueSemaphore.store(true,std::memory_order_release); // Make sure it stays at 1
//this->valueSemaphore.fetch_add(1,std::memory_order_release);
}

//////////////////////////////////////////////
int QPLA::InitParametersAgent(){// Client node have some parameters to adjust to the server node
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer
this->valueSemaphore=1; // Make sure it stays a
return 0; //All OK
}

int QPLA::SendParametersAgent(char* ParamsCharArray){// The upper layer gets the information to be send
	this->acquire();

	strcat(ParamsCharArray,"Phys;");
	if (string(this->PayloadSendBuffer)!=string("")){
		strcat(ParamsCharArray,this->PayloadSendBuffer);
	}
	else{
		strcat(ParamsCharArray,"none_none_");
	}
	strcat(ParamsCharArray,";");
memset(this->PayloadSendBuffer, 0, sizeof(this->PayloadSendBuffer));// Reset buffer

this->release();

return 0; // All OK

}

int QPLA::SetSendParametersAgent(char* ParamsCharArray){// Node accumulates parameters for the other node

	strcat(this->PayloadSendBuffer,ParamsCharArray);

return 0; //All OK
}

int QPLA::ReadParametersAgent(){// Node checks parameters from the other node
	if (string(this->PayloadReadBuffer)!=string("") and string(this->PayloadReadBuffer)!=string("none_none_")){
	//cout << "There are new Parameters" << endl;
		this->ProcessNewParameters();
	}
memset(this->PayloadReadBuffer, 0, sizeof(this->PayloadReadBuffer));// Reset buffer
return 0; // All OK
}

int QPLA::SetReadParametersAgent(char* ParamsCharArray){// The upper layer sets information to be read
	this->acquire();
//cout << "QPLA::ReadParametersAgent: " << ParamsCharArray << endl;
	char DiscardBuffer[NumBytesPayloadBuffer]={0};
	strcpy(DiscardBuffer,strtok(ParamsCharArray,";"));
	strcpy(this->PayloadReadBuffer,strtok(NULL,";"));

	this->release();
return 0; // All OK
}
////////////////////////////////////////////////////
int QPLA::countDoubleColons(char* ParamsCharArray) {
	int colonCount = 0;

	for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
		if (ParamsCharArray[i] == ':') {
			colonCount++;
		}
	}

	return colonCount/2;
}

int QPLA::countDoubleUnderscores(char* ParamsCharArray) {
	int underscoreCount = 0;
	for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
		if (ParamsCharArray[i] == '_') {
			underscoreCount++;
		}
	}
	return underscoreCount/2;
}

bool QPLA::isPotentialIpAddressStructure(const char* ipAddress) {
	int dots = 0;
	int digitCount = 0;

	for (int i = 0; ipAddress[i] != '\0'; ++i) {
		if (isdigit(ipAddress[i])) {
			digitCount++;
		} else if (ipAddress[i] == '.') {
			if (digitCount == 0 || digitCount > 3) {
        return false; // Invalid structure
      }
      dots++;
      digitCount = 0;
    } else {
      return false; // Invalid character
    }
  }

  return dots == 3 && digitCount > 0 && digitCount <= 3; // Check for 4 octets
}

int QPLA::countUnderscores(char* ParamsCharArray) {
	int underscoreCount = 0;

	for (int i = 0; ParamsCharArray[i] != '\0'; i++) {
		if (ParamsCharArray[i] == '_') {
			underscoreCount++;
		}
	}

	return underscoreCount;
}

int QPLA::ProcessNewParameters(){
	char ParamsCharArray[NumBytesPayloadBuffer]={0};
	char HeaderCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
	char ValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};
	char TokenValuesCharArray[NumParamMessagesMax][NumBytesPayloadBuffer]={0};

	strcpy(ParamsCharArray,this->PayloadReadBuffer);
	memset(this->PayloadReadBuffer, 0, sizeof(this->PayloadReadBuffer));// Reset buffer
	
	int NumDoubleUnderscores = this->countDoubleUnderscores(ParamsCharArray);
	//cout << "NumDoubleUnderscores: " << NumDoubleUnderscores << endl;

	for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
		if (iHeaders==0){
			strcpy(HeaderCharArray[iHeaders],strtok(ParamsCharArray,"_"));		
		}
		else{
			strcpy(HeaderCharArray[iHeaders],strtok(NULL,"_"));
		}
		strcpy(ValuesCharArray[iHeaders],strtok(NULL,"_"));
	}
	//cout << "Phys: Processing Parameters" << endl;
	//cout << "this->PayloadReadBuffer: " << this->PayloadReadBuffer << endl;
		for (int iHeaders=0;iHeaders<NumDoubleUnderscores;iHeaders++){
	//cout << "HeaderCharArray[iHeaders]: " << HeaderCharArray[iHeaders] << endl;
	// Missing to develop if there are different values
	/*
	if (string(HeaderCharArray[iHeaders])==string("EmitLinkNumberArray[0]")){
		this->EmitLinkNumberArray[0]=(int)atoi(ValuesCharArray[iHeaders]);	
		outGPIO=new GPIO(this->EmitLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
		outGPIO->setDirection(OUTPUT);
		outGPIO->streamOutOpen();
		outGPIO->streamOutWrite(LOW);//outGPIO.setValue(LOW);
	}
	else if (string(HeaderCharArray[iHeaders])==string("ReceiveLinkNumberArray[0]")){
		this->ReceiveLinkNumberArray[0]=(int)atoi(ValuesCharArray[iHeaders]);
		inGPIO=new GPIO(this->ReceiveLinkNumberArray[0]);// Produces a 250ms sleep, so it has to be executed at the beggining to not produce relevant delays
		inGPIO->setDirection(INPUT);
		inGPIO->setEdgeType(NONE);
		inGPIO->streamInOpen();
	}
	*/
	if (string(HeaderCharArray[iHeaders])==string("QuBitsPerSecondVelocity[0]")){this->QuBitsPerSecondVelocity[0]=(float)atoi(ValuesCharArray[iHeaders]);}
	else if (string(HeaderCharArray[iHeaders])==string("OtherClientNodeSynchParams")){// Information about synchronization from other nodes
		// Identify the IP of the informing node and store it accordingly
		//cout << "QPLA::ValuesCharArray[iHeaders]: " << ValuesCharArray[iHeaders] << endl;
		//ValuesCharArray[iHeaders] consists of IP of the node sending the information to this specific node: Offset:Rel.Freq.Diff:Period
		char CurrentReceiveHostIP[NumBytesBufferICPMAX]={0};
		char ValuesCharArrayiHeadersAux[NumBytesBufferICPMAX]={0};
		strcpy(ValuesCharArrayiHeadersAux,ValuesCharArray[iHeaders]); // Copy it to manipulate without losing information
		strcpy(CurrentReceiveHostIP,strtok(ValuesCharArrayiHeadersAux,":")); // Identifies index position for storage
		strcat(CurrentReceiveHostIP,"_"); // Add _ so that the identifier below works

		// Store the potential IP identification of the emitters (for Receive) and receivers for (Emit) - in order to identify the link
		CurrentSpecificLink=-1;// Reset value
		numSpecificLinkmatches=0; // Reset value
		int numCurrentEmitReceiveIP=countUnderscores(CurrentReceiveHostIP); // Which means the number of IP addresses that currently will send/receive qubits
		char CurrentReceiveHostIPAuxAux[NumBytesBufferICPMAX]={0}; // Copy to not destroy original
		strcpy(CurrentReceiveHostIPAuxAux,CurrentReceiveHostIP);
		char SpecificCurrentReceiveHostIPAuxAux[IPcharArrayLengthMAX]={0};
		//cout << "QPLA::ProcessNewParameters CurrentNumIdentifiedEmitReceiveIP: " << CurrentNumIdentifiedEmitReceiveIP << endl;

		for (int j=0;j<numCurrentEmitReceiveIP;j++){
			if (j==0){strcpy(SpecificCurrentReceiveHostIPAuxAux,strtok(CurrentReceiveHostIPAuxAux,"_"));}
			else{strcpy(SpecificCurrentReceiveHostIPAuxAux,strtok(NULL,"_"));}
			//cout << "QPLA::ProcessNewParameters SpecificCurrentReceiveHostIPAuxAux: " << SpecificCurrentReceiveHostIPAuxAux << endl;
			for (int i=0;i<CurrentNumIdentifiedEmitReceiveIP;i++){
				//cout << "QPLA::ProcessNewParameters LinkIdentificationArray[i]: " << LinkIdentificationArray[i] << endl;
				if (string(LinkIdentificationArray[i])==string(SpecificCurrentReceiveHostIPAuxAux)){// IP already present
					if (CurrentSpecificLink<0){CurrentSpecificLink=i;}// Take the first identified, which is the one that matters most
					CurrentSpecificLinkMultipleIndices[numSpecificLinkmatches]=i; // For multiple links at the same time
					numSpecificLinkmatches++;
				}
			}
			if (CurrentSpecificLink<0){// Not previously identified, so stored them if possible
				if ((CurrentNumIdentifiedEmitReceiveIP+1)<=LinkNumberMAX){
					strcpy(LinkIdentificationArray[CurrentNumIdentifiedEmitReceiveIP],SpecificCurrentReceiveHostIPAuxAux);// Update value
					CurrentSpecificLink=CurrentNumIdentifiedEmitReceiveIP;
					CurrentNumIdentifiedEmitReceiveIP++;
				}
				else{// Mal function we should not be here
					cout << "QPLA::ProcessNewParameters Number of identified emitters/receivers to this node has exceeded the expected value!!!" << endl;
				}
			}
		}

		//cout << "QPLA::ValuesCharArray[iHeaders]: " << ValuesCharArray[iHeaders] << endl;
		strcpy(ValuesCharArrayiHeadersAux,ValuesCharArray[iHeaders]); // Copy it to manipulate without losing information
		strcpy(CurrentReceiveHostIP,strtok(ValuesCharArrayiHeadersAux,":")); // Identifies index position for storage
		cout << "QPLA::Receiving synch. parameters from other node " << CurrentReceiveHostIP << endl;
		
		if (CurrentSpecificLink>-1 and CurrentSpecificLink<LinkNumberMAX){
			//cout << "QPLA::ProcessNewParameters CurrentSpecificLink " << CurrentSpecificLink << endl;
			SynchNetworkParamsLinkOther[CurrentSpecificLink][0]=stod(strtok(NULL,":")); // Save the provided values to the proper indices. Synch offset
			SynchNetworkParamsLinkOther[CurrentSpecificLink][1]=stod(strtok(NULL,":")); // Save the provided values to the proper indices. Relative frequency difference.
			SynchNetworkParamsLinkOther[CurrentSpecificLink][2]=stod(strtok(NULL,":")); // Save the provided values to the proper indices. Period.

			//cout << "QPLA::SynchNetworkParamsLinkOther[CurrentSpecificLink][0]: " << SynchNetworkParamsLinkOther[CurrentSpecificLink][0] << endl;
			//cout << "QPLA::SynchNetworkParamsLinkOther[CurrentSpecificLink][1]: " << SynchNetworkParamsLinkOther[CurrentSpecificLink][1] << endl;
			//cout << "QPLA::SynchNetworkParamsLinkOther[CurrentSpecificLink][2]: " << SynchNetworkParamsLinkOther[CurrentSpecificLink][2] << endl;
		}
		else{// We should be here
			cout << "QPLA::Bad CurrentSpecificLink index. Not updating other node synch values!" << endl;
		}
	}
	else if (string(HeaderCharArray[iHeaders])==string("OtherClientNodeFutureTimePoint")){// Also helps to wait here for the thread	
		std::chrono::nanoseconds duration_back(static_cast<unsigned long long int>(strtoull(ValuesCharArray[iHeaders],NULL,10)));
		this->OtherClientNodeFutureTimePoint=Clock::time_point(duration_back);
		//cout << "OtherClientNodeFutureTimePoint: " << static_cast<unsigned long long int>(strtoull(ValuesCharArray[iHeaders],NULL,10)) << endl;
		// Debugging
		//TimePoint TimePointClockNow=Clock::now();
		//auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
		// Convert duration to desired time
		//unsigned int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds) 
		//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
		//auto duration_since_epoch=this->OtherClientNodeFutureTimePoint.time_since_epoch();
		// Convert duration to desired time
		//unsigned int time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epoch).count(); // Convert 
		//cout << "time_as_count: " << time_as_count << endl;
	}
	else{// discard
	}
	}

	return 0; // All OK
}
////////////////////////////////////////////////////

int QPLA::InitAgentProcess(){
	// Then, regularly check for next job/action without blocking		  	
	// Not used void* params;
	// Not used this->threadRef=std::thread(&QTLAH::AgentProcessStaticEntryPoint,params);
	this->threadRef=std::thread(&QPLA::AgentProcessRequestsPetitions,this);
	  //if (ret) {
	    // Handle the error
	  //} 
	return 0; //All OK
}

struct timespec QPLA::SetFutureTimePointOtherNode(){// It is responsability of the host to distribute this time point to the other host's nodes
// Reset the ClientNodeFutureTimePoint
	this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
//struct timespec requestHalfPeriod,requestQuarterPeriod,requestPeriod,requestWhileWait;
//requestHalfPeriod.tv_sec=0;
//requestQuarterPeriod.tv_sec=0;
//requestPeriod.tv_sec=0;
//requestWhileWait.tv_sec=0;
//requestHalfPeriod.tv_nsec = (long)QuBitsNanoSecHalfPeriodInt[0];
//requestQuarterPeriod.tv_nsec = (long)QuBitsNanoSecQuarterPeriodInt[0];
//requestPeriod.tv_nsec = (long)QuBitsNanoSecPeriodInt[0];
	struct timespec requestWhileWait;
// Client sets a future TimePoint for measurement and communicates it to the server (the one sending the qubits)
// Somehow, here it is assumed that the two system clocks are quite synchronized (maybe with the Precise Time Protocol)
/*
// Debugging
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., milliseconds,microseconds) 
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;
//
*/
this->FutureTimePoint = Clock::now()+std::chrono::nanoseconds(WaitTimeToFutureTimePoint);// Set a time point in the future
auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Add some margin so that busywait can be implemented for faster response // Convert 
//cout << "time_as_count: " << time_as_count << endl;

// Tell to the other nodes
char ParamsCharArray[NumBytesPayloadBuffer] = {0};
char charNum[NumBytesPayloadBuffer] = {0};

int numUnderScores=countUnderscores(this->IPaddressesTimePointBarrier); // Which means the number of IP addresses to send the Time Point barrier
char IPaddressesTimePointBarrierAux[NumBytesBufferICPMAX]={0}; // Copy to not destroy original
strcpy(IPaddressesTimePointBarrierAux,IPaddressesTimePointBarrier);
for (int iIterIPaddr=0;iIterIPaddr<numUnderScores;iIterIPaddr++){// Iterate over the different nodes to tell
	// Mount the Parameters message for the other node
	if (iIterIPaddr==0){
		strcpy(ParamsCharArray,"IPdest_");// Initiates the ParamsCharArray, so use strcpy
		strcat(ParamsCharArray,strtok(IPaddressesTimePointBarrierAux,"_"));// Indicate the address to send the Future time Point
	} 
	else{
		strcat(ParamsCharArray,"IPdest_");// Continues the ParamsCharArray, so use strcat
		strcat(ParamsCharArray,strtok(NULL,"_"));// Indicate the address to send the Future time Point
	}
	strcat(ParamsCharArray,"_");// Add underscore separator
	strcat(ParamsCharArray,"OtherClientNodeFutureTimePoint_"); // Continues the ParamsCharArray, so use strcat		 
	sprintf(charNum, "%llu", TimePointFuture_time_as_count);//%llu: unsigned long long int
	strcat(ParamsCharArray,charNum);
	strcat(ParamsCharArray,"_"); // Final _
} // end for to the different addresses to send the params information
//cout << "QPLA::ParamsCharArray: " << ParamsCharArray << endl;
this->acquire();
this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other nodes
this->release();
this->RelativeNanoSleepWait((unsigned int)(100*(unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));// Give some time to be able to send the above message
////////////////////////////////////////////
TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-this->TimeClockMarging; // Add some margin so that busywait can be implemented for faster response
requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
//////////////////////////
return requestWhileWait;
}

struct timespec QPLA::GetFutureTimePointOtherNode(){
	struct timespec requestWhileWait;
int MaxWhileRound=100;// Amount of check to receive the other node Time Point Barrier
// Wait to receive the FutureTimePoint from other node
this->acquire();
while(MaxWhileRound>0){// Make sure we take the last one sent this->OtherClientNodeFutureTimePoint==std::chrono::time_point<Clock>() && MaxWhileRound>0){
	this->release();
	MaxWhileRound--;	
	this->RelativeNanoSleepWait((unsigned int)(0.1*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//Maybe some sleep to reduce CPU consumption	
	this->acquire();
};
if (this->OtherClientNodeFutureTimePoint==std::chrono::time_point<Clock>()){//MaxWhileRound<=0){
	this->OtherClientNodeFutureTimePoint=Clock::now();
	cout << "QPLA could not obtain in time the TimePoint from the other node" << endl;
}// Provide a TimePoint to avoid blocking issues
this->FutureTimePoint=this->OtherClientNodeFutureTimePoint;
// Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
this->release();
//cout << "MaxWhileRound: " << MaxWhileRound << endl;
/////////////////////////////////////////////
// Checks
TimePoint TimePointClockNow=Clock::now();
auto duration_since_epochTimeNow=TimePointClockNow.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimeNow_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochTimeNow).count(); // Convert duration to desired time unit (e.g., microseconds,microseconds)
//cout << "TimeNow_time_as_count: " << TimeNow_time_as_count << endl;

auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-this->TimeClockMarging; // Add some margin so that busywait can be implemented for faster response // // Convert duration to desired time unit (e.g., milliseconds,microseconds)
//cout << "TimePointFuture_time_as_count: " << TimePointFuture_time_as_count << endl;
unsigned long long int TimePointsDiff_time_as_count=0;
long long int CheckTimePointsDiff_time_as_count=0;
CheckTimePointsDiff_time_as_count=(long long int)(TimeNow_time_as_count-TimePointFuture_time_as_count);
cout << "CheckTimePointsDiff_time_as_count: " << CheckTimePointsDiff_time_as_count << endl;
if (CheckTimePointsDiff_time_as_count<=(-UTCoffsetBarrierErrorThreshold)){
	cout << "UTC TAI offset wrongly resolved!" << endl;
	TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count()-UTCoffsetBarrierErrorThreshold-this->TimeClockMarging; // Add some margin so that busy wait can be implemented for faster response // // Convert duration to desired time unit (e.g., milliseconds,microseconds)
	CheckTimePointsDiff_time_as_count=(long long int)(TimeNow_time_as_count-TimePointFuture_time_as_count);
	this->FutureTimePoint=this->FutureTimePoint-std::chrono::nanoseconds(UTCoffsetBarrierErrorThreshold);
	cout << "New CheckTimePointsDiff_time_as_count: " << CheckTimePointsDiff_time_as_count << endl;
}

requestWhileWait.tv_sec=(int)(TimePointFuture_time_as_count/((long)1000000000));
requestWhileWait.tv_nsec=(long)(TimePointFuture_time_as_count%(long)1000000000);
///////////////////////////////////
return requestWhileWait;
}

int QPLA::PurgeExtraordinaryTimePointsNodes(){
int MaxWhileRound=500;// Amount of check to receive the other node Time Point Barrier
// Wait to receive the FutureTimePoint from other node
this->acquire();
while(MaxWhileRound>0){// Make sure to purge remaining Time Points from other nodes
	this->release();
	MaxWhileRound--;	
	this->RelativeNanoSleepWait((unsigned int)(0.1*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));//Maybe some sleep to reduce CPU consumption	
	this->acquire();
};
// Reset the ClientNodeFutureTimePoint
this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();
return 0; // All ok
}

int QPLA::SimulateEmitQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux,int QuadEmitDetecSelecAux){
	this->acquire();
	this->FlagTestSynch=false;
	strcpy(this->ModeActivePassive,ModeActivePassiveAux);
	strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
	this->RetrieveOtherEmiterReceiverMethod(); // Identifies involved links by IPs mentioned
	// Retrieve the involved single specific quad group channel if possible
	QuadEmitDetecSelec=QuadEmitDetecSelecAux; // Identifies the quad groups to emit
	SpecificQuadChEmt=-1;// Reset value
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){// Identify the quad group channel if there is only one
		if (QuadEmitDetecSelec==(1<<iQuadChIter)){
			SpecificQuadChEmt=iQuadChIter;
		}
	}
	strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
	this->NumberRepetitionsSignal=numReqQuBitsAux;
// Adjust the network synchronization values
this->HistPeriodicityAux=HistPeriodicityAuxAux;// Update value
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=CurrentExtraSynchNetworkParamsLink[0];// synch trig offset
this->FineSynchAdjVal[1]=CurrentExtraSynchNetworkParamsLink[1];// synch trig frequency
//cout << "QPLA::SimulateEmitQuBit CurrentExtraSynchNetworkParamsLink[0]: " << CurrentExtraSynchNetworkParamsLink[0] << endl;
}
else{
this->FineSynchAdjVal[0]=0.0;// synch trig offset
this->FineSynchAdjVal[1]=0.0;// synch trig frequ
}
this->FineSynchAdjVal[0]+=FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]+=FineSynchAdjValAux[1];// synch trig frequency
//cout << "this->FineSynchAdjVal[1]: " << this->FineSynchAdjVal[1] << endl;
if (this->RunThreadSimulateEmitQuBitFlag and this->RunThreadAcquireSimulateNumStoredQubitsNode and this->RunThreadSimulateReceiveQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateEmitQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateEmitQuBitRefAux=std::thread(&QPLA::ThreadSimulateEmitQuBit,this);
threadSimulateEmitQuBitRefAux.join();//threadSimulateEmitQuBitRefAux.detach();
}
else{
	cout << "QPLA::Not possible to launch ThreadSimulateEmitQuBit!" << endl;
}
this->release();
/*
cout << "ModeActivePassive: " << ModeActivePassive << endl;
cout << "CurrentSpecificLink: " << CurrentSpecificLink << endl;
cout << "NumberRepetitionsSignal: " << NumberRepetitionsSignal << endl;
cout << "HistPeriodicityAux: " << HistPeriodicityAux << endl;
cout << "FineSynchAdjVal[0]: " << FineSynchAdjVal[0] << endl;
cout << "FineSynchAdjVal[1]: " << FineSynchAdjVal[1] << endl;
*/
return 0; // return 0 is for no error
}

int QPLA::SimulateEmitSynchQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux,char* IPaddressesAux,int numReqQuBitsAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass, int QuadEmitDetecSelecAux){
	this->acquire();
	// Reset values
	int CombinationLinksNumAux=static_cast<int>(2*((1LL<<LinkNumberMAX)-1));
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		for (int i=0;i<CombinationLinksNumAux;i++){
		  SmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  SmallOffsetDriftPerLinkError[iQuadChIter][i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  oldSmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Old Values, Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  ReferencePointSmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Identified by each link, annotate the first time offset that all other acquisitions should match to, so an offset with respect the SignalPeriod histogram
		  // Filtering qubits
		  NonInitialReferencePointSmallOffsetDriftPerLink[iQuadChIter][i]=false; // Identified by each link, annotate if the first capture has been done and hence the initial ReferencePoint has been stored
		  for (int j=0;j<NumSmallOffsetDriftAux;j++){
		  	SmallOffsetDriftAuxArray[iQuadChIter][i][j]=0;
		  }
		  IterSmallOffsetDriftAuxArray[iQuadChIter][i]=0;
		}
	}
	this->FlagTestSynch=true;
	strcpy(this->ModeActivePassive,ModeActivePassiveAux);
	strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
	this->RetrieveOtherEmiterReceiverMethod();
	// Retrieve the involved single specific quad group channel if possible
	QuadEmitDetecSelec=QuadEmitDetecSelecAux; // Identifies the quad groups to emit
	SpecificQuadChEmt=-1;// Reset value
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){// Identify the quad group channel if there is only one
		if (QuadEmitDetecSelec==(1<<iQuadChIter)){
			SpecificQuadChEmt=iQuadChIter;
		}
	}
	if (SpecificQuadChEmt<0){
		SpecificQuadChEmt=7;
		cout << "QPLA::SimulateEmitSynchQuBit wrongly identified single specific quad group channel...setting it to index 7 (all channels)" << endl;
	}
	strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
	this->NumberRepetitionsSignal=numReqQuBitsAux;
//this->NumRunsPerCenterMass=NumRunsPerCenterMassAux; hardcoded value
	if (NumCalcCenterMass>1){
	this->FreqSynchNormValuesArray[0]=FreqSynchNormValuesArrayAux[0];// first test frequency norm.
	this->FreqSynchNormValuesArray[1]=FreqSynchNormValuesArrayAux[1];// second test frequency norm.
	this->FreqSynchNormValuesArray[2]=FreqSynchNormValuesArrayAux[2];// third test frequency norm.
}
else{
	this->FreqSynchNormValuesArray[0]=FreqSynchNormValuesArrayAux[0];// first test frequency norm.
}
//cout << "this->FineSynchAdjVal[1]: " << this->FineSynchAdjVal[1] << endl;
// Remove previous synch values - probably not for the emitter (since calculation for synch values are done as receiver)
// Here run the several iterations with different testing frequencies
// Adjust the network synchronization values
this->HistPeriodicityAux=HistPeriodicityAuxAux;// Update value
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=0.0*CurrentExtraSynchNetworkParamsLink[0];// synch trig offset
this->FineSynchAdjVal[1]=0.0*CurrentExtraSynchNetworkParamsLink[1];// synch trig frequency
}
else{
this->FineSynchAdjVal[0]=0.0;// synch trig offset
this->FineSynchAdjVal[1]=0.0;// synch trig frequ
}
this->FineSynchAdjVal[0]+=FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]+=FineSynchAdjValAux[1]+FreqSynchNormValuesArray[iCenterMass];// synch trig frequency
if (this->RunThreadSimulateEmitQuBitFlag and this->RunThreadAcquireSimulateNumStoredQubitsNode and this->RunThreadSimulateReceiveQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateEmitQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateEmitQuBitRefAux=std::thread(&QPLA::ThreadSimulateEmitQuBit,this);
threadSimulateEmitQuBitRefAux.join();//threadSimulateEmitQuBitRefAux.detach();//threadSimulateEmitQuBitRefAux.join();//
}
else{		
	cout << "QPLA::Not possible to launch ThreadSimulateEmitQuBit!" << endl;
}

this->release();
/*
cout << "ModeActivePassive: " << ModeActivePassive << endl;
cout << "CurrentSpecificLink: " << CurrentSpecificLink << endl;
cout << "NumberRepetitionsSignal: " << NumberRepetitionsSignal << endl;
cout << "HistPeriodicityAux: " << HistPeriodicityAux << endl;
if (NumCalcCenterMass>1){
	cout << "FreqSynchNormValuesArray[0]: " << FreqSynchNormValuesArray[0] << endl;
	cout << "FreqSynchNormValuesArray[1]: " << FreqSynchNormValuesArray[1] << endl;
	cout << "FreqSynchNormValuesArray[2]: " << FreqSynchNormValuesArray[2] << endl;
}
else{
	cout << "FreqSynchNormValuesArray[0]: " << FreqSynchNormValuesArray[0] << endl;
}< endl;
cout << "FineSynchAdjVal[0]: " << FineSynchAdjVal[0] << endl;
cout << "FineSynchAdjVal[1]: " << FineSynchAdjVal[1] << endl;
cout << "iCenterMass: " << iCenterMass << endl;
cout << "iNumRunsPerCenterMass: " << iNumRunsPerCenterMass << endl;
*/
return 0; // return 0 is for no error
}

int QPLA::ThreadSimulateEmitQuBit(){
	cout << "Emiting Qubits" << endl;
	struct timespec requestWhileWait;
	if (string(this->ModeActivePassive)==string("Active")){
	this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// For sure we can clear OtherClientNodeFutureTimePoint, just to ensure resilence
	requestWhileWait=this->SetFutureTimePointOtherNode();
}
else{
	requestWhileWait = this->GetFutureTimePointOtherNode();
}
this->acquire();// So that there are no segmentatoin faults by grabbing the CLOCK REALTIME and also this has maximum priority
//clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL);// Synch barrier
//while (Clock::now() < this->FutureTimePoint);// Busy wait.

// After passing the TimePoint barrier, in terms of synchronizaton to the action in synch, it is desired to have the minimum indispensable number of lines of code (each line of code adds time jitter)

 //exploringBB::GPIO outGPIO=exploringBB::GPIO(this->EmitLinkNumberArray[0]); // GPIO number is calculated by taking the GPIO chip number, multiplying it by 32, and then adding the offset. For example, GPIO1_12=(1X32)+12=GPIO 44.

 //cout << "Start Emiting Qubits" << endl;// For less time jitter this line should be commented
 auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
// Convert duration to desired time
unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Add some margin 

PRUGPIO.SendTriggerSignals(this->QuadEmitDetecSelec,this->HistPeriodicityAux,static_cast<unsigned int>(NumberRepetitionsSignal),this->FineSynchAdjVal,TimePointFuture_time_as_count,FlagTestSynch);//PRUGPIO->SendTriggerSignals(); // It is long enough emitting sufficient qubits for the receiver to get the minimum amount of multiples of NumQuBitsPerRun

this->PurgeExtraordinaryTimePointsNodes();
this->RunThreadSimulateEmitQuBitFlag=true;//enable again that this thread can again be called. It is okey since it entered a block semaphore part and then no other sempahored part will run until this one finishes. At the same time, returning to true at this point allows the read to not go out of scope and losing this flag parameter
this->release();

 //cout << "Qubit emitted" << endl;
cout << "End Emiting Qubits" << endl;

 return 0; // return 0 is for no error
}

int QPLA::SimulateReceiveQuBit(char* ModeActivePassiveAux,char* CurrentEmitReceiveIPAux, char* IPaddressesAux,int numReqQuBitsAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux,int QuadEmitDetecSelecAux){
	this->acquire();
	this->FlagTestSynch=false;
	strcpy(this->ModeActivePassive,ModeActivePassiveAux);
	strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
	char CurrentEmitReceiveHostIP[NumBytesBufferICPMAX]={0};
	strcpy(CurrentEmitReceiveHostIP,CurrentEmitReceiveIPAux);	
	this->RetrieveOtherEmiterReceiverMethod();
	// Retrieve the involved single specific quad group channel if possible
	QuadEmitDetecSelec=QuadEmitDetecSelecAux; // Identifies the quad groups channels to detect
	SpecificQuadChDet=-1;// Reset value
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){// Identify the quad group channel if there is only one
		if (QuadEmitDetecSelec==(1<<iQuadChIter)){
			SpecificQuadChDet=iQuadChIter;
		}
	}
	strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
	this->NumQuBitsPerRun=numReqQuBitsAux;
	// Adjust the network synchronization values
	this->HistPeriodicityAux=HistPeriodicityAuxAux;// Update value
	if (CurrentSpecificLink>=0){
	this->FineSynchAdjVal[0]=CurrentSynchNetworkParamsLink[0];// synch trig offset
	this->FineSynchAdjVal[1]=CurrentSynchNetworkParamsLink[1];// synch trig frequency
	//cout << "QPLA::SimulateReceiveQuBit CurrentSynchNetworkParamsLink[0]: " << CurrentSynchNetworkParamsLink[0] << endl;
	}
	else{
	this->FineSynchAdjVal[0]=0.0;// synch trig offset
	this->FineSynchAdjVal[1]=0.0;// synch trig frequ
	}
	this->FineSynchAdjVal[0]+=FineSynchAdjValAux[0];// synch trig offset
	this->FineSynchAdjVal[1]+=FineSynchAdjValAux[1];// synch trig frequency
	if (this->RunThreadSimulateReceiveQuBitFlag and this->RunThreadAcquireSimulateNumStoredQubitsNode and this->RunThreadSimulateEmitQuBitFlag){// Protection, do not run if there is a previous thread running
	this->RunThreadSimulateReceiveQuBitFlag=false;//disable that this thread can again be called
	std::thread threadSimulateReceiveQuBitRefAux=std::thread(&QPLA::ThreadSimulateReceiveQubit,this);
	threadSimulateReceiveQuBitRefAux.join();//threadSimulateReceiveQuBitRefAux.detach();

	this->SmallDriftContinuousCorrection(CurrentEmitReceiveHostIP);// Run after threadSimulateReceiveQuBitRefAux
	}
	else{
		cout << "QPLA::Not possible to launch ThreadSimulateReceiveQubit!" << endl;
	}
	this->release();
	/*
	cout << "ModeActivePassive: " << ModeActivePassive << endl;
	cout << "CurrentSpecificLink: " << CurrentSpecificLink << endl;
	cout << "NumQuBitsPerRun: " << NumQuBitsPerRun << endl;
	cout << "HistPeriodicityAux: " << HistPeriodicityAux << endl;
	cout << "FineSynchAdjVal[0]: " << FineSynchAdjVal[0] << endl;
	cout << "FineSynchAdjVal[1]: " << FineSynchAdjVal[1] << endl;
	*/
	// Axiliar network synch tests
	//this->HistCalcPeriodTimeTags(iCenterMassAuxiliarTest,iNumRunsPerCenterMassAuxiliarTest);// Compute synch values
	//cout << "iCenterMassAuxiliarTest: " << iCenterMassAuxiliarTest << endl;
	//cout << "iNumRunsPerCenterMassAuxiliarTest: " << iNumRunsPerCenterMassAuxiliarTest << endl;
	//iNumRunsPerCenterMassAuxiliarTest=(iNumRunsPerCenterMassAuxiliarTest+1)%4;
	return 0; // return 0 is for no error
}

int QPLA::SetSynchParamsOtherNode(){// It is responsability of the host to distribute this synch information to the other involved nodes	
	// Tell to the other nodes
	char ParamsCharArray[NumBytesPayloadBuffer] = {0};
	char ParamsCharArrayAux[NumBytesPayloadBuffer] = {0};
	char CurrentHostIPAux[NumBytesPayloadBuffer] = {0};
	char CurrentHostIPAuxAux[NumBytesPayloadBuffer] = {0};
	char charNum[NumBytesPayloadBuffer] = {0};
	int numUnderScores=countUnderscores(this->CurrentEmitReceiveIP); // Which means the number of IP addresses to send the synch information
	char CurrentEmitReceiveIPAux[NumBytesBufferICPMAX]={0}; // Copy to not destroy original
	strcpy(CurrentEmitReceiveIPAux,this->CurrentEmitReceiveIP);
	int CurrentSpecificLinkAux=-1;
	if (!string(CurrentHostIP).empty()){// Send things if an initial syncronization calibration has happen since among other things it will have th eIP of the host of the node
		strcpy(CurrentHostIPAux,CurrentHostIP);
		strcpy(CurrentHostIPAuxAux,strtok(CurrentHostIPAux,"_"));
		for (int iIterIPaddr=0;iIterIPaddr<numUnderScores;iIterIPaddr++){// Iterate over the different nodes to tell
			// Mount the Parameters message for the other node			
			if (iIterIPaddr==0){
				strcpy(ParamsCharArray,"IPdest_");
				strcpy(ParamsCharArrayAux,strtok(CurrentEmitReceiveIPAux,"_"));			
			} 
			else{
				strcat(ParamsCharArray,"IPdest_");
				strcpy(ParamsCharArrayAux,strtok(NULL,"_"));			
			}
			strcat(ParamsCharArray,ParamsCharArrayAux);// Indicate the address to send the Synch parameters information
			cout << "QPLA::Sending synch. parameters to node " << ParamsCharArrayAux << endl;
			// Re-identify CurrentSpecificLinkAux
			CurrentSpecificLinkAux=-1;
			for (int i=0;i<CurrentNumIdentifiedEmitReceiveIP;i++){
				if (string(LinkIdentificationArray[i])==string(ParamsCharArrayAux)){// IP already present
					if (CurrentSpecificLinkAux<0){CurrentSpecificLinkAux=i;}// Take the first identified, which is th eone that matters most
				}
			}
			strcat(ParamsCharArray,"_");// Add underscore separator
			strcat(ParamsCharArray,"OtherClientNodeSynchParams_"); // Continues the ParamsCharArray, so use strcat
			// The values to send separated by :
			strcat(ParamsCharArray,CurrentHostIPAuxAux); // IP of sender (this node host)
			strcat(ParamsCharArray,":");
			sprintf(charNum, "%.8f",SynchNetworkParamsLink[CurrentSpecificLinkAux][0]); // Offset
			strcat(ParamsCharArray,charNum);
			strcat(ParamsCharArray,":");
			sprintf(charNum, "%.8f",SynchNetworkParamsLink[CurrentSpecificLinkAux][1]); // Relative frequency difference
			strcat(ParamsCharArray,charNum);
			strcat(ParamsCharArray,":");
			sprintf(charNum, "%.8f",SynchNetworkParamsLink[CurrentSpecificLinkAux][2]); // Period
			strcat(ParamsCharArray,charNum);
			strcat(ParamsCharArray,":"); // Final :
			strcat(ParamsCharArray,"_"); // Final _			
		} // end for to the different addresses to send the params information
		//cout << "QPLA::SetSynchParamsOtherNode ParamsCharArray: " << ParamsCharArray << endl;
		this->SetSendParametersAgent(ParamsCharArray);// Send parameter to the other nodes
	}
	//cout << "QPLA::SetSynchParamsOtherNode ParamsCharArray: " << ParamsCharArray << endl;
	//this->acquire(); // important not to do it
	
	//this->release(); // Important not to do it
	//this->RelativeNanoSleepWait((unsigned int)(100*(unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX))));// Give some time to be able to send the above message
	return 0; // All Ok
}

int QPLA::SimulateReceiveSynchQuBit(char* ModeActivePassiveAux,char* CurrentReceiveHostIPaux, char* CurrentEmitReceiveIPAux, char* IPaddressesAux,int numReqQuBitsAux,int NumRunsPerCenterMassAux,double* FreqSynchNormValuesArrayAux,double HistPeriodicityAuxAux,double* FineSynchAdjValAux,int iCenterMass,int iNumRunsPerCenterMass, int QuadEmitDetecSelecAux){
	//cout << "QPLA::SimulateReceiveSynchQuBit CurrentReceiveHostIPaux: " << CurrentReceiveHostIPaux << endl;
	this->acquire();
	// Reset values
	int CombinationLinksNumAux=static_cast<int>(2*((1LL<<LinkNumberMAX)-1));
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		for (int i=0;i<CombinationLinksNumAux;i++){
		  SmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  SmallOffsetDriftPerLinkError[iQuadChIter][i]=0.0; // Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  oldSmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Old Values, Identified by each link, accumulate the small offset error that acumulates over time but that can be corrected for when receiving every now and then from the specific node. This correction comes after filtering raw qubits and applying relative frequency offset and total offset computed with the synchronization algorithm
		  ReferencePointSmallOffsetDriftPerLink[iQuadChIter][i]=0.0; // Identified by each link, annotate the first time offset that all other acquisitions should match to, so an offset with respect the SignalPeriod histogram
		  // Filtering qubits
		  NonInitialReferencePointSmallOffsetDriftPerLink[iQuadChIter][i]=false; // Identified by each link, annotate if the first capture has been done and hence the initial ReferencePoint has been stored
		  for (int j=0;j<NumSmallOffsetDriftAux;j++){
		  	SmallOffsetDriftAuxArray[iQuadChIter][i][j]=0;
		  }
		  IterSmallOffsetDriftAuxArray[iQuadChIter][i]=0;
		}
	}
	this->FlagTestSynch=true;
	strcpy(this->ModeActivePassive,ModeActivePassiveAux);
	strcpy(this->CurrentEmitReceiveIP,CurrentEmitReceiveIPAux);
	char CurrentReceiveHostIP[NumBytesBufferICPMAX]={0};
	strcpy(CurrentReceiveHostIP,CurrentReceiveHostIPaux);	
	strcpy(CurrentHostIP,CurrentReceiveHostIP);// Identifies the Host IP of the current node
	this->RetrieveOtherEmiterReceiverMethod();
	// Retrieve the involved single specific quad group channel if possible
	QuadEmitDetecSelec=QuadEmitDetecSelecAux; // Identifies the quad groups channels to detect
	SpecificQuadChDet=-1;// Reset value
	for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){// Identify the quad group channel if there is only one
		if (QuadEmitDetecSelec==(1<<iQuadChIter)){
			SpecificQuadChDet=iQuadChIter;
		}
	}
	if (SpecificQuadChDet<0){
		SpecificQuadChDet=7;
		cout << "QPLA::SimulateReceiveSynchQuBit wrongly identified single specific quad group channel...setting it to index 7 (all channels)" << endl;
	}
	if (CurrentSpecificLink>=0){
		QuadChannelParamsLink[CurrentSpecificLink]=SpecificQuadChDet;
	}
	strcpy(this->IPaddressesTimePointBarrier,IPaddressesAux);
	this->NumQuBitsPerRun=numReqQuBitsAux;				
//this->NumRunsPerCenterMass=NumRunsPerCenterMassAux; hardcoded value
	if (NumCalcCenterMass>1){
	this->FreqSynchNormValuesArray[0]=FreqSynchNormValuesArrayAux[0];// first test frequency norm.
	this->FreqSynchNormValuesArray[1]=FreqSynchNormValuesArrayAux[1];// second test frequency norm.
	this->FreqSynchNormValuesArray[2]=FreqSynchNormValuesArrayAux[2];// third test frequency norm.
}
else{
	this->FreqSynchNormValuesArray[0]=FreqSynchNormValuesArrayAux[0];// first test frequency norm.
}
//if (iCenterMass==0 and iNumRunsPerCenterMass==0){
	// Reset previous synch values to zero - Maybe comment it in order to do an iterative algorithm
	//SynchCalcValuesArray[0]=0.0;
	//SynchCalcValuesArray[1]=0.0;
	//SynchCalcValuesArray[2]=0.0;
	//double SynchParamValuesArrayAux[2];
	//SynchParamValuesArrayAux[0]=SynchCalcValuesArray[1];
	//SynchParamValuesArrayAux[1]=SynchCalcValuesArray[2];
//}
// Here run the several iterations with different testing frequencies
// Adjust the network synchronization values
this->HistPeriodicityAux=HistPeriodicityAuxAux;// Update value
if (CurrentSpecificLink>=0){
this->FineSynchAdjVal[0]=0.0*CurrentSynchNetworkParamsLink[0];// synch trig offset
this->FineSynchAdjVal[1]=0.0*CurrentSynchNetworkParamsLink[1];// synch trig frequency
}
else{
this->FineSynchAdjVal[0]=0.0;// synch trig offset
this->FineSynchAdjVal[1]=0.0;// synch trig frequ
}
this->FineSynchAdjVal[0]+=FineSynchAdjValAux[0];// synch trig offset
this->FineSynchAdjVal[1]+=FineSynchAdjValAux[1];// synch trig frequency
if (this->RunThreadSimulateReceiveQuBitFlag and this->RunThreadAcquireSimulateNumStoredQubitsNode and this->RunThreadSimulateEmitQuBitFlag){// Protection, do not run if there is a previous thread running
this->RunThreadSimulateReceiveQuBitFlag=false;//disable that this thread can again be called
std::thread threadSimulateReceiveQuBitRefAux=std::thread(&QPLA::ThreadSimulateReceiveQubit,this);
threadSimulateReceiveQuBitRefAux.join();//threadSimulateReceiveQuBitRefAux.detach();
}
else{
	cout << "QPLA::Not possible to launch ThreadSimulateReceiveQubit!" << endl;
}
//cout << "QPLA::HistCalcPeriodTimeTags CurrentReceiveHostIP: " << CurrentReceiveHostIP << endl;
this->HistCalcPeriodTimeTags(CurrentReceiveHostIP,iCenterMass,iNumRunsPerCenterMass);// Compute synch values

this->release();
/*
cout << "ModeActivePassive: " << ModeActivePassive << endl;
cout << "CurrentSpecificLink: " << CurrentSpecificLink << endl;
cout << "NumQuBitsPerRun: " << NumQuBitsPerRun << endl;
cout << "HistPeriodicityAux: " << HistPeriodicityAux << endl;
if (NumCalcCenterMass>1){
	cout << "FreqSynchNormValuesArray[0]: " << FreqSynchNormValuesArray[0] << endl;
	cout << "FreqSynchNormValuesArray[1]: " << FreqSynchNormValuesArray[1] << endl;
	cout << "FreqSynchNormValuesArray[2]: " << FreqSynchNormValuesArray[2] << endl;
}
else{
	cout << "FreqSynchNormValuesArray[0]: " << FreqSynchNormValuesArray[0] << endl;
}
cout << "FineSynchAdjVal[0]: " << FineSynchAdjVal[0] << endl;
cout << "FineSynchAdjVal[1]: " << FineSynchAdjVal[1] << endl;
cout << "iCenterMass: " << iCenterMass << endl;
cout << "iNumRunsPerCenterMass: " << iNumRunsPerCenterMass << endl;
*/
return 0; // return 0 is for no error
}

unsigned long long int QPLA::IPtoNumber(char* IPaddressAux){
	unsigned long long int IPnumberAux=0;
	unsigned long long int OctetNumAux=0;
	char IPaddressAuxHolder[IPcharArrayLengthMAX];
strcpy(IPaddressAuxHolder,IPaddressAux); // to not destroy the information
for (int i=0;i<4;i++){
	if (i==0){OctetNumAux=static_cast<unsigned long long int>(atoi(strtok(IPaddressAuxHolder,".")));}
	else{OctetNumAux=static_cast<unsigned long long int>(atoi(strtok(NULL,".")));}
	IPnumberAux+=OctetNumAux<<(8*(4-i-1));
}

return IPnumberAux;
}

int QPLA::RetrieveOtherEmiterReceiverMethod(){// Stores and retrieves the current other emiter receiver
// Store the potential IP identification of the emitters (for Receive) and receivers for (Emit) - in order to identify the link
CurrentSpecificLink=-1;// Reset value
numSpecificLinkmatches=0; // Reset value
int numCurrentEmitReceiveIP=countUnderscores(this->CurrentEmitReceiveIP); // Which means the number of IP addresses that currently will send/receive qubits
char CurrentEmitReceiveIPAuxAux[NumBytesBufferICPMAX]={0}; // Copy to not destroy original
strcpy(CurrentEmitReceiveIPAuxAux,CurrentEmitReceiveIP);
char SpecificCurrentEmitReceiveIPAuxAux[IPcharArrayLengthMAX]={0};
for (int j=0;j<numCurrentEmitReceiveIP;j++){
	if (j==0){strcpy(SpecificCurrentEmitReceiveIPAuxAux,strtok(CurrentEmitReceiveIPAuxAux,"_"));}
	else{strcpy(SpecificCurrentEmitReceiveIPAuxAux,strtok(NULL,"_"));}
	for (int i=0;i<CurrentNumIdentifiedEmitReceiveIP;i++){
		if (string(LinkIdentificationArray[i])==string(SpecificCurrentEmitReceiveIPAuxAux)){// IP already present
			if (CurrentSpecificLink<0){CurrentSpecificLink=i;}// Take the first identified, which is th eone that matters most
			CurrentSpecificLinkMultipleIndices[numSpecificLinkmatches]=i; // For multiple links at the same time
			numSpecificLinkmatches++;
		}
	}
	if (CurrentSpecificLink<0){// Not previously identified, so stored them if possible
		if ((CurrentNumIdentifiedEmitReceiveIP+1)<=LinkNumberMAX){
			strcpy(LinkIdentificationArray[CurrentNumIdentifiedEmitReceiveIP],SpecificCurrentEmitReceiveIPAuxAux);// Update value
			CurrentSpecificLink=CurrentNumIdentifiedEmitReceiveIP;
			CurrentNumIdentifiedEmitReceiveIP++;
		}
		else{// Mal function we should not be here
			cout << "QPLA::Number of identified emitters/receivers to this node has exceeded the expected value!!!" << endl;
		}
	}	
}
//cout << "QPLA::CurrentSpecificLink: " << CurrentSpecificLink << endl;
// Develop for multiple links
//if (numSpecificLinkmatches>1){// For the time being only implemented for one-to-one link (otherwise it has to be develop...)
	//	cout << "QPLA::Multiple emitter/receivers nodes identified, so develop to correct small offset drift for each specific link...to be develop!!!" << endl;
	//}

// For holding multiple IP links
//cout << "numCurrentEmitReceiveIP: " << numCurrentEmitReceiveIP << endl;
// First always order the list of IPs involved (which are separated by "_")
char ListUnOrderedCurrentEmitReceiveIP[NumBytesPayloadBuffer];
char ListSeparatedUnOrderedCurrentEmitReceiveIP[numCurrentEmitReceiveIP][IPcharArrayLengthMAX];
if ((sizeof(ListUnOrderedCurrentEmitReceiveIP)-1)<strlen(this->CurrentEmitReceiveIP)){// List of IP for th elink to large, just copying part of it
	cout << "QPLA::CurrentEmitReceiveIP list of IP is too large...just copying part of it!!!" << endl;
}
strncpy(ListUnOrderedCurrentEmitReceiveIP,this->CurrentEmitReceiveIP,sizeof(ListUnOrderedCurrentEmitReceiveIP)-1);// To not destroy array. Just copy until the maximum amount of size of the array
unsigned long long int ListUnOrderedIPnum[numCurrentEmitReceiveIP]; // Declaration
unsigned long long int ListOrderedIPnum[numCurrentEmitReceiveIP]; // Declaration
//cout << "QPLA::ListUnOrderedCurrentEmitReceiveIP: " << ListUnOrderedCurrentEmitReceiveIP << endl;
for (int i=0;i<numCurrentEmitReceiveIP;i++){
	if (i==0){
		strcpy(ListSeparatedUnOrderedCurrentEmitReceiveIP[i],strtok(ListUnOrderedCurrentEmitReceiveIP,"_"));		
	}
	else{
		strcpy(ListSeparatedUnOrderedCurrentEmitReceiveIP[i],strtok(NULL,"_"));
	}
	//cout << "ListSeparatedUnOrderedCurrentEmitReceiveIP[i]: " << ListSeparatedUnOrderedCurrentEmitReceiveIP[i] << endl;
}
for (int i=0;i<numCurrentEmitReceiveIP;i++){// Separated for because inside there is another strok that would block things
	ListUnOrderedIPnum[i]=this->IPtoNumber(ListSeparatedUnOrderedCurrentEmitReceiveIP[i]);
	ListOrderedIPnum[i]=ListUnOrderedIPnum[i];// Just copy them
	//cout << "ListOrderedIPnum[i]: " << ListOrderedIPnum[i] << endl;
}

this->ULLIBubbleSort(ListOrderedIPnum,numCurrentEmitReceiveIP); // Order the numbers

char ListOrderedCurrentEmitReceiveIP[NumBytesPayloadBuffer];
int jIndexMatch=0;// Reset
for (int i=0;i<numCurrentEmitReceiveIP;i++){
	jIndexMatch=0;// Reset
	for (int j=0;j<numCurrentEmitReceiveIP;j++){// Search the index matching
		if (ListOrderedIPnum[i]==ListUnOrderedIPnum[j]){jIndexMatch=j;break;}
	}
	if (i==0){
		strcpy(ListOrderedCurrentEmitReceiveIP,ListSeparatedUnOrderedCurrentEmitReceiveIP[jIndexMatch]);
	}
	else{
		strcat(ListOrderedCurrentEmitReceiveIP,ListSeparatedUnOrderedCurrentEmitReceiveIP[jIndexMatch]);
	}
	strcat(ListOrderedCurrentEmitReceiveIP,"_");// Separator
}

//cout << "ListOrderedCurrentEmitReceiveIP: " << ListOrderedCurrentEmitReceiveIP << endl;

CurrentSpecificLinkMultiple=-1;// Reset value
// Then check if this entry exists
//cout << "ListOrderedCurrentEmitReceiveIP: " << ListOrderedCurrentEmitReceiveIP << endl;
for (int i=0;i<CurrentNumIdentifiedMultipleIP;i++){
	//cout << "ListCombinationSpecificLink[i]: " << ListCombinationSpecificLink[i] << endl;
	if (string(ListCombinationSpecificLink[i])==string(ListOrderedCurrentEmitReceiveIP)){
		CurrentSpecificLinkMultiple=i;
		break;
	}

}
//cout << "QPLA::CurrentSpecificLinkMultiple: " << CurrentSpecificLinkMultiple << endl;
//cout << "QPLA::CurrentNumIdentifiedMultipleIP: " << CurrentNumIdentifiedMultipleIP << endl;
// If exists, just return the index identifying it; if it does not exists store it and return the index identifying it
int CombinationLinksNumAux=static_cast<int>(2*((1LL<<LinkNumberMAX)-1));
if (CurrentSpecificLinkMultiple<0){
	if ((CurrentNumIdentifiedMultipleIP+1)<=CombinationLinksNumAux){
		strcpy(ListCombinationSpecificLink[CurrentNumIdentifiedMultipleIP],ListOrderedCurrentEmitReceiveIP);// Update value
		CurrentSpecificLinkMultiple=CurrentNumIdentifiedMultipleIP;
		CurrentNumIdentifiedMultipleIP++;		
	}
	else{// Mal function we should not be here
		cout << "QPLA::Number of identified multiple combinations of emitters/receivers to this node has exceeded the expected value!!!" << endl;
	}
}
//cout << "QPLA::CombinationLinksNumAux: " << CombinationLinksNumAux << endl;
//cout << "QPLA::CurrentSpecificLinkMultiple: " << CurrentSpecificLinkMultiple << endl;
//cout << "QPLA::CurrentNumIdentifiedMultipleIP: " << CurrentNumIdentifiedMultipleIP << endl;
// Update the holder values that need to be passed depending on the current link of interest
double dHistPeriodicityAux=static_cast<double>(HistPeriodicityAux);
double dHistPeriodicityHalfAux=static_cast<double>(HistPeriodicityAux/2.0);
double SynchNetTransHardwareAdjAux=1.0;
if (CurrentSpecificLink>=0 and numCurrentEmitReceiveIP==1 and SynchNetworkParamsLink[CurrentSpecificLink][2]>0.0 and FlagTestSynch==false){// This corresponds to RequestQubits Node to node or SendEntangled. The receiver always performs correction, so does not matter for the sender since they are zeroed
	// For receiver correction	
	if (SynchNetworkParamsLink[CurrentSpecificLink][1]<0.0){SynchNetTransHardwareAdjAux=SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1];}// For negative correction
	else if (SynchNetworkParamsLink[CurrentSpecificLink][1]>0.0){SynchNetTransHardwareAdjAux=SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2];}// For positivenegative correction
	else{SynchNetTransHardwareAdjAux=1.0;}
	//if (SynchNetworkParamsLink[CurrentSpecificLink][0]<0.0){
	//	CurrentSynchNetworkParamsLink[0]=-(fmod(-MultFactorEffSynchPeriodQPLA*dHistPeriodicityHalfAux-SynchNetworkParamsLink[CurrentSpecificLink][0],MultFactorEffSynchPeriodQPLA*dHistPeriodicityAux)+MultFactorEffSynchPeriodQPLA*dHistPeriodicityHalfAux);// Offset
	//}
	//else{
	//	CurrentSynchNetworkParamsLink[0]=fmod(MultFactorEffSynchPeriodQPLA*dHistPeriodicityHalfAux+SynchNetworkParamsLink[CurrentSpecificLink][0],MultFactorEffSynchPeriodQPLA*dHistPeriodicityAux)-MultFactorEffSynchPeriodQPLA*dHistPeriodicityHalfAux;// Offset
	//}
	// Maybe the offset should no be transformed
	CurrentSynchNetworkParamsLink[0]=SynchNetworkParamsLink[CurrentSpecificLink][0]; // Synch offset
	//if (SynchNetworkParamsLink[CurrentSpecificLink][1]<0.0){
	//	CurrentSynchNetworkParamsLink[1]=(-(fmod(-dHistPeriodicityHalfAux-SynchNetworkParamsLink[CurrentSpecificLink][1],dHistPeriodicityAux)+dHistPeriodicityHalfAux)/dHistPeriodicityAux)*(SynchNetAdj[CurrentSpecificLink]/SynchNetTransHardwareAdjAux);// Relative frequency offset
	//}
	//else{
	//	CurrentSynchNetworkParamsLink[1]=((fmod(dHistPeriodicityHalfAux+SynchNetworkParamsLink[CurrentSpecificLink][1],dHistPeriodicityAux)-dHistPeriodicityHalfAux)/dHistPeriodicityAux)*(SynchNetAdj[CurrentSpecificLink]/SynchNetTransHardwareAdjAux);// Relative frequency offset
	//}
	// Maybe the rel. freq. offset should no be transformed
	CurrentSynchNetworkParamsLink[1]=(SynchNetworkParamsLink[CurrentSpecificLink][1]/SynchNetworkParamsLink[CurrentSpecificLink][2]);///SynchNetTransHardwareAdjAux;
	CurrentSynchNetworkParamsLink[2]=SynchNetworkParamsLink[CurrentSpecificLink][2]; // Period in which it was calculated
	//CurrentSynchNetworkParamsLink[0]=SynchNetworkParamsLink[CurrentSpecificLink][0];// Offset
	//CurrentSynchNetworkParamsLink[1]=(SynchNetworkParamsLink[CurrentSpecificLink][1]/dHistPeriodicityAux)*(SynchNetAdj[CurrentSpecificLink]/SynchNetTransHardwareAdjAux);// Relative frequency offset
	//CurrentSynchNetworkParamsLink[2]=SynchNetworkParamsLink[CurrentSpecificLink][2]; // Period
	//For emitter correction - No correction, since it is taking place at the receiver
	CurrentExtraSynchNetworkParamsLink[0]=0.0;
	CurrentExtraSynchNetworkParamsLink[1]=0.0;
	CurrentExtraSynchNetworkParamsLink[2]=0.0;
	// Debugging
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod Correction for receiver (emitter does not correct)" << endl;
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentSynchNetworkParamsLink[0]: " << CurrentSynchNetworkParamsLink[0] << endl;
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentSynchNetworkParamsLink[1]: " << CurrentSynchNetworkParamsLink[1] << endl;
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentSynchNetworkParamsLink[2]: " << CurrentSynchNetworkParamsLink[2] << endl;
}
else if (CurrentSpecificLink>=0 and numCurrentEmitReceiveIP>1 and SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][2]>0.0 and FlagTestSynch==false){// correction has to take place at the emitter. this Corresponds to RequestMultiple, where the first IP identifies the correction at the sender to the receiver and the extra identifies the other sender, but no other action takes place more than identifying numSpecificLinkmatches>1
	// Ideally, the first IP indicates the sender, hence the index of the synch network parameters for detection to use another story is if compensating for emitter
	// Eventually, the correction has to be from the receiver inferred
	if ((SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][1])<0.0){SynchNetTransHardwareAdjAux=SynchAdjRelFreqCalcValuesArray[CurrentSpecificLinkMultipleIndices[0]][1];}// For negative correction
	else if ((SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][1])>0.0){SynchNetTransHardwareAdjAux=SynchAdjRelFreqCalcValuesArray[CurrentSpecificLinkMultipleIndices[0]][2];}// For positivenegative correction
	else{SynchNetTransHardwareAdjAux=1.0;}
	// Notice that the relative frequency and offset correction is negated (or maybe not)
	// For receiver correction - Correction has to take place at the emitter, where the first IP identifies the single receiver
	CurrentSynchNetworkParamsLink[0]=0.0;
	CurrentSynchNetworkParamsLink[1]=0.0;
	CurrentSynchNetworkParamsLink[2]=0.0;
	//For emitter correction - info provided by the other nodes
	// Maybe the offset does not have to be transformed
	CurrentExtraSynchNetworkParamsLink[0]=-SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][0];// Synch offset
	// Relative frequency difference
	//if (SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][1]<0.0){
	//	CurrentExtraSynchNetworkParamsLink[1]=(-(fmod(-dHistPeriodicityHalfAux-SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][1],dHistPeriodicityAux)+dHistPeriodicityHalfAux)/dHistPeriodicityAux)*(SynchNetAdj[CurrentSpecificLink]/SynchNetTransHardwareAdjAux);// Relative frequency offset
	//}
	//else{
	//	CurrentExtraSynchNetworkParamsLink[1]=((fmod(dHistPeriodicityHalfAux+SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][1],dHistPeriodicityAux)-dHistPeriodicityHalfAux)/dHistPeriodicityAux)*(SynchNetAdj[CurrentSpecificLink]/SynchNetTransHardwareAdjAux);// Relative frequency offset
	//}
	// Maybe the rel. freq. offset should no be transformed
	CurrentExtraSynchNetworkParamsLink[1]=(SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][1]/SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][2]);///SynchNetTransHardwareAdjAux;
	CurrentExtraSynchNetworkParamsLink[2]=SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][2]; // Period in which the parameters where calculated
	// Debugging
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod Correction for emitter (receiver does not correct)" << endl;
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentExtraSynchNetworkParamsLink[0]: " << CurrentExtraSynchNetworkParamsLink[0] << endl;
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentExtraSynchNetworkParamsLink[1]: " << CurrentExtraSynchNetworkParamsLink[1] << endl;
	//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentExtraSynchNetworkParamsLink[2]: " << CurrentExtraSynchNetworkParamsLink[2] << endl;
}
else{ // For instance when testing Synch mechanisms or when no previous synch parameters present (or lin not properly identified)
	if (FlagTestSynch==false){
		cout << "QPLA::RetrieveOtherEmiterReceiverMethod No synch. correction present." << endl;
	}
	CurrentSynchNetworkParamsLink[0]=0.0; // Reset valu es
	CurrentSynchNetworkParamsLink[1]=0.0; // Reset values
	CurrentSynchNetworkParamsLink[2]=0.0; // Reset values
	
	CurrentExtraSynchNetworkParamsLink[0]=0.0; // Reset values
	CurrentExtraSynchNetworkParamsLink[1]=0.0; // Reset values
	CurrentExtraSynchNetworkParamsLink[2]=0.0; // Reset values
}

// Debugging
//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentSpecificLink: " << CurrentSpecificLink << endl;
//cout << "QPLA::RetrieveOtherEmiterReceiverMethod CurrentSpecificLinkMultipleIndices[0]: " << CurrentSpecificLinkMultipleIndices[0] << endl;
//cout << "QPLA::RetrieveOtherEmiterReceiverMethod LinkIdentificationArray[CurrentSpecificLink]: " << LinkIdentificationArray[CurrentSpecificLink] << endl;
//cout << "QPLA::RetrieveOtherEmiterReceiverMethod ListCombinationSpecificLink[CurrentSpecificLinkMultiple]: " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << endl;
//cout << "QPLA::RetrieveOtherEmiterReceiverMethod SynchNetAdj[CurrentSpecificLink]: " << SynchNetAdj[CurrentSpecificLink] << endl;
//cout << "QPLA::RetrieveOtherEmiterReceiverMethod SynchNetTransHardwareAdjAux: " << SynchNetTransHardwareAdjAux << endl;

return 0; // All ok
}

int QPLA::makeEvenInt(int xAux) {
    return (xAux % 2 == 0) ? xAux : xAux + 1;
}

int QPLA::ThreadSimulateReceiveQubit(){
	cout << "Receiving Qubits" << endl;
	this->acquire();
	PRUGPIO.ClearStoredQuBits();//PRUGPIO->ClearStoredQuBits();
	//cout << "Clear previously stored Qubits...to be commented" << endl;
	this->release();
	int iIterRuns;
	NumQuBitsPerRun=makeEvenInt(NumQuBitsPerRun); // Force that it is an even number (needed for GPIO.cpp)
	int DetRunsCount = static_cast<int>(ceil(static_cast<double>(NumQuBitsPerRun)/static_cast<double>(NumQubitsMemoryBuffer))); // Number of iteration runs
	//cout << "DetRunsCount: " << DetRunsCount << endl;
	struct timespec requestWhileWait;
	if (string(this->ModeActivePassive)==string("Active")){
		this->OtherClientNodeFutureTimePoint=std::chrono::time_point<Clock>();// For sure we can clear OtherClientNodeFutureTimePoint, just to ensure resilence
		requestWhileWait=this->SetFutureTimePointOtherNode();
	}
	else{
		requestWhileWait = this->GetFutureTimePointOtherNode();
	}
	//cout << "Completed get/set future time point...to be commented" << endl;
	this->acquire();
	// So that there are no segmentation faults by grabbing the CLOCK REALTIME and also this has maximum priority
	//clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&requestWhileWait,NULL); // Synch barrier
	//while (Clock::now() < this->FutureTimePoint);// Busy wait 

	// After passing the TimePoint barrier, in terms of synchronizaton to the action in synch, it is desired to have the minimum indispensable number of lines of code (each line of code adds time jitter)

	//cout << "Start Receiving Qubits" << endl;// This line should be commented to reduce the time jitter

	// Start measuring
	//exploringBB::GPIO inGPIO=exploringBB::GPIO(this->ReceiveLinkNumberArray[0]); // Receiving GPIO. Of course gnd have to be connected accordingly.

	auto duration_since_epochFutureTimePoint=FutureTimePoint.time_since_epoch();
	// Convert duration to desired time
	unsigned long long int TimePointFuture_time_as_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epochFutureTimePoint).count(); // Add some margin
	for (iIterRuns=0;iIterRuns<DetRunsCount;iIterRuns++){	
		PRUGPIO.ReadTimeStamps(iIterRuns,this->QuadEmitDetecSelec,this->HistPeriodicityAux,static_cast<unsigned int>(NumQuBitsPerRun),this->FineSynchAdjVal,TimePointFuture_time_as_count,FlagTestSynch);//PRUGPIO->ReadTimeStamps();// Multiple reads can be done in multiples of NumQuBitsPerRun qubit timetags
	}
	//cout << "Detected timestamps...to be commented" << endl;
	this->LinearRegressionQuBitFilter();// Retrieve raw detected qubits and channel tags
	//cout << "Completed LinearRegressionQuBitFilter...to be commented" << endl;
	this->PurgeExtraordinaryTimePointsNodes();
	this->RunThreadSimulateReceiveQuBitFlag=true;//enable again that this thread can again be called
	//cout << "Completed PurgeExtraordinaryTimePointsNodes...to be commented" << endl;
	this->release();
	cout << "End Receiving Qubits" << endl;

return 0; // return 0 is for no error
}

bool QPLA::GetGPIOHardwareSynchedNode(){
	bool GPIOHardwareSynchedAux=false;
	this->acquire();
GPIOHardwareSynchedAux=GPIOHardwareSynched;// Update value
this->release();
return GPIOHardwareSynchedAux;
}

int QPLA::SmallDriftContinuousCorrection(char* CurrentEmitReceiveHostIPaux){// Eliminate small wander clock drifts because it is assumed that qubists fall within their histogram period
	int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];// Number of qubits to process

	// Check that we now exceed the QuBits buffer size
	if (SimulateNumStoredQubitsNodeAux>NumQubitsMemoryBuffer){SimulateNumStoredQubitsNodeAux=NumQubitsMemoryBuffer;}
	else if (SimulateNumStoredQubitsNodeAux==1){
		cout << "QPLA::SmallDriftContinuousCorrection not executing since 0 qubits detected!" << endl;
		return 0;
	}

	//Particularized for histogram analysis when MultFactorEffSynchPeriodQPLA=4 and for regular operation (non-histogram analysis)
	// Apply the small offset drift correction
	if (ApplyProcQubitsSmallTimeOffsetContinuousCorrection==true){
		if (CurrentSpecificLinkMultiple>-1){// The specific identification IP is present
			int numCurrentEmitReceiveIP=countUnderscores(CurrentEmitReceiveHostIPaux); // Which means the number of IP addresses that currently will send/receive qubits
			//cout << "QPLA::SmallDriftContinuousCorrection CurrentEmitReceiveHostIPaux: " << CurrentEmitReceiveHostIPaux << endl;
			for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){	
				if (RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>0){
					//cout << "QPLA::SmallDriftContinuousCorrection iter iQuadChIter: " << iQuadChIter << endl;
					// Re-identify CurrentSpecificLinkAux
					int CurrentSpecificLinkAux=-1;
					for (int i=0;i<CurrentNumIdentifiedEmitReceiveIP;i++){
						if (QuadChannelParamsLink[i]==iQuadChIter){
							if (CurrentSpecificLinkAux<0){CurrentSpecificLinkAux=i;}// Take the first identified, which is th eone that matters most
						}
					}
					//cout << "QPLA::SmallDriftContinuousCorrection CurrentSpecificLinkAux: " << CurrentSpecificLinkAux << endl;
				  // If it is the first time, annotate the relative time offset with respect HostPeriodicityAux
				  ReferencePointSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=0;// Reset value. ReferencePointSmallOffset could be used to allocate multiple channels separated by time
				  if (NonInitialReferencePointSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]==false){			
					  SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=0;// Reset value
					  NonInitialReferencePointSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=true;// Update value, so that it is not run again
					}
				  // First compute the relative new time offset from last iteration
					long long int SmallOffsetDriftAux=0;
					long long int SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink=0;//SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]+ReferencePointSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple];
					long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);	
					long long int LLIHistPeriodicityHalfAux=static_cast<long long int>(HistPeriodicityAux/2.0);
					long long int LLIMultFactorEffSynchPeriod=static_cast<long long int>(MultFactorEffSynchPeriodQPLA);
					long long int ChOffsetCorrection=0;// Variable to acomodate the 4 different channels in the periodic histogram analysis
					if (UseAllTagsForEstimation){
						long long int SmallOffsetDriftArrayAux[static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])]={0};
						long long int CheckChOffsetCorrectionArray[4][RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]]={0};
						long long int CheckChOffsetCorrection[4]={0};
						unsigned int CheckChOffsetCorrectionIter[4]={0};
						bool boolCheckChOffsetCorrectionflag=false;
						for (unsigned int i=0;i<4;i++){CheckChOffsetCorrectionIter[i]=0;}// Reset values			
						for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){					  
						  if (LLIMultFactorEffSynchPeriod==4){// When using histogram analysis
						  	ChOffsetCorrection=static_cast<long long int>(BitPositionChannelTags(ChannelTags[iQuadChIter][i])%4);// Maps the offset correction for the different channels to detect a specific state								
						  	if (((static_cast<long long int>(TimeTaggs[iQuadChIter][i])-ChOffsetCorrection*LLIHistPeriodicityAux)-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink)<0){
									SmallOffsetDriftArrayAux[i]=-((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux-((static_cast<long long int>(TimeTaggs[iQuadChIter][i])-ChOffsetCorrection*LLIHistPeriodicityAux)-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux));
									//if (i%200==0){
									//	cout << "QPLA::SmallDriftContinuousCorrection negative" << endl;
									//}
								}
								else{
									SmallOffsetDriftArrayAux[i]=(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+((static_cast<long long int>(TimeTaggs[iQuadChIter][i])-ChOffsetCorrection*LLIHistPeriodicityAux)-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux);
									//if (i%200==0){
									//	cout << "QPLA::SmallDriftContinuousCorrection positive" << endl;
									//}
								}
								CheckChOffsetCorrectionArray[ChOffsetCorrection][CheckChOffsetCorrectionIter[ChOffsetCorrection]]=SmallOffsetDriftArrayAux[i];
								CheckChOffsetCorrectionIter[ChOffsetCorrection]++;								
						  }
						  else{// When NOT using histogram analysis
							  if ((static_cast<long long int>(TimeTaggs[iQuadChIter][i])-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink)<0){
									SmallOffsetDriftArrayAux[i]=-((LLIHistPeriodicityHalfAux-(static_cast<long long int>(TimeTaggs[iQuadChIter][i])-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux));
								}
								else{
									SmallOffsetDriftArrayAux[i]=(LLIHistPeriodicityHalfAux+(static_cast<long long int>(TimeTaggs[iQuadChIter][i])-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux);
								}
							}
							//if (i%200==0){
							//	cout << "QPLA::SmallDriftContinuousCorrection ChannelTags[" << i << "]: " << ChannelTags[iQuadChIter][i] << endl;
							//	cout << "QPLA::SmallDriftContinuousCorrection BitPositionChannelTags(ChannelTags[" << i << "]): " << BitPositionChannelTags(ChannelTags[iQuadChIter][i]) << endl;
							//	cout << "QPLA::SmallDriftContinuousCorrection ChOffsetCorrection[" << i << "]: " << ChOffsetCorrection << endl;
							//	cout << "QPLA::SmallDriftContinuousCorrection SmallOffsetDriftArrayAux[" << i << "]: " << SmallOffsetDriftArrayAux[i] << endl;
							//}
						}
						// Checks
						for (unsigned int i=0;i<4;i++){
							if (CheckChOffsetCorrectionIter[i]>0){
								//CheckChOffsetCorrection[i]=LLIMeanFilterSubArray(CheckChOffsetCorrectionArray[i],static_cast<int>(CheckChOffsetCorrectionIter[i]));
								CheckChOffsetCorrection[i]=LLIMedianFilterSubArray(CheckChOffsetCorrectionArray[i],static_cast<int>(CheckChOffsetCorrectionIter[i])); // To avoid glitches
							}
						}
						for (unsigned int i=0;i<4;i++){
							for (unsigned int j=0;j<4;j++){
								if (i!=j and abs(CheckChOffsetCorrection[i]-CheckChOffsetCorrection[j])>(LLIHistPeriodicityAux) and CheckChOffsetCorrectionIter[i]>=5 and CheckChOffsetCorrectionIter[j]>=5){
									boolCheckChOffsetCorrectionflag=true;
									//cout << "QPLA::SmallDriftContinuousCorrection Potentially GPIO pins i=" << i << " " << CheckChOffsetCorrection[i] << " and j=" << j << " " << CheckChOffsetCorrection[j] << " with difference " << (CheckChOffsetCorrection[i]-CheckChOffsetCorrection[j]) << " on iQuadChIter: " << iQuadChIter << " for LLIHistPeriodicityAux: " << LLIHistPeriodicityAux << " connection order is wrong or too much jitter. Check!!!" << endl;
								}
							}
						}
						if (boolCheckChOffsetCorrectionflag==true){
							cout << "QPLA::SmallDriftContinuousCorrection Potentially GPIO pins connection order is wrong or too much jitter on iQuadChIter: " << iQuadChIter <<" Check!!!" << endl;
						}
					  //SmallOffsetDriftAux=LLIMeanFilterSubArray(SmallOffsetDriftArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]));//LLIMedianFilterSubArray(SmallOffsetDriftArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])); // Median averaging
					  SmallOffsetDriftAux=LLIMedianFilterSubArray(SmallOffsetDriftArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]));// To avoid glitches//LLIMedianFilterSubArray(SmallOffsetDriftArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])); // Median averaging
					  //cout << "QPLA::SmallDriftContinuousCorrection static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]): " << static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]) << endl;
					  //cout << "QPLA::SmallDriftContinuousCorrection SmallOffsetDriftAux: " << SmallOffsetDriftAux << endl;
					}
					else{
						if (LLIMultFactorEffSynchPeriod==4){// When using histogram analysis
							ChOffsetCorrection=static_cast<long long int>(BitPositionChannelTags(ChannelTags[iQuadChIter][0])%4);// Maps the offset correction for the different channels to detect a specific state
							if(((static_cast<long long int>(TimeTaggs[iQuadChIter][0])-ChOffsetCorrection*LLIHistPeriodicityAux)-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink)<0){
								SmallOffsetDriftAux=-((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux-((static_cast<long long int>(TimeTaggs[iQuadChIter][0])-ChOffsetCorrection*LLIHistPeriodicityAux)-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux));
							}
							else{
								SmallOffsetDriftAux=(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+((static_cast<long long int>(TimeTaggs[iQuadChIter][0])-ChOffsetCorrection*LLIHistPeriodicityAux)-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux);
							}
						}
						else{// When NOT using histogram analysis
							if((static_cast<long long int>(TimeTaggs[iQuadChIter][0])-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink)<0){
								SmallOffsetDriftAux=-((LLIHistPeriodicityHalfAux-(static_cast<long long int>(TimeTaggs[iQuadChIter][0])-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux));
							}
							else{
								SmallOffsetDriftAux=(LLIHistPeriodicityHalfAux+(static_cast<long long int>(TimeTaggs[iQuadChIter][0])-SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink))%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux);
							}
						}
						cout << "QPLA::SmallDriftContinuousCorrection Using only first timetag for small offset correction!...to be deactivated" << endl;
					}

				  //cout << "QPLA::Applying SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple] " << SmallOffsetDriftPerLink[CurrentSpecificLinkMultiple] << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << endl;
				  //cout << "QPLA::Applying SmallOffsetDriftAux " << SmallOffsetDriftAux << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << endl;
				  //////////////////////////////////////////
				  // Checks of proper values handling
				  //long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);
				  //long long int LLIHistPeriodicityHalfAux=static_cast<long long int>(HistPeriodicityAux/2);
				  //long long int CheckValueAux=(LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[0]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;
				  //cout << "QPLA::SmallDriftContinuousCorrection::CheckValueAux: "<< CheckValueAux << endl;
				  //cout << "QPLA::SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink: " << SmallOffsetDriftPerLinkCurrentSpecificLinkReferencePointSmallOffsetDriftPerLinkCurrentSpecificLink << endl;
				  ////////////////////////////////////////
					// Apply insitu correction
					long long int LLISmallOffsetDriftPerLinkCurrentSpecificLink=SmallOffsetDriftAux+0*SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple];
				  //long long int LLISmallOffsetDriftAux=static_cast<long long int>(SmallOffsetDriftAux);
					for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
						if ((static_cast<long long int>(TimeTaggs[iQuadChIter][i])-LLISmallOffsetDriftPerLinkCurrentSpecificLink)>=0){
							TimeTaggs[iQuadChIter][i]=static_cast<unsigned long long int>(static_cast<long long int>(TimeTaggs[iQuadChIter][i])-LLISmallOffsetDriftPerLinkCurrentSpecificLink);
						}
						else{
							TimeTaggs[iQuadChIter][i]=0;
							cout << "QPLA::SmallDriftContinuousCorrection static_cast<long long int>(TimeTaggs[iQuadChIter][i])-LLISmallOffsetDriftPerLinkCurrentSpecificLink: " << static_cast<long long int>(TimeTaggs[iQuadChIter][i])-LLISmallOffsetDriftPerLinkCurrentSpecificLink << " negative!!!...we sould not be here!!" << endl;
						}
					}

					// Instantaneous value reporting
					//cout << "QPLA::SmallDriftContinuousCorrection Instantaneous SmallOffsetDriftAux " << SmallOffsetDriftAux << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << ". Potentially lost ABSOLUTE temporal track of timetaggs from previous runs!!!" << endl;
				  //if (abs(static_cast<double>(SmallOffsetDriftAux))>(HistPeriodicityAux/2.0)){// Large step
				  //	cout << "QPLA::SmallDriftContinuousCorrection Instantaneous average" << endl;
				  //	cout << "QPLA::SmallDriftContinuousCorrection iQuadChIter: " << iQuadChIter << endl;
				  //	cout << "QPLA::Large small offset drift encountered SmallOffsetDriftAux " << SmallOffsetDriftAux << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << ". Potentially lost ABSOLUTE temporal track of timetaggs from previous runs!!!" << endl;
				  //	cout << "QPLA::Applying SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] " << SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << endl;
				  //}
				  
				  // Median filter the SmallOffsetDriftAux to avoid to much induced artificial jitter
				  // Update new value, just for monitoring of the wander - last value. With an acumulation sign it acumulates
				  //cout << "QPLA::SmallDriftContinuousCorrection oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]: " << oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] << endl;
				  //cout << "QPLA::SmallDriftContinuousCorrection SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]: " << SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] << endl;
				  				  				  
				  SmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple][IterSmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple]%NumSmallOffsetDriftAux]=SmallOffsetDriftAux;//SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple];
				  IterSmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple]++;// Update value
				  IterSmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple]=IterSmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple]%NumSmallOffsetDriftAux;// Wrap value
				  SmallOffsetDriftAux=LLIMedianFilterSubArray(SmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple],NumSmallOffsetDriftAux);// Median filter	//LLIMedianFilterSubArray(SmallOffsetDriftAuxArray[iQuadChIter][CurrentSpecificLinkMultiple],NumSmallOffsetDriftAux);// Median filter				  
				  SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=0*oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]-SmallOffsetDriftAux;// Update value

				  if (abs(static_cast<double>(SmallOffsetDriftAux))>(HistPeriodicityAux/2.0)){// Large step
				  	cout << "QPLA::SmallDriftContinuousCorrection Time window average" << endl;
				  	cout << "QPLA::SmallDriftContinuousCorrection iQuadChIter: " << iQuadChIter << endl;
				  	cout << "QPLA::Large small offset drift encountered SmallOffsetDriftAux " << SmallOffsetDriftAux << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << ". Potentially lost ABSOLUTE temporal track of timetaggs from previous runs!!!" << endl;
				  	cout << "QPLA::Applying SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] " << SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] << " for link " << ListCombinationSpecificLink[CurrentSpecificLinkMultiple] << endl;
				  }

				  // Implement PID
				  SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]+=SmallOffsetDriftAux; // Integral value of the PID
				  if (LLIMultFactorEffSynchPeriod==4){// When using histogram analysis
						if (SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]<0){
							SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]=-((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux-SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux));
						}
						else{
							SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]=(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux);
						}
					}
					else{// When NOT using histogram analysis
						if (SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]<0){
							SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]=-((LLIHistPeriodicityHalfAux-SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux));
						}
						else{
							SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]=(LLIHistPeriodicityHalfAux+SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux);
						}
					}
				  //cout << "QPLA::SmallDriftContinuousCorrection PID Integral SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple]: " << SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple] << endl;
				  // Update information to the other node about synch parameters				  
				  if (CurrentSpecificLinkAux>-1 && ApplyPIDOffsetContinuousCorrection==true){
				  	// Implement PID
				  	double SmallOffsetDriftPerLinkPIDvalAux=SplitEmitReceiverSmallOffsetDriftPerLinkP*static_cast<double>(SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple])+SplitEmitReceiverSmallOffsetDriftPerLinkI*static_cast<double>(SmallOffsetDriftPerLinkError[iQuadChIter][CurrentSpecificLinkMultiple])+SplitEmitReceiverSmallOffsetDriftPerLinkD*static_cast<double>(-SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]+oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]);
				  	//cout << "QPLA::SmallDriftContinuousCorrection PID total SmallOffsetDriftPerLinkPIDvalAux: " << SmallOffsetDriftPerLinkPIDvalAux << endl;
				  	//if (SmallOffsetDriftPerLinkPIDvalAux<0.0){
				  	//	SmallOffsetDriftPerLinkPIDvalAux=-fmod(-SmallOffsetDriftPerLinkPIDvalAux,HistPeriodicityAux);
				  	//}
				  	//else{
				  	//	SmallOffsetDriftPerLinkPIDvalAux=fmod(SmallOffsetDriftPerLinkPIDvalAux,HistPeriodicityAux);
				  	//}
				  	//cout << "QPLA::SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]: " << SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] << endl;
				  	//cout << "QPLA::SmallOffsetDriftPerLinkPIDvalAux: " << SmallOffsetDriftPerLinkPIDvalAux << endl;
				  	/*
				  	if (abs(SmallOffsetDriftPerLinkPIDvalAux)>(HistPeriodicityAux/4.0)){// Apply correction at the transmitter also
				  		EffectiveSplitEmitReceiverSmallOffsetDriftPerLink=SplitEmitReceiverSmallOffsetDriftPerLink;
				  	}
				  	else{// do not apply correction at the transmitter
				  		EffectiveSplitEmitReceiverSmallOffsetDriftPerLink=1.0;
				  	}
				  	*/
				  	EffectiveSplitEmitReceiverSmallOffsetDriftPerLink=SplitEmitReceiverSmallOffsetDriftPerLink;
				  	if (CurrentSpecificLink>=0 and numCurrentEmitReceiveIP==1 and SynchNetworkParamsLink[CurrentSpecificLink][2]>0.0 and FlagTestSynch==false){// This corresponds to RequestQubits Node to node or SendEntangled. The receiver always performs correction, so does not matter for the sender since they are zeroed
							// For receiver correction - it should be only one
							SynchNetworkParamsLink[CurrentSpecificLinkAux][0]=originalSynchNetworkParamsLink[CurrentSpecificLinkAux][0]+oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]-(1.0-EffectiveSplitEmitReceiverSmallOffsetDriftPerLink)*SmallOffsetDriftPerLinkPIDvalAux;// Offset difference
							//cout << "QPLA::SmallDriftContinuousCorrection iQuadChIter: " << iQuadChIter << endl;
							//cout << "QPLA::SmallDriftContinuousCorrection correction for receiver!" << endl;
						}
						else if (CurrentSpecificLink>=0 and numCurrentEmitReceiveIP>1 and SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][2]>0.0 and FlagTestSynch==false){// correction has to take place at the emitter. this Corresponds to RequestMultiple, where the first IP identifies the correction at the sender to the receiver and the extra identifies the other sender, but no other action takes place more than identifying numSpecificLinkmatches>1
							// For transmitter correction
							// Ideally, the first IP indicates the sender, hence the index of the synch network parameters for detection to use another story is if compensating for emitter
							SynchNetworkParamsLink[CurrentSpecificLinkAux][0]=originalSynchNetworkParamsLink[CurrentSpecificLinkAux][0]+oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]-(1.0-EffectiveSplitEmitReceiverSmallOffsetDriftPerLink)*SmallOffsetDriftPerLinkPIDvalAux;// Offset difference
							//cout << "QPLA::SmallDriftContinuousCorrection iQuadChIter: " << iQuadChIter << endl;
							//cout << "QPLA::SmallDriftContinuousCorrection correction for transmitter!" << endl;	
						}
						//SynchNetworkParamsLink[CurrentSpecificLink][1]=0.0*SynchNetworkParamsLink[CurrentSpecificLink][1]+SynchCalcValuesArray[2];// Relative frequency
						//SynchNetworkParamsLink[CurrentSpecificLink][2]=SynchCalcValuesArray[0];// Estimated period
						//SynchNetAdj[CurrentSpecificLink]=SynchNetAdjAux;
						oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]-=static_cast<long long int>((1.0-EffectiveSplitEmitReceiverSmallOffsetDriftPerLink)*SmallOffsetDriftPerLinkPIDvalAux);
						if (LLIMultFactorEffSynchPeriod==4){// When using histogram analysis
							if (oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]<0){
								oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=-((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux-oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux));
							}
							else{
								oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux);
							}
						}
						else{// When NOT using histogram analysis
							if (oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]<0){
								oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=-((LLIHistPeriodicityHalfAux-oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux));
							}
							else{
								oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=(LLIHistPeriodicityHalfAux+oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIHistPeriodicityAux)-(LLIHistPeriodicityHalfAux);
							}
						}
						//cout << "QPLA::SmallDriftContinuousCorrection oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]: " << oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple] << endl;
						// Split the effor between sender and receiver for constantly correcting the synch parameters
						SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=static_cast<long long int>(SplitEmitReceiverSmallOffsetDriftPerLink*SmallOffsetDriftPerLinkPIDvalAux);
					}
					else{
						cout << "QPLA::SmallDriftContinuousCorrection not implementing continuous re-offset PID...it should be activated..." << endl;
						if (CurrentSpecificLinkAux<=-1){
							cout << "QPLA::SmallDriftContinuousCorrection Network synchronization needed..." << endl;
						}
						//oldSmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple];
					}
					long long int SignAuxInstantCorr=0;
				  if (SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]>0){SignAuxInstantCorr=1;}
				  else if (SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]<0){SignAuxInstantCorr=-1;}
				  else {SignAuxInstantCorr=0;}
				  SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple]=SignAuxInstantCorr*(abs(SmallOffsetDriftPerLink[iQuadChIter][CurrentSpecificLinkMultiple])%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux));				  
				}// end if				 
			}// end for

			if (CurrentSpecificLink>=0 and numCurrentEmitReceiveIP>1 and SynchNetworkParamsLinkOther[CurrentSpecificLinkMultipleIndices[0]][2]>0.0 and FlagTestSynch==false){// correction has to take place at the emitter. this Corresponds to RequestMultiple, where the first IP identifies the correction at the sender to the receiver and the extra identifies the other sender, but no other action takes place more than identifying numSpecificLinkmatches>1
				// For transmitter correction
				// Send the updated values to the respective nodes
				this->SetSynchParamsOtherNode(); // Tell the synchronization information to the other nodes		
			}	
		}
		else{// Mal function we should not be here
			cout << "QPLA::The Emitter nodes have not been previously identified, so no small offset drift correction applied" << endl;
		}
	}
	else
	{
		cout << "QPLA::Not applying ApplyProcQubitsSmallTimeOffsetContinuousCorrection small drift offset correction...to be activated..." << endl;
	}
//cout << "QPLA::SmallDriftContinuousCorrection completed!" << endl;
return 0; // All Ok
}

int QPLA::GetSimulateNumStoredQubitsNode(double* TimeTaggsDetAnalytics){ // Function to compute results and return them to the Application layer
	this->acquire();
while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
this->RunThreadAcquireSimulateNumStoredQubitsNode=false;
int SimulateNumStoredQubitsNodeAux=this->SimulateNumStoredQubitsNode[0];// Number of qubits to process
int SimulateNumStoredQubitsNodeMinus1Aux=0;
for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
	if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
    SimulateNumStoredQubitsNodeMinus1Aux+=static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])-1;// Number of qubits to process
  }
}
TimeTaggsDetAnalytics[0]=0.0;
TimeTaggsDetAnalytics[1]=0.0;
TimeTaggsDetAnalytics[2]=0.0;
TimeTaggsDetAnalytics[3]=0.0;
TimeTaggsDetAnalytics[4]=0.0;
TimeTaggsDetAnalytics[5]=0.0;
TimeTaggsDetAnalytics[6]=0.0;
TimeTaggsDetAnalytics[7]=0.0;
TimeTaggsDetAnalytics[8]=0.0;
TimeTaggsDetAnalytics[9]=0.0;
TimeTaggsDetAnalytics[10]=0.0;
// General computations - might be overriden by the below activated particular analysis
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Compute interesting analytics on the TIMETAGGS and detection so that not all data has to be transfered through sockets
// It has to have double precision so that statistics are useful
// Param 0: Num detections channel 1
// Param 1: Num detections channel 2
// Param 2: Num detections channel 3
// Param 3: Num detections channel 4
// Param 4: Multidetection events (coincidences)
// Param 5: Mean time difference between tags
// Param 6: std time difference between tags
// Param 7: Absolute Time tagg value in the middle
// Param 8: Absolute Time tagg value first
// Param 9: Absolute Time tagg value last
// Param 10: Absolute Time tagg value reference

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Normalize time taggs to the first run since the node was started
if (FirstQPLACalcStats==true){// First time. Hence, acquire the normalization value of the time taggs
	FirstQPLACalcStats=false;// Negate forever more this condition
	FirstQPLAtimeTagNorm=static_cast<long long int>(65536*938020371964); // It can be a multiple of a large value of MultFactorEffSynchPeriodQPLA*HistPeriodicityAux, which will not be used (so that an other used MultFactorEffSynchPeriodQPLA*HistPeriodicityAux is multiple), and that does not exceed the current absolute time tag value
	// Old implementation which rendered the comparison between nodes not possible
	//for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
	//	if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>0){			
  //    for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
  //    	if (FirstQPLAtimeTagNorm==0 or (TimeTaggs[iQuadChIter][i]>0 and FirstQPLAtimeTagNorm>static_cast<long long int>(TimeTaggs[iQuadChIter][i]))){
  //    		FirstQPLACalcStats=false;// Negate forever more this condition
	//				FirstQPLAtimeTagNorm=static_cast<long long int>(TimeTaggs[iQuadChIter][i]);
	//			}
	//		}
	//	}
	//}
}
// Floor to the nearest point
FirstQPLAtimeTagNorm=(FirstQPLAtimeTagNorm/(static_cast<long long int>(MultFactorEffSynchPeriodQPLA*HistPeriodicityAux)))*(static_cast<long long int>(MultFactorEffSynchPeriodQPLA*HistPeriodicityAux));

//Actual normalization of Time taggs to the normalization value
for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
	if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>0){
    for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
    	TimeTaggs[iQuadChIter][i]=static_cast<unsigned long long int>(static_cast<long long int>(TimeTaggs[iQuadChIter][i])-FirstQPLAtimeTagNorm);
    }
  }
}

// Check that we now exceed the QuBits buffer size
if (SimulateNumStoredQubitsNodeAux>NumQubitsMemoryBuffer){SimulateNumStoredQubitsNodeAux=NumQubitsMemoryBuffer;}

// Generally mean averaging can be used since outliers (either noise or glitches) have been removed in LinearRegressionQuBitFilter
if (SimulateNumStoredQubitsNodeAux>1){
	// Multicoincidence analysis - Brute force for 3 quad channels
	//for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
	//	if (((ChannelTags[iQuadChIter][i]&0x0001)+((ChannelTags[iQuadChIter][i]>>1)&0x0001)+((ChannelTags[iQuadChIter][i]>>2)&0x0001)+((ChannelTags[iQuadChIter][i]>>3)&0x0001)+((ChannelTags[iQuadChIter][i]>>4)&0x0001)+((ChannelTags[iQuadChIter][i]>>5)&0x0001)+((ChannelTags[iQuadChIter][i]>>6)&0x0001)+((ChannelTags[iQuadChIter][i]>>7)&0x0001)+((ChannelTags[iQuadChIter][i]>>8)&0x0001)+((ChannelTags[iQuadChIter][i]>>9)&0x0001)+((ChannelTags[iQuadChIter][i]>>10)&0x0001)+((ChannelTags[iQuadChIter][i]>>11)&0x0001))>1){
	//		TimeTaggsDetAnalytics[4]+=1.0;
	//	}
	//}

	// Full coincidence in time and channel
	for (int i = 0; i < (QuadNumChGroups-1); i++) {
      for (int j = i+1; j < (QuadNumChGroups); j++) {
          for (unsigned int k = 0; k < RawTotalCurrentNumRecordsQuadCh[i]; k++){
            	for (unsigned int l = 0; l < RawTotalCurrentNumRecordsQuadCh[j]; l++){
	                if (abs(static_cast<long long int>(TimeTaggs[i][k]) - static_cast<long long int>(TimeTaggs[j][l]))<CoincidenceWindowPRU and TimeTaggs[i][k]!=0 and ((BitPositionChannelTags(ChannelTags[i][k])%4)==(BitPositionChannelTags(ChannelTags[j][l])%4))){
	                    TimeTaggsDetAnalytics[4]+=1.0;; // Repetition found
	                }
              }
          }
      }
  }

  // Individual quad channels analysis
	for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
      TimeTaggsDetAnalytics[7]=static_cast<double>(TimeTaggs[iQuadChIter][0]);// Timetag of the first capture
      for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
        //cout << "TimeTaggs[i]: "<< TimeTaggs[i] << endl;
        //cout << "ChannelTags[i]: "<< std::bitset<8>(ChannelTags[i]) << endl;
      	if (ChannelTags[iQuadChIter][i]&0x0001==1 or (ChannelTags[iQuadChIter][i]>>4)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>8)&0x0001==1){TimeTaggsDetAnalytics[0]++;}
      	if ((ChannelTags[iQuadChIter][i]>>1)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>5)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>9)&0x0001==1){TimeTaggsDetAnalytics[1]++;}
      	if ((ChannelTags[iQuadChIter][i]>>2)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>6)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>10)&0x0001==1){TimeTaggsDetAnalytics[2]++;}
      	if ((ChannelTags[iQuadChIter][i]>>3)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>7)&0x0001==1 or (ChannelTags[iQuadChIter][i]>>11)&0x0001==1){TimeTaggsDetAnalytics[3]++;}

        if (i>0){//Compute the mean value
        	TimeTaggsDetAnalytics[5]+=(1.0/(static_cast<double>(SimulateNumStoredQubitsNodeMinus1Aux)))*(static_cast<double>(TimeTaggs[iQuadChIter][i]-TimeTaggs[iQuadChIter][i-1]));

          //// Debugging
          //if ((TimeTaggs[i]-TimeTaggs[i-1])>100){
          //cout << "TimeTaggs[i]: " << TimeTaggs[i] << endl;
          //cout << "TimeTaggs[i-1]: " << TimeTaggs[i-1] << endl;
          //}
        }
      }
    }
  }
  for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
  	if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
  		for (unsigned int i=1;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
  			if (i>0){TimeTaggsDetAnalytics[6]+=(1.0/(static_cast<double>(SimulateNumStoredQubitsNodeMinus1Aux)))*pow(static_cast<double>(TimeTaggs[iQuadChIter][i]-TimeTaggs[iQuadChIter][i-1])-TimeTaggsDetAnalytics[5],2.0);}
  		}
  	}
  }
  TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]);  
}
else{
	TimeTaggsDetAnalytics[0]=0.0;
	TimeTaggsDetAnalytics[1]=0.0;
	TimeTaggsDetAnalytics[2]=0.0;
	TimeTaggsDetAnalytics[3]=0.0;
	TimeTaggsDetAnalytics[4]=0.0;
	TimeTaggsDetAnalytics[5]=0.0;
	TimeTaggsDetAnalytics[6]=0.0;
	TimeTaggsDetAnalytics[7]=0.0;
	TimeTaggsDetAnalytics[8]=0.0;
	TimeTaggsDetAnalytics[9]=0.0;
	TimeTaggsDetAnalytics[10]=0.0;
}
//cout << "TimeTaggsDetAnalytics[0]: " << TimeTaggsDetAnalytics[0] << endl;
//cout << "TimeTaggsDetAnalytics[1]: " << TimeTaggsDetAnalytics[1] << endl;
//cout << "TimeTaggsDetAnalytics[2]: " << TimeTaggsDetAnalytics[2] << endl;
//cout << "TimeTaggsDetAnalytics[3]: " << TimeTaggsDetAnalytics[3] << endl;
//cout << "TimeTaggsDetAnalytics[4]: " << TimeTaggsDetAnalytics[4] << endl;
//cout << "TimeTaggsDetAnalytics[5]: " << TimeTaggsDetAnalytics[5] << endl;
//cout << "TimeTaggsDetAnalytics[6]: " << TimeTaggsDetAnalytics[6] << endl;
//cout << "TimeTaggsDetAnalytics[7]: " << TimeTaggsDetAnalytics[7] << endl;
//cout << "TimeTaggsDetAnalytics[8]: " << TimeTaggsDetAnalytics[8] << endl;
//cout << "TimeTaggsDetAnalytics[9]: " << TimeTaggsDetAnalytics[9] << endl;
//cout << "TimeTaggsDetAnalytics[10]: " << TimeTaggsDetAnalytics[10] << endl;

// Start TIMETAGGING ANALYSIS time synchronization/detection rate
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Particular analysis Time synchronization
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Part to analyze if there is absolute synch between clocks with channel 1 and and histogram periodic signals of 4 steps (ch1, ch2, ch3, ch4).
// Accordingly a complete cycle has 8 counts (2 counts for each step)
// Accordingly, the mean wrapped count difference is stored in TimeTaggsDetAnalytics[5]
// Accordingly, the std wrapped count difference is stored in TimeTaggsDetAnalytics[6]
cout << "QPLA::TIMETAGGING ANALYSIS time synchronization/detection rate of QphysLayerAgent.cpp" << endl;
//cout << "QPLA::Attention, the absolute tag time value has been reset to the first tag since the node was started" << endl;
//cout << "It has to be used PRUassTrigSigScriptHist4Sig in PRU1" << endl;
//cout << "Attention TimeTaggsDetAnalytics[5] stores the mean wrap count difference" << endl;
//cout << "Attention TimeTaggsDetAnalytics[6] stores the std wrap count difference" << endl;
//cout << "Attention TimeTaggsDetAnalytics[7] stores the syntethically corrected absolute timetagg value in the middle" << endl;
//cout << "In GPIO it can be increased NumberRepetitionsSignal when deactivating this hist. analysis" << endl;
if (SimulateNumStoredQubitsNodeAux>1){
	TimeTaggsDetAnalytics[5]=0.0;
	TimeTaggsDetAnalytics[6]=0.0;
	TimeTaggsDetAnalytics[7]=0.0;
	TimeTaggsDetAnalytics[8]=0.0;
	TimeTaggsDetAnalytics[9]=0.0;
	TimeTaggsDetAnalytics[10]=0.0;
	//double TimeTaggsDetAnalytics7ArrayAux[QuadNumChGroups*MaxNumQuBitsPerRun]={0.0};
	//int TimeTaggsDetAnalytics7iterAux=0;
	TimeTaggsDetAnalytics[10]=static_cast<double>(static_cast<long long int>(RawLastTimeTaggRef[0])-FirstQPLAtimeTagNorm);
	unsigned int TimeTaggsDetAnalytics7iQuadChMax=0;
	for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
			if (RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>TimeTaggsDetAnalytics7iQuadChMax){// Considering taking the time tagg in the middle of the larger sequence of tags in iQuadChIter
				TimeTaggsDetAnalytics[7]=TimeTaggs[iQuadChIter][RawTotalCurrentNumRecordsQuadCh[iQuadChIter]/2];
				TimeTaggsDetAnalytics7iQuadChMax=RawTotalCurrentNumRecordsQuadCh[iQuadChIter];
			}
			if (TimeTaggsDetAnalytics[8]>TimeTaggs[iQuadChIter][0] or TimeTaggsDetAnalytics[8]<=0.0){ // Start value
				TimeTaggsDetAnalytics[8]=TimeTaggs[iQuadChIter][0];
			}
			if (TimeTaggsDetAnalytics[9]<TimeTaggs[iQuadChIter][RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1]){ // End value
				TimeTaggsDetAnalytics[9]=TimeTaggs[iQuadChIter][RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1];
			}
			cout << "QPLA::Quad group channel: " << iQuadChIter << endl;			
			for (unsigned int i=0;i<(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){				
				//TimeTaggsDetAnalytics7ArrayAux[TimeTaggsDetAnalytics7iterAux]=static_cast<double>((static_cast<unsigned long long int>(HistPeriodicityAux)/2+TimeTaggs[iQuadChIter][i])%static_cast<unsigned long long int>(HistPeriodicityAux)+static_cast<unsigned long long int>(HistPeriodicityAux)/2);
				//TimeTaggsDetAnalytics7iterAux++;
				if (i==0){cout << "TimeTaggs[iQuadChIter][1]-TimeTaggs[iQuadChIter][0]: " << (static_cast<long long int>(TimeTaggs[iQuadChIter][1])-static_cast<long long int>(TimeTaggs[iQuadChIter][0])) << endl;}
				else if(i==(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-2) and RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>2){cout << "TimeTaggs[iQuadChIter][i+1]-TimeTaggs[iQuadChIter][i]: " << (static_cast<long long int>(TimeTaggs[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggs[iQuadChIter][i])) << endl;}

				TimeTaggsDetAnalytics[5]+=(1.0/(static_cast<double>(SimulateNumStoredQubitsNodeMinus1Aux)))*((static_cast<double>((static_cast<long long int>(HistPeriodicityAux)/2+static_cast<long long int>(TimeTaggs[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggs[iQuadChIter][i]))%(static_cast<long long int>(HistPeriodicityAux))))-static_cast<double>(static_cast<long long int>(HistPeriodicityAux)/2));
			}
		}
	}
	//TimeTaggsDetAnalytics[7]=DoubleMeanFilterSubArray(TimeTaggsDetAnalytics7ArrayAux,TimeTaggsDetAnalytics7iterAux);// Dangerous if the array is too large. Mayybe better mean because if median then it is completely exact due to all the corrections taking place continuously

	for(int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
		if(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
			for (unsigned int i=0;i<(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){
				TimeTaggsDetAnalytics[6]+=(1.0/(static_cast<double>(SimulateNumStoredQubitsNodeMinus1Aux)))*pow((static_cast<double>((static_cast<long long int>(HistPeriodicityAux)/2+static_cast<long long int>(TimeTaggs[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggs[iQuadChIter][i]))%(static_cast<long long int>(HistPeriodicityAux))))-static_cast<double>(static_cast<long long int>(HistPeriodicityAux)/2)-TimeTaggsDetAnalytics[5],2.0);
			}
		}
	}
	TimeTaggsDetAnalytics[6]=sqrt(TimeTaggsDetAnalytics[6]);
  //cout << "Offset corrected TimeTaggs[0]: " << TimeTaggs[0] << endl;
  //cout << "Offset corrected TimeTaggs[1]: " << TimeTaggs[1] << endl;
  //cout << "Offset corrected TimeTaggs[2]: " << TimeTaggs[2] << endl;
  //cout << "Offset corrected TimeTaggs[3]: " << TimeTaggs[3] << endl;
}
else{
	TimeTaggsDetAnalytics[0]=0.0;
	TimeTaggsDetAnalytics[1]=0.0;
	TimeTaggsDetAnalytics[2]=0.0;
	TimeTaggsDetAnalytics[3]=0.0;
	TimeTaggsDetAnalytics[4]=0.0;
	TimeTaggsDetAnalytics[5]=0.0;
	TimeTaggsDetAnalytics[6]=0.0;
	TimeTaggsDetAnalytics[7]=0.0;
	TimeTaggsDetAnalytics[8]=0.0;
	TimeTaggsDetAnalytics[9]=0.0;
	TimeTaggsDetAnalytics[10]=0.0;

}
 // End TIMETAGGING ANALYSIS time synchronization

this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
this->release();
return SimulateNumStoredQubitsNodeAux;
}

int QPLA::GetSimulateSynchParamsNode(double* TimeTaggsDetSynchParams){
	this->acquire();
	while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
	this->RunThreadAcquireSimulateNumStoredQubitsNode=false;

	if (CurrentSpecificLink>=0){
		double dHistPeriodicityAux=static_cast<double>(HistPeriodicityAux);
		double dHistPeriodicityHalfAux=static_cast<double>(HistPeriodicityAux/2.0);
		double SynchNetTransHardwareAdjAux=1.0;// Not retrieving the rel. freq. difference whether positive correction or negative
		TimeTaggsDetSynchParams[0]=SynchNetworkParamsLink[CurrentSpecificLink][0];//(fmodl(dHistPeriodicityHalfAux+SynchNetworkParamsLink[CurrentSpecificLink][0],dHistPeriodicityAux)-dHistPeriodicityHalfAux)/(dHistPeriodicityAux); // Offset in the period it was computed
		TimeTaggsDetSynchParams[1]=SynchNetworkParamsLink[CurrentSpecificLink][1];//((fmod(dHistPeriodicityHalfAux+SynchNetworkParamsLink[CurrentSpecificLink][1],dHistPeriodicityAux)-dHistPeriodicityHalfAux)/dHistPeriodicityAux)*(SynchNetAdj[CurrentSpecificLink]/SynchNetTransHardwareAdjAux); // Relative frequency difference
		TimeTaggsDetSynchParams[2]=SynchNetworkParamsLink[CurrentSpecificLink][2]; // Period in which it was calculated
	}
	else{
		TimeTaggsDetSynchParams[0]=0.0;
		TimeTaggsDetSynchParams[1]=0.0;
		TimeTaggsDetSynchParams[2]=0.0;
	}

	this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
	this->release();

	return 0; // All ok
}

long long int QPLA::BitPositionChannelTags(unsigned long long int ChannelTagsPosAux){
	//long long int BitPosAux=0;
	//while (ChannelTagsPosAux>1 and BitPosAux<16){
	//	ChannelTagsPosAux=ChannelTagsPosAux>>1;
	//	BitPosAux++;
	//}
	//return BitPosAux;
	//return static_cast<long long int>(ChannelTagsPosAux);
	// TAble-based decoding for faster execution
	if (0x0001==ChannelTagsPosAux or 0x0010==ChannelTagsPosAux or 0x0100==ChannelTagsPosAux or 0x1000==ChannelTagsPosAux){
		return static_cast<long long int>(0);
	}
	else if (0x0002==ChannelTagsPosAux or 0x0020==ChannelTagsPosAux or 0x0200==ChannelTagsPosAux or 0x2000==ChannelTagsPosAux){
		return static_cast<long long int>(1);
	}
	else if (0x0004==ChannelTagsPosAux or 0x0040==ChannelTagsPosAux or 0x0400==ChannelTagsPosAux or 0x4000==ChannelTagsPosAux){
		return static_cast<long long int>(2);
	}
	else if (0x0008==ChannelTagsPosAux or 0x0080==ChannelTagsPosAux or 0x0800==ChannelTagsPosAux or 0x8000==ChannelTagsPosAux){
		return static_cast<long long int>(3);
	}
	else{
		cout << "QPLA::BitPositionChannelTags bad decoding...coding produced in GPIO!!!" << endl;
		return static_cast<long long int>(0);
	}
}

int QPLA::HistCalcPeriodTimeTags(char* CurrentReceiveHostIPaux, int iCenterMass,int iNumRunsPerCenterMass){
	//cout << "QPLA::HistCalcPeriodTimeTags CurrentReceiveHostIPaux: " << CurrentReceiveHostIPaux << endl;
	//this->acquire();
	//while(this->RunThreadSimulateReceiveQuBitFlag==false or this->RunThreadAcquireSimulateNumStoredQubitsNode==false){this->release();this->RelativeNanoSleepWait((unsigned int)(15*WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));this->acquire();}// Wait for Receiving thread to finish
	//this->RunThreadAcquireSimulateNumStoredQubitsNode=false;
	//cout << "QPLA::HistCalcPeriodTimeTags RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]: " << RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet] << endl;
	// Check that we not exceed the QuBits buffer size
	if (RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]>NumQubitsMemoryBuffer){RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]=NumQubitsMemoryBuffer;}
	else if (RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]<1){
		cout << "QPLA::HistCalcPeriodTimeTags SpecificQuadChDet " << SpecificQuadChDet << " not enough detections for proper synch. calibration RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]: " << RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet] << endl;
	}

	long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);
	long long int LLIHistPeriodicityHalfAux=static_cast<long long int>(HistPeriodicityAux/2.0);
	double dHistPeriodicityAux=static_cast<double>(HistPeriodicityAux);
	double dHistPeriodicityHalfAux=static_cast<double>(HistPeriodicityAux/2.0);
	long long int LLIMultFactorEffSynchPeriod=static_cast<long long int>(MultFactorEffSynchPeriodQPLA);

	// To make a strong point of all run measurement for the relative frequency difference retrieval
	// Store the information of the start of the detection
	SynchTimeTaggRef[iCenterMass][iNumRunsPerCenterMass]=static_cast<long long int>(RawLastTimeTaggRef[0]);
	long long int ChOffsetCorrection=0;// Variable to acomodate the 4 different channels in the periodic histogram analysis
	if (RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]>0){
		if (UseAllTagsForEstimation){
			// Median averaging
			for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet];i++){
				ChOffsetCorrection=static_cast<long long int>(BitPositionChannelTags(ChannelTags[SpecificQuadChDet][i])%4);// Maps the offset correction for the different channels to detect a specific state
				//cout << "ChOffsetCorrection: " << ChOffsetCorrection << endl;
				SynchFirstTagsArrayAux[i]=(static_cast<long long int>(TimeTaggs[SpecificQuadChDet][i])-ChOffsetCorrection*LLIHistPeriodicityAux)%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux);
				//if (i%300==0){// To be commented when not debugging
				//	cout << "QPLA::HistCalcPeriodTimeTags SynchFirstTagsArrayAux[" << i << "]: " << SynchFirstTagsArrayAux[i] << endl;
				//}
			}
			SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass]=LLIMedianFilterSubArray(SynchFirstTagsArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]));//LLIMeanFilterSubArray(SynchFirstTagsArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]));
			//cout << "QPLA::HistCalcPeriodTimeTags SynchFirstTagsArray[" << iCenterMass <<"][" << iNumRunsPerCenterMass << "]: " << SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass] << endl;
		}
		else{
			// Single value
			ChOffsetCorrection=static_cast<long long int>(BitPositionChannelTags(ChannelTags[SpecificQuadChDet][0])%4);// Maps the offset correction for the different channels to detect a states
			SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass]=(static_cast<long long int>(TimeTaggs[SpecificQuadChDet][0])-ChOffsetCorrection*LLIHistPeriodicityAux)%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux);
			cout << "QPLA::Using only first timetag for network synch computations!...to be deactivated" << endl;
		}
		//cout << "QPLA::SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass]: " << SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass] << endl;
	}

// To retrieve the offset
// If the first iteration, since no extra relative frequency difference added, store the values, for at the end compute the offset, at least within the HistPeriodicityAux
if (iCenterMass==0){// Here the modulo is dependent on the effective period		
	if (RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]>0){	
		if (UseAllTagsForEstimation){
			// Median averaging
			long long int CheckChOffsetCorrectionArray[4][RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]]={0};
			long long int CheckChOffsetCorrection[4]={0};
			unsigned int CheckChOffsetCorrectionIter[4]={0};
			bool boolCheckChOffsetCorrectionflag=false;
			for (unsigned int i=0;i<4;i++){CheckChOffsetCorrectionIter[i]=0;}// Reset values
			for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet];i++){
				ChOffsetCorrection=static_cast<long long int>(BitPositionChannelTags(ChannelTags[SpecificQuadChDet][i])%4);// Maps the offset correction for the different channels to detect a states
				//cout << "ChOffsetCorrection: " << ChOffsetCorrection << endl;
				//SynchFirstTagsArrayAux[i]=(static_cast<long long int>(TimeTaggs[SpecificQuadChDet][i])-ChOffsetCorrection*LLIHistPeriodicityAux)%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux);//(LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[i]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;//static_cast<long long int>(TimeTaggs[i])%LLIHistPeriodicityAux;
				SynchFirstTagsArrayAux[i]=(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[SpecificQuadChDet][i])-ChOffsetCorrection*LLIHistPeriodicityAux)%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux;//(LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[i]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;//static_cast<long long int>(TimeTaggs[i])%LLIHistPeriodicityAux;
				CheckChOffsetCorrectionArray[ChOffsetCorrection][CheckChOffsetCorrectionIter[ChOffsetCorrection]]=SynchFirstTagsArrayAux[i];
				CheckChOffsetCorrectionIter[ChOffsetCorrection]++;
				//if (i%10==0){// To be commented when not debugging
				//	cout << "QPLA::HistCalcPeriodTimeTags ChannelTags[" << i << "]: " << ChannelTags[SpecificQuadChDet][i] << endl;
				//	cout << "QPLA::HistCalcPeriodTimeTags BitPositionChannelTags(ChannelTags[" << i << "]): " << BitPositionChannelTags(ChannelTags[SpecificQuadChDet][i]) << endl;
				//	cout << "QPLA::HistCalcPeriodTimeTags ChOffsetCorrection[" << i << "]: " << ChOffsetCorrection << endl;
				//	cout << "QPLA::HistCalcPeriodTimeTags SynchFirstTagsArrayAux[" << i << "]: " << SynchFirstTagsArrayAux[i] << endl;
				//}
			}
			// Checks of correct GPIO pins alignment
			for (unsigned int i=0;i<4;i++){ // Computes for each of the four pins
				if (CheckChOffsetCorrectionIter[i]>0){
					//CheckChOffsetCorrection[i]=LLIMeanFilterSubArray(CheckChOffsetCorrectionArray[i],static_cast<int>(CheckChOffsetCorrectionIter[i]));
					CheckChOffsetCorrection[i]=LLIMedianFilterSubArray(CheckChOffsetCorrectionArray[i],static_cast<int>(CheckChOffsetCorrectionIter[i])); // To avoid glitches
				}
			}
			for (unsigned int i=0;i<4;i++){
				for (unsigned int j=0;j<4;j++){
					if (i!=j and abs(CheckChOffsetCorrection[i]-CheckChOffsetCorrection[j])>(LLIHistPeriodicityAux/2) and CheckChOffsetCorrectionIter[i]>=5 and CheckChOffsetCorrectionIter[j]>=5){ // If there are counts in each pin compared
						boolCheckChOffsetCorrectionflag=true;
						cout << "QPLA::HistCalcPeriodTimeTags Potentially GPIO pins i=" << i << " " << CheckChOffsetCorrection[i] << " and j=" << j << " " << CheckChOffsetCorrection[j] << " with difference " << (CheckChOffsetCorrection[i]-CheckChOffsetCorrection[j]) << " on SpecificQuadChDet: " << SpecificQuadChDet << " for LLIHistPeriodicityAux: " << LLIHistPeriodicityAux << " connection order is wrong. Check!!!" << endl;
					}
					//else if(i!=j and CheckChOffsetCorrectionIter[i]>=5 and CheckChOffsetCorrectionIter[j]>=5){// Just information
					//	cout << "QPLA::HistCalcPeriodTimeTags GPIO pins i=" << i << " and j=" << j << " with difference " << (CheckChOffsetCorrection[i]-CheckChOffsetCorrection[j]) << " on SpecificQuadChDet: " << SpecificQuadChDet << " for LLIHistPeriodicityAux: " << LLIHistPeriodicityAux << endl;
					//}
				}
			}
			if (boolCheckChOffsetCorrectionflag==true){
				cout << "QPLA::HistCalcPeriodTimeTags Potentially GPIO pins connection order is wrong on SpecificQuadChDet: " << SpecificQuadChDet <<" Check!!!" << endl;
			}
			SynchFirstTagsArrayOffsetCalc[iNumRunsPerCenterMass]=LLIMeanFilterSubArray(SynchFirstTagsArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]));//LLIMedianFilterSubArray(SynchFirstTagsArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]));
			//SynchFirstTagsArrayOffsetCalc[iNumRunsPerCenterMass]=LLIMedianFilterSubArray(SynchFirstTagsArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]));// To avoid glitches//LLIMedianFilterSubArray(SynchFirstTagsArrayAux,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[SpecificQuadChDet]));
			//cout << "QPLA::HistCalcPeriodTimeTags SynchFirstTagsArrayOffsetCalc[" << iNumRunsPerCenterMass << "]: " << SynchFirstTagsArrayOffsetCalc[iNumRunsPerCenterMass] << endl;
		}
		else{
			// Single value
			ChOffsetCorrection=static_cast<long long int>(BitPositionChannelTags(ChannelTags[SpecificQuadChDet][0])%4);// Maps the offset correction for the different channels to detect a states
			//SynchFirstTagsArrayOffsetCalc[iNumRunsPerCenterMass]=(static_cast<long long int>(TimeTaggs[SpecificQuadChDet][0])-ChOffsetCorrection*LLIHistPeriodicityAux)%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux);//(LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[0]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;//static_cast<long long int>(TimeTaggs[0])%LLIHistPeriodicityAux; // Considering only the first timetagg. Might not be very resilence with noise
			SynchFirstTagsArrayOffsetCalc[iNumRunsPerCenterMass]=(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[SpecificQuadChDet][0])-ChOffsetCorrection*LLIHistPeriodicityAux)%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux;//(LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[0]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;//static_cast<long long int>(TimeTaggs[0])%LLIHistPeriodicityAux; // Considering only the first timetagg. Might not be very resilence with noise
			cout << "QPLA::Using only first timetag for network synch computations!...to be deactivated" << endl;
		}
		//cout << "QPLA::SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass]: " << SynchFirstTagsArray[iCenterMass][iNumRunsPerCenterMass] << endl;
	}	
}

//this->RunThreadAcquireSimulateNumStoredQubitsNode=true;
//this->release();
// To retrieve the relative frequency difference
if (iNumRunsPerCenterMass==(NumRunsPerCenterMass-1)){
	// Median averaging
	double CenterMassValAux[NumRunsPerCenterMass-1]={0.0};	
	for (int i=0;i<(NumRunsPerCenterMass-1);i++){
		//if ((SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[iCenterMass][i])<0){
		//	CenterMassValAux[i]=-static_cast<double>(((LLIHistPeriodicityHalfAux-(SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[iCenterMass][i]))%(LLIHistPeriodicityAux))-LLIHistPeriodicityHalfAux);
		//}
		//else{
		//	CenterMassValAux[i]=static_cast<double>(((LLIHistPeriodicityHalfAux+(SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[iCenterMass][i]))%(LLIHistPeriodicityAux))-LLIHistPeriodicityHalfAux);
		//}

		if ((SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[0][i])<0){// With respect reference of the first run with no added relative frequency difference
			CenterMassValAux[i]=-static_cast<double>(((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux-(SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[0][i]))%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux))-LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux);
		}
		else{
			CenterMassValAux[i]=static_cast<double>(((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+(SynchFirstTagsArray[iCenterMass][i+1]-SynchFirstTagsArray[0][i]))%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux))-LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux);
		}
	}
	SynchHistCenterMassArray[iCenterMass]=DoubleMedianFilterSubArray(CenterMassValAux,(NumRunsPerCenterMass-1));//DoubleMeanFilterSubArray(CenterMassValAux,(NumRunsPerCenterMass-1));//DoubleMedianFilterSubArray(CenterMassValAux,(NumRunsPerCenterMass-1)); // Center of mass. Beter to use median for robustness
	//cout << "QPLA::HistCalcPeriodTimeTags SynchHistCenterMassArray[" << iCenterMass << "]: " << SynchHistCenterMassArray[iCenterMass] << endl;
}

if (iCenterMass==(NumCalcCenterMass-1) and iNumRunsPerCenterMass==(NumRunsPerCenterMass-1)){// Achieved number measurements to compute values
	long long int SynchTimeTaggRefMedianArrayAux[NumCalcCenterMass];
	long long int SynchTimeTaggRefMedianArrayAuxAux[NumRunsPerCenterMass-1];
	double SynchTimeTaggRefMedianAux=0.0;// AVerage values of the time interval between measurements
	if (NumCalcCenterMass>1){// when using multiple frequencies - Allows retrieving hardaware adjustments to correct for relative frequency difference (but actually not needed) and takes 3 times longer the mechanism
		if (FreqSynchNormValuesArray[1]>=0.0){
			cout << "QPLA::FreqSynchNormValuesArray[1] is not negative (it has to be negative): " << FreqSynchNormValuesArray[1] << ". Correct using a negative value!!!" << endl;	
		}
		if (FreqSynchNormValuesArray[2]<=0.0){
			cout << "QPLA::FreqSynchNormValuesArray[2] is not positive (it has to be positive): " << FreqSynchNormValuesArray[2] << ". Correct using a positive value!!!" << endl;	
		}
		// Compute the average time between measurements
		for (int i=0;i<(NumRunsPerCenterMass-1);i++){// Offset
			SynchTimeTaggRefMedianArrayAuxAux[i]=SynchTimeTaggRef[0][i+1]-SynchTimeTaggRef[0][i];
		}
		SynchTimeTaggRefMedianArrayAux[0]=LLIMeanFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);//LLIMedianFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);
		for (int i=0;i<(NumRunsPerCenterMass-1);i++){
			SynchTimeTaggRefMedianArrayAuxAux[i]=SynchTimeTaggRef[1][i+1]-SynchTimeTaggRef[1][i];
		}
		SynchTimeTaggRefMedianArrayAux[1]=LLIMeanFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);//LLIMedianFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);
		for (int i=0;i<(NumRunsPerCenterMass-1);i++){
			SynchTimeTaggRefMedianArrayAuxAux[i]=SynchTimeTaggRef[2][i+1]-SynchTimeTaggRef[2][i];
		}
		SynchTimeTaggRefMedianArrayAux[2]=LLIMeanFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);//LLIMedianFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);
		
		long long int SynchTimeTaggRefMedianArrayAuxAuxAux=SynchTimeTaggRefMedianArrayAux[0];//LLIMedianFilterSubArray(SynchTimeTaggRefMedianArrayAux,NumCalcCenterMass);
		SynchTimeTaggRefMedianAux=static_cast<double>(SynchTimeTaggRefMedianArrayAuxAuxAux)*(5e-9);// Conversion to seconds from PRU clock tick
		//
		SynchCalcValuesArray[0]=dHistPeriodicityAux;//((SynchHistCenterMassArray[1]-SynchHistCenterMassArray[0])/(adjFreqSynchNormRatiosArray[1]*FreqSynchNormValuesArray[1] - adjFreqSynchNormRatiosArray[0]*FreqSynchNormValuesArray[0])); //Period adjustment	
		// Computations related to retrieve the relative frequency difference
		// Adjustment of the coefficient into hardware
		//cout << "QPLA::SynchCalcValuesArray[0]: " << SynchCalcValuesArray[0] << endl;	
				
		double SynchCalcValuesArrayFreqAux[NumCalcCenterMass];
		SynchCalcValuesArrayFreqAux[0]=(SynchHistCenterMassArray[0]-FreqSynchNormValuesArray[0]*SynchCalcValuesArray[0])/static_cast<double>(SynchTimeTaggRefMedianArrayAuxAuxAux);// /SynchCalcValuesArray[0]//+FreqSynchNormValuesArray[0]; // Relative Frequency adjustment
		// The retrieved frequency difference is retrieved from the no added frequency measurement		
		SynchCalcValuesArray[2]=SynchCalcValuesArrayFreqAux[0]; // Here the base relative frequency difference correction is computed. then, below is computed an adjusting factor.

		// The two other frequencies help calibrate the hardware constant, either for negative or for positive directions
		//cout << "QPLA::SynchHistCenterMassArray[0]: " << SynchHistCenterMassArray[0] << endl;
		//cout << "QPLA::SynchHistCenterMassArray[1]: " << SynchHistCenterMassArray[1] << endl;
		//cout << "QPLA::SynchHistCenterMassArray[2]: " << SynchHistCenterMassArray[2] << endl;

		// For negative adjustment
		SynchCalcValuesArrayFreqAux[1]=(SynchHistCenterMassArray[1]-SynchHistCenterMassArray[0])/SynchCalcValuesArray[0]/FreqSynchNormValuesArray[1];

		// For positive adjustment
		SynchCalcValuesArrayFreqAux[2]=(SynchHistCenterMassArray[2]-SynchHistCenterMassArray[0])/SynchCalcValuesArray[0]/FreqSynchNormValuesArray[2];
		
		// Storage of the adjustment depending on the relative frequency offset correction direction. Actually, we have to store both direction corrections, since for receiver correction will be one but for sendere correction will be the other direciton
		SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][0]=1.0;// For 0 rel. freq. diff. the correction is 1.0
		SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]=SynchCalcValuesArrayFreqAux[1]; // Negative rel. freq. correction adjustment value
		SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]=SynchCalcValuesArrayFreqAux[2]; // Positive rel. freq. correction adjustment value
		
		if (SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]<=0.0){
			cout << "QPLA::Bad calculation (negative) of SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]: " << SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1] << ". Setting it to 1.0!" << endl;
			SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]=1.0;
		}
		else if (SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]>10.0)
		{
			cout << "QPLA::Bad calculation (too large) of SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]: " << SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1] << ". Setting it to 1.0!" << endl;
			SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]=1.0;
		}

		if (SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]<=0.0){
			cout << "QPLA::Bad calculation (negative) of SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]: " << SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2] << ". Setting it to 1.0!" << endl;
			SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]=1.0;
		}
		else if (SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]>10.0){
			cout << "QPLA::Bad calculation (too large) of SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]: " << SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2] << ". Setting it to 1.0!" << endl;
			SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]=1.0;
		}
		
		//cout << "QPLA::SynchCalcValuesArrayFreqAux[0]: " << SynchCalcValuesArrayFreqAux[0] << endl;
		//cout << "QPLA::SynchCalcValuesArrayFreqAux[1]: " << SynchCalcValuesArrayFreqAux[1] << endl;
		//cout << "QPLA::SynchCalcValuesArrayFreqAux[2]: " << SynchCalcValuesArrayFreqAux[2] << endl;		
	}	
	else{	// When using the base frequency to synchronize (only one frequency testing)
		// Compute the average time between measurements
		for (int i=0;i<(NumRunsPerCenterMass-1);i++){
			SynchTimeTaggRefMedianArrayAuxAux[i]=SynchTimeTaggRef[0][i+1]-SynchTimeTaggRef[0][i];
		}
		SynchTimeTaggRefMedianArrayAux[0]=LLIMedianFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);// MEdian more robustness LLIMeanFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);//LLIMedianFilterSubArray(SynchTimeTaggRefMedianArrayAuxAux,NumRunsPerCenterMass-1);
		long long int SynchTimeTaggRefMedianArrayAuxAuxAux=SynchTimeTaggRefMedianArrayAux[0];// In PRU units of time
		SynchTimeTaggRefMedianAux=static_cast<double>(SynchTimeTaggRefMedianArrayAux[0])*(5e-9);// Conversion to seconds from PRU clock tick. For human representation.
		SynchCalcValuesArray[0]=dHistPeriodicityAux;//Period adjustment
		
		SynchCalcValuesArray[2]=(SynchHistCenterMassArray[0]-FreqSynchNormValuesArray[0]*SynchCalcValuesArray[0])/static_cast<double>(SynchTimeTaggRefMedianArrayAuxAuxAux);// Relative frequency difference

		// Hardware rel. freq. diff. adjustment can not be computed with only one frequency testing, so setting them to 1.0
		SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][0]=1.0;// For 0 rel. freq. diff. the correction is 1.0
		SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]=1.0;// For negative rel. freq. diff. the correction is 1.0
		SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]=1.0;// For positive rel. freq. diff. the correction is 1.0
	}
	
	if (abs(SynchCalcValuesArray[2])<SynchCalcValuesFreqThresh){SynchCalcValuesArray[2]=0.0;}// Zero the retrieved frequency difference because it is too small

	// The SynchNetAdjAux is a value around 2.0, generally
	double InitialCalValueHardwareSynch=(15.0/10.0)/0.5; // Value manually inserted to the factor/ratio between the automatic synch script time between tests and the time used manually to check for the factor 6.0. In this example 15.0 seconds / 10.0 seconds, and corrected with a 0.5 division factor (as for the relative frequency calculation).
	double SynchNetAdjAux=1.0;// Deactivated InitialCalValueHardwareSynch*(6.0/SynchTimeTaggRefMedianAux); // Adjustment value consisting of the 6.0 of the GPIO and divided by the time measurement interval (around 30 seconds), to not produce further skews
	
	SynchCalcValuesArray[2]=-SynchCalcValuesArray[2]*dHistPeriodicityAux; // Normalized frequency difference to the histogram period. Negative because it is a correction

	//cout << "QPLA::SynchCalcValuesArray[2]: " << SynchCalcValuesArray[2] << endl;
	//cout << "QPLA::SynchTimeTaggRefMedianAux: " << SynchTimeTaggRefMedianAux << endl;
	//cout << "QPLA::SynchNetAdjAux: " << SynchNetAdjAux << endl;
	//cout << "QPLA::SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1]: " << SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][1] << ", adjustment factor for negative rel. freq. correction" << endl;
	//cout << "QPLA::SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2]: " << SynchAdjRelFreqCalcValuesArray[CurrentSpecificLink][2] << ", adjustment factor for positive rel. freq. correction" << endl;
	
	/////////////////////////////////////////////////////////////////////////////////
	// Offset calculation
	//cout << "QPLA::Rel. Freq. Difference zeroed...to be deactivated" << endl;
	//SynchCalcValuesArray[2]=0.0; // To be removed

	double SynchCalcValuesArrayAux[NumRunsPerCenterMass];// Split between the four channels		
	for (int i=0;i<NumRunsPerCenterMass;i++){
		// Offset wrapping
		SynchCalcValuesArrayAux[i]=static_cast<double>((LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux+SynchFirstTagsArrayOffsetCalc[i])%(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityAux)-(LLIMultFactorEffSynchPeriod*LLIHistPeriodicityHalfAux));
		//cout << "QPLA::SynchFirstTagsArrayOffsetCalc[i]: " << SynchFirstTagsArrayOffsetCalc[i] << endl;
		//cout << "QPLA::SynchCalcValuesArray[2]: " << SynchCalcValuesArray[2] << endl;
		//cout << "QPLA::SynchCalcValuesArrayAux[i]: " << SynchCalcValuesArrayAux[i] << endl;
	}	
	
	SynchCalcValuesArray[1]=-DoubleMedianFilterSubArray(SynchCalcValuesArrayAux,NumRunsPerCenterMass);//DoubleMeanFilterSubArray(SynchCalcValuesArrayAux,NumRunsPerCenterMass);//DoubleMedianFilterSubArray(SynchCalcValuesArrayAux,NumRunsPerCenterMass); // The actual computed offset. To avoid glitches, it is better to make a median
	//cout << "QPLA::SynchCalcValuesArray[1]: " << SynchCalcValuesArray[1] << endl;
	//cout << "QPLA::SynchCalcValuesArray[2]: " << SynchCalcValuesArray[2] << endl;
	
	// Check if nan values, then convert them to 0 and inform through the terminal
	if (std::isnan(SynchCalcValuesArray[0])){
		SynchCalcValuesArray[0]=0.0;
		cout << "QPLA::Attention QPLA HistCalcPeriodTimeTags nan values!!!" << endl;
		SynchCalcValuesArray[0]=dHistPeriodicityAux; // Set it to nominal histogram value
	}
	if (std::isnan(SynchCalcValuesArray[1])){// Offset
		SynchCalcValuesArray[1]=0.0;
		cout << "QPLA::Attention QPLA HistCalcPeriodTimeTags nan values!!!" << endl;
	}
	if (std::isnan(SynchCalcValuesArray[2])){
		SynchCalcValuesArray[2]=0.0;
		cout << "QPLA::Attention QPLA HistCalcPeriodTimeTags nan values!!!" << endl;
	}

	cout << "QPLA::SynchCalcValuesArray[1]: " << SynchCalcValuesArray[1]/(MultFactorEffSynchPeriodQPLA*dHistPeriodicityAux) << " modulo offset" << endl; // Offset
	cout << "QPLA::SynchCalcValuesArray[2]: " << SynchCalcValuesArray[2]/dHistPeriodicityAux << " rel. freq. " << endl; // Relative frequency difference
	cout << "QPLA::SynchCalcValuesArray[0]: " << SynchCalcValuesArray[0] << " hist. period " << endl; // Period
	
	// Identify the specific link and store/update iteratively the values
	
	if (CurrentSpecificLink>-1){		
		SynchNetworkParamsLink[CurrentSpecificLink][0]=0.0*SynchNetworkParamsLink[CurrentSpecificLink][0]+SynchCalcValuesArray[1];// Offset difference		
		SynchNetworkParamsLink[CurrentSpecificLink][1]=0.0*SynchNetworkParamsLink[CurrentSpecificLink][1]+SynchCalcValuesArray[2];// Relative frequency
		SynchNetworkParamsLink[CurrentSpecificLink][2]=SynchCalcValuesArray[0];// Estimated period
		originalSynchNetworkParamsLink[CurrentSpecificLink][0]=SynchNetworkParamsLink[CurrentSpecificLink][0];
		originalSynchNetworkParamsLink[CurrentSpecificLink][1]=SynchNetworkParamsLink[CurrentSpecificLink][1];
		originalSynchNetworkParamsLink[CurrentSpecificLink][2]=SynchNetworkParamsLink[CurrentSpecificLink][2];
		SynchNetAdj[CurrentSpecificLink]=SynchNetAdjAux;		
		this->SetSynchParamsOtherNode(); // Tell the synchronization information to the other nodes		
	}
	cout << "QPLA::Synchronization parameters updated for this node" << endl;
}
return 0; // All Ok
}

int QPLA::LinearRegressionQuBitFilter(){// remove detection out of detection window
// This script aims at detectig the offset in PRU units, in order to remove outliers (presumambly noise).
// We do so by considering that timetaggs are offset with respect the general absolute time bin, which is a multiple of the Histogram period
//this->acquire(); It is already within an acquire/release
	if (ApplyRawQubitFilteringFlag==true){//and FlagTestSynch==false){
	  this->SimulateNumStoredQubitsNode[0]=0; // Reset this value
	  int RawNumStoredQubits=PRUGPIO.RetrieveNumStoredQuBits(RawLastTimeTaggRef,RawTotalCurrentNumRecordsQuadCh,RawTimeTaggs,RawChannelTags); // Get raw values
	  for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
	  	//cout << "QPLA::RawTotalCurrentNumRecordsQuadCh[iQuadChIter] " << RawTotalCurrentNumRecordsQuadCh[iQuadChIter] << endl;
	  	if (RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>0){
				// If the SNR is not well above 20dB or 30dB, this methods perform really bad
				//////////////////////////////////////////////////////////////////////////
				// Check. It can be commented for normal operation
				//bool CheckOnceAux=false; //bool CheckOnceAux=false;
				//if (RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
				//	for (unsigned int i=0;i<(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){		
				//		if ((static_cast<long long int>(TimeTaggs[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggs[iQuadChIter][i]))<=0){
				//			cout << "QPLA::LinearRegressionQuBitFilter disorded TimeTaggs before processing!!! for i: " << i << ". Involved values TimeTaggs[iQuadChIter][i+1]: " << static_cast<long long int>(TimeTaggs[iQuadChIter][i+1]) << " and static_cast<long long int>(TimeTaggs[iQuadChIter][i]): " << static_cast<long long int>(TimeTaggs[iQuadChIter][i]) << endl;
				//			CheckOnceAux=true;
				//		}
				//	}
				//	if (CheckOnceAux==true){
				//		cout << "QPLA::LinearRegressionQuBitFilter disorded TimeTaggs before processing!!! for iQuadChIter: " << iQuadChIter << endl;
				//	}
				//}
				/////////////////////////////////////////////////////////////////////////
				// Estimate the x values for the linear regression from the y values (RawTimeTaggs)
				long long int TimeTaggRefAux=static_cast<long long int>(RawLastTimeTaggRef[0]);
				long long int xEstimateRawTimeTaggs[RawTotalCurrentNumRecordsQuadCh[iQuadChIter]]={0}; // Timetaggs of the detections raw
				//long long int RoundingAux;
				long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);
				long long int LLIHistPeriodicityHalfAux=static_cast<long long int>(HistPeriodicityAux/2.0);
				//cout << "QPLA::LinearRegressionQuBitFilter LLIHistPeriodicityAux: " << LLIHistPeriodicityAux << endl;
				//cout << "QPLA::LinearRegressionQuBitFilter LLIHistPeriodicityHalfAux: " << LLIHistPeriodicityHalfAux << endl;
				for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
					/*if (i==0){
						RoundingAux=(HistPeriodicityAux/2+RawTimeTaggs[i])%HistPeriodicityAux-HistPeriodicityAux/2;
						if (RoundingAux>=(HistPeriodicityAux/4)){RoundingAux=1;}
						else if (RoundingAux<=(-HistPeriodicityAux/4)){RoundingAux=-1;}
						else{RoundingAux=0;}
						xEstimateRawTimeTaggs[i]=(RawTimeTaggs[i]/HistPeriodicityAux+RoundingAux)*HistPeriodicityAux;
					}
					else{
						RoundingAux=(HistPeriodicityAux/2+RawTimeTaggs[i]-RawTimeTaggs[i-1])%HistPeriodicityAux-HistPeriodicityAux/2;
						if (RoundingAux>=(HistPeriodicityAux/4)){RoundingAux=1;}
						else if (RoundingAux<=(-HistPeriodicityAux/4)){RoundingAux=-1;}
						else{RoundingAux=0;}
						xEstimateRawTimeTaggs[i]=xEstimateRawTimeTaggs[i-1]+((RawTimeTaggs[i]-RawTimeTaggs[i-1])/HistPeriodicityAux+RoundingAux)*HistPeriodicityAux;
					}*/
					//RoundingAux=(HistPeriodicityAux/2+RawTimeTaggs[i])%HistPeriodicityAux-HistPeriodicityAux/2;
					//if (RoundingAux>=(HistPeriodicityAux/4)){RoundingAux=1;}
					//else if (RoundingAux<=(-HistPeriodicityAux/4)){RoundingAux=-1;}
					//else{RoundingAux=0;}

					// Absolute x axis slotting
					xEstimateRawTimeTaggs[i]=((static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])-TimeTaggRefAux+LLIHistPeriodicityHalfAux)/LLIHistPeriodicityAux)*LLIHistPeriodicityAux+TimeTaggRefAux;	// Important to account from -Period/2 to Period/2 as the same x bin
					// Relative x axis slotting
					//if (i==0){
					//	xEstimateRawTimeTaggs[i]=((static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])+LLIHistPeriodicityHalfAux)/LLIHistPeriodicityAux)*LLIHistPeriodicityAux;	// Important to account from -Period/2 to Period/2 as the same x bin
					//}
					//else{
					//	xEstimateRawTimeTaggs[i]=static_cast<long long int>(RawTimeTaggs[iQuadChIter][i-1])+(((static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])-static_cast<long long int>(RawTimeTaggs[iQuadChIter][i-1]))+LLIHistPeriodicityHalfAux)/LLIHistPeriodicityAux)*LLIHistPeriodicityAux;
					//}
				}

				// Find the intercept, since the slope is supposed to be know and equal to 1 (because it has been normalized to HistPeriodicityAux)
				long long int y_mean = 0.0;
				//long long int x_mean = 0.0;
				long long int y_meanArray[RawTotalCurrentNumRecordsQuadCh[iQuadChIter]]={0};
				//long long int x_meanArray[RawNumStoredQubits]={0};
				// Relative
			        //for (int i=0; i < (RawNumStoredQubits-1); i++) {
			        //    y_meanArray[i]=static_cast<double>((HistPeriodicityAux/2+RawTimeTaggs[i+1]-RawTimeTaggs[i])%HistPeriodicityAux)-HistPeriodicityAux/2.0;
			        //    //x_meanArray[i]=static_cast<double>(xEstimateRawTimeTaggs[i]%HistPeriodicityAux);// Not really needed
			        //    // We cannot use mean averaging since there might be outliers
				      ////y_mean += static_cast<double>(RawTimeTaggs[i]%HistPeriodicityAux)/static_cast<double>(RawNumStoredQubits);
				      ////x_mean += static_cast<double>(xEstimateRawTimeTaggs[i]%HistPeriodicityAux)/static_cast<double>(RawNumStoredQubits);
			        //}
			        //y_mean=DoubleMedianFilterSubArray(y_meanArray,(RawNumStoredQubits-1)); // Median average
			        // Absolute
				for (unsigned int i=0; i < RawTotalCurrentNumRecordsQuadCh[iQuadChIter]; i++) {
					//y_meanArray[i]=(LLIHistPeriodicityHalfAux+static_cast<long long int>(RawTimeTaggs[iQuadChIter][i]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;
					y_meanArray[i]=(static_cast<long long int>(RawTimeTaggs[iQuadChIter][i]))%LLIHistPeriodicityAux;
					//if (y_meanArray[i]>LLIHistPeriodicityHalfAux){
					//	y_meanArray[i]=y_meanArray[i]-LLIHistPeriodicityAux;
					//}
					//else if(y_meanArray[i]<-LLIHistPeriodicityHalfAux){
					//	y_meanArray[i]=y_meanArray[i]+LLIHistPeriodicityAux;
					//}
			    //x_meanArray[i]=static_cast<long long int>(xEstimateRawTimeTaggs[i]%LLIHistPeriodicityAux);// Not really needed
				}
        y_mean=LLIMedianFilterSubArray(y_meanArray,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])); // Median average
        //y_mean=LLIMeanFilterSubArray(y_meanArray,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])); // Mean average
        //x_mean=LLIMedianFilterSubArray(x_meanArray,static_cast<int>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter])); // Median average. Not really needed x_mean
        //cout << "QPLA::y_mean: " << y_mean << endl;
        //cout << "QPLA::x_mean: " << x_mean << endl;

        long long int EstInterceptVal = y_mean;// - x_mean;
				//long long int EstInterceptVal = 0;
				//if (y_mean<0){
				//	EstInterceptVal = -y_mean;// - x_mean;
				//}
				//else{//} if(y_mean>=0){
				//	EstInterceptVal = y_mean;// - x_mean;
				//}

				//cout << "QPLA::LinearRegressionQuBitFilter EstInterceptVal: " << EstInterceptVal << endl;

				// Re-escale the xEstimated values with the intercept point
				for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
					xEstimateRawTimeTaggs[i]=xEstimateRawTimeTaggs[i]+EstInterceptVal;
				}

				unsigned int FilteredNumStoredQubits=0;
				double FilterDiffCheckAux=0.0;
				// Filter out detections not falling within the defined detection window and calculated signal positions				
				for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
					if (abs(static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])-xEstimateRawTimeTaggs[i])<=FilteringAcceptWindowSize){// Within acceptance window
						TimeTaggs[iQuadChIter][FilteredNumStoredQubits]=RawTimeTaggs[iQuadChIter][i];
						ChannelTags[iQuadChIter][FilteredNumStoredQubits]=RawChannelTags[iQuadChIter][i];
						FilteredNumStoredQubits++;
					}
					//else{// This can be commented for normal operation
					//	if (FilterDiffCheckAux==0.0){
					//		FilterDiffCheckAux=static_cast<double>((static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])-xEstimateRawTimeTaggs[i]));
					//		cout << "QPLA::LinearRegressionQuBitFilter FilterDiffCheckAux initial: " << FilterDiffCheckAux << endl;
					//	}
					//	else{
					//		FilterDiffCheckAux=0.5*FilterDiffCheckAux+0.5*static_cast<double>((static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])-xEstimateRawTimeTaggs[i]));
					//		if (i%25==0){
					//			cout << "QPLA::LinearRegressionQuBitFilter FilterDiffCheckAux final i[" << i << "]: " << static_cast<double>((static_cast<long long int>(RawTimeTaggs[iQuadChIter][i])-xEstimateRawTimeTaggs[i])) << endl;
					//		}
					//	}
					//}
				}
				//cout << "QPLA::LinearRegressionQuBitFilter FilterDiffCheckAux: " << FilterDiffCheckAux << endl;
				///////////////////////////////////////////////////////////////////////////////////////////////////
				//// Check for the slopes values
				////// Compute the slope value for the x axis
				//double SlopeDetTagsAuxArrayXaxis[RawTotalCurrentNumRecordsQuadCh[iQuadChIter]]={0.0};
				//double SlopeDetTagsAuxXaxis=0.0;
    		//int iAux=0;
    		//for (unsigned int i=0;i<(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){
    		//	SlopeDetTagsAuxArrayXaxis[iAux]=static_cast<double>((LLIHistPeriodicityHalfAux+(xEstimateRawTimeTaggs[i+1]-xEstimateRawTimeTaggs[i]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux);
    		//}
    		//
    		//SlopeDetTagsAuxXaxis=DoubleMeanFilterSubArray(SlopeDetTagsAuxArrayXaxis,iAux);//DoubleMedianFilterSubArray(SlopeDetTagsAuxArrayXaxis,iAux);
		    //cout << "QPLA::LinearRegressionQuBitFilter SlopeDetTagsAuxXaxis: " << SlopeDetTagsAuxXaxis << endl;
				//
		    //double SlopeDetTagsAuxArrayYaxis[RawTotalCurrentNumRecordsQuadCh[iQuadChIter]]={0.0};
				//double SlopeDetTagsAuxYaxis=0.0;
    		//iAux=0;
    		//for (unsigned int i=0;i<(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){
    		//	SlopeDetTagsAuxArrayYaxis[iAux]=static_cast<double>(static_cast<long long int>(RawTimeTaggs[iQuadChIter][i+1])-static_cast<long long int>(RawTimeTaggs[iQuadChIter][i]))/static_cast<double>(xEstimateRawTimeTaggs[i+1]-xEstimateRawTimeTaggs[i]);
    		//}
    		//
    		//SlopeDetTagsAuxYaxis=DoubleMeanFilterSubArray(SlopeDetTagsAuxArrayYaxis,iAux);//DoubleMedianFilterSubArray(SlopeDetTagsAuxArrayYaxis,iAux);
		    //cout << "QPLA::LinearRegressionQuBitFilter SlopeDetTagsAuxYaxis: " << SlopeDetTagsAuxYaxis << endl;
				//////////////////////////////////////////////////////////////////////////////////////////////////
	
				// Compute quality of estimation, related to the SNR
				double EstimatedSNRqubitsRatio=1.0-static_cast<double>(FilteredNumStoredQubits)/static_cast<double>(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]);// in linear	

				if (EstimatedSNRqubitsRatio>0.1){ // 0.1 equivalent to 10 dB// < Bad SNR
					cout << "QPLA::LinearRegressionQuBitFilter EstimatedSNRqubitsRatio " << EstimatedSNRqubitsRatio << " for quad group Channel "<< iQuadChIter << " does not have enough SNR (>10 dB) to perform good when filtering raw qubits!!! Not filtering outlier qubits!!!" << endl;
					FilteredNumStoredQubits=0;// Reset value
					for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
						TimeTaggs[iQuadChIter][FilteredNumStoredQubits]=RawTimeTaggs[iQuadChIter][i];
						ChannelTags[iQuadChIter][FilteredNumStoredQubits]=RawChannelTags[iQuadChIter][i];
						FilteredNumStoredQubits++;
					}
				}
				
				//cout << "QPLA::FilteredNumStoredQubits: " << FilteredNumStoredQubits << endl;
							
				// Update final values
				this->SimulateNumStoredQubitsNode[0]+=static_cast<unsigned int>(FilteredNumStoredQubits); // Update value
				RawTotalCurrentNumRecordsQuadCh[iQuadChIter]=static_cast<unsigned int>(FilteredNumStoredQubits); // Update value
				
				//////////////////////////////////////////
				// Checks of proper values handling
				//long long int LLIHistPeriodicityAux=static_cast<long long int>(HistPeriodicityAux);
				//long long int LLIHistPeriodicityHalfAux=static_cast<long long int>(HistPeriodicityAux/2);
				//long long int CheckValueAux=(LLIHistPeriodicityHalfAux+static_cast<long long int>(TimeTaggs[0]))%LLIHistPeriodicityAux-LLIHistPeriodicityHalfAux;
				//cout << "QPLA::LinearRegressionQuBitFilter::CheckValueAux: "<< CheckValueAux << endl;
				////////////////////////////////////////
				//////////////////////////////////////////////////////////////////////////
				// Check. It can be commented for normal operation
				//CheckOnceAux=false; //bool CheckOnceAux=false;
				//if (RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>1){
				//	for (unsigned int i=0;i<(RawTotalCurrentNumRecordsQuadCh[iQuadChIter]-1);i++){		
				//		if ((static_cast<long long int>(TimeTaggs[iQuadChIter][i+1])-static_cast<long long int>(TimeTaggs[iQuadChIter][i]))<=0){
				//			cout << "QPLA::LinearRegressionQuBitFilter disorded TimeTaggs after processing!!! for i: " << i << ". Involved values TimeTaggs[iQuadChIter][i+1]: " << static_cast<long long int>(TimeTaggs[iQuadChIter][i+1]) << " and static_cast<long long int>(TimeTaggs[iQuadChIter][i]): " << static_cast<long long int>(TimeTaggs[iQuadChIter][i]) << endl;
				//			CheckOnceAux=true;
				//		}
				//	}
				//	if (CheckOnceAux==true){
				//		cout << "QPLA::LinearRegressionQuBitFilter disorded TimeTaggs after processing!!! for iQuadChIter: " << iQuadChIter << endl;
				//	}
				//}
				/////////////////////////////////////////////////////////////////////////
			}
		}// for
	}
	else{ // Do not apply filtering
		//if (FlagTestSynch==false){
		//	cout << "QPLA::Not applying ApplyRawQubitFilteringFlag...to be activated" << endl;
		//}
		cout << "QPLA::Not applying ApplyRawQubitFilteringFlag...to be activated" << endl;
		this->SimulateNumStoredQubitsNode[0]=0; // Reset this value
	  int RawNumStoredQubits=PRUGPIO.RetrieveNumStoredQuBits(RawLastTimeTaggRef,RawTotalCurrentNumRecordsQuadCh,RawTimeTaggs,RawChannelTags); // Get raw values
	  for (int iQuadChIter=0;iQuadChIter<QuadNumChGroups;iQuadChIter++){
	  	unsigned int FilteredNumStoredQubits=0;// Reset value
	  	if (RawTotalCurrentNumRecordsQuadCh[iQuadChIter]>0){				
				for (unsigned int i=0;i<RawTotalCurrentNumRecordsQuadCh[iQuadChIter];i++){
					TimeTaggs[iQuadChIter][FilteredNumStoredQubits]=RawTimeTaggs[iQuadChIter][i];
					ChannelTags[iQuadChIter][FilteredNumStoredQubits]=RawChannelTags[iQuadChIter][i];
					FilteredNumStoredQubits++;
				}
			}
						
			// Update final values
			this->SimulateNumStoredQubitsNode[0]+=static_cast<unsigned int>(FilteredNumStoredQubits); // Update value
			RawTotalCurrentNumRecordsQuadCh[iQuadChIter]=static_cast<unsigned int>(FilteredNumStoredQubits);
		} // for
	}
	//this->release(); It is already within an acquire/release
//cout << "QPLA::LinearRegressionQuBitFilter completed!" << endl;
return 0; // All Ok
}

int QPLA::RelativeNanoSleepWait(unsigned int TimeNanoSecondsSleep){
	struct timespec ts;
	ts.tv_sec=(int)(TimeNanoSecondsSleep/((long)1000000000));
	ts.tv_nsec=(long)(TimeNanoSecondsSleep%(long)1000000000);
clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL); //

return 0; // All ok
}

QPLA::~QPLA() {
// destructor
/* Very slow GPIO BBB not used anymore
outGPIO->streamOutClose();
inGPIO->streamInClose();
*/
//delete PRUGPIO; // Destructor for the PRUGPIO instance
this->threadRef.join();// Terminate the process thread
this->PRUGPIO.~GPIO(); // Destruct the instance of the below layer
}

int QPLA::NegotiateInitialParamsNode(){
	try{
		this->acquire();

		this->release();
} // try
  catch (...) { // Catches any exception
  	cout << "Exception caught" << endl;
  }

return 0;// All OK
}

int QPLA::RegularCheckToPerform(){
if (GPIOHardwareSynched==false){// Only until it is synched
	GPIOHardwareSynched=PRUGPIO.GetHardwareSynchStatus();// Since only transportN agent can send messages to host, then it is ersponsibility of transportN to also check for this and then send message to host
}

return 0; // All ok
}

void QPLA::AgentProcessRequestsPetitions(){// Check next thing to do

	//this->NegotiateInitialParamsNode();

	bool isValidWhileLoop = true;
	while(isValidWhileLoop){
		this->acquire();// Wait semaphore until it can proceed
		try{
			try {   	
    	this->ReadParametersAgent(); // Reads messages from above layer
    	this->RegularCheckToPerform();// Every now and then some checks have to happen    	
        
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
        this->RelativeNanoSleepWait((unsigned int)(WaitTimeAfterMainWhileLoop*(1.0+(float)rand()/(float)RAND_MAX)));// Wait a few microseconds for other processes to enter
    } // while
  }

  double QPLA::DoubleMedianFilterSubArray(double* ArrayHolderAux,int MedianFilterFactor){
  	if (MedianFilterFactor<=1){
  		return ArrayHolderAux[0];
  	}
  	else{
			/* // Non-efficient code
			// Step 1: Copy the array to a temporary array
  		double temp[MedianFilterFactor]={0.0};
  		for(int i = 0; i < MedianFilterFactor; i++) {
  			temp[i] = ArrayHolderAux[i];
  		}

    	// Step 2: Sort the temporary array
  		this->DoubleBubbleSort(temp,MedianFilterFactor);
    	// If odd, middle number
  		return temp[MedianFilterFactor/2];*/

  		// Efficient code
  		int ArrayHolderAuxTemp[MedianFilterFactor];
			for(int i = 0; i < MedianFilterFactor; i++) {
				ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
			}
			// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
  	}
  }

// Function to implement Bubble Sort
  int QPLA::DoubleBubbleSort(double* arr,int MedianFilterFactor) {
  	double temp=0.0;
  	for (int i = 0; i < MedianFilterFactor-1; i++) {
  		for (int j = 0; j < MedianFilterFactor-i-1; j++) {
  			if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
  				temp = arr[j];
  				arr[j] = arr[j+1];
  				arr[j+1] = temp;
  			}
  		}
  	}
    return 0; // All ok
  }

  double QPLA::DoubleMeanFilterSubArray(double* ArrayHolderAux,int MeanFilterFactor){
		if (MeanFilterFactor<=1){
			return ArrayHolderAux[0];
		}
		else{
		// Step 1: Copy the array to a temporary array
			double temp=0.0;
			for(int i = 0; i < MeanFilterFactor; i++) {
				temp = temp + ArrayHolderAux[i];
			}
			
			temp=temp/((double)(MeanFilterFactor));
			return temp;
		}
	}

  unsigned long long int QPLA::ULLIMedianFilterSubArray(unsigned long long int* ArrayHolderAux,int MedianFilterFactor){
  	if (MedianFilterFactor<=1){
  		return ArrayHolderAux[0];
  	}
  	else{
  		/* // Non-efficient code
			// Step 1: Copy the array to a temporary array
  		unsigned long long int temp[MedianFilterFactor]={0};
  		for(int i = 0; i < MedianFilterFactor; i++) {
  			temp[i] = ArrayHolderAux[i];
  		}
	    // Step 2: Sort the temporary array
	  	this->ULLIBubbleSort(temp,MedianFilterFactor);
	    // If odd, middle number
	  	return temp[MedianFilterFactor/2];*/

	  	// Efficient code
	  	unsigned long long int ArrayHolderAuxTemp[MedianFilterFactor];
			for(int i = 0; i < MedianFilterFactor; i++) {
				ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
			}
			// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
  	}
  }

// Function to implement Bubble Sort
  int QPLA::ULLIBubbleSort(unsigned long long int* arr,int MedianFilterFactor) {
  	unsigned long long int temp=0;
  	for (int i = 0; i < MedianFilterFactor-1; i++) {
  		for (int j = 0; j < MedianFilterFactor-i-1; j++) {
  			if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
  				temp = arr[j];
  				arr[j] = arr[j+1];
  				arr[j+1] = temp;
  			}
  		}
  	}
    return 0; // All ok
  }

  long long int QPLA::LLIMeanFilterSubArray(long long int* ArrayHolderAux,int MeanFilterFactor){
		if (MeanFilterFactor<=1){
			return ArrayHolderAux[0];
		}
		else{
		// Step 1: Copy the array to a temporary array
			long long int temp=0.0;
			for(int i = 0; i < MeanFilterFactor; i++) {
				temp = temp + ArrayHolderAux[i];
			}
			
			temp=static_cast<long long int>(static_cast<long double>(temp)/static_cast<long double>(MeanFilterFactor));
			return temp;
		}
	}

  long long int QPLA::LLIMedianFilterSubArray(long long int* ArrayHolderAux,int MedianFilterFactor){
  	if (MedianFilterFactor<=1){
  		return ArrayHolderAux[0];
  	}
  	else{
  		/* // Non-efficient code
			// Step 1: Copy the array to a temporary array
  		long long int temp[MedianFilterFactor]={0};
  		for(int i = 0; i < MedianFilterFactor; i++) {
  			temp[i] = ArrayHolderAux[i];
  		}
  		// Step 2: Sort the temporary array
  		this->LLIBubbleSort(temp,MedianFilterFactor);
    	// If odd, middle number
  		return temp[MedianFilterFactor/2];*/

  		// Efficient code
  		long long int ArrayHolderAuxTemp[MedianFilterFactor];
			for(int i = 0; i < MedianFilterFactor; i++) {
				ArrayHolderAuxTemp[i] = ArrayHolderAux[i];
			}
			// Step 1: Find the median element without fully sorting
	    int midIndex = MedianFilterFactor / 2;
	    std::nth_element(ArrayHolderAuxTemp, ArrayHolderAuxTemp + midIndex, ArrayHolderAuxTemp + MedianFilterFactor);

	    // Step 2: Return the median
	    return ArrayHolderAuxTemp[midIndex];
  	}
  }

// Function to implement Bubble Sort
  int QPLA::LLIBubbleSort(long long int* arr,int MedianFilterFactor) {
  	long long int temp=0;
  	for (int i = 0; i < MedianFilterFactor-1; i++) {
  		for (int j = 0; j < MedianFilterFactor-i-1; j++) {
  			if (arr[j] > arr[j+1]) {
                // Swap arr[j] and arr[j+1]
  				temp = arr[j];
  				arr[j] = arr[j+1];
  				arr[j+1] = temp;
  			}
  		}
  	}
    return 0; // All ok
  }

} /* namespace nsQphysLayerAgent */

/*
// For testing
using namespace nsQphysLayerAgent;

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
 
 int objectEmitLinkNumberArray[2], objectReceiveLinkNumberArray[2];
 QPLA objectQPLA(2, objectEmitLinkNumberArray, objectReceiveLinkNumberArray); 
 if (argc>1){objectQPLA.emitQuBit();} // ./QphysLayerAgent 1
 else{objectQPLA.receiveQuBit();}   // ./QphysLayerAgent

 return 0;
}
*/


