#####################################################
# Author: Marc Jofre
# Script Quantum LAN presentation layer
#####################################################

import sys, os

import QsessionLayerAgent

class QPLA:
	def __init__(self,): # Constructor of this class
        	self.QSLAagent = QsessionLayerAgent.QSLA() # Create instance of the Agent below
 
	def InitAgent(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Initialize this Agent and below Agent
		self.QSLAagent.InitAgent(ParamsDescendingCharArray,ParamsAscendingCharArray) # Initialize below Agent

