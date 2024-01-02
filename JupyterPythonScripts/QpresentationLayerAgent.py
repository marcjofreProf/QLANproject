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

import QsessionLayerAgent

class QPLA:
	def __init__(self,ParamsDescendingCharArray,ParamsAscendingCharArray): # Constructor of this class
        	self.QSLAagent = QsessionLayerAgent.QSLA(ParamsDescendingCharArray,ParamsAscendingCharArray) # Create instance of the Agent below
 
	def InitAgentProcess(self,): # Pass to the below agent
		self.QSLAagent.InitAgentProcess()
	
	def SendMessageAgent(self,ParamsDescendingCharArray): # Send message to the below Agent
		self.QSLAagent.SendMessageAgent(ParamsDescendingCharArray)
