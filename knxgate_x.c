/* --------------------------------------------------------------------------------------------
 * UART test utility - la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * -------------------------------------------------------------------------------------------*/
// test dimmer
// definire dimmer (4) 0B28     (24) 0B29
// definire dimmer (4) 0B31     (24) 0B32
//
// test validi:
// ricevere msg 81 address 28		@w128 ->> E ha pubblicato value/0B28
// ricevere msg 81 address 31		@w131 ->> E ha pubblicato value/0B31
// ricevere msg 89-88 address 29	@w929 @w829 -> ok ha aumentato value/0b28
// ricevere msg 89-88 address 32	@w932 @w832 -> ok ha aumentato value/0b31
// ricevere msg 81-80 address 29	@w129 @w029 -> ok ha diminuito value/0b28
// ricevere msg 81-80 address 31	@w129 @w029 -> ok ha diminuito value/0b28
// comando mqtt sw on /off 28		ok 28 81    28 80
// comando mqtt sw on /off 31		ok 31 81    31 80
// comando mqtt setlevel  28        ok + 89 29   88 29    -  81 29   80 29    
// comando mqtt setlevel  31		ok + 89 29   88 29    -  81 29   80 29

#define PROGNAME "KNXGATE_X "
#define VERSION  "1.64"
//#define KEYBOARD

// =============================================================================================
#define CONFIG_FILE "knxconfig"
// =============================================================================================

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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#include <tcpserver.h>
#include <udpserver.h>
#include <iostream>
#include "basesocket.h"
#include <functional>
#include <thread>
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/if_link.h>

#include <vector>
#include "knx_mqtt.h"
#include "knx_hue.h"

using namespace std;
// =============================================================================================
		time_t	rawtime;
struct	tm *	timeinfo;
int		timeToexec = 0;
char	immediatePicUpdate = 0;
char	verbose = 0;	// 1=verbose     2=verbose+      3=debug      4=trace
// =============================================================================================
char    huegate = 0;		// simulazione hue gate per alexa
char    mqttgate = 0;		// connessione in/out a broker mqtt
char    huemqtt_direct = 0;	// 1: ponte diretto hue -> mqtt (stati)     2: ponte hue -> mqtt (comandi)
char    uartgate = 1;		// connessione in/out serial0 (uart)
char	mqttbroker[32] = {0};
char	user[24] = {0};
char	password[24] = {0};
// =============================================================================================
struct termios tios_bak;
struct termios tios;
int	   fduart = 0;
// =============================================================================================
FILE   *fConfig;
char	filename[64];
int		timeToClose = 0;
char	previous_address = 0;
// =============================================================================================
  typedef union _WORD_VAL
  {
    uint16_t  Val;
    char v[2];
    struct
    {
        char LB;
        char HB;
    } byte;
  } WORD_VAL;
// =============================================================================================
char axTOchar(char * aData);
uint16_t  axTOint (char * aData);
char aTOchar (char * aData);
uint16_t  aTOint  (char * aData);

static char parse_opts(int argc, char *argv[]);
void uSleep(int microsec);
void mSleep(int millisec);
// =============================================================================================
#define MAXDEVICE 0x1FFF	//
char busdevType[MAXDEVICE] = {0};
				// 0x01:1-switch				
				// 0x04:4-dimmer on/off			0x18:24-dimmer up/dw		   
				// 0x08:8-tapparella stop		0x09:9-tapparella pct stop   
				// 0x12:18-tapparella up/dwn 	0x13:19-tapparella pct up/dwn
				// 0x0B:11-generic				0x0C:12-generic 
				// 0x0E:14-:allarme				0x0F:15-termostato
				// 0x30-0x37:rele i2c
				// 0x40-0x47:pulsanti i2c

// tapparelle e tapparelle pct:
//            ogni tapparella va censita con 2 indirizzi:
//                 tipo  8 o  9 l'indirizzo base, quello di STOP	(cmd 0x80)
//                 tipo 19 o 19 l'indirizzo base+1, quello di OPEN (0x80) /CLOSE (0x81)
//            in mqtt i topic di stato e di posizione appaiono sempre e solo sull'indirizzo base
//                    i topic di comando vanno sempre e solo dati sull'indirizzo base
//
// dimmer     ogni dimmer va censito con 2 indirizzi: 
//                 tipo  4 l'indirizzo base, quello di ON (0x81) /OFF (0x80)
//                 tipo  5 l'indirizzo base+1, quello di MORE (0x89 - 0x88) / LESS (0x81 - 0x80)
//            in mqtt i topic di stato e di posizione appaiono sempre e solo sull'indirizzo base
//                    i topic di comando vanno sempre e solo dati sull'indirizzo base
//         

char busdevHue [MAXDEVICE] = {0};

