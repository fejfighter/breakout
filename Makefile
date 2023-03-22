
all: breakout path

breakout: main.c
	gcc -o breakout -Wall -ggdb3 -lsystemd main.c

path: path.c
	gcc -o path -Wall -ggdb3 path.c
