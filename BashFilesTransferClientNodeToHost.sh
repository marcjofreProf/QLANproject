# It needs installed in the host computer: sudo apt install openssh-server
# ./BashFilesTransferClientNodeToHost.sh arg1
# arg1: to specify if NTP or PTP or others
sudo scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null /home/debian/Scripts/QLANproject/PRUdata/TimetaggingData marcjofre@10.0.0.1:/home/marcjofre/Scripts/QLANproject/PRUdata/ClientRawStoredQubitsNode$1

sudo scp -r -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null /home/debian/Scripts/OSrealtimeTools/rt-tests/LatencyHistData/ marcjofre@10.0.0.1:/home/marcjofre/Scripts/OSrealtimeTools/rt-tests/

echo 'Sent files from client node to host'
# Typically, if a message like ssh: connect to host 10.0.0.1 port 22: Connection refused.lost connection.
# The receiver needs to have openssh-server
# sudo apt install openssh-server