extern std::vector<hue_device_t> _devices;
std::vector<hue_knx_queue> _schedule;
std::vector<bus_knx_queue> _schedule_b;
// =============================================================================================
enum _TCP_SM
{
    TCP_FREE = 0,
    TCP_SOCKET_OK,
    TCP_BIND_OK,
    TCP_LISTENING,
    TCP_CONNECTING,
    TCP_CONNECTED,
    TCP_RECEIVED
} sm_tcp = TCP_FREE;
// ===================================================================================
    int server_file_descriptor, new_connection;
    long tcpread;
    struct sockaddr_in server_address, client_address;
    socklen_t server_len, client_len;
    int  opt = 1;
    char tcpBuffer[1024] = {0};
	char tcpuart = 0;
// =============================================================================================
char	sbyte;
char    rx_prefix;
char    rx_buffer[255];
int     rx_len;
int     rx_max = 250;
char    rx_internal;
// ===================================================================================
int  setFirst(void);
static void print_usage(const char *prog);
static char parse_opts(int argc, char *argv[]);
void uSleep(int microsec);
void mSleep(int millisec);
void initDevice (void);
void UART_start(void);
int  tcpJarg(char * mybuffer, const char * argument, char * value);
void rxBufferLoad(int tries);
int	 waitReceive(char w);
void bufferPicLoad(char * decBuffer);
void BufferMemo(char * decBuffer, char hueaction);
void mqtt_dequeueExec(void);
void hue_dequeueExec(void);
// ===================================================================================
#ifdef KEYBOARD
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
#endif
// =============================================================================================
char axTOchar(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (char) ret;
}
// =============================================================================================
uint16_t axTOint(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 16);
    return (uint16_t) ret;
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
int setFirst(void)
{
  if (!uartgate) return 0;
  char requestBuffer[24];
  int requestLen = 0;
  int n;

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0x15; // evita memo in eeprom (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 0xF1; // set led lamps std-freq (client mode)

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'M'; // (in 0x17)
  requestBuffer[requestLen++] = 'X'; // (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'Y'; // (in 0x17)
  requestBuffer[requestLen++] = '1'; // (in 0x17)

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'F'; // (in 0x17)
  requestBuffer[requestLen++] = '2';

  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'O';
  requestBuffer[requestLen++] =  4;  // domotic_options;

  //  requestBuffer[requestLen++] = '@';
//  requestBuffer[requestLen++] = 'U'; // gestione tapparelle senza percentuale
//  requestBuffer[requestLen++] = '9';

  requestBuffer[requestLen++] = '�';
  requestBuffer[requestLen++] = 'l'; // (in 0x17)

  n = write(fduart,requestBuffer,requestLen);			// scrittura su knxgate

  mSleep(50);
  rx_len = 0;
  rxBufferLoad(100);

  requestLen = 0;
  requestBuffer[requestLen++] = '�';
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
	printf("KNXGATE connection failed!\n");
	return -1;
  }
  rx_len = 0;
  rx_max = 250;
  return n;
}
// ===================================================================================
static void print_usage(const char *prog)	// NOT USED
{
	printf("Usage: %s [-uvHBDN]\n", prog);
	puts("  -u --picupdate  immediate update pic eeprom \n"
		 "  -v --verbose 1/2/3  \n"
		 "  -H --huegate interface(alexa)\n"
		 "  -B --broker address  broker name/address:port (default localhost)\n"
		 "  -U --broker username\n"
		 "  -P --broker password\n"
		 "  -D --direct connection hue->mqtt  1:state  2:command\n"
//		 "  -N --nouart no serial connection\n"		// comando privato
		 );
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 9))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "picupdate",  0, 0, 'u' },
			{ "verbose",    2, 0, 'v' },
			{ "huegate",    0, 0, 'H' },
			{ "broker",     2, 0, 'B' },
			{ "user",       2, 0, 'U' },
			{ "password",   2, 0, 'P' },
			{ "direct",     2, 0, 'D' },
			{ "nouart",     0, 0, 'N' },
			{ "help",		0, 0, '?' },
			{ NULL, 0, 0, 0 },
		};
		int c;
		c = getopt_long(argc, argv, "uv::HB::U:P:DNh", lopts, NULL);
		if (c == -1)
			return 0;

		switch (c) {
		case 'h':
			print_usage(PROGNAME);
			break;
		case 'u':
			immediatePicUpdate = 1;
			printf("Immediate pic update\n");
			break;
		case 'H':
			printf("HUE interface on\n");
			huegate = 1;
			break;
		case 'D':
			printf("direct connection HUE->MQTT\n");
			// 1: ponte diretto hue -> mqtt (stati)     2: ponte hue -> mqtt (comandi)
			if (optarg) 
				huemqtt_direct=aTOchar(optarg);
			if ((huemqtt_direct == 0) || (huemqtt_direct > 2)) huemqtt_direct = 1;		// simulazione hue gate per alexa
			break;
		case 'N':
			printf("no uart serial connection\n");
			uartgate = 0;	// senza connessione uart
			break;
		case 'B':
			if (optarg) 
				strcpy(mqttbroker, optarg);
			else
				strcpy(mqttbroker,"localhost:1883");

			mqttgate = 1;		// simulazione hue gate per alexa
			break;
		case 'U':
			if (optarg) 
				strcpy(user, optarg);
			break;
		case 'P':
			if (optarg) 
				strcpy(password, optarg);
			break;
		case 'v':
			if (optarg) 
				verbose=aTOchar(optarg);
			if (verbose == 0) verbose=1;
			if (verbose > 9) verbose=9;

			printf("Verbose %d\n",verbose);
			break;

		case '?':
            printf("Unknown option `-%c'.\n", optopt);
			return 2;		

		default:
			print_usage(argv[0]);
			break;
		}
	}
	return 0;
}
// ===================================================================================
void uSleep(int microsec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = microsec * 1000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void mSleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}

