CC=gcc
SRCS=AudioSink.c main.c fft.c
OBJS=$(patsubst %.c,%.o,$(SRCS))
CFLAGS= -lfftw3 -lm -g $$(pkg-config --cflags --libs libpipewire-0.3) 
INC=-Iinc
BIN=Indicate
FILES=buffer

all: $(OBJS) $(FILES) 
	$(CC) $(OBJS) -o $(BIN) $(INC) $(CFLAGS)

%.o:./src/%.c
	$(CC) -c $^ $(INC) $(CFLAGS)

$(FILES):
	touch buffer

clean:
	rm -f ./*.o $(BIN) ./*.json buffer
