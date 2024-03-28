# It needs installed in the host computer: sudo apt install openssh-server
# ./BashFilesTransferDealerNodeToHost.sh arg1
# arg1: to specify if NTP or PTP or others
sudo scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null /home/debian/Scripts/QLANproject/PRUdata/TimetaggingData marcjofre@10.0.0.1:/home/marcjofre/Scripts/QLANproject/PRUdata/DealerRawStoredQubitsNode$1
echo 'Sent files from dealer node to host'

