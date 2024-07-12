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

cleanup_on_SIGINT() {
  echo "** Trapped SIGINT (Ctrl+C)! Cleaning up..."
  # Kill potentially previously running processes
  sudo pkill -f ptp4l
  sudo pkill -f phc2sys
  sudo pkill -f QtransportLayerAgentN
  sudo pkill -f BBBclockKernelPhysicalDaemon
  
  sudo systemctl enable --now systemd-timesyncd # start system synch
  sudo systemctl start systemd-timesyncd # start system synch
  sudo systemctl daemon-reload
  sudo timedatectl set-ntp true # Start NTP
  sudo hwclock --systohc
  echo 'Stopped PTP'
  exit 0
}

trap cleanup_on_SIGINT SIGINT
trap "kill 0" EXIT
echo 'Running PTP'
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

pidAux=$(pidof -s ptp0)
sudo chrt -f -p 1 $pidAux
sudo renice -n $NicenestPriorValue $pidAux

sudo /etc/init.d/rsyslog stop # stop logging
# Get the current time in seconds and nanoseconds
#current_time=$(date +%s)
#current_nano=$(date +%N)
#sudo phc_ctl /dev/ptp0 set $current_time # $current_nano # if the initial phc2sys offset is really huge. Then, run "sudo phc_ctl /dev/ptp0 set" before starting the ptp4l service, so that it has an initial time based on the RTC that is "in the ballpark" and and set "step_threshold" at least or below to 0.00002 in the config file so that it can jump to converge
sudo hwclock --systohc

# Configure SYSTEM CLOCKS: CLOCK_REALTIME and CLOCK_TAI
# utc_offset should be 37, but seems that some slaves do not acquire it propperly, so set to zero (so TAI and UTC time will be the same)
sudo pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
        clockAccuracy 0xfe offsetScaledLogVariance 0xffff \
        currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
        ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 \
        timeSource 0xa0"

sudo timedatectl set-ntp false
sudo systemctl stop systemd-timesyncd # stop system synch
sudo systemctl disable systemd-timesyncd # disable system synch
# Maybe since systemd-timesyncd is disabled, then maybe adjtimex might update some needed parameters such as the difference between UTC and TAI clocks
#sudo adjtimex --print # Print something to make sure that adjtimex is installed (sudo apt-get update; sudo apt-get install adjtimex
# 	If ethtool not installed then the utc and tai offsets are not well configured 
#sudo adjtimex ...# manually make sure to adjust the conversion from utc to tai and viceversa
sudo nice -n $NicenestPriorValue ./linuxptp/ptp4l -i eth0 -s -H -f PTP4lConfigQLANprojectSlave.cfg &
pidAux=$(pgrep -f "ptp4l")
sudo chrt -f -p 1 $pidAux
sudo nice -n $NicenestPriorValue ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -f PTP4lConfigQLANprojectSlave.cfg & # -w -f PTP2pcConfigQLANprojectSlave.cfg & # -m # Important to launch phc2sys first (not in slave)
pidAux=$(pgrep -f "phc2sys")
sudo chrt -f -p 1 $pidAux

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
sudo nice -n $NicenestPriorValue ./CppScripts/QtransportLayerAgentN server 10.0.0.253 10.0.0.3 & #192.168.9.2 192.168.9.1 &
pidAux=$(pgrep -f "QtransportLayerAgentN")
sudo chrt -f -p 1 $pidAux

read -r -p "Press Ctrl+C to kill launched processes
" # Block operation until Ctrl+C is pressed

