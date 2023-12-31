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