// ===================================================================================
void initDevice (void)
{
  // Free the name for each device
  for (auto& device : _devices) {
    free(device.name);
  }
  // Delete devices
  _devices.clear();

  // Delete schedule
  _schedule.clear();
  _schedule_b.clear();
}
// ===================================================================================
void UART_start(void)
{
	if (!uartgate) return;

	printf("UART_Initialization\n");
	fduart = -1;
	
	fduart = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (fduart == -1) 
	{
		perror("open_port: Unable to open /dev/serial0 - ");
        exit(EXIT_FAILURE);
	}

	struct termios options;
	tcgetattr(fduart, &options);
//	cfsetispeed(&options, B115200);
//	cfsetospeed(&options, B115200);
//	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_cflag = B115200 | CS8 | CLOCAL | CREAD | CSTOPB;		//<Set baud rate - 2 bits stop?
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(fduart, TCIFLUSH);
	tcsetattr(fduart, TCSANOW, &options);
}
// ===================================================================================
int tcpJarg(char * mybuffer, const char * argument, char * value)
{
  int rc = 0;
  char* p1 = strstr(mybuffer, argument); // cerca l'argomento
  if (p1)
  {
//  char* p2 = strstr(p1, ":");          // cerca successivo :
    char* p2 = strchr(p1, ':');          // cerca successivo :
    if (p2)
    {
//    char* p3 = strstr(p2, "\"");       // cerca successivo "
      char* p3 = strchr(p2, '\"');       // cerca successivo "
      if (p3)
      {
        p3++;
        while ((*p3 != '\"') && (rc < 120))
        {
          *value = *p3;
		  value++;
		  p3++;
		  rc++;
		}
      }
    }
  }
  *value = 0;
  return rc;
}
// ===================================================================================
void rxBufferLoad(int tries)
{
	if (!uartgate)  return;

	int r;
	int loop = 0;

    while ((rx_len < rx_max) && (loop < tries))
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (rx_len < rx_max))
		{
			if (verbose) fprintf(stderr,"%02x ", sbyte);	// scrittura a video
			rx_buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx_buffer[rx_len] = 0;        // aggiunge 0x00
	if ((verbose) && (rx_len)) fprintf(stderr," - ");	// scrittura a video
}
// ===================================================================================
int	waitReceive(char w)
{
	if (!uartgate) return 0;

	int r;
	int loop = 0;

	if (verbose>2) fprintf(stderr,"wait answer: %2x\n", w);	// scrittura a video
    while (loop < 10000) // 1 sec
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if (r > 0) 
		{
			if (verbose>2) fprintf(stderr,"received: %2x\n", sbyte);	// scrittura a video
			if (sbyte == w) 
			{
				if (verbose>2) fprintf(stderr,"OK\n");	// scrittura a video
				return 1;
			}
		}
		else
			loop++;
		uSleep(100);
    }
