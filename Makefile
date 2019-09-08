CC=gcc
CFLAGS=-I.
DEPS = 
LIBS = -lbcm2835 -lpthread

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

picfan: picfan.o 
	$(CC) -o picfan picfan.o $(LIBS)
