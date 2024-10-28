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
# Notes:
# When using the Sublime Text editor, I was able to select the segment of my code that was giving me the inconsistent use of tabs and spaces in indentation error and select:
#
# Menu View → Indentation → convert indentation to spaces
#####################################################
import os, sys
import time, signal
import numpy as np
pathScriptBelowAgentScript='./../CppScripts/'
sys.path.append(pathScriptBelowAgentScript)

import QtransportLayerAgent

sTimeOutRequestQSLA=300.0 # Time to wait (in seconds) to proceed with a request before doing something

class QSLA:
    def __init__(self,ParamsDescendingCharArrayInitAux,ParamsAscendingCharArrayInitAux): # Constructor of this class        
            # save the parameters of initialization
            self.ParamsDescendingCharArrayInit=ParamsDescendingCharArrayInitAux
            self.ParamsAscendingCharArrayInit=ParamsAscendingCharArrayInitAux
            # Initialize the agent below
            self.InitAgentBelow()
    
    ##############################################################
    # Methods
    def QSLA_timeout_handler(signum, frame):
        raise TimeoutError("Execution timed out")
    
    def InitAgentBelow(self,):
        self.QTLAagent = QtransportLayerAgent.QTLAH(0,self.ParamsDescendingCharArrayInit,self.ParamsAscendingCharArrayInit) # Create instance of the Agent below
    
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
        signal.signal(signal.SIGALRM, timeout_handler)
        signal.alarm(sTimeOutRequestQSLA)  # Set the time out in seconds
        try: # Long-running operation
            self.QTLAagent.WaitUntilActiveActionFreePreLock(ParamsCharArrayArg,nChararray)
        except TimeoutError:
            print('QSLA::Execution time out. Restarting/Reconnecting QtransportLayerAgent agent.')
            self.__del__() # destruct QtransportLayerAgent agent
            # Restart/Reconnect QtransportLayerAgent agent
            self.InitAgentBelow()
            self.InitAgentProcess()
            self.QTLAagent.WaitUntilActiveActionFreePreLock(ParamsCharArrayArg,nChararray)

    def UnBlockActiveActionFreePreLock(self,ParamsCharArrayArg,nChararray):
        self.QTLAagent.UnBlockActiveActionFreePreLock(ParamsCharArrayArg,nChararray)
    
    ## Methods to retrieve information from the nodes or hosts
    def SimulateRetrieveNumStoredQubitsNode(self,IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray): # Supposing that node has received quBits, make use of them
        self.QTLAagent.SimulateRetrieveNumStoredQubitsNode(IPhostReply,IPhostRequest,ParamsIntArray,ParamsFloatArray)
    
    def SimulateRetrieveSynchParamsNode(self,IPhostReply,IPhostRequest,ParamsFloatArray): # Supposing that node has received quBits, retrieve the synch parameters computed
        self.QTLAagent.SimulateRetrieveSynchParamsNode(IPhostReply,IPhostRequest,ParamsFloatArray)
    
    def __del__(self): # Destructor of the class
        del self.QTLAagent
    
