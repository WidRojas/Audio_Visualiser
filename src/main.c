#include <pthread.h>
#include <stdio.h>
#include "AudioSink.h"
#include "fftw.h"


int main(int argc,char **argv){

	pthread_t Visualizer;

	pthread_create(&Visualizer, NULL,AudioSink,NULL);
	pthread_create(&Visualizer, NULL,FFTW(argc, argv),NULL);

	pthread_join(Visualizer, NULL);

	return 0;
}
