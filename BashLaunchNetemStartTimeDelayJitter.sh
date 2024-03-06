echo 'Start Time Delay Jitter'
# ./BashLaunchNetemStartTimeDelayJitter.sh arg1 arg2 arg3
# arg1: interface, for example enp0s8
# arg2: delay, for example 0ns
# arg3: jitter, for example 10ns
sudo tc qdisc add dev $1 root handle 1: prio
sudo tc qdisc add dev $1 parent 1:1 handle 10: netem delay $2 $3
echo 'Affecting NTP and PTP with Delay and Jitter'
# Specify which UDP ports to affect NTP
sudo tc filter add dev $1 protocol ip parent 1: prio 1 u32 match ip dport 123 0xffff flowid 1:1
sudo tc filter add dev $1 protocol ip parent 1: prio 1 u32 match ip sport 123 0xffff flowid 1:1
# Specify which UDP ports to affect PTP
# PTP Event Messages (Port 319):
sudo tc filter add dev $1 protocol ip parent 1: prio 1 u32 match ip dport 319 0xffff flowid 1:1
sudo tc filter add dev $1 protocol ip parent 1: prio 1 u32 match ip sport 319 0xffff flowid 1:1
# PTP General Messages (Port 320):
sudo tc filter add dev $1 protocol ip parent 1: prio 1 u32 match ip dport 320 0xffff flowid 1:1
sudo tc filter add dev $1 protocol ip parent 1: prio 1 u32 match ip sport 320 0xffff flowid 1:1

