# Just launching a PTP master

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
  # Kill potentially previously running processes
  sudo pkill -f ptp4l
  sudo pkill -f phc2sys
  sudo pkill -f QtransportLayerAgentN
  sudo pkill -f BBBclockKernelPhysicalDaemon
  
  sudo systemctl enable --now systemd-timesyncd # start system synch
  sudo systemctl start systemd-timesyncd # start system synch
  sudo systemctl enable systemd-timedated
  sudo systemctl start systemd-timedated
  sudo systemctl daemon-reload
  sudo timedatectl set-ntp true # Start NTP
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

pidAux=$(pidof -s ptp0)
sudo renice -n $NicenestPriorValue $pidAux

sudo /etc/init.d/rsyslog stop # stop logging

# Get the current time in seconds and nanoseconds
#current_time=$(date +%s)
#current_nano=$(date +%N)
#sudo phc_ctl /dev/ptp0 set $current_time # $current_nano # if the initial phc2sys offset is really huge. Then, run "sudo phc_ctl /dev/ptp0 set" before starting the ptp4l service, so that it has an initial time based on the RTC taht is "in the ballpark" and and set "step_threshold" at least or below to 0.00002 in the config file so that it can jump to converge

# Configure SYSTEM CLOCKS: CLOCK_REALTIME and CLOCK_TAI
# utc_offset should be 37, but seems that some slaves do not acquire it propperly, so set to zero (so TAI and UTC time will be the same)
#sudo pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
#        clockAccuracy 0xfe offsetScaledLogVariance 0xffff \
#        currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
#        ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 \
#        timeSource 0xa0"

# Maybe since systemd-timesyncd is disabled, then maybe adjtimex might update some needed parameters such as the difference between UTC and TAI clocks
#sudo adjtimex --print # Print something to make sure that adjtimex is installed (sudo apt-get update; sudo apt-get install adjtimex
#sudo adjtimex ...# manually make sure to adjust the conversion from utc to tai and viceversa

sudo nice -n $NicenestPriorValue ./linuxptp/ptp4l -i eth0 -H -f PTP4lConfigQLANprojectMaster.cfg -m & #-m

### If at least the grand master is synch to NTP ((good long stability reference - but short time less stable)) - difficult then to converge because also following NTP
#sudo systemctl enable systemd-timesyncd # start system synch
#sudo systemctl start systemd-timesyncd # start system synch
#sudo systemctl enable systemd-timedated
#sudo systemctl start systemd-timedated
#sudo systemctl daemon-reload
#sudo timedatectl set-ntp true # Start NTP
#sudo nice -n $NicenestPriorValue ./linuxptp/phc2sys -s CLOCK_REALTIME -c eth0 -w -f PTP4lConfigQLANprojectMaster.cfg -m & #-f PTP4lConfigQLANprojectMaster.cfg & -m


## If synch to the RTC of the system, stop the NTP. The quality of the internal crystal/clock matters
sudo timedatectl set-ntp false
sudo systemctl stop systemd-timesyncd # stop system synch
sudo systemctl disable systemd-timesyncd # start system synch
sudo systemctl stop systemd-timedated
sudo systemctl disable systemd-timedated
sudo nice -n $NicenestPriorValue ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -f PTP4lConfigQLANprojectMaster.cfg -m & #-f PTP2pcConfigQLANprojectMaster.cfg & -m

# adjust kernel clock (also known as system clock) to hardware clock (also known as cmos clock)
#sleep 30 # give time to time protocols to lock
sudo adjtimex -f 0 #-a --force-adjust # -f 0 

if ! sudo crontab -l > /dev/null 2>&1; then
    sudo crontab -e
fi

line_to_check="adjtimex"
line_to_add="30 * * * * sudo /sbin/adjtimex -f 0" #-a --force-adjust" # -f 0"

sudo crontab -l | grep -q "$line_to_check"

if [ $? -eq 0 ]; then
  sudo crontab -l | grep -v "$line_to_check" | sudo crontab -
fi

echo "$line_to_add" | sudo crontab -

## Update process priority values
pidAux=$(pidof -s ptp0)
sudo chrt -f -p $PriorityValue $pidAux
pidAux=$(pidof -s ptp4l)
sudo chrt -f -p $PriorityValue $pidAux
pidAux=$(pidof -s phc2sys)
sudo chrt -f -p $PriorityValue $pidAux
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

