#include <asm-generic/ioctls.h>
#include <asm-generic/termbits.h>
#include <fftw3.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define FRAME_DATA 2047

void parse_frame(fftw_complex *input, FILE *fptr);
void write_out(fftw_complex *out);
void appendchar(char *array, char symbol);
void display(fftw_complex *out, int compression, double sensitivity, uint8_t *LEDbuffer);

//struct termios2 tio;


void *FFTW(int argc,char **argv){

  FILE *fptr;

  /*

  int fd = open("/dev/ttyUSB0",O_RDWR | O_NOCTTY);

  if (fd < 0 ) {
    perror("open");
    return -1;
  }
  struct termios2 options;
 if ( ioctl(fd,TCGETS2,&options) < 0) {
    perror("TCGETS");
    return -1;
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

  */

  fftw_complex *in, *out;
  fftw_plan p;

  in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_DATA);
  out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FRAME_DATA);
  p = fftw_plan_dft_1d(FRAME_DATA, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  // size , in , out , sign (- in this case) , flags
  // the data stored defaults to a double [2] where :
  // real = in[i][0] ; imaginary in[i][1]
  // https://www.fftw.org/fftw2_doc/fftw_2.html (for more informarion)

  if ((fptr = fopen("buffer", "r")) == NULL) { // open pipewire file
    perror("FAILED TO OPEN FRAMEBUFFER\n");
    return NULL;
  }

  uint8_t LEDBUFF[6]; // this should be use malloc along with amount of lines

  while (1) {

    parse_frame(in, fptr); // parse from pipewire filter
    fftw_execute(p); // apply fftw

    for (int i = 0; i < FRAME_DATA; i++) {
      out[i][0] = fabs(out[i][0]); // take absolute value from fftw due to sound properties
      if (out[i][0] > 10) { // max out data at 10 for simplicity;
        out[i][0] = 10; 
      }
    } 

    usleep(10);
    display(out, 140, 0.04,LEDBUFF); // display to terminal  || this should be corrected to lines desired and sensitivity should be inverse
    // 80 = 11 lines
    //
    for (int i = 0 ; i < 5 ; i++) { printf("::%d",LEDBUFF[i]);}
    
    LEDBUFF[5] = 32;
    //write(fd,LEDBUFF,sizeof(LEDBUFF));
  }

  fclose(fptr);
  //close(fd);
  fftw_free(in);
  fftw_free(out);

  return NULL;
}

void parse_frame(fftw_complex *input, FILE *fptr) {

  fseek(fptr, 0, SEEK_SET);
  fflush(fptr);

  for (int i = 0; i < FRAME_DATA; i++) {
    fscanf(fptr, "%lf", &input[i][0]);
  }
}

void appendchar(char *array, char symbol) {
  int len = strlen(array);
  array[len] = symbol;
  array[len + 1] = '\0';
  return;
}


void display(fftw_complex *out, int compression, double sensitivity, uint8_t *LEDbuffer) {

  char buffer[8000] = "\n\t|Indicate V1.1|\n";
  double avg;
  int Bar_Max = 50;

  int debug_counter = 0;

  printf("\e[1;1H\e[2J");

  for (int i = 0; i < (FRAME_DATA / 2.5); i += compression) {  // process each bar
    for (int j = 0; j < compression; j++) {
      avg += out[i + j][0];
    }
    avg = (double)avg / (double)compression; 

    // debug code
    printf("[POS]:%d [RAW]:%lf ", debug_counter,avg);
    debug_counter++;
    ///////////////////////////////////////

    if (avg / sensitivity > Bar_Max) {
      for (int i = 0 ; i < Bar_Max; i++){
        appendchar(buffer, '+');
      } // if sound exceeds threshhold 
      printf("[BAR]:%d\n",Bar_Max);
      *LEDbuffer = Bar_Max/10; // $$ hardcoded for now
    } else {
      uint8_t iter = 0;
      for (double k = 0; k < avg; k += sensitivity) {
        appendchar(buffer, ':');
        iter++;
      }
      printf("[BAR]:%d\n",iter);
      *LEDbuffer = iter/10; // $$
    }
    appendchar(buffer, '\n');
    avg = 0;
    LEDbuffer++;
  } 

  printf("%s", buffer);

  //debug code//
  debug_counter = 0;
}
