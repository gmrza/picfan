# picfan
##Raspberry Pi PWM fan controller

The Raspberry Pi 4 tends to run rather hot, and active cooling is a more important option.
PiCFan provides a PWM Cooling Fan Controller written in C, which is installed as a Systemd service.

This software is very much in alpha state, and is probably riddled with bugs.  It also lacks decent documentation.  I am however making this repo visible while I work on it.

##Installation
I haven't got around to familiarising myself with Autotools, so I have only written a rudimentary Makefile.
This software depends on the bcms2835 library by [Mike McCauley](mikem@airspayce.com).  The bcm2835 library can be found at [http://www.airspayce.com/mikem/bcm2835/index.html](http://www.airspayce.com/mikem/bcm2835/index.html)

Installation currently requires the following steps, best performed as root, as it will try to install bcm2835.a in /usr/local/lib:
```
make
make install
```
