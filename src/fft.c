#include <asm-generic/ioctls.h>
#include <asm-generic/termbits.h>

#include <fftw3.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define FRAME_DATA 2048
// this include mirror signals so we divide by 2 in data fitting

// starting postion
// range 
// counter
  typedef struct {
    int start;
    int range;
    int counter;
  } audioSection;

void parse_frame(double *input, FILE *fptr);
void write_out(fftw_complex *out);
void fitdata(fftw_complex *out, double *fitted, int size);
void display(double *data, int size, double sensitivity,int cap,uint8_t *UART);
void normalizeLoudness(double *fitted,int size);
void updateFrame(audioSection *frame, int i);

// Fixed : fit data mostly works 
//TODO :
//  fit data is extremely unoptimized due to updateframe (easy fix)
//  file parsing need to replaced with some type of thread coordinated variable

void enableSerial();


void *FFTW(int argc,char **argv){


  fftw_complex *out;
  fftw_plan p;
  uint8_t *LEDBUFF; 
  double *bar_data;
  FILE *fptr;
  
  int bar_data_size = 10;
  double bar_sensitivity = 0.5;
  int bar_cap = 25;


  if (argc == 2) { // this prob needs error checks
    bar_data_size = strtol(argv[1],NULL,10);
  }

  if (argc == 3) {
    bar_data_size = strtol(argv[1],NULL,10);
    bar_sensitivity = strtod(argv[2],NULL);
  }

  if (argc == 4) {
    bar_data_size = strtol(argv[1],NULL,10);
    bar_sensitivity = strtod(argv[2],NULL);
    bar_cap = strtol(argv[3],NULL,10);
  }

  if ((bar_data = (double*)calloc(bar_data_size,sizeof(double))) == NULL){
    printf("failed to allocate bar data array");
    return NULL;
  }

 if (( LEDBUFF =(uint8_t*)calloc(bar_data_size, sizeof(uint8_t))) == NULL ){
    printf("failed to allocate LEDBUFF memory");
    return NULL;
  }

  double in[FRAME_DATA];

  if ((fptr = fopen("buffer", "r")) == NULL) { // open pipewire file
    perror("FAILED TO OPEN FRAMEBUFFER\n");
    return NULL;
  }
  

  out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_DATA);
  p = fftw_plan_dft_r2c_1d(FRAME_DATA,in,out,FFTW_ESTIMATE);
  // https://www.fftw.org/fftw3_doc/One_002dDimensional-DFTs-of-Real-Data.html

  while (1) {
    parse_frame(in, fptr); // parse from pipewire filter , this needs to not use a file in order to run independently
    fftw_execute(p); // apply fftw
    usleep(10); // this only exist because file, remove later
    printf("\e[1;1H\e[2J");
    fitdata(out, bar_data, bar_data_size);
    display(bar_data,bar_data_size,bar_sensitivity,bar_cap,LEDBUFF);
    //write(fd,LEDBUFF,sizeof(LEDBUFF)); LED UART
    // instead of while 1 we need to handle sigterm in main so we actaully run clean up code
    memset(bar_data,0,bar_data_size * sizeof(double));
  }

  free(bar_data);
  free(LEDBUFF);
  fclose(fptr);
  //close(fd);
  fftw_free(out);

  return NULL;
}

void parse_frame(double *input, FILE *fptr) {

  fseek(fptr, 0, SEEK_SET);
  fflush(fptr);

  for (int i = 0; i < FRAME_DATA; i++) {
    fscanf(fptr, "%lf", &input[i]);
  }
}

void fitdata(fftw_complex *out, double *fitted ,int size){

  int uniqueSamples = 1025;
  double magnitudes[uniqueSamples];
  

  /*
  at the moment sampling rate is 48k
  and samples taken is 2048
  this means that we are dealing with 24khz 
  24k / 1048 unique samples = 23hz per parsed element
  */
  int frames = 7;
  audioSection frame[] = {
    {0,3}, // 1. 20 - 60 sub bass    
    {3,8}, // 2. 60 - 250 bass
    {11,10}, // 3. 250 - 500 low mids
    {21,65}, // 4. 500 - 2000 midrange 
    {86,86}, // 5. 2k - 4k high mids
    {172,86}, // 6. 4k - 6k presence
    {256,769} // 7. 6k - 24k brilliance
  };

  for (int i = 0; i < uniqueSamples; i++){
    magnitudes[i] = sqrt((pow(out[i][0],2))+(pow(out[i][1],2)));
  }

  int i = 0; 
  while (i < size){
    updateFrame(frame, i);
    i++;
  }

  int currentindex = 0;

  for (int i = 0 ; i < frames; i++){
      if (frame[i].counter == frame[i].range){
        for (int j = 0 ; j < frame[i].range;j++){
          fitted[currentindex] = magnitudes[frame[i].start + j];
          currentindex++;
        }
      } else if (frame[i].counter < frame[i].range){
        int temp = frame[i].counter -1;
        for (int j = 0 ; j < frame[i].counter - 1 ; j++){ 
          fitted[currentindex] = magnitudes[frame[i].start + j];
          currentindex++;
        } 
        for (int j = 0; j <= (frame[i].range - frame[i].counter) ; j++ ){
          fitted[currentindex] += magnitudes[frame[i].start + temp + j];
        }
        fitted[currentindex] = fitted[currentindex] / (frame[i].range - (frame[i].counter -1)) ;
        currentindex++;
      }
    }
  
    // each range is then multiplied based on some constant rather than a log scale ( because idk and good enough for visualization)
}

void updateFrame(audioSection *frame, int i) {
  while(frame[i % 3].counter >= frame[i%3].range){
    i++;
  }
    frame[i % 3].counter++;

  return;
}

void enableSerial(){
   //this is for writing to serial
  int fd = open("/dev/ttyUSB0",O_RDWR | O_NOCTTY);

  // error checking needed

  if (fd < 0 ) {
    perror("open");
    return;
  }
  struct termios2 options;
 if ( ioctl(fd,TCGETS2,&options) < 0) {
    perror("TCGETS");
    return;
  }

  options.c_cflag &= ~CBAUD;
  options.c_cflag |= BOTHER;
  options.c_ispeed = 115200; // baud io speed 
  options.c_ospeed = 115200;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8; // 8 bit data
  options.c_cflag &= ~PARENB; // no parity
  options.c_cflag &= ~CSTOPB; // 1 stop bit

  ioctl(fd,TCSETS2,&options);
}


void display(double *data, int size, double sensitivity,int cap,uint8_t *UART){
  int barPosition =0;
  double fillAmount =0;

  // each bar maxes at  
  // + cap + '+' + \n 
  // + \0
  int buffsize = (size * (cap + 3)) + 1;
  
  char* buffer;
   if (( buffer =  (char*)malloc(sizeof(char) * buffsize)) == NULL){
    printf("failed to allocate display buffer mem");
    return;
  }
  char* curr = buffer;

  printf("\t[Indicate V1.2]\n");
  while( barPosition < size) {
    if (fillAmount < data[barPosition]) {
      fillAmount += (1/sensitivity);

      if ((fillAmount / (1/sensitivity)) >= cap){
        *curr = '+';
        curr++;
          fillAmount = data[barPosition];
      } else {
        *curr= ':';
        curr++;
      }
    } else {
      barPosition++;
      fillAmount = 0;
        *curr= '\n';
        curr++;
    }
  }
  *curr = '\0';
  printf("%s",buffer);
  curr = NULL;
  free(buffer);
}