//			if (verbose) fprintf(stderr,"%2x ", sbyte);	// scrittura a video
	return 0;
}
// ===================================================================================
void bufferPicLoad(char * decBuffer)
{
  char busid[8];
  WORD_VAL  device;
  char devtype;
  char alexadescr[32] = {0};
  char stype[8];

  tcpJarg(decBuffer,"\"device\"",busid);
  if (busid[0] != 0)
  {
	device.Val = axTOint(busid);
	if ((device.Val) && (uartgate)) 
	{
	  tcpJarg(decBuffer,"\"descr\"",alexadescr);
	  tcpJarg(decBuffer,"\"type\"",stype);
	  devtype = aTOchar(stype);
//	  devType[(int)device] = devtype;

	  WORD_VAL maxp;
	  maxp.Val = 0;

	  if (devtype == 9)			// w6 - aggiorna tapparelle pct su pic
	  {
		char smaxpos[8];
		tcpJarg(decBuffer,"\"maxp\"",smaxpos);

		maxp.Val = aTOint(smaxpos);
		if (maxp.Val > 250) maxp.Val = 250;
	  }

	  if ((devtype == 8) || (devtype == 9) || (devtype == 4)) 			// w6 - aggiorna tapparelle pct su pic
	  {
		rx_len = 0;
		rxBufferLoad(10);	// discard uart input

		if (verbose) fprintf(stderr,"req 8 base\n");
		char requestBuffer[16];
		int  requestLen = 0;
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'U';
		requestBuffer[requestLen++] = '8';
		requestBuffer[requestLen++] = device.byte.HB;     // device id - line sector
		requestBuffer[requestLen++] = device.byte.LB;     // device id - address
		previous_address			= device.byte.LB;     // device id - address
		requestBuffer[requestLen++] = devtype;			  // device type
		requestBuffer[requestLen++] = 0;				  // move address
		requestBuffer[requestLen++] = maxp.byte.LB;       // max position L
		write(fduart,requestBuffer,requestLen);			  // scrittura su knxgate

		if (waitReceive('k') == 0)
			fprintf(stderr,"  -->PIC communication ERROR...\n");
	  }

	  if ((devtype == 18) || (devtype == 19) || (devtype == 24)) 			// w6 - aggiorna tapparelle pct su pic
	  {
		rx_len = 0;
		rxBufferLoad(10);	// discard uart input

		if (verbose) fprintf(stderr,"req 8 move\n");
		char requestBuffer[16];
		int  requestLen = 0;
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'U';
		requestBuffer[requestLen++] = '8';
		requestBuffer[requestLen++] = device.byte.HB;     // device id - line sector
		requestBuffer[requestLen++] = previous_address;     // device id - address
		requestBuffer[requestLen++] = devtype;			  // device type
		requestBuffer[requestLen++] = device.byte.LB;	  // move address
		requestBuffer[requestLen++] = maxp.byte.LB;       // max position L
		write(fduart,requestBuffer,requestLen);			  // scrittura su knxgate

		if (waitReceive('k') == 0)
			fprintf(stderr,"  -->PIC communication ERROR...\n");
	  }

	} // deviceX > 0
  }  // busid != ""
  else
  {
	  char cover[8];
	  tcpJarg(decBuffer,"\"coverpct\"",cover);
	  if ((strcmp(cover,"false") == 0) && (uartgate)) 
	  {
		rx_len = 0;
		rxBufferLoad(10);	// discard uart input
		write(fduart,"�U9",3);			// scrittura su knxgate
		if (waitReceive('k') == 0)
			fprintf(stderr,"  -->PIC communication ERROR...\n");
	  }  // cover == "false"
  }
}	
// ===================================================================================
void BufferMemo(char * decBuffer, char hueaction)  //  hueaction 1=add device
{
  char busid[8];
  int  device;
  char devtype;
  char alexadescr[32] = {0};
  char stype[8];

  tcpJarg(decBuffer,"\"device\"",busid);
  if (busid[0] != 0)
  {
	device = axTOint(busid);
	if (device)
	{
	  tcpJarg(decBuffer,"\"descr\"",alexadescr);
	  tcpJarg(decBuffer,"\"type\"",stype);
	  devtype = aTOchar(stype);
	  busdevType[(int)device] = devtype;
/*
	  if (devtype == 8)
		  busdevType[(int)device+1] = 18;
	  if (devtype == 9)
		  busdevType[(int)device+1] = 19;
	  if (devtype == 4)
		  busdevType[(int)device+1] = 24;
*/
	  printf("device %04X tipo %02u - %s \n",device,devtype,alexadescr);

	  if ((hueaction) && (devtype < 11))		// w6 - alexa non ha bisogno dei types 0x0B 0x0C (11-12) 0x0E 0x0F 0x12 0x13 (18 19) - usa solo 01-04-08-09
	  {
          if (devtype == 9)
			busdevHue[(int)device] = addDevice(alexadescr, 1,device);
          else
			busdevHue[(int)device] = addDevice(alexadescr, 128,device);
	  }
	} // deviceX > 0
  }  // busid != ""
}	
// ===================================================================================
void mqtt_dequeueExec( void)
{
	int  m = 0; // _schedule.begin();
	WORD_VAL  busid;
	busid.Val = _schedule_b[m].busid;
	char bustype = busdevType[(int)busid.Val];
	if (bustype == 0) 
	{
		bustype = _schedule_b[m].bustype;
		busdevType[(int)busid.Val] = bustype;
	}
//	int  hueid = (int) _schedule_b[m].hueid;
	char command = _schedule_b[m].buscommand;
	char value = _schedule_b[m].busvalue;
//	char request = _schedule_b[m].busrequest;
	char from = (_schedule_b[m].busfrom & 0xFF);
//	_schedule_b.erase(_schedule_b.begin());

//	printf("dequeued - id:%02x  command:%02x  value:%d  bus:%02x  type:%02x  des: %s\n", hueid, huecommand, pctvalue, busid, bustype, _devices[hueid].name);
//	// comandi  01 accendi       02 spegni     04 alza / aumenta       05 abbassa 

	char requestBuffer[32];
	int requestLen = 0;
	if ((command == 0xFF) && (busid.Val != 0) && (bustype == 0xDB))	// 0xDB = 219
	{
// debug command
		fprintf(stderr,"********* DEBUG TEST ************\n");
		char topic[32];
		char payload[8];
		strcpy(topic, "knx/switch/set/0D01");
		strcpy(payload, "ON");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D02");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D03");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D04");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D05");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D06");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D07");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D08");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D09");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		strcpy(topic, "knx/switch/set/0D0A");

		fprintf(stderr,"** pub set **\n");
		publish(topic, payload, 0); 
		fprintf(stderr,"********* DEBUG TEST OK *********\n");
	}
	else
	if ((command == 0xFF) && (busid.Val != 0) && (bustype == 4))
	{
	  requestBuffer[requestLen++] = '�';
	  requestBuffer[requestLen++] = 'm'; 
	  requestBuffer[requestLen++] = busid.byte.HB; // to   device
	  requestBuffer[requestLen++] = busid.byte.LB; // to   device
	  requestBuffer[requestLen++] = value;	// %
	}
	else
	if ((command == 0xFF) && (busid.Val != 0) && (bustype == 9))
	{
	  requestBuffer[requestLen++] = '�';
	  requestBuffer[requestLen++] = 'u'; 
	  requestBuffer[requestLen++] = busid.byte.HB; // to   device
	  requestBuffer[requestLen++] = busid.byte.LB; // to   device
	  requestBuffer[requestLen++] = value;	// %
	}
	else
	if ((command >= 0xF0) && (busid.Val != 0) && ((bustype == 8) || (bustype == 9) || (bustype == 18) || (bustype == 19)))
	{
	  requestBuffer[requestLen++] = '�';
	  requestBuffer[requestLen++] = 'u'; 
	  requestBuffer[requestLen++] = busid.byte.HB; // to   device
	  requestBuffer[requestLen++] = busid.byte.LB; // to   device
	  requestBuffer[requestLen++] = command;	// %
	}
	else
	if ((command != 0xFF) && (busid.Val != 0) && (bustype != 0))
	{
	    if ((bustype == 11) && (_schedule_b[m].busbuffer[0] > 0))	
		{
		  requestBuffer[requestLen++] = '�';
		  requestBuffer[requestLen++] = 'j';    // 0x   (@j: invia a pic da MQTT cmd esteso da inviare sul bus)
		  requestBuffer[requestLen++] = from;   // from device
		  requestBuffer[requestLen++] = busid.byte.HB; // to   device
		  requestBuffer[requestLen++] = busid.byte.LB; // to   device
		  int iLen = _schedule_b[m].busbuffer[0]; 
		  int iX = 0;
		  while (iX<=iLen)
			  {requestBuffer[requestLen++] = _schedule_b[m].busbuffer[iX++];} // length + stream command
		}
		else
		{
		  requestBuffer[requestLen++] = '�';
		  requestBuffer[requestLen++] = 'y';    // 0x79 (@y: invia a pic da MQTT cmd standard da inviare sul bus)
		  requestBuffer[requestLen++] = from;   // from device
		  requestBuffer[requestLen++] = busid.byte.HB; // to   device
		  requestBuffer[requestLen++] = busid.byte.LB; // to   device
		  requestBuffer[requestLen++] = command;// command
		}
	}

	_schedule_b.erase(_schedule_b.begin());

	if (requestLen > 0)
	{
		if (uartgate) write(fduart,requestBuffer,requestLen);			// scrittura su knxgate
		printf("KNX cmd from mqtt: %c %c ",requestBuffer[0],requestBuffer[1]);
		for (int i=2;i<requestLen;i++)
		{
			printf("%02X ",requestBuffer[i]);
		}
		printf("\n");
	}
}
// ===================================================================================
void hue_dequeueExec( void)
{
	int  m = 0; // _schedule.begin();
	int  hueid = (int) _schedule[m].hueid;
	char huecommand = _schedule[m].huecommand;
	char pctvalue = _schedule[m].pctvalue;
	char huevalue = _schedule[m].huevalue;
	WORD_VAL busid;
	busid.Val = _devices[hueid].busaddress;
	char bustype = busdevType[(int)busid.Val];
	_schedule.erase(_schedule.begin());

	printf("dequeued - id:%02x  command:%02x  value:%d  bus:%04x  type:%02x  des: %s\n", hueid, huecommand, pctvalue, busid.Val, bustype, _devices[hueid].name);
	// comandi  01 accendi       02 spegni     04 alza / aumenta       05 abbassa 

	char requestBuffer[16];
	int requestLen = 0;

	char command = 0;		// da passare a esecuzione comando mqtt
	char buscommand = 0;	// da inviare sul bus
	int  pct = 0;
	char stato = _devices[hueid].state;
	switch (bustype)
	{
	// --------------------------------------- SWITCHES -------------------------------------------
	case 1:
	  if (huecommand == 1) // accendi <----------------
	  {
		command = 1;
		buscommand = 0x81;
		setState(hueid, stato | 1, 128);
	  }
	  else if (huecommand == 2) // spegni  <----------------
	  {
		command = 0;
		buscommand = 0x80;
		setState(hueid, stato & 0xFE, 128);
	  }
	  else
		break;
	    
	// comando �y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = 0x01;    // from device id
		requestBuffer[requestLen++] = busid.byte.HB;   // to device id
		requestBuffer[requestLen++] = busid.byte.LB;   // to device id
		requestBuffer[requestLen++] = buscommand; // command char
	  break;


	case 4:
	  // --------------------------------------- LIGHTS DIMM ------------------------------------------
	  if (huecommand == 1) // accendi  <----------------
	  {
		command = 1;
		buscommand = 0x81;
		setState(hueid, stato | 1, 0);
	// comando �y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = 0x01;    // from device id
		requestBuffer[requestLen++] = busid.byte.HB;   // to device id
		requestBuffer[requestLen++] = busid.byte.LB;   // to device id
		requestBuffer[requestLen++] = buscommand; // command char
	  }
	  else if (huecommand == 2) // spegni <----------------
	  {
		command = 0;
		buscommand = 0x80;
		setState(hueid, stato & 0xFE, 0);
	// comando �y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = 0x01;    // from device id
		requestBuffer[requestLen++] = busid.byte.HB;   // to device id
		requestBuffer[requestLen++] = busid.byte.LB;   // to device id
		requestBuffer[requestLen++] = buscommand; // command char
	  }
	  else if ((huecommand == 3) || (huecommand == 4) || (huecommand == 5)) // cambia il valore  <----------------
	  {
		setState(hueid, 1, huevalue);
	// comando �y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'm';
		requestBuffer[requestLen++] = busid.byte.HB;   // to device id
		requestBuffer[requestLen++] = busid.byte.LB;   // to device id
		requestBuffer[requestLen++] = pctvalue; // command char
	  }
	  break;


	case 8:
	//              case 18:
	  // --------------------------------------- COVER ---------------------------------------------------
	  if ((huecommand == 2) || (huecommand == 1)) // spegni / ferma  <--oppure accendi------
	  {
	    command = 0;
		buscommand = 0x80;
		setState(hueid, stato | 0xC1, huevalue); // 0xc1: dara' errore ma almeno evita il blocco
	  }
	  else if (huecommand == 4) // alza  <----------------
	  {
	    command = 1;
		buscommand = 0x80;
		busid.Val++;
		setState(hueid, stato | 0xC0, huevalue); // 0xc0: dopo aver inviato lo stato setta value a 128
	  }
	  else if (huecommand == 5) // abbassa  <----------------
	  {
	    command = 2;
		buscommand = 0x81;
		busid.Val++;
		setState(hueid, stato | 0xC0, huevalue); // 0xc0: dopo aver inviato lo stato setta value a 128
	  }
	  else
		break;

	// comando �y<destaddress><source><type><command>
		requestBuffer[requestLen++] = '�';
		requestBuffer[requestLen++] = 'y';
		requestBuffer[requestLen++] = 0x01;    // from device id
		requestBuffer[requestLen++] = busid.byte.HB;   // to device id
		requestBuffer[requestLen++] = busid.byte.LB;   // to device id
		requestBuffer[requestLen++] = buscommand; // command char
	  break;

	case 9:
	//              case 19:
	  // --------------------------------------- COVERPCT alexa------------------------------------------------
	  busid.Val++;
//	  setState(hueid, 1, huevalue);  // coverpct - lo stato deve sempre essere ON
	  pct = pctvalue;
//	  pct *= 100;
//	  pct /= 255;

	  if (huecommand == 2) // spegni / ferma  <----------------
	  {
		command = 0;
		buscommand = 0x80;
		pct = 0;
//		if (pctvalue < 5)
//		  setState(hueid, 0, huevalue);
	  }
	  else if (huecommand == 1) // accendi (SU) <----------
	  {
		command = 1;
		buscommand = 0x80;
	  }
	  else if (huecommand == 4) // alza  <----------------
	  {
		command = 11;
		buscommand = 0x80;
	  }
	  else if (huecommand == 5) // abbassa  <----------------
	  {
		command = 12;
		buscommand = 0x81;
	  }
	  else // (huecommand == 3) // non cambia  <----------------
	  {
		command = 0x89;
		break;
	  }

	  requestBuffer[requestLen++] = '�';
	  requestBuffer[requestLen++] = 'u';
	  requestBuffer[requestLen++] = busid.byte.HB;   // to device id
	  requestBuffer[requestLen++] = busid.byte.LB;   // to device id
	  requestBuffer[requestLen++] = (char) pct; // command char
	  break;
	} // end switch

	if (requestLen > 0)
	{
		if (uartgate) write(fduart,requestBuffer,requestLen);			// scrittura su knxgate
		printf("KNX cmd from HUE: %c %c ",requestBuffer[0],requestBuffer[1]);
		for (int i=2;i<requestLen;i++)
		{
			printf("%02X ",requestBuffer[i]);
		}
		printf("\n");



		if	(huemqtt_direct)
		{// ponte diretto hue -> mqtt
			bus_knx_queue busdata;
			busdata.busid = busid.Val;
			busdata.bustype = bustype;
			busdata.busvalue = (char) pct; 
			busdata.busfrom = 0;
//			busdata.baseaddress = 0;
//			busdata.buscommand = command;
//			printf("mqttr %02x t:%02x c:%02x v:%d\n",busid,bustype,command,(char)pct);
			if	(huemqtt_direct == 1)	// ponte diretto hue alexa -> pubblicazione stati
			{
				busdata.buscommand = buscommand;
				MQTTrequest(&busdata);
			}
			else
			if	(huemqtt_direct == 2)
			{
				busdata.buscommand = command;
				MQTTcommand(&busdata);  // ponte diretto hue alexa -> pubblicazione comandi
			}
		}

	}
}
// ===================================================================================











