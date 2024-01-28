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
import numpy as np
pathScriptBelowAgentScript='./../CppScripts/'
sys.path.append(pathScriptBelowAgentScript)

import QtransportLayerAgent

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
	
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QTLAagent.SendMessageAgent(ParamsDescendingCharArray)
	
	def RetrieveNumStoredQubitsNode(self,ParamsIntArray): # Supposing that node has received quBits, make use of them
		self.QTLAagent.RetrieveNumStoredQubitsNode(ParamsIntArray)

