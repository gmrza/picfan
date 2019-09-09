if [ ! -d bcm2835 ]  ; then  mkdir bcm2835 ;fi
cd bcm2835
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.60.tar.gz
tar xvfz bcm2835-1.60.tar.gz
cd bcm2835-1.60
./configure
make
sudo make install
