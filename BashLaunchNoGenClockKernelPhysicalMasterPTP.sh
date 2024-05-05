# Just launching a PTP master
trap "kill 0" EXIT
echo 'Running PTP'

sudo /etc/init.d/rsyslog stop # stop logging

## If at least the grand master is synch to NTP (good reference)
sudo systemctl enable systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true # Start NTP
sudo ./linuxptp/ptp4l -i eth0 & #-f PTP4lConfigQLANproject.cfg & #-m
sudo ./linuxptp/phc2sys -s CLOCK_REALTIME -c eth0 -w -m #& #-f PTP2pcConfigQLANprojectSlave.cfg & -m

## If synch to the RTC of the system, stop the NTP. The quality of the internal crystal/clock matters
#sudo timedatectl set-ntp false
#sudo systemctl stop systemd-timesyncd # stop system synch
#sudo systemctl disable systemd-timesyncd # start system synch
#sudo ./linuxptp/ptp4l -i eth0 -m #& #-f PTP4lConfigQLANproject.cfg & #-m
##sudo ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -m #& #-f PTP2pcConfigQLANprojectSlave.cfg & -m

sudo hwclock --systohc

sudo systemctl enable --now systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true # Start NTP
echo 'Stopped PTP'
#sudo /etc/init.d/rsyslog start # start logging
# Kill all the launched processes with same group PID
#kill -INT $$
