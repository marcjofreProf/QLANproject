# Just launching a PTP slave
trap "kill 0" EXIT
echo 'Running PTP slave'

# Kill potentially previously running PTP clock processes
sudo pkill -f ptp4l
sudo pkill -f phc2sys
########################################################

sudo /etc/init.d/rsyslog stop # stop logging

sudo timedatectl set-ntp false
sudo systemctl stop systemd-timesyncd # stop system synch
sudo systemctl disable systemd-timesyncd # disable system synch
sudo ./linuxptp/ptp4l -i eth0 -s -f PTP4lConfigQLANprojectSlave.cfg &
sudo ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w -m #& # -f PTP2pcConfigQLANprojectSlave.cfg & # -m

sudo hwclock --systohc

sudo systemctl enable --now systemd-timesyncd # start system synch
sudo systemctl start systemd-timesyncd # start system synch
sudo systemctl daemon-reload
sudo timedatectl set-ntp true # Start NTP
echo 'Stopped PTP slave'
#sudo /etc/init.d/rsyslog start # start logging
# Kill all the launched processes with same group PID
#kill -INT $$
