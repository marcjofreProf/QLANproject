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
pathScriptBelowAgentScript='./../CppScripts/'
sys.path.append(pathScriptBelowAgentScript)

import QtransportLayerAgent

class QSLA:
	def __init__(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Constructor of this class
        	self.QTLAagent = QtransportLayerAgent.QTLAH(0,ParamsDescendingCharArray,ParamsAscendingCharArray) # Create instance of the Agent below
 
	def InitAgentProcess(self,): # Pass to the below agent
		self.QTLAagent.InitAgentProcess()
	
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QTLAagent.SendMessageAgent(ParamsDescendingCharArray)

