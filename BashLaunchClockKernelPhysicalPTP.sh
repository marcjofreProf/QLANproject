trap "kill 0" EXIT
echo 'Running PTP'
sudo /etc/init.d/rsyslog stop # stop logging
sudo systemctl start systemd-timesyncd # start system synch
sudo timedatectl set-ntp false
sudo ./linuxptp/ptp4l -i eth0 & #-f PTP4lConfigQLANprojectMaster.cfg &
sudo ./linuxptp/phc2sys -c eth0 -s CLOCK_REALTIME -w & #-f PTP2pcConfigQLANprojectSlave.cfg &
echo 'Enabling BBB pins'
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
sudo ./BBBclockKernelPhysical/BBBclockKernelPhysicalDaemon
sudo timedatectl set-ntp true # Start NTP
echo 'Stopped PTP'
#sudo /etc/init.d/rsyslog start # start logging
# Kill all the launched processes with same group PID
#kill -INT $$
