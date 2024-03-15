# It needs installed in the host computer: sudo apt install openssh-server
# ./BashFilesTransferClientNodeToHost.sh arg1
# arg1: to specify if NTP or PTP or others
sudo scp /home/debian/Scripts/QLANproject/PRUdata/TimetaggingData marcjofre@10.0.0.1:/home/marcjofre/Scripts/QLANproject/PRUdata/ClientRawStoredQubitsNode$1
echo 'Sent files from client node to host'

