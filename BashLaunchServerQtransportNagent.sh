sudo timedatectl set-ntp false
sudo ./linuxptp/ptp4l -i eth0 &
sudo ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w &
sudo ./CppScripts/QtransportLayerAgentN server 192.168.9.2 192.168.9.1 
# Kill all the launched processes with same group PID
kill -- -"$$"
