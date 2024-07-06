# A script to clean and make space in the memory of BeagleBone Black
trap "kill 0" EXIT
echo 'Cleaning memory space BBB...'

# Remove some temporal logs
sudo rm /var/log/*.log
sudo rm /var/log/*.log.1
sudo rm /var/log/*.gz

