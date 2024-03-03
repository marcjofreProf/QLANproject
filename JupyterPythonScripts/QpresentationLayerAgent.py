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

import sys, os, signal
import time
import numpy as np

import QsessionLayerAgent

def signal_handler(signal_received, frame):
    # This function will be called when SIGINT or SIGPIPE is received
    print(f'Signal {signal_received} received, exiting gracefully.')
    # Perform any cleanup here
    sys.exit(0)

# Set up signal handlers
signal.signal(signal.SIGINT, signal_handler)  # Handle Ctrl+C
signal.signal(signal.SIGPIPE, signal_handler) # Handle broken pipe errors
signal.signal(signal.SIGSEGV, signal_handler) # Handle broken pipe errors

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
	
	def SimulateRequestQubitsHost(self,IPhostDestOpNet,IPhostOrgOpNet,IPhostDestConNet,IPhostOrgConNet,NumRequestedQubits): # Request that host's node sends qubits to this host's node		
		messagePayloadAux=str(NumRequestedQubits)
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPhostDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=str(NumRequestedQubits)
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgOpNet
		messageIPdest=IPhostDestOpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)	
	
	def SimulateSendEntangledQubitsHost(self,IPhostDest1OpNet,IPhostOrg1OpNet,IPhostDest2OpNet,IPhostOrg2OpNet,IPnodeDestConNet,IPhostOrgConNet,NumSendQubits): # Request that the other nodes of the specified hosts get ready to receive entangled qubits from the dealer's node		
		messagePayloadAux=str(NumSendQubits)
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg2OpNet
		messageIPdest=IPhostDest2OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)		
		messagePayloadAux=str(NumSendQubits)
		messageCommandAux="SimulateReceiveQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrg1OpNet
		messageIPdest=IPhostDest1OpNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		messagePayloadAux=str(NumSendQubits)
		messageCommandAux="SimulateSendQubits"
		messageTypeAux="Control"
		messageIPorg=IPhostOrgConNet
		messageIPdest=IPnodeDestConNet
		messageAuxChar = self.ListCharArrayParser([messageIPdest,messageIPorg,messageTypeAux,messageCommandAux,messagePayloadAux])
		self.QSLAagent.SendMessageAgent(messageAuxChar)
		
	def SimulateRetrieveNumStoredQubitsNode(self,IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray): # Supposing that node has received quBits, make use of them
		self.QSLAagent.SimulateRetrieveNumStoredQubitsNode(IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray)
