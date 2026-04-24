/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "KNXMONITOR "
#define VERSION  "1.00"

// #define HIGHSPEED

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
char	uartname[24] = {0};
unsigned char sspeed = 3;
char	verbose = 0;	// 1=verbose     2=verbose+      3=debug    ...
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
// =============================================================================================
char aTOchar(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 10);
    return (char) ret;
}
// =============================================================================================
uint16_t aTOint(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 10);
    return (uint16_t) ret;
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
static void print_usage(const char *prog)
{
	printf("Usage: %s [-vSN]\n", prog);
	puts("  -v --verbose \n"
		 "  -N --uart dev name (/dev/serial0)\n"
		 "  -S --uart speed (1-2-3) default 3\n"
		 "  -N --uart dev name (/dev/serial0)\n"
		 );
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	
{
	if ((argc < 1) || (argc > 3))
	{
		print_usage(PROGNAME);
		return 3;
	}
	strcpy(uartname,"/dev/serial0");

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "verbose",    2, 0, 'v' },
			{ "speed",		2, 0, 'S' },
			{ "uart_name",	1, 0, 'N' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;
		c = getopt_long(argc, argv, "v::S:N:h", lopts, NULL);
		if (c == -1)
			return 0;

		switch (c) {
		case 'h':
			print_usage(PROGNAME);
			break;
		case 'S': // uart speed
			if (optarg) 
			{
				sspeed=aTOint(optarg);
				printf("uart speed: %d\n",sspeed);
			}
			break;
		case 'N': // uart name
			if (optarg) 
				strcpy(uartname, optarg);
			break;
		case 'v':
			if (optarg) 
				verbose=aTOchar(optarg);
			if (verbose == 0) verbose=1;
			if (verbose > 9) verbose=9;

			printf("Verbose %d\n",verbose);
			break;

		case '?':
            fprintf(stderr,"Unknown option `-%c'.\n", optopt);
			return 2;		

		default:
			print_usage(argv[0]);
			break;
		}
	}
	return 0;
}
// ===================================================================================


// ===================================================================================
int main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	if (parse_opts(argc, argv))
		return 0;

	initkeyboard();
	atexit(endkeyboard);
	
	printf(PROGNAME "\n");
	printf("UART_Initialization\n");
	int fd = -1;
	
//	fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	fd = open(uartname, O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fd == -1) 
	{
//		perror("open_port: Unable to open /dev/serial0 - ");
		fprintf(stderr, "unable to open %s\n",uartname);
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
