#ifndef FFTW_H
#define FFTW_H

#include <pthread.h>
#include <bits/pthreadtypes.h>
typedef struct {
	int argc;
	char **argv;
	double *data;
	pthread_mutex_t *lock;
} startup;

void *FFTW(void *arg);

#endif
