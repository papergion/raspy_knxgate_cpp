/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "KNXMONITOR "
#define VERSION  "1.00"

#define HIGHSPEED

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <time.h>
#include <errno.h>
#include <termios.h>


#define CLR  "\x1B[2J"
//#define CLR  "\033[H\033[2J"
#define NRM  "\x1B[0m"
#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define YEL  "\x1B[33m"
#define BLU  "\x1B[34m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define WHT  "\x1B[37m"

#define BOLD  "\x1B[1m"
#define UNDER "\x1B[4m"
#define BLINK "\x1B[5m"
#define REVER "\x1B[7m"
#define HIDD  "\x1B[8m"

// =============================================================================================
		time_t	rawtime;
struct	tm *	timeinfo;
char	pduFormat = 0;
char	deviceFrom = 0;
// =============================================================================================
struct termios tios_bak;
struct termios tios;
// =============================================================================================
char aConvert(char * aData);
void msleep(int millisec);
// =============================================================================================
void initkeyboard(void){
    tcgetattr(0,&tios_bak);
    tios=tios_bak;

    tios.c_lflag&=~ICANON;
    tios.c_lflag&=~ECHO;
    tios.c_cc[VMIN]=0;	// Read one char at a time
    tios.c_cc[VTIME]=0; // polling

    tcsetattr(0,TCSAFLUSH,&tios);
//	cfmakeraw(&tios); /// <------------
}
// =============================================================================================
void endkeyboard(void){
    tcsetattr(0,TCSAFLUSH,&tios_bak);
}
// =============================================================================================
char aConvert(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// ===================================================================================
int getinWait( void )
{
	int ch;
//	ch = getch();
	ch = fgetc(stdin);
	return ch;
}
// ===================================================================================
int getinNowait( void )
{
	int n = 0;
	read(fileno(stdin), &n, 1);
	return n;
}
// ===================================================================================







// ===================================================================================
void msleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================


// ===================================================================================
int main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	initkeyboard();
	atexit(endkeyboard);
	
	printf(PROGNAME "\n");
	printf("UART_Initialization\n");
	int fd = -1;
	
	fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fd == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
		return(-1);
	}

	struct termios options;
	tcgetattr(fd, &options);

	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fd, TCIFLUSH);

	tcsetattr(fd, TCSANOW, &options);

	printf("\ninitialized - OK\n");

	int  c = 0x15;
	int  n;

#ifdef HIGHSPEED
	printf("\ntest high speed...\n");
	n = write(fd,"@^",2);	// baud = 2Mhz
	msleep(10);				// pausa
	options.c_cflag = B2000000 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &options);
	msleep(10);				// pausa
#endif


	// First write to the port
	n = write(fd,"@",1);	 
	if (n < 0) 
	{
		perror("Write failed - ");
		return -1;
	}
	else
		printf("\nwrite - OK\n");

	n = write(fd,&c,1);			// per evitare memo setup in eeprom
	n = write(fd,"@MA@o@l",7);	// modalita ascii, senza abbreviazioni, log attivo 
	msleep(10);					// pausa
	n = write(fd,"@h",2);		// help
	char cb;
	while (1)
	{
		msleep(1);
		n = read(fd, &cb, 1);			// lettura da KNXgate
		if (n > 0) 
		    fprintf(stderr,"%c", cb);	// scrittura a video

		c = getinNowait();				// lettura tastiera
		if (c)
		{
			fprintf(stderr, "%c", (char) c);// echo a video
			n = write(fd,&c,1);			// scrittura su KNXgate
			if (n < 0) 
			{
				perror("Write failed - ");
				return -1;
			}
		}
	}

  // Don't forget to clean up
	close(fd);
	endkeyboard();
	return 0;
}
// ===================================================================================
