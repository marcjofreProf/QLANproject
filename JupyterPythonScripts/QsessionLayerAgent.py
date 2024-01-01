#####################################################
# Author: Marc Jofre
# Script Quantum LAN session layer
#####################################################

import os, sys
pathScriptBelowAgentScript='./../CppScripts/'
sys.path.append(pathScriptBelowAgentScript)

import QtransportLayerAgent

class QSLA:
	def __init__(self,): # Constructor of this class
        	self.QTLAagent = QtransportLayerAgent.QTLAH(0) # Create instance of the Agent below
 
	def InitAgent(self,ParamsDescendingCharArray,ParamsAscendingCharArray):# Initialize
		self.QTLAagent.InitAgent(ParamsDescendingCharArray,ParamsAscendingCharArray) # Initialize below Agent
	
	def InitAgentProcess(self,): # Pass to the below agent
		self.QTLAagent.InitAgentProcess()
	
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QTLAagent.SendMessageAgent(ParamsDescendingCharArray)

