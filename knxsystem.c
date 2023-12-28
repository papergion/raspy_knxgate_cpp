/* ---------------------------------------------------------------------------
 * by-me extended LOG
 * la GPIO seriale va abilitata in raspi-config 
 *      - INTERFACING OPTIONS - P6 SERIAL : login shell NO  
 *      -  serial port hardware enabled YES
 * verifica 	ls /dev/serial*
 * ---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
simula un attuatore 1851.2 pronto a ricevere dal centralino un device address
  e/o un application address  con richieste di property-write
------------------------------------------------------------------------------*/

#define PROGNAME "KNXSYSTEM "
#define VERSION  "1.55"
#define TRUE_REPLY 1

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
    uint16_t  Val;
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
#define  MAXDEVICE 0x1FFF	//
char	busdevType[MAXDEVICE] = {0};
int		ixdevice;
uint16_t newAddress;
uint16_t myAddress = 0;
uint16_t newAppAddress = 0;
uint16_t newBlockId = 0;
char     newBlockDisplay = 0;

int		requestctr = 0;
// =============================================================================================
int		fduart = -1;
struct  termios tios_bak;
struct  termios tios;
int     keepRunning = 1;
char    answerMode = 0;   // 1 push config button: acquire device address   
// =============================================================================================
FILE   *buslog;
char	logfilename[64] = {0};
FILE   *fConfig;
char	filename[64];
int		timeToClose = 0;
// =============================================================================================
#define TLGR_MAXLEN  128

int     rx_len;
int     rx_max = TLGR_MAXLEN-1;

// ===================================================================================
typedef union  _EIB_TOP       {
        struct                {             // b4 = 10110100  allarmi
                                            // bc = 10111100  bassa
                                            // b8 = 10111000  normale

                unsigned char   Bottom            : 2;    // 00               bit 1-0
                unsigned char   Priority          : 2;    // Priorita'        bit 3-2
                                                          // 00=sistema
                                                          // 01=allarmi
                                                          // 10=normale
                                                          // 11=bassa
                unsigned char   KNXFormat_Fix     : 1;    // 1                bit 4
                unsigned char   Repeat            : 1;    // 0   ripetizione  bit 5
                unsigned char   KNXFormat_frame   : 2;    // 10               bit 7-6
               } ;
        unsigned char Byte;
} EIB_TOP;
typedef union  _EIB_ADDRESS  {
        struct {
                unsigned char   lineSector        : 8;    //                  
                unsigned char   device            : 8;    //                  
               }            ;
        struct {
                unsigned char   Sector            : 4;    //                  bit 3-2-1-0
                unsigned char   Line              : 4;    //                  bit 7-6-5-4
                unsigned char   Device            : 8;    //                  byte
               }            ;
        struct {
                unsigned char   SubGroupH         : 3;    //                  byte
                unsigned char   MainGroup         : 5;    //
                unsigned char   SubGroupL         : 8;    //                  byte
               }            ;
		uint16_t	Word;
       } EIB_ADDRESS;
typedef union  _EIB_COUNTER       {
        struct {
                unsigned char   DataLength        : 4;    // Data length (1-8)bit 0123
                unsigned char   RoutingCounter    : 3;    //                  bit 4-5-6
                unsigned char   GroupAddress      : 1;    //                  bit 7
               } ;
        struct {
                unsigned char   DataLL	          : 4;    // Data length (1-8)bit 0123
                unsigned char   Lprefix			  : 4;    //                  bit 4-5-6-7
               } ;
        unsigned char Byte;
       } EIB_COUNTER;

typedef union  _EIB_PDU      {
        struct {
                unsigned char   filler            : 2;    //                  bit 01
                unsigned char   Sequence          : 4;    // per NDP o NCD    bit 2345
                unsigned char   Type              : 2;    // 00=UDP           bit 67
               } ;                                        // 01=NDP           bit 67
        unsigned char Byte;
       } EIB_PDU;
                                                          // 10=UCD           bit 67
                                                          // 11=NCD           bit 67
typedef union _EIB_MESSAGE {
// 08 B4 10 0C 0B 31 E1 00 81 0D
  struct {
//      uart len    (1 byte)
        uint8_t	      Ulength;
//      control byte    (1 byte)
        EIB_TOP       TopByte;
//      source address  (2 bytes)
        EIB_ADDRESS   SourceAddress;
//      destin.address  (2 bytes)
        EIB_ADDRESS   DestinationAddress;
//      network protocol data unit (1 byte)
        EIB_COUNTER   Counter;      // DAF, npci; length

//      transport   protocol data unit (1 byte)
        EIB_PDU       Tpdu8;
//      transport   protocol data unit (1 byte)
        EIB_PDU       Tpdu8a;
//      application protocol data unit (il resto)
        unsigned char Apdu[TLGR_MAXLEN-9];  // dal byte 8
  };
  struct {
//      ulen/top/address/address/counter    (7 bytes)
        char		  fill2[9];
//      system transport   protocol data unit (2 bytes)
//      uint16_t      Tpdu16;  // attenzione - disallineato sulla word - non può essere usato
//      system protocol data unit (il resto)
        char		  Spdu[TLGR_MAXLEN-9];  // dal byte 10
  };
  struct {
  char top;
  char data[TLGR_MAXLEN-1];
  };
  char buffer[TLGR_MAXLEN];
} EIB_MESSAGE;
// ===================================================================================

