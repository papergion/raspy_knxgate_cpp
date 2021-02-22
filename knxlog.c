/* ---------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/
#define PROGNAME "KNXLOG "
#define VERSION  "1.00"

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
#include <signal.h>

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
  typedef union _WORD_VAL
  {
    int  Val;
    char v[2];
    struct
    {
        char LB;
        char HB;
    } byte;
  } WORD_VAL;
// =============================================================================================

// =============================================================================================
		time_t	rawtime;
struct	tm *	timeinfo;
char	verbose = 0;
#define MAXDEVICE 0x1FFF	//
char busdevType[MAXDEVICE] = {0};
int		ixdevice;
// =============================================================================================
int		fduart = -1;
struct termios tios_bak;
struct termios tios;
int    keepRunning = 1;
char   initialize = 0;
char   log = 0;
// =============================================================================================
FILE   *buslog;
char	logfilename[64] = {0};
FILE   *busmsg;
char	msgfilename[64] = {0};
FILE   *fConfig;
char	filename[64];
int  timeToClose = 0;
char    rx_buffer[512];
int     rx_len;
int     rx_max = 500;
// =============================================================================================
const char * type_descri[] =
{
	"nn",
	"switch",	// 1
	"nn",
	"dimmer",	// 3
	"dimmer",	// 4
	"nn",
	"nn",
	"nn",
	"tapparella",	// 8
	"nn",
	"nn",
	"nn",
	"nn",
	"nn",
	"allarme",
	"termostato",
	"nn"
};
// =============================================================================================
void rxBufferLoad(int tries);
char aConvert(char * aData);
void mSleep(int millisec);
// =============================================================================================
char aConvert(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// ===================================================================================






// ===================================================================================
void intHandler(int sig) {
	(void) sig;
    keepRunning = 0;
//	printf("\nCaught signal %d\n", sig); 
	printf("\nINTERRUPTED\n");
//	sprintf(rxbuffer,"\nCaught signal %d\n", sig); 
}
// ===================================================================================
//void intHandlerB(int sig) {
//	printf("\nCaught signal %d\n", sig); 
//	sprintf(rxbuffer,"\nCaught signal %d\n", sig); 
//}
// ===================================================================================



// ===================================================================================
void mSleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void uSleep(int microsec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = microsec * 1000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================

// ===================================================================================
static void print_usage(const char *prog)	// NOT USED
{
	printf("Usage: %s [-vil]\n", prog);
	puts("  -v --verbose \n"
	     "  -i --initialize \n"
	     "  -l --long log \n"
		 );
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 3))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "initialize", 0, 0, 'i' },
			{ "long log",   0, 0, 'l' },
			{ "verbose",    0, 0, 'v' },
			{ "file",       1, 0, 'f' },
			{ "File",       1, 0, 'F' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;
		c = getopt_long(argc, argv, "i v l h f:F:", lopts, NULL);
		if (c == -1)
			return 0;

		switch (c) {
		case 'h':
			print_usage(PROGNAME);
			break;
		case 'f':
			strcpy (logfilename,optarg);
//            printf("filename: %s\n", logfilename); 
			break;
		case 'F':
			strcpy (msgfilename,optarg);
//            printf("filename: %s\n", msgfilename); 
			break;
		case 'v':
			verbose=1;
			printf("Verbose\n");
			break;
		case 'i':
			initialize=1;
			printf("Initialize\n");
			break;
		case 'l':
			log = 2;
			printf("Long log\n");
			break;

		case '?':
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
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
int setFirst(void)
{
  char requestBuffer[32];
  int requestLen = 0;
  int n;

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0x15; // evita memo in eeprom (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'o'; 

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0xF1; // set led lamps std-freq (client mode)

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'M'; 
  requestBuffer[requestLen++] = 'A'; 
  if (initialize)
  {
	 requestBuffer[requestLen++] = '@';
	 requestBuffer[requestLen++] = 'i';
  }
  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'l';

//  requestBuffer[requestLen++] = '@';
//  requestBuffer[requestLen++] = 'h'; 

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'M'; 
  requestBuffer[requestLen++] = 'X'; 
  n = write(fduart,requestBuffer,requestLen);			// scrittura su knxgate

  mSleep(50);
  
  rx_len = 0;
  rx_max = 500;
  rxBufferLoad(100);
  if (verbose) printf("%s\n",rx_buffer);

  requestLen = 0;
  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'Q'; 
  requestBuffer[requestLen++] = 'Q'; 
  n = write(fduart,requestBuffer,requestLen);			// scrittura su knxgate

  rx_max = 16;
  rx_len = 0;
  rxBufferLoad(100);
  if (memcmp(rx_buffer,"KNX ",3) == 0)
  {
	  printf("===============> %s <================\n",rx_buffer);
  }
  else
  {
    printf("%s\n",rx_buffer);
	printf("knxgate connection failed!\n");
	return -1;
  }
  rx_len = 0;
  rx_max = 250;
  return n;
}
// ===================================================================================
void writeFile(void)
{
	strcpy(filename,"discovered");
	fConfig = fopen(filename, "wb");
	if (!fConfig)
	{
	  printf("\nfile open error...");
	  exit(EXIT_FAILURE);
	}

	fprintf(fConfig,"{\"coverpct\":\"false\",\"devclear\":\"true\"}\n"); 

	for (int i=0;i<MAXDEVICE;i++)
	{
		switch(busdevType[i])
		{
				// 0x01:switch       0x03:dimmer   
				// 0x08:tapparella   0x09:tapparella pct   
				// 0x0B generic		 0x0C generic 
				// 0x0E:allarme		 0x0F termostato
				// 0x30-0x37:rele i2c
				// 0x40-0x47:pulsanti i2c
		case 1:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"1\",\"descr\":\"switch %04x\"}\n",i,i); 
			break;
		case 4:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"4\",\"descr\":\"dimmer %04x\"}\n",i,i); 
			break;
		case 8:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"8\",\"maxp\":\"\",\"descr\":\"tapparella %04x\"}\n",i,i); 
			break;
		case 15:
			fprintf(fConfig,"{\"device\":\"%04X\",\"type\":\"15\",\"maxp\":\"\",\"descr\":\"termostato %04x\"}\n",i,i); 
			break;
		}
	}
	fclose(fConfig);
	fConfig = NULL;
}
// ===================================================================================
void rxBufferLoad(int tries)
{
	int r = -1;
	int loop = 0;
	char sbyte;
	rx_len = 0;
	if (tries < 10)
	{
		r = read(fduart, &sbyte, 1);
		if ((r) && (sbyte))
		{
			rx_max = sbyte;
			rx_buffer[rx_len++] = sbyte;
		}
		else
			return;
	}
	while ((rx_len <= rx_max) && (loop < tries))
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (rx_len <= rx_max))
		{
			rx_buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx_buffer[rx_len] = 0;        // aggiunge 0x00
}
// ===================================================================================


// ===================================================================================
void niceEnd(void)
{
	printf("nice end\n");
}
// ===================================================================================
int main(int argc, char *argv[])
{
	printf(PROGNAME VERSION "\n");

	if (parse_opts(argc, argv))
		return 0;

	printf("UART_Initialization\n");
	
	fduart = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fduart == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
		return(-1);
	}
	if (logfilename[0])
	{
		buslog = fopen(logfilename, "wb");
	}
	if (msgfilename[0])
	{
		busmsg = fopen(msgfilename, "wb");
	}

	struct termios options;
	tcgetattr(fduart, &options);
//	cfsetispeed(&options, B115200);
//	cfsetospeed(&options, B115200);

	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | CSTOPB;		//<Set baud rate - 2 bits stop?
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fduart, TCIFLUSH);

	tcsetattr(fduart, TCSANOW, &options);

	printf("initialized - OK\n");

	// First write to the port
	int n = setFirst();
 	if (n < 0) 
	{
		perror("Write failed - ");
		return -1;
	}
	else
		if (verbose)
			printf("write - OK\n");

	for (int i=0; i<MAXDEVICE; i++) 
	{
		busdevType[i] = 0;
	}

	mSleep(100);
	rx_max = 500;
	rxBufferLoad(100);	// discard uart input
	int i;

	
	signal(SIGINT, intHandler);
	/*
	signal(1, intHandlerB);

	signal(3, intHandlerB);
	signal(4, intHandlerB);
	signal(5, intHandlerB);
	signal(6, intHandlerB);
	signal(7, intHandlerB);
	signal(8, intHandlerB);
	*/
