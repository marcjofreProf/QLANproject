# Just launching a PTP slave
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
  echo 'Stopped PTP slave'
  exit 0
}

trap cleanup_on_SIGINT SIGINT
trap "kill 0" EXIT
echo 'Running PTP slave'
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
  pidAux=$(pgrep -f "irq/25-rtc0")
  sudo renice -n $NicenestPriorValue $pidAux
  pidAux=$(pgrep -f "irq/26-rtc0")
  sudo renice -n $NicenestPriorValue $pidAux
  # Specific to PTP
  pidAux=$(pgrep -f "ptp0")
  sudo renice -n $NicenestPriorValue $pidAux
  
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

# Configure SYSTEM CLOCKS: CLOCK_REALTIME and CLOCK_TAI
# utc_offset should be 37 with respect TAI
#sudo pmc -u -b 0 -t 1 "SET GRANDMASTER_SETTINGS_NP clockClass 248 \
#        clockAccuracy 0xfe offsetScaledLogVariance 0xffff \
#        currentUtcOffset 37 leap61 0 leap59 0 currentUtcOffsetValid 1 \
#        ptpTimescale 1 timeTraceable 1 frequencyTraceable 0 \
#        timeSource 0xa0"

sudo timedatectl set-ntp false
sudo systemctl stop systemd-timesyncd # stop system synch
sudo systemctl disable systemd-timesyncd # disable system synch
sudo systemctl stop systemd-timedated
sudo systemctl disable systemd-timedated
# Maybe since systemd-timesyncd is disabled, then maybe adjtimex might update some needed parameters such as the difference between UTC and TAI clocks
#sudo adjtimex --print # Print something to make sure that adjtimex is installed (sudo apt-get update; sudo apt-get install adjtimex
#sudo adjtimex ...# manually make sure to adjust the conversion from utc to tai and viceversa
sudo nice -n $NicenestPriorValue ./linuxptp/ptp4l -i eth0 -s -H -f PTP4lConfigQLANprojectSlave.cfg -m &
pidAux=$(pgrep -f "ptp4l")
sudo chrt -f -p 1 $pidAux
sudo nice -n $NicenestPriorValue ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -f PTP4lConfigQLANprojectSlave.cfg -m & # -w -f PTP2pcConfigQLANprojectSlave.cfg & # -m # Important to launch phc2sys first (not in slave)
pidAux=$(pgrep -f "phc2sys")
sudo chrt -f -p 1 $pidAux

# adjust kernel clock (also known as system clock) to hardware clock (also known as cmos clock)
sleep 30 # give time to time protocols to lock
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

##

read -r -p "Press Ctrl+C to kill launched processes
" # Block operation until Ctrl+C is pressed


