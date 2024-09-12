# The Python folder in /usr/include/pythonX.XX (and specifically the file Python.h) has to exist.
# Install Python3.10 libraries as:
# sudo apt install software-properties-common -y
# sudo add-apt-repository ppa:deadsnakes/ppa
# sudo apt-get install libpython3.10-dev
# Otherwise, it is good to copy the compressed folder python3.10 to the location /usr/include/ as "sudo tar -xvzf python3_10.tar.gz -C /usr/include/python3.10 (first create python3.10 directory "sudo mkdir python3.10")
swig -c++ -python QtransportLayerAgent.i
gcc -O2 -fPIC -c QtransportLayerAgentH.cpp -I/usr/local/lib/python3.10/dist-packages/numpy/core/include
gcc -O2 -fPIC -c QtransportLayerAgent_wrap.cxx -I/usr/include/python3.10 -I/usr/local/lib/python3.10/dist-packages/numpy/core/include
g++ -shared -pthread QtransportLayerAgentH.o QtransportLayerAgent_wrap.o -o _QtransportLayerAgent.so -I/usr/local/lib/python3.10/dist-packages/numpy/core/include 
