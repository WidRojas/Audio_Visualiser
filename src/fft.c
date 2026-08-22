//  TODO :
//  malloc should probably only be freed at the very end and use realloc during runtime
//  include the uart as a flag (also clearer flags)
//
//  change rendering framerate and use syswrites to improve performance
//  rather than clearing reset the cursor

#include <asm-generic/ioctls.h>
#include <asm-generic/termbits.h>

#include <fcntl.h>
#include <fftw3.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "shared.h"
// this include mirror signals so we divide by 2 in data fitting
#define FRAME_DATA 2048

/* this organizes how data is distributed per bar depending on size

start : where a segment begins in data
range : how long a segment is
multiplier : gain on a segment
*/
typedef struct {
  int start;
  int range;
  double multipler;
  int counter;
} audioSection;

/*
at the moment sampling rate is 48k
and samples taken is 2048
this means that we are dealing with 24khz
24k / 1048 unique samples = 23hz per parsed element
*/

// adjust the multipliers to taste
int frames = 7;
audioSection frame[] = {
    {0, 3, 0.8},     // 1. 20 - 60 sub bass
    {3, 8, 0.8},     // 2. 60 - 250 bass
    {11, 10, 1},   // 3. 250 - 500 low mids
    {21, 65, 3},   // 4. 500 - 2000 midrange
    {86, 86, 6},   // 5. 2k - 4k high mids
    {172, 86, 7},  // 6. 4k - 6k presence
    {256, 769, 10} // 7. 6k - 24k brilliance
};

void parse_frame(double *input, startup *arg);
void write_out(fftw_complex *out);
void fitdata(fftw_complex *out, double *fitted, int size);
void display(double *data, int size, double sensitivity, int cap,
             uint8_t *UART);
void updateFrame(audioSection *frame, int size);


void enableSerial();

void *FFTW(void *arg) {

  startup *args = arg;

  fftw_complex *out;
  fftw_plan p;
  uint8_t *LEDBUFF;
  double *bar_data;

  int bar_data_size = 25;
  double bar_sensitivity = 0.2;
  int bar_cap = 40;

  if (args->argc == 2) { // this prob needs error checks
    bar_data_size = strtol(args->argv[1], NULL, 10);
  }

  if (args->argc== 3) {
    bar_data_size = strtol(args->argv[1], NULL, 10);
    bar_sensitivity = strtod(args->argv[2], NULL);
  }

  if (args->argc == 4) {
    bar_data_size = strtol(args->argv[1], NULL, 10);
    bar_sensitivity = strtod(args->argv[2], NULL);
    bar_cap = strtol(args->argv[3], NULL, 10);
  }

  if ((bar_data = (double *)calloc(bar_data_size, sizeof(double))) == NULL) {
    printf("failed to allocate bar data array");
    return NULL;
  }

  if ((LEDBUFF = (uint8_t *)calloc(bar_data_size, sizeof(uint8_t))) == NULL) {
    printf("failed to allocate LEDBUFF memory");
    return NULL;
  }

  double in[FRAME_DATA];
  memset(in, 0, sizeof(double)* FRAME_DATA);

  out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_DATA);
  p = fftw_plan_dft_r2c_1d(FRAME_DATA, in, out, FFTW_ESTIMATE);
  // https://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html

  updateFrame(frame, bar_data_size);
  
  while ( (int volatile)args->isRunning[0] == 1) {

    pthread_mutex_lock(args->lock);

    if(args->is_ready[0] == 1){
      parse_frame(in, args); 
      
      //for (int i = 0 ; i < 2048 ; i++){
        //printf("%f %d\n",in[i],i);
      //}

      fftw_execute(p);       // apply fftw
      pthread_mutex_unlock(args->lock);

    }  else {
      pthread_mutex_unlock(args->lock);
    }
    
    //
    usleep(33333);            // framerate (30fps)
    printf("\e[1;1H\e[2J"); // clear screen
    fitdata(out, bar_data, bar_data_size);
    display(bar_data, bar_data_size, bar_sensitivity, bar_cap, LEDBUFF);
    
    // UART feature
    // write(fd,LEDBUFF,sizeof(LEDBUFF)); LED UART

    // TODO :
    //  instead of while 1 we need to handle sigterm in main so we actaully run
    //  clean up code
    //
    memset(bar_data, 0, bar_data_size * sizeof(double));
  }

  // UART feature
  // close(fd);
  
  free(bar_data);
  free(LEDBUFF);
  fftw_free(out);

  return NULL;
}

