# Parameters to pass
# arg1: Daemon ticks to fine adust to required Frequency: For example, 0.0 Defined duble, but it has to be small in order to not produce negative half periods (defined as unsigned int)
# arg2: Daemon PID proportional factor. For example: 0.0. 0<= arg2 <= 1.0. Probably, only larger than 0.0, if there is too much jitter.
# arg3: Daemon print PID values: true or false
trap "kill 0" EXIT
echo 'Running PTP'
sudo /etc/init.d/rsyslog stop # stop logging
sudo systemctl start systemd-timesyncd # start system synch
sudo timedatectl set-ntp false
sudo ./linuxptp/ptp4l -i eth0 -f PTP4lConfigQLANproject.cfg & #-m
sudo ./linuxptp/phc2sys -s eth0 -c CLOCK_REALTIME -w & #-f PTP2pcConfigQLANprojectSlave.cfg & -m
echo 'Enabling PWM for 24 MHz ref clock'
sudo config-pin P8.19 pwm
sudo sudo sh -c "echo '40' >> /sys/class/pwm/pwmchip7/pwm-7\:0/period"
sudo sudo sh -c "echo '20' >> /sys/class/pwm/pwmchip7/pwm-7\:0/duty_cycle"
sudo sudo sh -c "echo '1' >> /sys/class/pwm/pwmchip7/pwm-7\:0/enable"
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
sudo ./BBBclockKernelPhysical/BBBclockKernelPhysicalDaemon $1 $2 $3
sudo timedatectl set-ntp true # Start NTP
echo 'Stopped PTP'
#sudo /etc/init.d/rsyslog start # start logging
# Kill all the launched processes with same group PID
#kill -INT $$
