swig -c++ -python QtransportLayerAgent.i
g++ -fPIC -c QtransportLayerAgent_wrap.cxx -I/usr/include/python3.10
g++ -shared QtransportLayerAgent_wrap.o -o _QtransportLayerAgent.so 
