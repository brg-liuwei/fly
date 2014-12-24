#usage:
sudo killall fly
#sudo /usr/local/bin/spawn-fcgi -a 127.0.0.1 -p 9876 -F 1 bin/fly
sudo /usr/local/bin/spawn-fcgi -s /tmp/fysock -F 1 bin/fly
