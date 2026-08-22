CC=gcc
SRCS=AudioSink.c main.c fft.c shared.c
OBJS=$(patsubst %.c,%.o,$(SRCS))
CFLAGS= -lfftw3 -O0 -lm -g -pthread $$(pkg-config --cflags --libs libpipewire-0.3) 
INC=-Iinc
BIN=Indicate
# add -O2 at some point current breaks due to parse frame

all: $(OBJS) $(FILES) 
	$(CC) $(OBJS) -o $(BIN) $(INC) $(CFLAGS)

%.o:./src/%.c
	$(CC) -c $^ $(INC) $(CFLAGS)

clean:
	rm -f ./*.o $(BIN) ./*.json 
