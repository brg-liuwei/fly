sudo killall fly
ulimit -c unlimited
sudo /usr/local/spawnfcgi/bin/spawn-fcgi -a xxx.xxx.xxx.xxx -p xxxx -F 1 ./fly
