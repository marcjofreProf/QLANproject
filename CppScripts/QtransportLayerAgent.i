/* Author: Marc Jofre */

%module QtransportLayerAgent

%{
#define SWIG_FILE_WITH_INIT
#include "QtransportLayerAgentH.h"
#include <pthread.h>
%}

%include "QtransportLayerAgentH.h"
using namespace nsQtransportLayerAgentH;
