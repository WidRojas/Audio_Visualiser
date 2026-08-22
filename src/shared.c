#include "shared.h"
#include <string.h>
#include <stdio.h>

void fill_data_buffer(startup *buffer, int incomingSize,
                      float *incomingStream) {

  startup *buf = buffer;
  float *out = buffer->data;
  int maxSize = buffer->expected_Chunksize;
  int finishedParsing= 0;

    // if incoming is less than required
    if ((buffer->buffered_Chunksize[0] + incomingSize) < (maxSize)) { // data may not be parsing properly here
      memcpy(&out[buffer->buffered_Chunksize[0]], incomingStream, sizeof(float) * incomingSize); 
      buffer->buffered_Chunksize[0] += incomingSize; 

    } else if ((buffer->buffered_Chunksize[0] + incomingSize) >= (maxSize)) {
      memcpy(&out[buffer->buffered_Chunksize[0]],incomingStream,sizeof(float) * (maxSize - buffer->buffered_Chunksize[0]));
        buffer->buffered_Chunksize[0] = maxSize; 
    }

    if (buffer->buffered_Chunksize[0] == maxSize){
      buffer->is_ready[0] = 1;
      finishedParsing = 1;
    }
}
