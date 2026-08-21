#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <bits/pthreadtypes.h>
typedef struct {
	int argc;
	char **argv;
	pthread_mutex_t *lock;
	int volatile *isRunning;

	float *data;
	int expected_Chunksize;
	int buffered_Chunksize;

	// possibly include flags indicating whether buffer is full and whether is has been read
	// to coordinate with display thread

} startup;

void fill_data_buffer(startup *buffer, int incomingSize, float *incomingStream);

#endif
