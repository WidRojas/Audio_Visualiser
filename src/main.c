#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include "AudioSink.h"
#include "fftw.h"
#include "shared.h"


	pthread_mutex_t lock;

static volatile int status = 1;

void interupthandler (int sig){
	status = 0;
}

int main(int argc,char **argv){

	signal(SIGINT,interupthandler);

	int size = 2048;
	float data_buffer[2048];

	pthread_mutex_init(&lock,NULL);

	startup startargs = {argc,argv,&lock,&status,data_buffer,size,0};


	pthread_t fftwsink;
	pthread_t Visualizer;

	pthread_create(&fftwsink, NULL,AudioSink,&startargs);
	pthread_create(&Visualizer, NULL,FFTW,&startargs);

	pthread_join(fftwsink, NULL);
	pthread_join(Visualizer, NULL);

	return 0;
}
