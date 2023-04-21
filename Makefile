
all: breakout 

breakout: main.c
	gcc -o breakout -Wall -O3 -lsystemd main.c

install: breakout
	install -D -s breakout ${PREFIX}/bin/breakout
