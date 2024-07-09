# Parameters to pass
# arg1: Daemon ticks to fine adust to required Frequency: For example, 0.0 for 1pps, or -99900000 for 1 KHz. Defined double, but it has to be small in order to not produce negative half periods (defined as unsigned int)
# arg2: Daemon average filter, median filter or mean window. For example (odd number): 5. 1<= arg2 <= 50. Probably, only larger if there is too much jitter.
# arg3: Daemon print PID values: true or false

# Define default values (access by position)
default_arg1="0.0"
default_arg2="1"
default_arg3="false"

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
  if [[ $is_rt_kernel -eq 1 ]]; then
	  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip7/pwm-7\:0/enable"
	  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip4/pwm-4\:0/enable"
	  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip1/pwm-1\:0/enable"
  fi
  # Kill potentially previously running processes
  sudo pkill -f ptp4l
  sudo pkill -f phc2sys
  sudo pkill -f QtransportLayerAgentN
  sudo pkill -f BBBclockKernelPhysicalDaemon
  
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
# Set realtime priority with chrt -f and priority 1
########################################################
if [[ $is_rt_kernel -eq 1 ]]; then
  pidAux=$(pgrep -f "irq/22-TI-am335")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/22-s-TI-am3")
  sudo renice -n -20 $pidAux
  
  pidAux=$(pgrep -f "irq/59-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/60-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/61-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/62-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/63-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/64-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/65-pruss_ev")
  sudo renice -n -20 $pidAux
  pidAux=$(pgrep -f "irq/66-pruss_ev")
  sudo renice -n -20 $pidAux
else
  pidAux=$(pgrep -f "irq/66-TI-am335")
  #sudo chrt -f -p 1 $pidAux
  sudo renice -n -20 $pidAux
fi

########################################################
sudo /etc/init.d/rsyslog stop # stop logging
sudo systemctl enable --now systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true
sudo hwclock --systohc
#echo 'Enabling PWM for 24 MHz ref clock'
#sudo config-pin P8.19 pwm
#sudo sh -c "echo '38' >> /sys/class/pwm/pwmchip7/pwm-7\:0/period"
#sudo sh -c "echo '19' >> /sys/class/pwm/pwmchip7/pwm-7\:0/duty_cycle"
#sudo sh -c "echo '1' >> /sys/class/pwm/pwmchip7/pwm-7\:0/enable"
#echo 'Enabling PWM for 1 KHz ref clock'
#sudo config-pin P9.14 pwm 
#sudo sh -c "echo '1000000' >> /sys/class/pwm/pwmchip4/pwm-4\:0/period" 
#sudo sh -c "echo '5' >> /sys/class/pwm/pwmchip4/pwm-4\:0/duty_cycle" 
#sudo sh -c "echo '1' >> /sys/class/pwm/pwmchip4/pwm-4\:0/enable"
#echo 'Enabling PWM for 10 MHz ref clock'
#sudo config-pin P9.22 pwm 
#sudo sh -c "echo '100' >> /sys/class/pwm/pwmchip1/pwm-1\:0/period" 
#sudo sh -c "echo '50' >> /sys/class/pwm/pwmchip1/pwm-1\:0/duty_cycle" 
#sudo sh -c "echo '1' >> /sys/class/pwm/pwmchip1/pwm-1\:0/enable"
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
sudo ./BBBclockKernelPhysical/BBBclockKernelPhysicalDaemon ${1-default_arg1} ${2-default_arg2} ${3-default_arg3} &
pidAux=$(pgrep -f "BBBclockKernelPhysicalDaemon")
sudo chrt -f -p 1 $pidAux

read -r -p "Press Ctrl+C to kill launched processes
" # Block operation until Ctrl+C is pressed


