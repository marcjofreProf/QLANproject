/* Author: Marc Jofre */

%module QtransportLayerAgent

%{
#define SWIG_FILE_WITH_INIT
#include "QtransportLayerAgentH.h"
%}

%include "QtransportLayerAgentH.h" // Instead of including everything, only include the functions needed
using namespace nsQtransportLayerAgentH;
//int QTLAH::InitAgent(char* ParamsDescendingCharArray,char* ParamsAscendingCharArray);
