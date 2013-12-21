CC   = gcc
OPTS = -Wall

all: server client lib

server: server.o udp.o
	$(CC) -o server server.o udp.o 

client: client.o udp.o
	$(CC) -o client client.o udp.o 

lib: mfs.o udp.o
	$(CC) -Wall -shared -fPIC -o libmfs.so mfs.c udp.c

clean:
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -f server.o client.o udp.o mfs.o libmfs.so server client lib