//	atexit(niceEnd);
	// ====================================================================================
	while (keepRunning)
	{
		mSleep(1);
		rx_max = 32;
		rxBufferLoad(3);	// discard uart input
		if (rx_len > 1) 
		{
			if (verbose)
			{
				fprintf(stderr,"-rx-> ");	// scrittura a video
				for (i=1; i<=rx_len; i++)
				{
					fprintf(stderr,"%02X ",rx_buffer[i]);
				}
				fprintf(stderr,"\n");	// scrittura a video
			}


			if ((rx_len > 7) && ((rx_buffer[1] & 0xF0) == 0xB0) && (log>1)) 
			{
				int msglen=(rx_buffer[6] & 0x0F);
				if (msglen > 0)
				{
					msglen--;
					int ix=9;
					printf("%02x " BLU "[%02X%02X]" GRN "[%02X%02X]" NRM " %02x %02x " RED BOLD "{%02X",rx_buffer[1],rx_buffer[2],rx_buffer[3],rx_buffer[4],rx_buffer[5],rx_buffer[6],rx_buffer[7],rx_buffer[8]);
					if (buslog)
					{
						fprintf(buslog,"%02x [%02X%02X][%02X%02X] %02x %02x {%02X",rx_buffer[1],rx_buffer[2],rx_buffer[3],rx_buffer[4],rx_buffer[5],rx_buffer[6],rx_buffer[7],rx_buffer[8]);
					}
					if (busmsg)
					{
						fprintf(busmsg,"[%02X%02X]->[%02X%02X] : {%02X",rx_buffer[2],rx_buffer[3],rx_buffer[4],rx_buffer[5],rx_buffer[8]);
					}
					while (msglen)
					{
						printf(" %02X",rx_buffer[ix]);
						if (buslog)
						{
							fprintf(buslog," %02X",rx_buffer[ix]);
						}
						if (busmsg)
						{
							fprintf(busmsg," %02X",rx_buffer[ix]);
						}
						ix++;
						msglen--;
					}
					printf("}" NRM " %02x\n",rx_buffer[ix]);
					if (buslog)
					{
						fprintf(buslog,"} %02x\n",rx_buffer[ix]);
					}
					if (busmsg)
					{
						fprintf(busmsg,"}\n");
					}
				}
			}


			// ?? riceve [y] 0C 0B 31 81 (abbreviato)

			if ((rx_len > 7) && ((rx_buffer[1] & 0xF0) == 0xB0) && (rx_buffer[6] == 0xE1) && (log<2)) 
			{
// 08 B4 10 0C 0B 31 E1 00 81 0D
//				WORD_VAL busfrom;
				WORD_VAL busdevice;
//				char busrequest = rx_buffer[6];	  // E1

				busdevice.Val = 0; 
				busdevice.byte.HB = rx_buffer[4]; // 0B 
				busdevice.byte.LB = rx_buffer[5]; //    31
//				busfrom.byte.HB	  = rx_buffer[2]; // 10 
//				busfrom.byte.LB   = rx_buffer[3]; //    0C
				char buscommand   = rx_buffer[8]; // 81
				char devtype = 0;

				// 1:switch       3:dimmer   
				// 8:tapparella   9:tapparella pct   
				// 0B generic    0C generic 
				// 0E:allarme	 0F termostato

				{
					if ((buscommand == 0x80) || (buscommand == 0x81))		// switch/luce
						devtype = 1;

					if ((buscommand == 0x88) || (buscommand == 0x89))		// dimmer
					{
						devtype = 4;
						busdevice.byte.LB--;
					}

					int ixdevice = (busdevice.Val & MAXDEVICE);

					if ((devtype) && (busdevType[ixdevice] == 0))
					{
						busdevType[ixdevice] = devtype;
						printf(GRN BOLD "NUOVO -> [%04X] - tipo %02u -> %s " NRM "\n",busdevice.Val, devtype, type_descri[(int)devtype]);
					}
					else
					if ((devtype) && (busdevType[ixdevice] == 1) && (devtype == 4))
					{
						busdevType[ixdevice] = devtype;
						printf(YEL BOLD "VARIATO -> [%04X] - tipo %02u -> %s " NRM "\n",busdevice.Val, devtype, type_descri[(int)devtype]);
					}
					else
					if ((devtype) && (busdevType[ixdevice] != devtype))
					{
	//					busdevType[ixdevice] = devtype;
						printf(RED BOLD "INCOERENTE -> [%04X] - tipo %02u -> %s " NRM "\n",busdevice.Val, devtype, type_descri[(int)devtype]);
					}

	//				if ((buscommand == 0x88) || (buscommand == 0x89))		// dimmer
	//				{
	//					busdevType[ixdevice+1] = 24;
	//				}
				}
			}
		}
	}

  // Don't forget to clean up
	close(fduart);
	if (buslog)
	{
		fclose(buslog);
		buslog = NULL;
	}
	if (busmsg)
	{
		fclose(busmsg);
		busmsg = NULL;
	}
	if (log<2) writeFile();
	printf("end\n");
	return 0;
}
// ===================================================================================
