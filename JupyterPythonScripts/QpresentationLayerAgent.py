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

class QPLA:
	def __init__(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Constructor of this class
        	self.QSLAagent = QsessionLayerAgent.QSLA(ParamsDescendingCharArray,ParamsAscendingCharArray) # Create instance of the Agent below
        
        ##############################################################
	# Methods
	def ListCharArrayParser(self,ListCharArrayAux):
	    # Actually concatenating a python list of strings to a single string
	    ParsedCharArrayAux=",".join(ListCharArrayAux)+","
	    return ParsedCharArrayAux
 	
	def InitAgentProcess(self,): # Pass to the below agent
		self.QSLAagent.InitAgentProcess()
	
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QSLAagent.SendMessageAgent(ParamsDescendingCharArray)
	
	def RequestQubitsHost(self,IPhostDestOpNet,IPhostOrgOpNet,IPhostDestConNet,IPhostOrgConNet,NumRequestedQubits): # Request that host's node sends qubits to this host's node		
		messagePayloadAux=str(NumRequestedQubits)
		messageCommandAux="ReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPhostDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=str(NumRequestedQubits)
		messageCommandAux="SendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgOpNet
		messageIPdest=IPhostDestOpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		#ParamsIntArray = np.zeros(1, dtype=np.intc)
		#self.QSLAagent.RetrieveNumStoredQubitsNode(ParamsIntArray)# (although redundant maybe) run it because it also executes the thread join in the receiving Node
	
	def RetrieveNumStoredQubitsNode(self,ParamsIntArray): # Supposing that node has received quBits, make use of them
		self.QSLAagent.RetrieveNumStoredQubitsNode(ParamsIntArray)