// ===================================================================================
int main(int argc, char *argv[])
{
#ifdef KEYBOARD
	initkeyboard();
	atexit(endkeyboard);
#endif 

	//	printf(CLR WHT BOLD UNDER PROGNAME BOLD VERSION NRM "\n");

	printf(PROGNAME VERSION "\n");

	if (parse_opts(argc, argv))
		return 0;
	if ((mqttgate == 0) && (huegate == 0))
	{
		print_usage(argv[0]);
		return -1;
	}

	if ((mqttgate == 0) || (huegate == 0))
		huemqtt_direct = 0;	

	UART_start();

	initDevice ();

	if (huegate) 
	{
		if ((HUE_start(verbose)) < 0)
		{
			printf("HUE start failed\n");
			return -1;
		}
	}

	if (mqttgate) 
	{
		printf("MQTT broker connect: %s  - user: %s  password: %s\n", mqttbroker,user,password);
		if (MQTTconnect(mqttbroker,user,password,verbose))
			printf("MQTT open ok\n");
		else
		{
			printf("MQTT connect failed\n");
			return -1;
		}
	}

	if (verbose) fprintf(stderr,"\n");


	// First write to the port
	int c = setFirst();
	if (c < 0) 
	{
		perror("Serial0 write failed - ");
		return -1;
	}
	else
		if (verbose) fprintf(stderr,"Serial0 initialized - OK\n");

	mSleep(20);					// pausa
	rx_len = 0;
	rxBufferLoad(10);	// discard uart input


	char sbyte;
	strcpy(filename,CONFIG_FILE);
	for (int i=0; i<MAXDEVICE; i++) 
	{
		busdevType[i] = 0;
		busdevHue [i] = 0xff;
	}

// preload config file ----------------------------------------------------------------------------
	c = 0;
	fConfig = fopen(filename, "rb");
	if (fConfig)
	{
		char line[128];
		while (fgets(line, 128, fConfig))
		{
		  char busid[8];
		  tcpJarg(line,"\"device\"",busid);
		  if (busid[0] != 0)  c++;
		  if (immediatePicUpdate)
		  {
			bufferPicLoad(line);  
		  }
		  BufferMemo(line, 1);  // 1=add device in hue table
		}
		fclose(fConfig);
		fConfig = NULL;
	}

	if (verbose) fprintf(stderr,"%d devices loaded from file\n",c);

// =======================================================================================================
	while (1)
	{
		if (verbose>3) fprintf(stderr," $");
		if (timeToClose)
		{
			timeToClose--;
			if (timeToClose == 0)
			{
				if (fConfig)
				{
					fclose(fConfig);
					fConfig = NULL;
				}
			}
		}

		mSleep(1);
		rx_internal = 9;
// ---------------------------------------------------------------------------------------------------------------------------------
		if (uartgate) 
			c = read(fduart, &sbyte, 1);			// lettura da knxgate
		else
			c = 0;
		if (c > 0) 
		{
			if (verbose) fprintf(stderr,"\n[%02X] ", sbyte);	// scrittura a video
			rx_len = 0;
			rx_buffer[rx_len++] = sbyte;
			rx_prefix = sbyte;
		    if ((rx_prefix > 0xF0) && (rx_prefix < 0xF9))	// 0xf5 y aa bb cc dd
			{
				rx_internal = 1;
				rx_max = (rx_prefix & 0x0F);
				rx_max++;
			}
			else									// rx: F5 y 31 00 12 00 
			{
				rx_internal = 0;
				rx_max = 255;
			}
			rxBufferLoad(3);
		}
// ---------------------------------------------------------------------------------------------------------------------------------

		if ((rx_internal == 1) && (rx_buffer[1] == 'y') && ((rx_buffer[0] == 0xF6) || (rx_buffer[0] == 0xF7)))   
//		if ((rx_internal == 1) && (rx_buffer[1] == 'y') && ((rx_buffer[0] == 0xF5) || (rx_buffer[0] == 0xF6)))   
		{ // START pubblicazione STATO device 

// address letto dal bus, pu� essere base o move...

  // KNX intero  [9] B4 10 29 0B 65 E1 00 81 7C
  // knx ridotto  [0xF5] [y] 29 0B 65 81

  // knx ridotto  [0xF6] [y] 01 0F 01 81 aa
  // knx ridotto  [0xF7] [y] 01 0F 01 81 01 aa

			WORD_VAL busid;
			char devtype;
			rx_internal = 9;
			if (verbose) fprintf(stderr," (%c) msg\n",rx_buffer[1]);	// scrittura a video

			bus_knx_queue _knxrx;
		    busid.byte.HB = rx_buffer[3];  // to
		    busid.byte.LB = rx_buffer[4];  // to

//			if (vebose>2) fprintf(stderr,"busid %04X\n",busid.Val);

			if (busid.Val > MAXDEVICE)
			{
				printf("INVALID bus-id %04X\n",busid.Val);
				busid.Val = 0;
			}
			devtype = busdevType[(int)busid.Val];
			if (devtype == 0) devtype = 1;  // defult luce
		    _knxrx.busfrom = (int)rx_buffer[2];  // from
			_knxrx.bustype = devtype;
			if ((devtype > 17) && (devtype < 30))
			{
			    busid.byte.LB = rx_buffer[rx_len-1];  // base address
			}
			_knxrx.buscommand = rx_buffer[5];
			_knxrx.busvalue = busdevHue [(int)busid.Val];
//			_knxrx.baseaddress = rx_buffer[rx_len-1];
		    _knxrx.busid = busid.Val; 

	// ==========================================================================================================
//			setState(unsigned char id, char state, unsigned char value) // non indispensabile -
//			if (verbose) fprintf(stderr,"MQTTrequest\n");	// scrittura a video
			MQTTrequest(&_knxrx);
		}

		if ((rx_internal == 1) && ((rx_buffer[1] == 'u') || (rx_buffer[1] == 'm')) && (rx_buffer[0] == 0xF4)) 
		{ // START pubblicazione stato device        [0xF4] [u] 0B 23 31
			WORD_VAL busid;
			char devtype;
			rx_internal = 9;
			if (verbose) fprintf(stderr," %c msg\n",rx_buffer[1]);	// scrittura a video

			bus_knx_queue _knxrx;

		    busid.byte.HB = rx_buffer[2];  // to
		    busid.byte.LB = rx_buffer[3];  // to

			devtype = busdevType[(int)busid.Val];

			_knxrx.busid = busid.Val;
			_knxrx.busvalue = rx_buffer[4];
			_knxrx.buscommand = rx_buffer[1];	// u / m
			_knxrx.busfrom = 0;
			_knxrx.bustype = devtype;
//			_knxrx.baseaddress = 0;

			char hueid = busdevHue[(int)busid.Val];

			if (hueid != 0xff) 
			{
				int pct = ((_knxrx.busvalue * 255)+50) /100;
				setState(hueid, 0xFF, (char)pct);
			}
			MQTTrequest(&_knxrx);
		}


#ifdef KEYBOARD
		c = getinNowait();				// lettura tastiera
		if (c)
		{
			if (verbose) fprintf(stderr,"%c", (char) c);// echo a video
			if (uartgate)
			{
				n = write(fduart,&c,1);			// scrittura su knxgate
				if (n < 0) 
				{
					perror("Write failed - ");
					return -1;
				}
			}
		}
#endif
		if (timeToexec == 0)
		{
			if (_schedule.size() > 0)
			{
				if (verbose>3) fprintf(stderr," H");
				hue_dequeueExec();
				timeToexec = 100;  // msec to wait until next command execute
			}
			else
			if (_schedule_b.size() > 0)
			{
				if (verbose>3) fprintf(stderr," M");
				mqtt_dequeueExec();
				timeToexec = 100;  // msec to wait until next command execute
			}
		}
		else
			timeToexec--;

		if (mqttgate) 
		{
			if (verbose>3) fprintf(stderr," Q");
			publish_dequeue();
		}
		if (mqttgate)
		{
			if (verbose>3) fprintf(stderr," V");
			MQTTverify();
		}
	}
// =======================================================================================================

		
		// Don't forget to clean up
	if (uartgate) close(fduart);
	if (new_connection)	close(new_connection);

    // Close the server before exiting the program.
	if (huegate) 
	{
		HUE_stop();
	}
	if (mqttgate) 
		MQTTstop();

#ifdef KEYBOARD
	endkeyboard();
#endif
	return 0;
}
// ===================================================================================
