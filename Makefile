
all: breakout 

breakout: main.c
	gcc -o breakout -Wall -ggdb3 -lsystemd main.c
