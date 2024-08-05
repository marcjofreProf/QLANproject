#####################################################
# Author: Prof. Marc Jofre
# Dept. Network Engineering
# Universitat Politècnica de Catalunya - Technical University of Catalonia
#
# Modified: 2024
# Created: 2024
#
# Script Quantum LAN session layer
#####################################################

import os, sys
import time
import numpy as np
pathScriptBelowAgentScript='./../CppScripts/'
sys.path.append(pathScriptBelowAgentScript)

import QtransportLayerAgent

sActiveActionProcTimePoint=5.0 # Time to wait (seconds) between iterations of the synch mechanisms to allow time to send and receive the necessary qubits

class QSLA:
	def __init__(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Constructor of this class
        	self.QTLAagent = QtransportLayerAgent.QTLAH(0,ParamsDescendingCharArray,ParamsAscendingCharArray) # Create instance of the Agent below
 	
 	##############################################################
	# Methods
	def ListCharArrayParser(self,ListCharArrayAux):
	    # Actually concatenating a python list of strings to a single string
	    ParsedCharArrayAux=",".join(ListCharArrayAux)+","
	    return ParsedCharArrayAux
	
	def InitAgentProcess(self,): # Pass to the below agent
		self.QTLAagent.InitAgentProcess()
	
	# Methods to the host
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QTLAagent.SendMessageAgent(ParamsDescendingCharArray)
	
	def WaitUntilActiveActionFreePreLock(self,ParamsCharArrayArg,nChararray):
		self.QTLAagent.WaitUntilActiveActionFreePreLock(ParamsCharArrayArg,nChararray)
	
	def UnBlockActiveActionFreePreLock(self,ParamsCharArrayArg,nChararray):
		#time.sleep(sActiveActionProcTimePoint)# Give time between iterations to send and receive qubits
		self.QTLAagent.UnBlockActiveActionFreePreLock(ParamsCharArrayArg,nChararray)
		#time.sleep(sActiveActionProcTimePoint)# Give time between iterations to send and receive qubits
	
	## Methods to retrieve information from the nodes or hosts
	def SimulateRetrieveNumStoredQubitsNode(self,IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray): # Supposing that node has received quBits, make use of them
		self.QTLAagent.SimulateRetrieveNumStoredQubitsNode(IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray)
	
	def SimulateRetrieveSynchParamsNode(self,IPhostReply,IPhostRequest,ParamsFloatArray): # Supposing that node has received quBits, retrieve the synch parameters computed
		self.QTLAagent.SimulateRetrieveSynchParamsNode(IPhostReply,IPhostRequest,ParamsFloatArray)
	
	def __del__(self): # Destructor of the class
		del self.QTLAagent