EIB_MESSAGE  rx;
EIB_MESSAGE  tx;
EIB_MESSAGE  prev;

uint16_t	 gatewayAddress = 0;
// =============================================================================================
// ANSWER TABLES

uint16_t sreply_0140[] =   {0x00,0xFF,0x00,0x00,0xE1,0x01,0x40,0xFFFF};  // pdu 0140: reply for pdu 0100 response 1 
//   assigned address          ^^   ^^

uint16_t sreply_0342_1[] = {0x10,0x01,0x10,0xAB,0x6F,0x03,0x42,0x00,0x2C,0x01,0x68,0x0C,0x7F,0x40,0x02,0x20,0x01,0x20,0x01,0x20,0x01,0xFFFF};  // pdu 0342: reply for pdu 0302 response 1 
//   assigned address__________^^___^^   ^^___^^____gateway address     -  parametri del dispositivo (modello 1851.2)

uint16_t sreply_0342_0[] = {0x10,0x01,0x10,0xAB,0x6F,0x03,0x42,0x00,0x2C,0x00,0x68,0x0C,0x7F,0x40,0x02,0x20,0x01,0x20,0x01,0x20,0x01,0xFFFF};  // pdu 0342: reply for pdu 0302 response 0 
//   assigned address__________^^___^^   ^^___^^____gateway address     -  parametri del dispositivo (modello 1851.2)

uint16_t sreply_03E6[] =   {0x10,0x01,0x10,0xAB,0x6B,0x03,0xE6,0x05,0x11,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x0C,0x01,0xFFFF};  // pdu 03E6: reply for pdu 03E7

