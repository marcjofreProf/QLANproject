# Readme file for linuxPTP configuration options
# Marc Jofre
# Technical University of Catalonia - Universitat Politècnica de Catalunya
# 2024

All configured through PTP4l conf files. PTP2pc do not have configurations nor are used

# Master insights

# With asCapable in the master configuration as false, it eliminates the message "master clock quality received is greater than configured, ignoring master!"


# Slave insights
# The PTP will be more accurate with which ever method of one-step or two-steps that the hardware supports. Seems that BBB supports two step timestamping.
# summary interval is the period between message displays in seconds as 2^N
# utc_offset should be 37, but seems that some slaves do not acquire it propperly, so set to zero (so TAI and UTC time will be the same)

# shorter synch intervals help to reduce jitter since hardware clock is more regularly corrected and hence the stability role of the hardware clock is reduced

# Maybe P2P has better time accuracy (instead of E2E), provided the network topology can support it.
# In P2P mode, receiving the console message: "received PDELAY_REQ without timestamp" might be indicative of high network congestion (specially in P2P which the amount of messages increases).
# Probably P2P the synch intervals have to be order of magnitude larger than for E2E, due to th ehigh amount of network trafficit generates.
# P2P seems not to work with BBB (either slave or master)

# delay_filter_length is very important to be sufficiently large to not have too much jitter but small enough so that time corrections in the different slaves happen at the same time - within the time slot of interest. The jitter might come from the interconnecting interfaces in between and t the end-points (for instance a switch might add more jitter than a hub; better to use a PRP transparent switch)

# Seems that P2P, either because of slave or master BBB, this inhibit_delay_req has to be 1; otherwise the master PTP indicates the message "received PDELAY_REQ without timestamp"

# Clock servo: pi, linreg, nullf

# Write phase mode not supported in BBB
write_phase_mode	0

# Required to quickly correct Time Jumps in slave
# step threshold: long values produces large jumps every now and then but allow better precition (less jitter). So it is a trade-off.
# Maybe, for frequency synchronization this value can be low (e.g., 1 us). Instead for Time Synchronization this values should be larger (e.g., 10-20 us)

