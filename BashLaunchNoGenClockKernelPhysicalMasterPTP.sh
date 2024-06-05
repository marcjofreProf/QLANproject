# Just launching a PTP master
trap "kill 0" EXIT
echo 'Running PTP'

# Kill potentially previously running PTP clock processes
sudo pkill -f ptp4l
sudo pkill -f phc2sys
########################################################

sudo /etc/init.d/rsyslog stop # stop logging

## If at least the grand master is synch to NTP (good reference) - difficult then to converge because also following NTP
sudo systemctl enable systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true # Start NTP
# Get the current time in seconds and nanoseconds
#current_time=$(date +%s)
#current_nano=$(date +%N)
#sudo phc_ctl /dev/ptp0 set $current_time # $current_nano # if the initial phc2sys offset is really huge. Then, run "sudo phc_ctl /dev/ptp0 set" before starting the ptp4l service, so that it has an initial time based on the RTC taht is "in the ballpark" and and set "step_threshold" at least or below to 0.00002 in the config file so that it can jump to converge
sudo hwclock --systohc
sudo ./linuxptp/phc2sys -s CLOCK_REALTIME -c eth0 -w -m & #-f PTP2pcConfigQLANprojectSlave.cfg & -m
sudo ./linuxptp/ptp4l -i eth0 -f PTP4lConfigQLANproject.cfg -m #& #-m

## If synch to the RTC of the system, stop the NTP. The quality of the internal crystal/clock matters
#sudo timedatectl set-ntp false
#sudo systemctl stop systemd-timesyncd # stop system synch
#sudo systemctl disable systemd-timesyncd # start system synch
#sudo ./linuxptp/ptp4l -i eth0 -f PTP4lConfigQLANproject.cfg -m & #-m
##sudo ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -m #& #-f PTP2pcConfigQLANprojectSlave.cfg & -m

sudo systemctl enable --now systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true # Start NTP
echo 'Stopped PTP'
#sudo /etc/init.d/rsyslog start # start logging
# Kill all the launched processes with same group PID
#kill -INT $$
