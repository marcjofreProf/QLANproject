echo 'Stop Time Delay Jitter'
# ./BashLaunchNetemStopTimeDelayJitter.sh arg1
# arg1: interface. For example enp0s8
sudo tc qdisc del dev $1 root

