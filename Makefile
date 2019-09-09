CC=gcc
CFLAGS=-I.
DEPS = 
LIBS = -lbcm2835 -lpthread

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

picfan: picfan.o bcm2835
	$(CC) -o picfan picfan.o $(LIBS)


.PHONY:	bcm2835-force
bcm2835-force:
	bash ./inst-bcm2835.sh

bcm2835: /usr/local/lib/libbcm2835.a
	bash ./inst-bcm2835.sh
