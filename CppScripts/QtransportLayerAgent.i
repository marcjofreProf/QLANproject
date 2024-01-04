/* Author: Marc Jofre */

%module QtransportLayerAgent

%{
#define SWIG_FILE_WITH_INIT
#include "QtransportLayerAgentH.h"
#include <thread>
#include <atomic>
%}
%include "numpy.i"
%init %{
import_array();
%}
%apply (int* IN_ARRAY1, int DIM1) {(int* ParamsIntArray, int nIntarray)};// Follow the structure in c++ arguments
%include "QtransportLayerAgentH.h"
using namespace nsQtransportLayerAgentH;
