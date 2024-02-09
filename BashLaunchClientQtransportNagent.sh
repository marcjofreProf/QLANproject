sudo timedatectl set-ntp false
sudo ./linuxptp/ptp4l -i eth0 -m &
sudo ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -m &
sudo ./CppScripts/QtransportLayerAgentN client 192.168.8.2 192.168.8.1 
# Kill all the launched processes with same group PID
kill -- -"$$"
