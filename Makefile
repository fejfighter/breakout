
all: breakout 

breakout: main.c
	gcc -o breakout -Wall -flto -O3 -lsystemd main.c
