swig -c++ -python QtransportLayerAgent.i
gcc -O2 -fPIC -c QtransportLayerAgentH.cpp -I/usr/local/lib/python3.10/dist-packages/numpy/core/include
gcc -O2 -fPIC -c QtransportLayerAgent_wrap.cxx -I/usr/include/python3.10 -I/usr/local/lib/python3.10/dist-packages/numpy/core/include
g++ -shared -pthread QtransportLayerAgentH.o QtransportLayerAgent_wrap.o -o _QtransportLayerAgent.so -I/usr/local/lib/python3.10/dist-packages/numpy/core/include 
