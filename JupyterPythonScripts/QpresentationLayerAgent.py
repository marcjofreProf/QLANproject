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
  
msSynchProciterRunsTimePoint=10.0 # Time to wait (seconds) between iterations of the synch mechanisms to allow time to send and receive the necessary qubits

class QPLA:
	def __init__(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Constructor of this class
            self.QSLAagent = QsessionLayerAgent.QSLA(ParamsDescendingCharArray,ParamsAscendingCharArray) # Create instance of the Agent below
                
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
	
	def SimulateRequestQubitsHost(self,IPhostDestOpNet,IPhostOrgOpNet,IPhostDestConNet,IPhostOrgConNet,NumRequestedQubits,SynchPRUoffFreqVal): # Request that host's node sends qubits to this host's node		
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDestOpNet]),str(NumRequestedQubits)])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPhostDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),str(NumRequestedQubits),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgOpNet
		messageIPdest=IPhostDestOpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)	
	
	def SimulateSendEntangledQubitsHost(self,IPhostDest1OpNet,IPhostOrg1OpNet,IPhostDest2OpNet,IPhostOrg2OpNet,IPnodeDestConNet,IPhostOrgConNet,NumSendQubits,SynchPRUoffFreqVal): # Request that the other nodes of the specified hosts get ready to receive entangled qubits from the dealer's node
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostOrg1OpNet]),str(NumSendQubits)])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg2OpNet
		messageIPdest=IPhostDest2OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)		
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostDest2OpNet,IPhostOrg2OpNet]),str(NumSendQubits)])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg1OpNet
		messageIPdest=IPhostDest1OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),str(NumSendQubits),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPnodeDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		"""	
		Very weird, if emitting node tries to set the time barrier, then there is a lot of jitter in the detections???
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg2OpNet]),str(NumSendQubits)])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg2OpNet
		messageIPdest=IPhostDest2OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)		
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg1OpNet]),str(NumSendQubits)])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg1OpNet
		messageIPdest=IPhostDest1OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),str(NumSendQubits),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPnodeDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		"""
		
	def SimulateRequestMultipleNodesQubitsHost(self,IPhostDest1OpNet,IPhostOrg1OpNet,IPhostDest2OpNet,IPhostOrg2OpNet,IPnodeDestConNet,IPhostOrgConNet,NumSendQubits,SynchPRUoffFreqVal1,SynchPRUoffFreqVal2): # Request other nodes to send to this node qubits
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg2OpNet]),str(NumSendQubits),str(SynchPRUoffFreqVal2[0]),str(SynchPRUoffFreqVal2[1])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg2OpNet
		messageIPdest=IPhostDest2OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)		
		messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrg1OpNet]),str(NumSendQubits),str(SynchPRUoffFreqVal1[0]),str(SynchPRUoffFreqVal1[1])])
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg1OpNet
		messageIPdest=IPhostDest1OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDest1OpNet,IPhostDest2OpNet]),str(NumSendQubits)])
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPnodeDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
	
	def SimulateRequestSynchsHost(self,IPhostDestOpNet,IPhostOrgOpNet,IPhostDestConNet,IPhostOrgConNet,NumRunsPerCenterMass,SynchFreqPRUarrayTest,SynchPRUoffFreqVal): # Request that host's node sends qubits to this host's node
	NumCalcCenterMass=len(SynchFreqPRUarrayTest)
	for iCenterMass in range(0,NumCalcCenterMass,1):
		for iNumRunsPerCenterMass in range(0,NumRunsPerCenterMass,1):
		messagePayloadAux=self.SemiColonListCharArrayParser(["Active",self.UnderScoreListCharArrayParser([IPhostDestOpNet]),str(NumRunsPerCenterMass),str(iCenterMass),str(iNumRunsPerCenterMass),str(SynchFreqPRUarrayTest[0]),str(SynchFreqPRUarrayTest[1]),str(SynchFreqPRUarrayTest[2])])
			messageCommandAux="SimulateReceiveSynchQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrgConNet
			messageIPdest=IPhostDestConNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)
			messagePayloadAux=self.SemiColonListCharArrayParser(["Passive",self.UnderScoreListCharArrayParser([IPhostOrgOpNet]),str(NumRunsPerCenterMass),str(iCenterMass),str(iNumRunsPerCenterMass),str(SynchFreqPRUarrayTest[0]),str(SynchFreqPRUarrayTest[1]),str(SynchFreqPRUarrayTest[2]),str(SynchPRUoffFreqVal[0]),str(SynchPRUoffFreqVal[1])])
			messageCommandAux="SimulateSendSynchQubits"
			messageTypeAux="Control"
			messageIPorg=IPhostOrgOpNet
			messageIPdest=IPhostDestOpNet
			messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
			self.QSLAagent.SendMessageAgent(messageAuxChar)
			
			time.sleep(msSynchProciterRunsTimePoint)# Give time between iterations to send and receive qubits
	
	## Methods to retrieve information from the nodes or hosts
	def SimulateRetrieveNumStoredQubitsNode(self,IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray): # Supposing that node has received quBits, make use of them
		self.QSLAagent.SimulateRetrieveNumStoredQubitsNode(IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray)
	
	def SimulateRetrieveSynchParamsNode(self,IPhostReply,IPhostRequest,ParamsFloatArray): # Supposing that node has received quBits, retrieve the synch parameters computed
		self.QSLAagent.SimulateRetrieveSynchParamsNode(IPhostReply,IPhostRequest,ParamsFloatArray)
	
	def __del__(self): # Destructor of the class
		del self.QSLAagent
        	
