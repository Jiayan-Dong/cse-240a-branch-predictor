CC=gcc
OPTS=-g -std=c99 -Werror

all: main.o predictor.o
	$(CC) $(OPTS) -lm -o predictor main.o predictor.o tage.o

main.o: main.c predictor.h
	$(CC) $(OPTS) -c main.c

predictor.o: predictor.h predictor.c tage.c
	$(CC) $(OPTS) -c predictor.c tage.c

clean:
	rm -f *.o predictor;
