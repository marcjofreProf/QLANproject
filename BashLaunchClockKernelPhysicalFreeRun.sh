# Single board generating clock at 1pps (1 Hz) in free run mode (no correction from time synchronization protocols)
# Parameters to pass
# arg1: Daemon ticks to fine adust to required Frequency: For example, 0.0 for 1pps, or -99900000 for 1 KHz. Defined double, but it has to be small in order to not produce negative half periods (defined as unsigned int)
# arg2: Daemon average filter, median filter or mean window. For example (odd number): 5. 1<= arg2 <= 50. Probably, only larger if there is too much jitter.
# arg3: Daemon print PID values: true or false

# Define default values (access by position)
default_arg1="0.0"
default_arg2="1"
default_arg3="false"

# Stop superflous processes to stop
sudo systemctl stop nginx
sudo systemctl stop wpa_supplicant
sudo systemctl stop avahi-daemon
pidAux=$(pgrep -f "systemd-journal")
sudo kill $pidAux
sudo systemctl stop haveged

## https://ubuntu.com/blog/real-time-kernel-tuning
#sudo sysctl kernel.sched_rt_runtime_us=-1
#sudo sysctl kernel.timer_migration=0

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
NicenestPriorValue=-10 # The smaller, the better
PriorityValue=75 # The larger, the better. Above 60 is well enough
PriorityNoSoHighValue=50 # The larger, the better.

# Check if watchdog is installed using dpkg
if dpkg -l | grep -q watchdog; then
    echo "watchdog is installed."
    sudo systemctl enable --now watchdog
else
    echo "whatchdog is not installed. sudo apt watchdog and configure it"
fi

# Check if adjtimex is installed using dpkg
if dpkg -l | grep -q adjtimex; then
    echo "adjtimex is installed."
    sudo adjtimex -f 0 # Reset any adjtimex previous configuration
else
    echo "adjtimex is not installed. sudo apt-get install adjtimex."
fi

cleanup_on_SIGINT() {
  echo "** Trapped SIGINT (Ctrl+C)! Cleaning up..."
  if [[ $is_rt_kernel -eq 0 ]]; then
	  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip7/pwm-7\:0/enable"
	  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip4/pwm-4\:0/enable"
	  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip1/pwm-1\:0/enable" 
  fi
  # Kill potentially previously running processes
  sudo pkill -f ptp4l
  sudo pkill -f phc2sys
  sudo pkill -f QtransportLayerAgentN
  sudo pkill -f BBBclockKernelPhysicalDaemon
  
  sudo systemctl enable --now systemd-timesyncd # enable system synch
  sudo systemctl start systemd-timesyncd # start system synch
  sudo systemctl daemon-reload
  sudo timedatectl set-ntp true # Start NTP
  
  echo 'Stopped PTP'
  exit 0
}

trap cleanup_on_SIGINT SIGINT
trap "kill 0" EXIT
echo 'Free Running'
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
  pidAux=$(pgrep -f "ksoftirqd/0")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/25-rtc0")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/26-rtc0")
  sudo renice -n $NicenestPriorValue $pidAux
  # Specific to the TI AM335
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

sudo /etc/init.d/rsyslog stop # stop logging
# Get the current time in seconds and nanoseconds
#current_time=$(date +%s)
#current_nano=$(date +%N)
#sudo phc_ctl /dev/ptp0 set $current_time # $current_nano # if the initial phc2sys offset is really huge. Then, run "sudo phc_ctl /dev/ptp0 set" before starting the ptp4l service, so that it has an initial time based on the RTC that is "in the ballpark" and and set "step_threshold" at least or below to 0.00002 in the config file so that it can jump to converge

sudo timedatectl set-ntp false
sudo systemctl stop systemd-timesyncd # stop system synch
sudo systemctl disable systemd-timesyncd # disable system synch
sudo systemctl stop systemd-timedated
sudo systemctl disable systemd-timedated

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

# adjust kernel clock (also known as system clock) to hardware clock (also known as cmos clock)
#sleep 30 # give time to time protocols to lock
sudo adjtimex -f 0 #-a --force-adjust # -f 0

if ! sudo crontab -l > /dev/null 2>&1; then
    sudo crontab -e
fi

line_to_check="adjtimex"
line_to_add="30 * * * * sudo /sbin/adjtimex -f 0" #-a --force-adjust" # -f 0

sudo crontab -l | grep -q "$line_to_check"

if [ $? -eq 0 ]; then
  sudo crontab -l | grep -v "$line_to_check" | sudo crontab -
fi

echo "$line_to_add" | sudo crontab -

##

BcKPDarg1=${1:-$default_arg1}
BcKPDarg2=${2:-$default_arg2}
BcKPDarg3=${3:-$default_arg3}
sudo nice -n $NicenestPriorValue ./BBBclockKernelPhysical/BBBclockKernelPhysicalDaemon $BcKPDarg1 $BcKPDarg2 $BcKPDarg3 &

## Update process priority values
if [[ $is_rt_kernel -eq 1 ]]; then
  pidAux=$(pgrep -f "ksoftirqd/0")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/25-rtc0")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/26-rtc0")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/59-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/60-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/61-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/62-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/63-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/64-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/65-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  pidAux=$(pgrep -f "irq/66-pruss_ev")
  sudo chrt -f -p $PriorityValue $pidAux
  #pidAux=$(pgrep -f "irq/41-4a100000")
  #sudo chrt -f -p $PriorityValue $pidAux
  #pidAux=$(pgrep -f "irq/42-4a100000")
  #sudo chrt -f -p $PriorityValue $pidAux
  #pidAux=$(pgrep -f "irq/43-4a100000")
  #sudo chrt -f -p $PriorityValue $pidAux
fi

pidAux=$(pgrep -f "BBBclockKernelPhysicalDaemon")
sudo chrt -f -p $PriorityNoSoHighValue $pidAux

# Maybe using adjtimex is bad idea because it is an extra layer not controlled by synchronization protocols
## Once priorities have been set, hence synch-protocols fine adjusted, adjust kernel clock (also known as system clock) to hardware clock (also known as cmos clock)
sleep 60 # give time to time protocols to lock
sudo adjtimex -a --force-adjust #-a --force-adjust # -f 0#

if ! sudo crontab -l > /dev/null 2>&1; then
    sudo crontab -e
fi

line_to_check="adjtimex"
line_to_add="30 * * * * sudo /sbin/adjtimex -a --force-adjust" #-a --force-adjust" #-f 0"

sudo crontab -l | grep -q "$line_to_check"

if [ $? -eq 0 ]; then
  sudo crontab -l | grep -v "$line_to_check" | sudo crontab -
fi

echo "$line_to_add" | sudo crontab -

read -r -p "Press Ctrl+C to kill launched processes
" # Block operation until Ctrl+C is pressed


