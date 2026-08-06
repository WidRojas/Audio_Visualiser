CC=gcc
SRCS=AudioSink.c main.c fft.c
OBJS=$(patsubst %.c,%.o,$(SRCS))
CFLAGS= -lfftw3 -lm -g $$(pkg-config --cflags --libs libpipewire-0.3) 
INC=-Iinc
BIN=Indicate

all:
	$(CC) -c ./src/main.c $(INC)
	$(CC) -c ./src/fft.c $(INC) $(CFLAGS) 
	$(CC) -c ./src/AudioSink.c $(INC) $(CFLAGS)
	$(CC) $(OBJS) -o $(BIN) $(INC) $(CFLAGS)
	touch buffer

clean:
	rm -f ./*.o $(BIN) ./*.json buffer
