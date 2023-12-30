#####################################################
# Author: Marc Jofre
# Script Quantum LAN session layer
#####################################################

import os, sys
pathScriptBelowAgentScript='./../CppScripts/'
sys.path.append(pathScriptBelowAgentScript)

import QtransportLayerAgent

def InitAgent(ParamsDescendingCharArray,ParamsAscendingCharArray):# Initialize this Agent and below Agent
	QTLAGagent = QtransportLayerAgent.QTLAH(0) # Initialize and instance
	QTLAGagent.InitAgent(ParamsDescendingCharArray,ParamsAscendingCharArray) # Initialize below Agent
