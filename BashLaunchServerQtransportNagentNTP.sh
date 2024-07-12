# Function to check for real-time kernel
is_rt_kernel() {
  kernel_version=$(uname -r) # Get the kernel version
  if [[ $kernel_version =~ [[:alnum:]]*ti-rt[[:alnum:]]* ]]; then
    echo "Real-time kernel detected"
    return 1  # Real-time kernel detected (return 0)
  else
    echo "Non-real-time kernel detected"
    return 0  # Non-real-time kernel detected (return 1)
  fi
}

# Check for real-time kernel
is_rt_kernel
# Set variable based on function return value
is_rt_kernel=$?  # $? stores the exit code of the last command (function)

# Nicenest value [-20, 20]
NicenestPriorValue=-10

cleanup_on_SIGINT() {
  echo "** Trapped SIGINT (Ctrl+C)! Cleaning up..."
  # Kill potentially previously running processes
  sudo pkill -f ptp4l
  sudo pkill -f phc2sys
  sudo pkill -f QtransportLayerAgentN
  sudo pkill -f BBBclockKernelPhysicalDaemon
  
  echo 'Stopped'
  exit 0
}

trap cleanup_on_SIGINT SIGINT
trap "kill 0" EXIT
echo 'Running NTP'
# Kill non-wanted processes
sudo pkill -f nodejs # javascript applications
# Kill potentially previously running PTP clock processes and processes
sudo pkill -f ptp4l
sudo pkill -f phc2sys
sudo pkill -f QtransportLayerAgentN
sudo pkill -f BBBclockKernelPhysicalDaemon
sleep 1 # wait 1 second to make sure to kill the old processes
########################################################
# Set realtime priority with chrt -f and priority 0
########################################################
if [[ $is_rt_kernel -eq 1 ]]; then
  pidAux=$(pgrep -f "irq/22-TI-am335")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/22-s-TI-am3")
  sudo renice -n $NicenestPriorValue $pidAux
  
  pidAux=$(pgrep -f "irq/59-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/60-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/61-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/62-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/63-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/64-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/65-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/66-pruss_ev")
  sudo renice -n $NicenestPriorValue $pidAux
else
  pidAux=$(pgrep -f "irq/66-TI-am335")
  #sudo chrt -f -p 1 $pidAux
  sudo renice -n $NicenestPriorValue $pidAux
fi
########################################################
sudo /etc/init.d/rsyslog stop # stop logging
sudo systemctl enable --now systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true # Start NTP
sudo hwclock --systohc
if [[ $is_rt_kernel -eq 0 ]]; then
	echo 'Enabling PRU pins'
	sudo config-pin P9_28 pruin
	sudo config-pin P9_29 pruin
	sudo config-pin P9_30 pruin
	sudo config-pin P9_31 pruin
	sudo config-pin P8_15 pruin
	sudo config-pin P8_16 pruin
	sudo config-pin P9_25 pruin
	sudo config-pin P9_27 pruin
	sudo config-pin P9_41 pruin
	sudo config-pin P9_91 pruin
	sudo config-pin P9_92 pruin
	sudo config-pin P8_27 pruout
	sudo config-pin P8_28 pruout
	sudo config-pin P8_29 pruout
	sudo config-pin P8_30 pruout
	sudo config-pin P8_39 pruout
	sudo config-pin P8_40 pruout
	sudo config-pin P8_41 pruout
	sudo config-pin P8_42 pruout
	sudo config-pin P8_43 pruout
	sudo config-pin P8_44 pruout
	sudo config-pin P8_45 pruout
	sudo config-pin P8_46 pruout
fi
sudo ./CppScripts/QtransportLayerAgentN server 10.0.0.253 10.0.0.3 & #192.168.9.2 192.168.9.1 &
pidAux=$(pgrep -f "QtransportLayerAgentN")
sudo chrt -f -p 1 $pidAux

read -r -p "Press Ctrl+C to kill launched processes
" # Block operation until Ctrl+C is pressed

