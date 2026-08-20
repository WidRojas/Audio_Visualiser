#include <pthread.h>
#include <stdio.h>
#include "AudioSink.h"
#include "fftw.h"

	pthread_mutex_t lock;

int main(int argc,char **argv){

	int size = 2048;
	double data_buffer[2048];

	pthread_mutex_init(&lock,NULL);

	startup startargs = {argc,argv,data_buffer,&lock};


	pthread_t fftwsink;
	pthread_t Visualizer;

	pthread_create(&fftwsink, NULL,AudioSink,&startargs);
	pthread_create(&Visualizer, NULL,FFTW,&startargs);

	pthread_join(fftwsink, NULL);
	pthread_join(Visualizer, NULL);

	return 0;
}
