all: library

library:
	$gcc -ggdb -c -fpic mem.c -Wall -Werror
	$gcc -shared -o libmem.so mem.o
	
clean:
	rm -rf libmem.so
	rm -rf mem.o