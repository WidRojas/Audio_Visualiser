#include "shared.h"
#include <string.h>

void fill_data_buffer(startup *buffer, int incomingSize,
                      float *incomingStream) {

  startup *buf = buffer;
  float *out = buffer->data;
  int copiedSize = buffer->buffered_Chunksize;
  int maxSize = buffer->expected_Chunksize;

  if (copiedSize < maxSize) { // if there is insuf data

    // if incoming is less than required
    if ((copiedSize + incomingSize) < (maxSize)) {
      memcpy(&out[copiedSize], incomingStream, sizeof(float) * incomingSize); 
      buffer->buffered_Chunksize += incomingSize; 

    } else if ((copiedSize + incomingSize) >= (maxSize)) {
	memcpy(&out[copiedSize],incomingStream,sizeof(float) * (maxSize - copiedSize));
        buffer->buffered_Chunksize = maxSize; 
    }
  }
}
