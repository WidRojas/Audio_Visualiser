//  TODO :
//  figure out the cleanup code crashing
//
//  malloc should probably only be freed at the very end and use realloc during
//  change rendering framerate and use syswrites to improve performance
//  rather than clearing reset the cursor in the terminal
//

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
    {0, 3, 0.8},   // 1. 20 - 60 sub bass
    {3, 8, 0.8},   // 2. 60 - 250 bass
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
int enableSerial(int *fd, struct termios2 *fdstruct, char *location);

void *FFTW(void *arg) {

  startup *args = arg;

  fftw_complex *out;
  fftw_plan p;
  uint8_t *LEDBUFF;
  double *bar_data;

  int bar_data_size = 25;
  double bar_sensitivity = 0.2;
  int bar_cap = 40;
  char *UART_location;

  int UARTstatus = 0;
  int UARTFileDescriptor;
  int *UARTFDptr = &UARTFileDescriptor;
  struct termios2 UARTfileStruct;
  struct termios2 *fdstructPTR = &UARTfileStruct;

  switch(args->argc){
    case 2:
      bar_data_size = strtol(args->argv[1], NULL, 10);
      break;
    case 3:
      bar_data_size = strtol(args->argv[1], NULL, 10);
      bar_sensitivity = strtod(args->argv[2], NULL);
      break;
    case 4:
      bar_data_size = strtol(args->argv[1], NULL, 10);
      bar_sensitivity = strtod(args->argv[2], NULL);
      bar_cap = strtol(args->argv[3], NULL, 10);
    break;
    case 5:
      bar_data_size = strtol(args->argv[1], NULL, 10);
      bar_sensitivity = strtod(args->argv[2], NULL);
      bar_cap = strtol(args->argv[3], NULL, 10);

      if (strcmp(args->argv[4], "enable") == 0) {
          args->isRunning[0] = 0;
          printf("closing application: please type a valid serial port\n");
        } else {
        printf("UART Disabled, continuing...\n");
        sleep(1);
      }
    break;
    case 6:
      bar_data_size = strtol(args->argv[1], NULL, 10);
      bar_sensitivity = strtod(args->argv[2], NULL);
      bar_cap = strtol(args->argv[3], NULL, 10);

      if (strcmp(args->argv[4], "enable") == 0) {

        UART_location = malloc(strlen(args->argv[5]) * sizeof(char));
        memcpy(UART_location,args->argv[5],strlen(args->argv[5]) * sizeof(char));

        printf("%s\n", UART_location);

        printf("UART Enabled\n");
        UARTstatus = 1;
         if (enableSerial(UARTFDptr, fdstructPTR,UART_location) == 1){
          args->isRunning[0] = 0;
          printf("closing application: please type a valid serial port\n");
        }
        sleep(1);
      } else {
        printf("UART Disabled, continuing...\n");
      }
      break;
    default:
    args->isRunning[0] = 0;
    printf("invalid argument count");
  }

  if ((bar_data = (double *)calloc(bar_data_size, sizeof(double))) == NULL) {
    printf("failed to allocate bar data array");
    return NULL;
  }

  if ((LEDBUFF = (uint8_t *)calloc(bar_data_size + 1, sizeof(uint8_t))) ==
      NULL) {
    printf("failed to allocate LEDBUFF memory");
    return NULL;
  }



  double in[FRAME_DATA];
  memset(in, 0, sizeof(double) * FRAME_DATA);

  out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_DATA);
  p = fftw_plan_dft_r2c_1d(FRAME_DATA, in, out, FFTW_ESTIMATE);
  // https://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html

  updateFrame(frame, bar_data_size);

  while ((int volatile)args->isRunning[0] == 1) {

    pthread_mutex_lock(args->lock);

    if (args->is_ready[0] == 1) {
      parse_frame(in, args);
      fftw_execute(p); // apply fftw
      pthread_mutex_unlock(args->lock);
    } else {
      pthread_mutex_unlock(args->lock);
    }

    /* Hardcoded for now
    current system was built under assumption that frames format would always be
    2048 turns out applications negotiate this with pipewire so when aiming for
    30 fps consider...

    when frame size = 2048 timeout = 33333us
    when frame size = 512  timeout = (33333/4)us

    this is because if frames are less than 2048 we need to run the filter 4
    times
    */
    usleep(8333);

    printf("\e[1;1H\e[2J"); // clear screen

    fitdata(out, bar_data, bar_data_size);
    display(bar_data, bar_data_size, bar_sensitivity, bar_cap, LEDBUFF);

    LEDBUFF[bar_data_size + 1] = 222;

    if (UARTstatus == 1){
      write(UARTFileDescriptor,LEDBUFF,sizeof(LEDBUFF));
    } 

    for (int i = 0 ; i < bar_data_size + 2; i++){
      printf("%d ",LEDBUFF[i]);
    }

    memset(bar_data, 0, bar_data_size * sizeof(double));
  }

  fftw_free(out);

  free(UART_location);
  free(bar_data);
  free(LEDBUFF);

  return NULL;
}

