# Readme file for linuxPTP configuration options
# Marc Jofre
# Technical University of Catalonia - Universitat Politècnica de Catalunya
# 2024

# References: https://linuxptp.nwtime.org/documentation/default/

All configured through PTP4l conf files. PTP2pc do not have configurations nor are used.

Very important, if the PTP synchronization is behaving erratically. Hard reset of the BeagleBone Blacks!!! (the reset button)

# Master insights

# With asCapable in the master configuration as false, it eliminates the message "master clock quality received is greater than configured, ignoring master!"


# Slave insights
# The PTP will be more accurate with which ever method of one-step or two-steps that the hardware supports. Seems that BBB supports two step timestamping.
# summary interval is the period between message displays in seconds as 2^N
# utc_offset should be 37, but seems that some slaves do not acquire it propperly, so set to zero (so TAI and UTC time will be the same)

# shorter synch intervals help to reduce jitter since hardware clock is more regularly corrected and hence the stability role of the hardware clock is reduced

# General observations

# twoStepFlag activated means that the initial first time information comes in the follow-up message (somehow it is simpler to implement because the devices do not have to timestamp+transmit this timestamp at the same message, but the message containint the information comes later).

# Messages rate is very important. Range from -7 to +6.

# Maybe P2P has better time accuracy (instead of E2E), provided the network topology can support it. P2P makes sense for devices in-between, that want to be transparent to PTP messages (but it is of no use of end devices like the BBB nodes).
# In P2P mode, receiving the console message: "received PDELAY_REQ without timestamp" might be indicative of high network congestion (specially in P2P which the amount of messages increases).
# Probably P2P the synch intervals have to be order of magnitude larger than for E2E, due to the high amount of network trafficit generates.
# P2P seems not to work with BBB (either slave or master)

# delay_filter_length is very important to be sufficiently large to not have too much jitter but small enough so that time corrections in the different slaves happen at the same time - within the time slot of interest. The jitter might come from the interconnecting interfaces in between and t the end-points (for instance a switch might add more jitter than a hub; better to use a PRP transparent switch)

# Seems that P2P, either because of slave or master BBB, this inhibit_delay_req has to be 1; otherwise the master PTP indicates the message "received PDELAY_REQ without timestamp". But inhibiting this messages, makes the protocol not to work.
# Maybe for frequency synchronization this inhibit_delay_req can be useful, since only one way packets from the master are needed to synchronize in frequency.

# Clock servo: pi, linreg, nullf

# Write phase mode not supported in BBB
# Required to quickly correct Time Jumps in slave
# step threshold: long values produces large jumps every now and then but allow better precition (less jitter). So it is a trade-off.
# Maybe, for frequency synchronization this value can be low (e.g., 1 us). Instead for Time Synchronization this values should be larger (e.g., 10-20 us)

