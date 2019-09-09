CC=gcc
CFLAGS=-I.
DEPS = 
LIBS = -lbcm2835 -lpthread
BINDIR = /usr/local/bin/

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: picfan

picfan: picfan.o bcm2835
	$(CC) -o picfan picfan.o $(LIBS)


.PHONY:	bcm2835-force clean all install install-picfan
bcm2835-force:
	bash ./inst-bcm2835.sh

bcm2835: /usr/local/lib/libbcm2835.a
	bash ./inst-bcm2835.sh

clean:
	rm -f *.o
	rm picfan
	rm -rf bcm2835

install-picfan:
	install -m 500 -o root -g bin picfan $(BINDIR)

install:
	install -m 500 -o root -g bin picfan $(BINDIR)
	install -m 500 -o root -g root picfan.service /lib/systemd/system/
	systemctl enable picfan.service
	systemctl start picfan.service