void parse_frame(double *input, startup *arg) {

  for (int i = 0; i < FRAME_DATA; i++) {
    input[i] = (double)arg->data[i];
    // printf("%f %d\n",input[i],i);
  }
  arg->buffered_Chunksize[0] = 0;
  arg->is_ready[0] = 0;
}

void fitdata(fftw_complex *out, double *fitted, int size) {

  int uniqueSamples = 1025;
  double magnitudes[uniqueSamples];

  for (int i = 0; i < uniqueSamples; i++) {
    magnitudes[i] = sqrt((pow(out[i][0], 2)) + (pow(out[i][1], 2)));
    // printf("%f: %f :%f: %d\n",out[i][0],out[i][1],magnitudes[i],i);
  }

  int currentindex = 0;

  for (int i = 0; i < frames; i++) {
    if (frame[i].counter == frame[i].range) {
      for (int j = 0; j < frame[i].range; j++) {
        fitted[currentindex] = magnitudes[frame[i].start + j];
        fitted[currentindex] = fitted[currentindex] * frame[i].multipler;
        currentindex++; // this is reused below make a function
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

int enableSerial(int *fd, struct termios2 *fdstruct, char *location) {
  // this is for writing to serial
  *fd = open(location, O_RDWR | O_NOCTTY);

  // error checking needed
  if (*fd < 0) {
    perror("open");
    return 1;
  }

  if (ioctl(*fd, TCGETS2, fdstruct) < 0) {
    perror("TCGETS");
    return 1;
  }

  fdstruct->c_cflag &= ~CBAUD;
  fdstruct->c_cflag |= BOTHER;
  fdstruct->c_ispeed = 115200; // baud io speed
  fdstruct->c_ospeed = 115200;
  fdstruct->c_cflag &= ~CSIZE;
  fdstruct->c_cflag |= CS8;     // 8 bit data
  fdstruct->c_cflag &= ~PARENB; // no parity
  fdstruct->c_cflag &= ~CSTOPB; // 1 stop bit

  ioctl(*fd, TCSETS2, fdstruct);
  return 0;
}

void display(double *data, int size, double sensitivity, int cap, uint8_t *UART) {
  int barPosition = 0;
  double fillAmount = 0;
  int uart_buffindex = 0;

  // each bar maxes at
  // + cap + '+' + \n
  // + \0
  int buffsize = (size * (cap + 3)) + 1;

  char *buffer;
  if ((buffer = (char *)malloc(sizeof(char) * buffsize)) == NULL) {
    printf("failed to allocate display buffer mem");
    return;
  }

  UART[uart_buffindex] = 111;
  uart_buffindex++;

  char *curr = buffer;

  printf("\t[Indicate V1.4]\n");
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

      if (((((fillAmount / (1 / sensitivity)) / (double)cap)) * 100) > 100) {
        UART[uart_buffindex] = 100;
      } else {
        UART[uart_buffindex] = ((((fillAmount / (1 / sensitivity)) / (double)cap)) * 100);
      }
      barPosition++;
      fillAmount = 0;
      *curr = '\n';
      uart_buffindex++;
      curr++;
    }
  }
  *curr = '\0';
  printf("%s", buffer);
  curr = NULL;
  free(buffer);
}