// =============================================================================================
void rxBufferLoad(int tries);
void mSleep(int millisec);
// =============================================================================================
char aTOchar(char * aData)
{
char *ptr;
long ret;
    ret = strtoul(aData, &ptr, 10);
    return (char) ret;
}
// ===================================================================================
uint16_t wbReverse(unsigned char inp_HB, unsigned char inp_LB) // da valore uint16  big-endian estrae valore uint16 in formato little endian
{
WORD_VAL cout;
	cout.byte.HB = inp_HB;
	cout.byte.LB = inp_LB;
	return cout.Val;
}
// ===================================================================================
uint16_t wReverse(uint16_t inp) // da valore uint16  big-endian estrae valore uint16 in formato little endian
{
WORD_VAL cinp;
WORD_VAL cout;
	cinp.Val = inp;
	cout.byte.HB = cinp.byte.LB;
	cout.byte.LB = cinp.byte.HB;
	return cout.Val;
}
// ===================================================================================
uint16_t wExtract(char * inp) // da stream estrae valore uint16 in formato little endian
{
WORD_VAL cout;
	cout.byte.LB = *inp++;
	cout.byte.HB = *inp;
	return cout.Val;
}
// ===================================================================================
void wReplace(char * out, uint16_t data)
{
WORD_VAL cdata;
	cdata.Val = data;
	*out++ = cdata.byte.LB;
	*out++ = cdata.byte.HB;
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
uint16_t axTOintR(char * aData)
{
uint16_t temp;
uint16_t ret;
	temp = axTOint(aData);
	ret = wReverse(temp);
    return (uint16_t) ret;
}
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
	printf("Usage: %s [-val]\n", prog);
	puts("  -v --verbose \n"
	     "  -n --device address acquisition - new device \n"
		 "  -g --gateway address [xxxx]\n"
		 "  -m --my address [xxxx]\n"
	     "  -f --file log name \n"
		 );
	exit(1);
}
// ===================================================================================
static char parse_opts(int argc, char *argv[])	// NOT USED
{
	if ((argc < 1) || (argc > 7))
	{
		print_usage(PROGNAME);
		return 3;
	}

	while (1) {
		static const struct option lopts[] = {
//------------longname---optarg---short--      0=no optarg    1=optarg obbligatorio     2=optarg facoltativo
			{ "newdevice",	0, 0, 'n' },
			{ "file",		2, 0, 'f' },
			{ "gateway",	2, 0, 'g' },
			{ "my",			2, 0, 'm' },
			{ "verbose",    2, 0, 'v' },
			{ "help",		0, 0, '?' },
			{ NULL,			0, 0,  0  },
		};
		int c;
		c = getopt_long(argc, argv, "n v::h f:g:m:", lopts, NULL);
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
		case 'g':
			gatewayAddress = axTOintR(optarg);
			printf("- gateway address: %04X\n",wReverse(gatewayAddress));
			break;
		case 'm':
			myAddress = axTOintR(optarg);
			printf("- my address: %04X\n",wReverse(myAddress));
			break;
		case 'v':
			if (optarg) 
				verbose=aTOchar(optarg);
			else
				verbose=1;
			if (verbose > 9)  verbose=9;
			printf("Verbose %d\n",verbose);
			break;
		case 'n':
			answerMode = 1;
			printf("push button: acquire new device address - model 1851.2\n");
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

//  requestBuffer[requestLen++] = '@'; 
//  requestBuffer[requestLen++] = 'M'; 
//  requestBuffer[requestLen++] = 'A'; 

  requestBuffer[requestLen++] = '@'; 
  requestBuffer[requestLen++] = 'F'; 
  requestBuffer[requestLen++] = '2';  // esclude ack 

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
  rx_max = TLGR_MAXLEN-1;
  rxBufferLoad(100);
  if (verbose>1) printf("%s\n",rx.buffer);

  requestLen = 0;
  requestBuffer[requestLen++] = '@';
  requestBuffer[requestLen++] = 'Q'; 
  requestBuffer[requestLen++] = 'Q'; 
  n = write(fduart,requestBuffer,requestLen);			// scrittura su knxgate

  rx_max = 16;
  rx_len = 0;
  rxBufferLoad(100);
  if (memcmp(rx.buffer,"KNX ",3) == 0)
  {
	  printf("===============> %s <================\n",rx.buffer);
  }
  else
  {
    printf("%s\n",rx.buffer);
	printf("knxgate connection failed!\n");
	return -1;
  }
  rx_len = 0;
  rx_max = 250;
  return n;
}
// ===================================================================================
void rxBufferLoad(int tries)
{
	int r;
	int loop = 0;
	rx_len = 0;
	char	sbyte;

    while ((rx_len < rx_max) && (loop < tries))
    {
		r = -1;
		r = read(fduart, &sbyte, 1);
	    if ((r > 0) && (rx_len < rx_max))
		{
			if (verbose>2) fprintf(stderr,"%02x ", sbyte);	// scrittura a video
			rx.buffer[rx_len++] = sbyte;
			loop = 0;
		}
		else
			loop++;
		uSleep(90);
    }
    rx.buffer[rx_len] = 0;        // aggiunge 0x00 - rx_len è la lunghezza + 1 (comprende la lunghezza di busta)
								  //   invece rx.buffer[0] contiene la lunghezza di msg
	if ((verbose>2) && (rx_len)) fprintf(stderr," - ");	// scrittura a video
}
// ===================================================================================


// ===================================================================================
void niceEnd(void)
{
	printf("nice end\n");
}

#define ENDOFPDU " "
// ===================================================================================
void printout(char * dati)
{
	printf("%s",dati);
	if (buslog) fprintf(buslog,"%s",dati);
}
// ===================================================================================
void printspdu(int msglen, int ix)
{
	char riga[8];
	msglen--;
	while (ix < msglen)
	{
		sprintf(riga,"%02X ", rx.Spdu[ix++]);
		printout(riga);
	}
	sprintf(riga,ENDOFPDU "\n");
	printout(riga);
}
// ===================================================================================
int rowDisplay(EIB_MESSAGE * knx, int knx_len)
{
(void) knx_len;
    char riga[128];
	int rc = 0;
	int msglen=(knx->buffer[6] & 0x0F);
	if (msglen > 0)
	{
		printf("%02x " BLU "[%02X%02X]" GRN "[%02X%02X]" NRM " %02x " YEL " %02X%02X " RED BOLD "{",knx->buffer[1],knx->buffer[2],knx->buffer[3],knx->buffer[4],knx->buffer[5],knx->buffer[6],knx->buffer[7],knx->buffer[8]);
		if (buslog)
		{
			fprintf(buslog,"%02x [%02X%02X][%02X%02X] %02x %02X%02X {",knx->buffer[1],knx->buffer[2],knx->buffer[3],knx->buffer[4],knx->buffer[5],knx->buffer[6],knx->buffer[7],knx->buffer[8]);
		}
		int ix=9;
		msglen--; // perchè comprende anche il primo byte di TPDU
		while (msglen)
		{
			sprintf(riga," %02X",knx->buffer[ix]);
			printout(riga);
			ix++;
			msglen--;
		}
		printf("}" NRM " %02x\n",knx->buffer[ix]);
		if (buslog)
		{
			fprintf(buslog,"} %02x\n",knx->buffer[ix]);
		}
	}
	return rc;
}
// ===================================================================================
void getGateway(EIB_MESSAGE * knx)
{
	char riga[128];
	if (gatewayAddress == 0)
	{
		gatewayAddress = knx->SourceAddress.Word;
		sprintf(riga,"=>   gateway address: %04X\n",wReverse(gatewayAddress));
		printout(riga);
	}
}
// ===================================================================================
int decodificaTpu(EIB_MESSAGE * knx, int knx_len)
{
(void) knx_len;
int rc = 0;
//-------------------------------------------------------------------
//          0 = non trattato
//          1 = richiesta tasto config							0100
//          2 = test indirizzo device - non è mio				0302
//          3 = test indirizzo device - è mio					0302
//			4 = assegnazione device address						00C0
//			5 = risposta di dispositivo senza indirizzo			0140
//			6 = risposta di dispositivo indirizzato				0140
//			7 = risposta parametri di dispositivo				0342
//			8 = fine configurazione								0380
//			9 = risponde indirizzo applicativo di blocco		03E6
//		   10 = assegna indirizzo applicativo ad un blocco		03E7
//		   11 = property read indirizzo applicativo / blocco	03D5
//		   12 = property response indirizzo applicativo/blocco	03D6
//		   13 = property write indirizzo applicativo/blocco		03D7
//-------------------------------------------------------------------

int msglen=knx->Counter.DataLength; // - 1;
char riga[128];
uint16_t thisDest;

	thisDest = knx->DestinationAddress.Word;
	switch (wbReverse(knx->Tpdu8.Byte, knx->Tpdu8a.Byte))
	{

	case 0x00C0:
		//  B4 10 AB 00 00 E3 00 C0 10 01 C2 
		getGateway(knx);
		newAddress = wExtract(&(knx->Spdu[0]));
		sprintf(riga,"=> PDU 00C0:  gateway assegna al dispositivo indirizzo %04X\n" ENDOFPDU,wReverse(newAddress)); // knx->Spdu[0],knx->Spdu[1]);
		printout(riga);
		sprintf(riga,"            -----> [[[[ %04X ]]]] <----- \n" ENDOFPDU,wReverse(newAddress)); // knx->Spdu[0],knx->Spdu[1]);
		printout(riga);
		rc = 4;
		break;

	case 0x0100: 
		//  B4 10 AB 00 00 E1 01 00 10 
		gatewayAddress = knx->SourceAddress.Word;
		sprintf(riga,"=> PDU 0100:  gateway %04X chiama dispositivo da censire\n" ENDOFPDU, wReverse(gatewayAddress));
		printout(riga);
		rc = 1;
		break;

	case 0x0140:
		//  B4 00 FF 00 00 E1 01 40 14 

		sprintf(riga,"=> PDU 0140:  risponde dispositivo %04X",wReverse(knx->SourceAddress.Word));
		printout(riga);
		if (knx->SourceAddress.Word == 0xFF00)
		{
			sprintf(riga," - senza indirizzo\n" ENDOFPDU);
			rc = 5;
		}
		else
		{
			sprintf(riga," - gia' indirizzato!\n" ENDOFPDU);
			rc = 6;
			if (verbose>1)
			{
				sprintf(riga,"    (myAddress is %04X)\n",wReverse(myAddress));
				printout(riga);
			}
		}
		printout(riga);
		break;

	case 0x0300:
		//   B0 10 01 10 29 61 03 00 05
		getGateway(knx);
		sprintf(riga,"=> PDU 0300:  gateway - diagnosi dispositivo %04X\n" ENDOFPDU,wReverse(thisDest));
		printout(riga);
		if (thisDest == myAddress)
			rc = 3;
		else
		{
			rc = 2;
			if (verbose>1)
			{
				sprintf(riga,"    (myAddress is %04X)\n",wReverse(myAddress));
				printout(riga);
			}
		}

		break;

	case 0x0302:
		//  B4 10 AB 10 01 61 03 02 81 
		getGateway(knx);
		sprintf(riga,"=> PDU 0302:  gateway chiama dispositivo %04X\n" ENDOFPDU,wReverse(thisDest));
		printout(riga);
		if (thisDest == myAddress)
			rc = 3;
		else
		{
			rc = 2;
			if (verbose>1)
			{
				sprintf(riga,"    (myAddress is %04X)\n",wReverse(myAddress));
				printout(riga);
			}
		}
		break;

	case 0x0342:
		//  B0 10 01 10 AB 6F 03 42 00 2C 01 68 0C 7F 40 02 20 01 20 01 20 01 9E - 1851

//		if (knx->SourceAddress.Word == knx->DestinationAddress.Word)  break;

		sprintf(riga,"=> PDU 0342:  %04X risponde - modello %02X - e parametri modello: ",wReverse(knx->SourceAddress.Word), knx->Spdu[3]);
		// modello 68: 1851.2
		//         C6: 01480
		//         C7: 01485
		//         C8: 01481


		printout(riga);
		printspdu(msglen, 4);
		rc = 7;
		break;

	case 0x0380:
		//  B4 10 AB 10 01 61 03 80 03 
		getGateway(knx);
		sprintf(riga,"=> PDU 0380:  gateway invia a %04X cmd reset/fine configurazione\n" ENDOFPDU,wReverse(knx->DestinationAddress.Word));
		printout(riga);
		rc = 8;
		break;



	case 0x03E6:
		//  B0 10 01 10 AB 6B 03 E6 05 11 00 FF 00 FF 00 FF 0C 01 8D - 1851

//						obj = 1 + ((knx->Spdu[0] - 1) / 4); // ? solo per 1851.2 ?
		sprintf(riga,"=> PDU 03E6:  risposta del dispositivo %04X - blocco %02X - l'indirizzo applicativo %02X%02X\n" ENDOFPDU, wReverse(knx->SourceAddress.Word), knx->Spdu[0] ,knx->Spdu[8] ,knx->Spdu[9]);
		printout(riga);
		rc = 9;
		break;

	case 0x03E7:
		// B4 10 AB 10 01 65 03 E7 05 00 0C 01 68 - 1851
		getGateway(knx);
//						obj = 1 + ((knx->Spdu[0] - 1) / 4); // ? solo per 1851.2 ?
		if (knx->Spdu[1] == 0x02)  // disassocia
			sprintf(riga,"=> PDU 03E7:  toglie dal dispositivo %04X - blocco %02X - l'indirizzo applicativo %02X%02X\n" ENDOFPDU, wReverse(knx->DestinationAddress.Word),  knx->Spdu[0] ,knx->Spdu[2] ,knx->Spdu[3]);
		else
			sprintf(riga,"=> PDU 03E7:  assegna al dispositivo %04X - blocco %02X - l'indirizzo applicativo %02X%02X\n" ENDOFPDU, wReverse(knx->DestinationAddress.Word),  knx->Spdu[0] ,knx->Spdu[2] ,knx->Spdu[3]);
		printout(riga);
		rc = 10;
		break;

	case 0x03D5:
		//  B4 10 AB 10 01 65 03 D5 00 CA 10 04 8C 
		getGateway(knx);
		sprintf(riga,"=> PDU 03D5:  property read dispositivo %04X - ObjID %02x - propertyID %02x - element %X - address %02X\n" ENDOFPDU,wReverse(knx->DestinationAddress.Word),knx->Spdu[0],knx->Spdu[1],knx->Spdu[2]/16,knx->Spdu[3]);
		printout(riga);
		rc = 11;
		break;

	case 0x03D6:
		//  B0 10 01 10 AB 66 03 D6 00 CA 10 04 00 88 

		sprintf(riga,"=> PDU 03D6:  property response dispositivo %04X - ObjID %02x - propertyID %02x - element %X - address %02X - valore: ",wReverse(knx->DestinationAddress.Word),knx->Spdu[0],knx->Spdu[1],knx->Spdu[2]/16,knx->Spdu[3]);
		printout(riga);
		printspdu(msglen, 4);
		rc = 12;
		break;

	case 0x03D7:
		//   B4 10 AB 10 01 65 03 D7 01 C9 10 05 8C - setup
		//   B4 10 AB 10 04 69 03 D7 00 CC 40 01 FF FF FF FF D4 - cancella dispositivo 
		getGateway(knx);
		sprintf(riga,"=> PDU 03D7:  property setup dispositivo %04X - ObjID %02x - propertyID %02x - element %X - address %02X - valore: ",wReverse(knx->DestinationAddress.Word),knx->Spdu[0],knx->Spdu[1],knx->Spdu[2]/16,knx->Spdu[3]);
//						sprintf(riga,"=> PDU 03D7:  property setup dispositivo %04X - ObjID %02x - propertyID %02x - element %X - set ",wReverse(knx->DestinationAddress.Word),knx->Spdu[0],knx->Spdu[1],knx->Spdu[2]/16);
		printout(riga);
		printspdu(msglen, 4);
		rc = 13;
		break;

	default:
		if (verbose>1)
		{
			sprintf(riga,"=> PDU %04X not relevant\n",wbReverse(knx->Tpdu8.Byte, knx->Tpdu8a.Byte));
			printout(riga);
		}
		break;
	}
	return rc;
}


// ===================================================================================

int answerPrepare(char * datastream, uint16_t * answer)
{
	char riga[128];

// B4 00 FF 00 00 E1 01 40 14
	WORD_VAL pduresp;
int	resplen = 1;
	*datastream++ = 0xB4;
	while (*answer != 0xFFFF)
	{
		if (resplen == 6)
			pduresp.byte.HB = (char) *answer;
		else
		if (resplen == 7)
			pduresp.byte.LB = (char) *answer;

		*datastream++ = (char) *answer++;
		resplen++;
	}
	sprintf(riga,"        ==========> rispondo con PDU %04X \n", pduresp.Val);
	printout(riga);
	return resplen;
}
// ===================================================================================
int answerReply(EIB_MESSAGE * knx, int knx_len)
{
	char requestBuffer[32];
	char riga[128];
	char check = 0xFF;
	int  requestLen = 0;
	int  xBuf = 0;

	knx->Ulength = (char) knx_len+1;

	requestBuffer[requestLen++] = '@'; 
	requestBuffer[requestLen++] = 'W'; 
	requestBuffer[requestLen++] = (char) knx->Ulength; 
	if (verbose>1)
		printf("@W %x -> ", knx->Ulength);

	while (xBuf < knx_len)
	{
		check ^= knx->data[xBuf];
		requestBuffer[requestLen++] = knx->data[xBuf]; 
		if (verbose>1)
			printf(" %02X", knx->data[xBuf]);
		xBuf++;
	}
	knx->data[xBuf] = check;
	requestBuffer[requestLen++] = knx->data[xBuf]; 
	if (verbose>1)
		printf(" %02X <-\n", knx->data[xBuf]);


	sprintf(riga,"TX ==> ");
	printout(riga);
	rowDisplay(knx, knx_len+1);
	decodificaTpu(knx, knx_len+1);
	sprintf(riga,"-----------------------------------------------------------------------------------------------------------------\n");
	printout(riga);

	if (TRUE_REPLY)
	    write(fduart,requestBuffer,requestLen);			// scrittura su knxgate
	return requestLen;
}
// ===================================================================================
int answerACK(void)
{
	char requestBuffer[8];
	int  requestLen = 0;
	requestBuffer[requestLen++] = '@'; 
	requestBuffer[requestLen++] = 'W'; 
	requestBuffer[requestLen++] = 1; 
	requestBuffer[requestLen++] = 0xCC; 
	if (verbose>1)
		printf("@W 1 -> CC\n");
	if (TRUE_REPLY)
	    write(fduart,requestBuffer,requestLen);			// scrittura su knxgate
	return requestLen;
}
// ===================================================================================
int main(int argc, char *argv[])
{
	char riga[128];
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
	strcpy(logfilename,"syslog");

	if (logfilename[0])
	{
		buslog = fopen(logfilename, "wb");
	}

	sprintf(riga,"-----------------------------------------------------------------------------------------------------------------\n");
	printout(riga);
	if (answerMode == 1)
	{
		printf(RED BOLD);
		sprintf (riga,"- DEVICE CONFIGURATION MODE -\n");
		printout(riga);
	}
	else
	if (myAddress != 0)
	{
		printf(YEL BOLD);
		sprintf (riga,"- APP CONFIGURATION MODE -\n");
		printout(riga);
	}
	else
	{
		printf(GRN BOLD);
		sprintf (riga,"- PASSIVE MODE -\n");
		printout(riga);
	}
	printf(NRM);
	sprintf(riga,"-----------------------------------------------------------------------------------------------------------------\n");
	printout(riga);

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
		rx_max = 64;
		rx_len = 0;
		rxBufferLoad(1);	
		if (rx_len > 2) // aggiunge 0x00 - rx_len è la lunghezza + 1 (comprende la lunghezza di busta) e punta al byte terminatore 0x00
		{				//  invece rx.buffer[0] contiene la lunghezza di msg  (Ulength)
			char riga[128];
			rx_len--;	//  ora rx_len è allineata a Ulength
			if (verbose>1)
			{
				fprintf(stderr,"\n-rx-> ");	// scrittura a video
				for (i=1; i<=rx_len; i++)
				{
					fprintf(stderr,"%02X ",rx.buffer[i]);
				}
				fprintf(stderr,"\n");	// scrittura a video
			}


			if ((rx.SourceAddress.Word == rx.DestinationAddress.Word) && (verbose < 2)) // salto le righe da gateway a se stesso
			{
				rx_len = 0;
			}

			if ((rx_len > 7) && (rx.TopByte.KNXFormat_frame == 0b10) && (rx.TopByte.KNXFormat_Fix == 0b1)) 
//                                                TOP=10......           TOP=...1....            
			{

				sprintf(riga,"RX <== ");
				printout(riga);

				rowDisplay(&rx, rx_len);

				int response = 0;

//				int msglen=(rx.buffer[6] & 0x0F);
				int msglen=rx.Counter.DataLength; // - 1;
				int ix=0;
//				if (verbose)
				{
					printf(GRN BOLD "FROM->[%04X] - to [%04X] -> PDU [%04X] : " NRM ,wReverse(rx.SourceAddress.Word), wReverse(rx.DestinationAddress.Word), wbReverse(rx.Tpdu8.Byte, rx.Tpdu8a.Byte));
					ix = 8;
					while (ix<msglen)
					{
						printf("%02X",rx.Spdu[ix++]);
					}
					printf(" <nl>\n");

//				}
//				if (memcmp(&prev.data[0], &rx.data[0], TLGR_MAXLEN-2) != 0)
//				{

					
				// 08 B4 10 0C 0B 31 E1 00 81 0D
					response = decodificaTpu(&rx, rx_len);
					sprintf(riga,"-----------------------------------------------------------------------------------------------------------------\n");
					printout(riga);
//-------------------------------------------------------------------
//          0 = non trattato
//          1 = richiesta tasto config							0100
//          2 = test indirizzo device - non è mio				0302
//          3 = test indirizzo device - è mio					0302
//			4 = assegnazione device address						00C0
//			5 = risposta di dispositivo senza indirizzo			0140
//			6 = risposta di dispositivo indirizzato				0140
//			7 = risposta parametri di dispositivo				0342
//			8 = fine configurazione								0380
//			9 = risponde indirizzo applicativo di blocco		03E6
//		   10 = assegna indirizzo applicativo ad un blocco		03E7
//		   11 = property read indirizzo applicativo / blocco	03D5
//		   12 = property response indirizzo applicativo/blocco	03D6
//		   13 = property write indirizzo applicativo/blocco		03D7
//-------------------------------------------------------------------
					memcpy(&prev.buffer, &rx.buffer, TLGR_MAXLEN);
				}






// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================
//				if (answerMode)
				{
					int bLen;
//-------------------------------------------------------------------
//          0 = non trattato
//          1 = richiesta tasto config							0100
//          2 = test indirizzo device - non è mio				0302
//          3 = test indirizzo device - è mio					0302
//			4 = assegnazione device address						00C0
//			5 = risposta di dispositivo senza indirizzo			0140
//			6 = risposta di dispositivo indirizzato				0140
//			7 = risposta parametri di dispositivo				0342
//			8 = fine configurazione								0380
//			9 = risponde indirizzo applicativo di blocco		03E6
//		   10 = assegna indirizzo applicativo ad un blocco		03E7
//		   11 = property read indirizzo applicativo / blocco	03D5
//		   12 = property response indirizzo applicativo/blocco	03D6
//		   13 = property write indirizzo applicativo/blocco		03D7
//-------------------------------------------------------------------
					switch (response)
					{
						case 1: //		    1 = richiesta tasto config							0100
// --------------------------------------------------------------------------------------------------------------------
//	PDU 01 00 len 1	richiesta tasto config dal server   				B4 10 AB 00 00 E1 01 00 10 		[0100][1]
// --------------------------------------------------------------------------------------------------------------------
//--> risponde simulando sia stato premuto il tasto config su dispositivo non configurato:
//	PDU 01 40 len 1	risposta del dispositivo (dice cha ha indirizzo 00FF)		 B4 00 FF 00 00 E1 01 40 14
// --------------------------------------------------------------------------------------------------------------------
							if (myAddress == 0)
							{
								bLen = answerPrepare(&tx.data[0], (uint16_t*) sreply_0140);
								answerReply(&tx,bLen);
							}
							else
							{
								answerACK();
// --------------------------------------------------------------------------------------------------------------------
//--> risponde confermando l'indirizzo acquisito:
//	PDU 01 40 len 1	risposta del dispositivo (dice cha ha indirizzo 1001)		 B4 10 01 00 00 E1 01 40 FA  
// --------------------------------------------------------------------------------------------------------------------
								bLen = answerPrepare(&tx.data[0], (uint16_t*) sreply_0140);
								tx.SourceAddress.Word = myAddress;
								answerReply(&tx,bLen);
							}
							break;

						case 2:	//          2 = test indirizzo device - non è mio				0302
// --------------------------------------------------------------------------------------------------------------------
//<-- il server prova a chiamare l'indirizzo che intende assegnare (NON RISPONDERE)
//	PDU 03 02 len 1	server prova a chiamare nuovo indirizzo		 		B4 10 AB 10 01 61 03 02 81 		[0302][2]
// --------------------------------------------------------------------------------------------------------------------
							break;

						case 3: //          3 = test indirizzo device - è mio					0302
// --------------------------------------------------------------------------------------------------------------------
//<-- il server prova a chiamare l'indirizzo assegnato
//	PDU 03 02 len 1	server prova a chiamare nuovo indirizzo		 		B4 10 AB 10 01 61 03 02 81 		[0302][3]
// --------------------------------------------------------------------------------------------------------------------
//--> risponde con i parametri attuali del dispositivo (modello 1851.2)
//	PDU 03 42 obj 00 prop 2C len F device parameters  B0 10 01 10 AB 6F 03 42 00 2C 01 68 0C 7F 40 02 20 01 20 01 20 01 9E 
// --------------------------------------------------------------------------------------------------------------------
							answerACK();
							if (answerMode == 1)
								bLen = answerPrepare(&tx.data[0], (uint16_t*) sreply_0342_1);
							else
								bLen = answerPrepare(&tx.data[0], (uint16_t*) sreply_0342_0);
							tx.SourceAddress.Word = myAddress;
							tx.DestinationAddress.Word = gatewayAddress;
							answerReply(&tx,bLen);
							break;

						case 4: //			4 = assegnazione device address						00C0
// --------------------------------------------------------------------------------------------------------------------
//<-- il server assegna l'indirizzo di device (ACQUISIRE senza rispondere) (o ack?)
//	PDU 00 C0 len 3	gateway assegna device address	device address	 		B4 10 AB 00 00 E3 00 C0 10 01 C2	[00c0][4]
// --------------------------------------------------------------------------------------------------------------------
							myAddress = newAddress;
							answerACK();
							break;

						case 5: //			5 = risposta di altro dispositivo senza indirizzo	0140
							
							break;

						case 6: //			6 = risposta di altro dispositivo indirizzato		0140
							
							break;

						case 7: //			7 = risposta parametri di altro dispositivo			0342
							
							break;

						case 8:  //			8 = fine configurazione								0380
							answerACK();
							if (newBlockId && newBlockDisplay)
							{
								if (newBlockDisplay == 1)
								{
									sprintf (riga,"\n\n SUCCESSFULLY CONFIG - DEVICE %04X - block %02X - APPLICATION %04X \n",wReverse(myAddress),newBlockId,wReverse(newAppAddress));
								}
								if (newBlockDisplay == 2)
								{
									sprintf (riga,"\n\n SUCCESSFULLY CONFIG - DEVICE %04X - block %02X - FREE \n",wReverse(myAddress),newBlockId);
								}
								printout(riga);
								newBlockDisplay = 0;
							}
							else
							if (answerMode && myAddress)
							{
								sprintf (riga,"\n\n SUCCESSFULLY CONFIG - NEW DEVICE %04X\n",wReverse(myAddress));
								answerMode = 0;
							}
							break;

						case 10: //		   10 = assegna indirizzo applicativo ad un blocco		03E7
				//  B4 10 AB 10 01 65 03 E7 05 00 0C 01 68
				//                         blk ^^ addres      ^^00=ASSOCIA   ^^02=DISASSOCIA
							if (gatewayAddress == 0)
							{
								gatewayAddress = rx.SourceAddress.Word;
								sprintf(riga,"=>   gateway address: %04X\n",wReverse(gatewayAddress));
								printout(riga);
							}
							if (myAddress == 0)
							{
								myAddress = rx.DestinationAddress.Word;
								sprintf(riga,"=>   my address: %04X\n",wReverse(myAddress));
								printout(riga);
							}

// estrae block id
							newBlockId = rx.Spdu[0];
							newBlockDisplay = 1;

// estrae application address
							newAppAddress = wExtract(&(rx.Spdu[2]));

							answerACK();
							if (rx.Spdu[1] == 0x02)  // disassocia
							{
								newBlockDisplay = 2;
								newAppAddress = 0xFF00;
							}

// risposta       B0 10 01 10 AB 6B 03 E6 05 11 00 FF 00 FF 00 FF 0C 01 8D
//                   ^^-^^ ^^-^^          ^^                      ^^-^^
							bLen = answerPrepare(&tx.data[0], (uint16_t*) sreply_03E6);
							tx.SourceAddress.Word = myAddress;
							tx.DestinationAddress.Word = gatewayAddress;
							tx.Spdu[0] = newBlockId;
							wReplace(&(tx.Spdu[8]), newAppAddress);
							answerReply(&tx,bLen);
							break;

						case 11: //		   11 = property read indirizzo applicativo / blocco	03D5
							// ad accensione gateway 

//  B4 10 AB 10 01 65 03 D5 00 CA 10 02 8A 

//  B0 10 01 10 AB 66 03 D6 00 CA 10 02 00 8E 

							// ==> VA implementato in KNXGATE_Y <==================================

							break;

						case 13: //		   13 = property write indirizzo applicativo/blocco		03D7
// B4 10 AB 10 01 66 03 D7 01 C9 10 05 01 8F
							answerACK();
							memcpy(&tx, &rx, (int) rx.Ulength);
							bLen = rx.Ulength - 1;
// B4 10 01 10 AB 66 03 D6 01 C9 10 05 01 8F													03D6
							tx.SourceAddress.Word = myAddress;
							tx.DestinationAddress.Word = gatewayAddress;
							tx.Tpdu8a.Byte--;
							answerReply(&tx,bLen);
							break;

						default:
						break;
					} // end switch
				} // end answer
// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================
// ================================================================================================================================================



			} // end if ((rx_len > 7) && (rx.TopByte.KNXFormat_frame == 0b10) && (rx.TopByte.KNXFormat_Fix == 0b1))  
		} // end if (rx_len > 2)
	} // end while

  // Don't forget to clean up
	close(fduart);
	if (buslog)
	{
		fclose(buslog);
		buslog = NULL;
	}
	printf("end\n");
	return 0;
}
// ===================================================================================