void parse_frame(double *input, startup *arg) {


    for (int i = 0; i < FRAME_DATA; i++) {
      input[i] = (double)arg->data[i];
      //printf("%f %d\n",input[i],i);
    }
      arg->buffered_Chunksize[0] = 0;
      arg->is_ready[0] = 0;
      
}

void fitdata(fftw_complex *out, double *fitted, int size) {

  int uniqueSamples = 1025;
  double magnitudes[uniqueSamples];

  for (int i = 0; i < uniqueSamples; i++) {
    magnitudes[i] = sqrt((pow(out[i][0], 2)) + (pow(out[i][1], 2)));
    //printf("%f: %f :%f: %d\n",out[i][0],out[i][1],magnitudes[i],i);
  }
  

  int currentindex = 0;

  for (int i = 0; i < frames; i++) {
    if (frame[i].counter == frame[i].range) {
      for (int j = 0; j < frame[i].range; j++) {
        fitted[currentindex] = magnitudes[frame[i].start + j];
        fitted[currentindex] = fitted[currentindex] * frame[i].multipler;
        currentindex++;
      }
    } else if (frame[i].counter < frame[i].range) {
      int temp = frame[i].counter - 1;
      for (int j = 0; j < frame[i].counter - 1; j++) {
        fitted[currentindex] = magnitudes[frame[i].start + j];
        fitted[currentindex] = fitted[currentindex] * frame[i].multipler;
        currentindex++;
      }
      for (int j = 0; j <= (frame[i].range - frame[i].counter); j++) {
        fitted[currentindex] += magnitudes[frame[i].start + temp + j];
      }
      fitted[currentindex] =
          fitted[currentindex] / (frame[i].range - (frame[i].counter - 1));
      fitted[currentindex] = fitted[currentindex] * frame[i].multipler;
      currentindex++;
    }
  }
}

void updateFrame(audioSection *frame, int size) {
  int accum = 0;
  int whole = 0;
  int remainingFrames = frames;
  double capture = 0;
  double mantissa = 0;

  for (int i = 0; i < frames; i++) {
    capture = (double)(size - accum) / (double)remainingFrames;
    if (capture > frame[i].range - 1) {
      frame[i].counter = frame[i].range;
      accum += frame[i].range;
      remainingFrames--;
    } else {
      whole = (int)capture;
      mantissa = capture - whole;

      for (int j = 0; j < remainingFrames; j++) {
        frame[i + j].counter += whole;
      }
      // take the int part from capture and add to each remaining frame
      for (int j = 0; j < mantissa * remainingFrames; j++) {
        frame[i + j].counter++;
      }
      // for the decimal part multiply by currentframes
      // then increment those involved
      break;
    }
  }
}

void enableSerial() {
  // this is for writing to serial
  int fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);

  // error checking needed
  if (fd < 0) {
    perror("open");
    return;
  }
  struct termios2 options;
  if (ioctl(fd, TCGETS2, &options) < 0) {
    perror("TCGETS");
    return;
  }
  options.c_cflag &= ~CBAUD;
  options.c_cflag |= BOTHER;
  options.c_ispeed = 115200; // baud io speed
  options.c_ospeed = 115200;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;     // 8 bit data
  options.c_cflag &= ~PARENB; // no parity
  options.c_cflag &= ~CSTOPB; // 1 stop bit

  ioctl(fd, TCSETS2, &options);
}

void display(double *data, int size, double sensitivity, int cap,
             uint8_t *UART) {
  int barPosition = 0;
  double fillAmount = 0;

  // each bar maxes at
  // + cap + '+' + \n
  // + \0
  int buffsize = (size * (cap + 3)) + 1;

  char *buffer;
  if ((buffer = (char *)malloc(sizeof(char) * buffsize)) == NULL) {
    printf("failed to allocate display buffer mem");
    return;
  }
  char *curr = buffer;

  printf("\t[Indicate V1.3]\n");
  while (barPosition < size) {
    if (fillAmount < data[barPosition]) {
      fillAmount += (1 / sensitivity);

      if ((fillAmount / (1 / sensitivity)) >= cap) {
        *curr = '+';
        curr++;
        fillAmount = data[barPosition];
      } else {
        *curr = ':';
        curr++;
      }
    } else {
      barPosition++;
      fillAmount = 0;
      *curr = '\n';
      curr++;
    }
  }
  *curr = '\0';
  printf("%s", buffer);
  curr = NULL;
  free(buffer);
}
