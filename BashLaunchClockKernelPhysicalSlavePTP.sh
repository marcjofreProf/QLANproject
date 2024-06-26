# Parameters to pass
# arg1: Daemon ticks to fine adust to required Frequency: For example, 0.0 for 1pps, or -99900000 for 1 KHz. Defined double, but it has to be small in order to not produce negative half periods (defined as unsigned int)
# arg2: Daemon average filter, median filter or mean window. For example (odd number): 5. 1<= arg2 <= 50. Probably, only larger if there is too much jitter.
# arg3: Daemon print PID values: true or false
cleanup_on_SIGINT() {
  echo "** Trapped SIGINT (Ctrl+C)! Cleaning up..."
  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip7/pwm-7\:0/enable"
  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip4/pwm-4\:0/enable"
  sudo sh -c "echo '0' >> /sys/class/pwm/pwmchip1/pwm-1\:0/enable" 
  
  sudo systemctl enable --now systemd-timesyncd # enable system synch
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
pidAux=$(pgrep -f "irq/66-TI-am335")
#sudo chrt -f -p 1 $pidAux
sudo renice -n -20 $pidAux

pidAux=$(pidof -s ptp0)
sudo chrt -f -p 1 $pidAux
sudo renice -n -20 $pidAux

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
#sudo adjtimex ...# manually make sure to adjust the conversion from utc to tai and viceversa
sudo nice -n -20 ./linuxptp/ptp4l -i eth0 -s -H -f PTP4lConfigQLANprojectSlave.cfg -m & #-m
pidAux=$(pgrep -f "ptp4l")
sudo chrt -f -p 1 $pidAux

sudo nice -n -20 ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -f PTP4lConfigQLANprojectSlave.cfg & # -w -f PTP2pcConfigQLANprojectSlave.cfg & # -m # Important to launch phc2sys first (not in slave)
pidAux=$(pgrep -f "phc2sys")
sudo chrt -f -p 1 $pidAux

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
sudo nice -n -20 ./BBBclockKernelPhysical/BBBclockKernelPhysicalDaemon $1 $2 $3 &
pidAux=$(pgrep -f "BBBclockKernelPhysicalDaemon")
sudo chrt -f -p 1 $pidAux

read -r -p "Press Ctrl+C to kill launched processes
" # Block operation until Ctrl+C is pressed

