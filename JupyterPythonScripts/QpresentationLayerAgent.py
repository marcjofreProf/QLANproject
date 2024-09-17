#####################################################
# Author: Prof. Marc Jofre
# Dept. Network Engineering
# Universitat Politècnica de Catalunya - Technical University of Catalonia
#
# Modified: 2024
# Created: 2024
#
# Script Quantum LAN presentation layer
#####################################################

import sys, os
import time
import numpy as np

import QsessionLayerAgent
  
sSynchProcIterRunsTimePoint=30.0 # Time to wait (seconds) between iterations of the synch mechanisms to allow time to send and receive the necessary qubits. Important that it is long enough to not induce a sort of extra relative frequency difference (hence we are in the linearity region)

class QPLA:
	def __init__(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Constructor of this class
            self.QSLAagent = QsessionLayerAgent.QSLA(ParamsDescendingCharArray,ParamsAscendingCharArray) # Create instance of the Agent below
            # Variables of the class
            self.BiValueIteratorVal=0
                
        ##############################################################
	# Methods
	def ListCharArrayParser(self,ListCharArrayAux):
	    # Actually concatenating a python list of strings to a single string
	    ParsedCharArrayAux=",".join(ListCharArrayAux)+","
	    return ParsedCharArrayAux
	
	def SemiColonListCharArrayParser(self,ListCharArrayAux):
	    # Actually concatenating a python list of strings to a single string
	    ParsedCharArrayAux=";".join(ListCharArrayAux)+";"
	    return ParsedCharArrayAux
	
	def UnderScoreListCharArrayParser(self,ListCharArrayAux):
	    # Actually concatenating a python list of strings to a single string
	    ParsedCharArrayAux="_".join(ListCharArrayAux)+"_"
	    return ParsedCharArrayAux
	
	def ColonListCharArrayParser(self,ListCharArrayAux):
	    # Actually concatenating a python list of strings to a single string
	    ParsedCharArrayAux=":".join(ListCharArrayAux)+":"
	    return ParsedCharArrayAux
 	
	def InitAgentProcess(self,): # Pass to the below agent
		self.QSLAagent.InitAgentProcess()
	
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QSLAagent.SendMessageAgent(ParamsDescendingCharArray)
	
	def WaitUntilActiveActionFreePreLock(self,argsPayloadListAux): # Block other involved hosts/nodes
		argsPayloadList=argsPayloadListAux#[IPhostDestOpNet]
		argsPayloadAux=self.ListCharArrayParser(argsPayloadList)
		self.QSLAagent.WaitUntilActiveActionFreePreLock(argsPayloadAux,len(argsPayloadList))
	
	def UnBlockActiveActionFreePreLock(self, argsPayloadListAux): # Unblock other involved hosts/nodes
		argsPayloadList=argsPayloadListAux#[IPhostDestOpNet]
		argsPayloadAux=self.ListCharArrayParser(argsPayloadList)
		self.QSLAagent.UnBlockActiveActionFreePreLock(argsPayloadAux,len(argsPayloadList))
	
	# Active actions that use the nodes, so they have to be blocked when the node is in use (blocked from the above agent)
	def SimulateRequestQubitsHost(self,IPhostDestOpNet,IPhostOrgOpNet,IPhostDestConNet,IPhostOrgConNet,NumRequestedQubitsSend,NumRequestedQubitsReceive,PeriodSignalHistVal,SynchPRUoffFreqVal,QuadChEmtDet): # Request that host's node sends qubits to this host's node
		# For SimulateReceiveQubits, the messagePayload consist of indicating (separated by;) whether the node will be "Active" or "Passive" on sending the TimePoint Barrier, a list of IP identifying the Node/s (separated by "_") that will emit qubits in this specific rounds, a list of IPs (separated by "_") to send the TimePoint Barrier if this receiving node was to be "active", a NumRequestedQubits, the histogram/period of receiving signals, an added offset value when sending qubits (typically 0.0) and an added relative frequency difference when sending qubits (typically 0.0), and an array identifying the quad group channel coding for emitter and receiver
		ReceiveSynchPRUoffFreqVal=0.0*SynchPRUoffFreqVal
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDestOpNet]),self.UnderScoreListCharArrayParser([IPhostDestOpNet]),str(NumRequestedQubitsReceive),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[1])])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPhostDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		
		# For SimulateSendQubits, the messagePayload consist of indicating (separated by;) whether the node will be "Active" or "Passive" on sending the TimePoint Barrier (typically a Send node will be "Passive"), a list of IP identifying the Node/s (separated by "_") that will receive qubits in this specific rounds, a list of IPs (separated by "_") to send the TimePoint Barrier if this receiving node was to be "active" and a NumRequestedQubits, the histogram/period of sending signals, an added offset value when sending qubits (typically 0.0) and an added relative frequency difference when sending qubits (typically 0.0)
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),str(NumRequestedQubitsSend),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1]),str(QuadChEmtDet[0])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgOpNet
		messageIPdest=IPhostDestOpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		
	def SimulateSendEntangledQubitsHost(self,IPhostDest1OpNet,IPhostOrg1OpNet,IPhostDest2OpNet,IPhostOrg2OpNet,IPnodeDestConNet,IPhostOrgConNet,NumRequestedQubitsSend,NumRequestedQubitsReceive,PeriodSignalHistVal,SynchPRUoffFreqVal,QuadChEmtDet): # Request that the other nodes of the specified hosts get ready to receive entangled qubits from the dealer's node
		# Attention, this method QuadChEmtDet contains an array with one quad group channel emitter and at the end two the quad group channel receiver
		ReceiveSynchPRUoffFreqVal=0.0*SynchPRUoffFreqVal
		if (self.BiValueIteratorVal==1):#Alternate who is active
			messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostOrg2OpNet]),self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostOrg1OpNet]),str(NumRequestedQubitsReceive),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[2])])
			messageCommandAux="SimulateReceiveQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrg2OpNet
			messageIPdest=IPhostDest2OpNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)		
			messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg1OpNet]),self.UnderScoreListCharArrayParser([IPhostDest2OpNet,IPhostOrg2OpNet]),str(NumRequestedQubitsReceive),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[1])])
			messageCommandAux="SimulateReceiveQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrg1OpNet
			messageIPdest=IPhostDest1OpNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)
			
			# For identifying instead of the reciever, only identify the sender - since no network correction has to take place
			messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostDest2OpNet]),self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),str(NumRequestedQubitsSend),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1]),str(QuadChEmtDet[0])])
			messageCommandAux="SimulateSendQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrgConNet
			messageIPdest=IPnodeDestConNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)
		else:
			messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostOrg1OpNet]),self.UnderScoreListCharArrayParser([IPhostDest2OpNet,IPhostOrg2OpNet]),str(NumRequestedQubitsReceive),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[1])])
			messageCommandAux="SimulateReceiveQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrg1OpNet
			messageIPdest=IPhostDest1OpNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)
			
			messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg2OpNet]),self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostOrg1OpNet]),str(NumRequestedQubitsReceive),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[2])])
			messageCommandAux="SimulateReceiveQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrg2OpNet
			messageIPdest=IPhostDest2OpNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)	
		
			# For identifying instead of the reciever, only identify the sender - since no network correction has to take place
			messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostDest1OpNet]),self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),str(NumRequestedQubitsSend),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1]),str(QuadChEmtDet[0])])
			messageCommandAux="SimulateSendQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrgConNet
			messageIPdest=IPnodeDestConNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)
		
		self.BiValueIteratorVal=int(np.mod(self.BiValueIteratorVal+1,2))# Alternate who is active sender
		"""	
		Very weird, if emitting node tries to set the time barrier, then there is a lot of jitter in the detections??? Maybe it has been corrected because now the TimePoint Barrier is only sent to the specific nodes needing it (instead of braodcasting to all nodes)
		"""
				
	def SimulateRequestMultipleNodesQubitsHost(self,IPhostDest1OpNet,IPhostOrg1OpNet,IPhostDest2OpNet,IPhostOrg2OpNet,IPnodeDestConNet,IPnodeDest1ConNet,IPnodeDest2ConNet,IPhostOrgConNet,NumRequestedQubitsSend,NumRequestedQubitsReceive,PeriodSignalHistVal,SynchPRUoffFreqVal1,SynchPRUoffFreqVal2,QuadChEmtDet): # Request other nodes to send to this node qubits
		ReceiveSynchPRUoffFreqVal=0.0*(0.5*SynchPRUoffFreqVal1+0.5*SynchPRUoffFreqVal2)
		# For the list of IP for the correction, it is very important the order, first the destination and then the other sender
		# Attention, this method QuadChEmtDet contains an array with more than one quad group channel emitter and at the end the quad group channel receiver
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg1OpNet,IPhostDest2OpNet]),self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostOrg2OpNet]),str(NumRequestedQubitsSend),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal2[0]),str(SynchPRUoffFreqVal2[1]),str(QuadChEmtDet[0])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg2OpNet
		messageIPdest=IPhostDest2OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)		
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg2OpNet,IPhostDest1OpNet]),self.UnderScoreListCharArrayParser([IPhostDest2OpNet,IPhostOrg1OpNet]),str(NumRequestedQubitsSend),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal1[0]),str(SynchPRUoffFreqVal1[1]),str(QuadChEmtDet[1])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg1OpNet
		messageIPdest=IPhostDest1OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),str(NumRequestedQubitsReceive),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[2])])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPnodeDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
	
	def SimulateRequestSynchsHost(self,IPhostDestOpNet,IPhostOrgOpNet,IPhostDestConNet,IPhostOrgConNet,NumRequestedQubitsSend,NumRequestedQubitsReceive,NumRunsPerCenterMass,SynchFreqPRUarrayTest,PeriodSignalHistVal,SynchPRUoffFreqVal,QuadChEmtDet): # Request that host's node sends qubits to this host's node
		ReceiveSynchPRUoffFreqVal=0.0*SynchPRUoffFreqVal	
		NumCalcCenterMass=len(SynchFreqPRUarrayTest)
		for iCenterMass in range(0,NumCalcCenterMass,1):
			for iNumRunsPerCenterMass in range(0,NumRunsPerCenterMass,1):				
				if (NumCalcCenterMass>1):
					# With three values of frequency test for network synchronization
					messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDestOpNet]),self.UnderScoreListCharArrayParser([IPhostDestOpNet]),str(NumRequestedQubitsReceive),str(NumRunsPerCenterMass),str(iCenterMass),str(iNumRunsPerCenterMass),str(SynchFreqPRUarrayTest[0]),str(SynchFreqPRUarrayTest[1]),str(SynchFreqPRUarrayTest[2]),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1]),str(QuadChEmtDet[1])])
				else:
					# With one value of frequency test for network synchronization
					messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDestOpNet]),self.UnderScoreListCharArrayParser([IPhostDestOpNet]),str(NumRequestedQubitsReceive),str(NumRunsPerCenterMass),str(iCenterMass),str(iNumRunsPerCenterMass),str(SynchFreqPRUarrayTest[0]),str(PeriodSignalHistVal),str(ReceiveSynchPRUoffFreqVal[0]),str(ReceiveSynchPRUoffFreqVal[1]),str(QuadChEmtDet[1])])
				
				messageCommandAux="SimulateReceiveSynchQubits"
				messageTypeAux="Control"
				messageIPorg=IPhostOrgConNet
				messageIPdest=IPhostDestConNet
				messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
				self.QSLAagent.SendMessageAgent(messageAuxChar)
				#print(messageAuxChar)
				
				if (NumCalcCenterMass>1):
					# With three values of frequency test for network synchronization
					messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),str(NumRequestedQubitsSend),str(NumRunsPerCenterMass),str(iCenterMass),str(iNumRunsPerCenterMass),str(SynchFreqPRUarrayTest[0]),str(SynchFreqPRUarrayTest[1]),str(SynchFreqPRUarrayTest[2]),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1]),str(QuadChEmtDet[0])])
				else:
					# With one value of frequency test for network synchronization
					messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),str(NumRequestedQubitsSend),str(NumRunsPerCenterMass),str(iCenterMass),str(iNumRunsPerCenterMass),str(SynchFreqPRUarrayTest[0]),str(PeriodSignalHistVal),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1]),str(QuadChEmtDet[0])])
				
				messageCommandAux="SimulateSendSynchQubits"
				messageTypeAux="Control"
				messageIPorg=IPhostOrgOpNet
				messageIPdest=IPhostDestOpNet
				messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
				self.QSLAagent.SendMessageAgent(messageAuxChar)
				#print(messageAuxChar)
				time.sleep(sSynchProcIterRunsTimePoint)# Give time between iterations to send and receive qubits
			
	## Methods to retrieve information from the nodes, so they also have to be blocked (from the above agent)
	def SimulateRetrieveNumStoredQubitsNode(self,IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray): # Supposing that node has received quBits, make use of them
		self.QSLAagent.SimulateRetrieveNumStoredQubitsNode(IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray)
	
	def SimulateRetrieveSynchParamsNode(self,IPhostReply,IPhostRequest,ParamsFloatArray): # Supposing that node has received quBits, retrieve the synch parameters computed
		self.QSLAagent.SimulateRetrieveSynchParamsNode(IPhostReply,IPhostRequest,ParamsFloatArray)
	
	def __del__(self): # Destructor of the class
		del self.QSLAagent
        	
