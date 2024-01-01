swig -c++ -python QtransportLayerAgent.i
gcc -O2 -fPIC -c QtransportLayerAgentH.cpp  
gcc -O2 -fPIC -c QtransportLayerAgent_wrap.cxx -I/usr/include/python3.10
g++ -shared -pthread QtransportLayerAgentH.o QtransportLayerAgent_wrap.o -o _QtransportLayerAgent.so 
