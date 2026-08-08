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

void parse_frame(double *input, FILE *fptr);
void write_out(fftw_complex *out);
void fitdata(fftw_complex *out, double *fitted, int size);
void display(double *data, int size, double sensitivity,int cap,uint8_t *UART);
void normalizeLoudness(double *fitted,int size);

// TO DO : currently there is an issue with displaying and the averaging that causes buffer to fail when sounds drops.

void enableSerial();


void *FFTW(int argc,char **argv){


  fftw_complex *out;
  fftw_plan p;
  uint8_t *LEDBUFF; 
  double *bar_data;
  FILE *fptr;
  
  int bar_data_size = 30;
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

  if ((bar_data = (double*)malloc(bar_data_size * sizeof(double))) == NULL){
    printf("failed to allocate bar data array");
    return NULL;
  }

 if (( LEDBUFF =(uint8_t*)malloc(bar_data_size * sizeof(uint8_t))) == NULL ){
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
    normalizeLoudness(bar_data,bar_data_size);
    display(bar_data,bar_data_size,bar_sensitivity,bar_cap,LEDBUFF);
    //write(fd,LEDBUFF,sizeof(LEDBUFF)); LED UART
    // instead of while 1 we need to handle sigterm in main so we actaully run clean up code
  }

  free(bar_data);
  free(LEDBUFF);
  fclose(fptr);
  //close(fd);
  fftw_free(in);
  fftw_free(out);

  return NULL;
}

void normalizeLoudness(double *fitted,int size){
  double lowest = fitted[0];

  for (int index = 0; index < size; index++ ){
    if (fitted[index] < lowest) {
      lowest = fitted[index];
    }
  }
  for (int index = 0; index < size; index++ ){
    fitted[index] = 10 * log10(fitted[index]/ lowest);
  } // 
}

void parse_frame(double *input, FILE *fptr) {

  fseek(fptr, 0, SEEK_SET);
  fflush(fptr);

  for (int i = 0; i < FRAME_DATA; i++) {
    fscanf(fptr, "%lf", &input[i]);
  }
}

void fitdata(fftw_complex *out, double *fitted ,int size){

  int orignal = size;
  int SamplesPerBar = ceil((double)(FRAME_DATA/2.0 + 1)/(double)size);
  int remaining = ((FRAME_DATA/2) + 1) % SamplesPerBar;

  int i = 0;
  int j = 0;
  double magnitude = 0;

  if (remaining != 0) {
    size--;
    for (int k = 1 ; k <= remaining; k++){
      magnitude = sqrt(pow((out[FRAME_DATA/2 +1 - k][0]),2) + pow((out[FRAME_DATA/2 +1 - k][1]),2));
      fitted[size] += magnitude;
    }
      fitted[size] = fitted[size] / remaining;
  }

  while (i < size){
    magnitude = sqrt(pow((out[j][0]),2) + pow((out[j][1]),2));
    fitted[i] += magnitude;
    j++;
    if ((j % (SamplesPerBar)) == 0 ) {
      fitted[i] = fitted[i] / SamplesPerBar;
      i++;
    }
  }
  /*
  for (int i = 0 ; i < orignal; i++) {
    printf("%f at %d\n",fitted[i],i);
  } */

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
