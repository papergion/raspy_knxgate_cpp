/* ---------------------------------------------------------------------------
 * modulo per la gestione di mqtt  con knxgate
 * ---------------------------------------------------------------------------
*/
//     **61*38 3342446589 **10#
//
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
#include "MQTTClient.h"
#include "knx_mqtt.h"
#include <sys/stat.h>
#include <vector>
// =============================================================================================
#include <functional>
// =============================================================================================
using namespace std;
// =============================================================================================
static int  mqttTimer = 0;
static char mqVerbose = 0;
static char mqttopen = 0;  //0=spento    1=richiesto    2=in corso     3=connesso
static char domoticMode = 'H';
static char mqttAddress[24];
// =============================================================================================
extern std::vector<bus_knx_queue> _schedule_b;
// =============================================================================================
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;
// =============================================================================================
void publish(char * pTopic, char * pPayload, int retain);
// =============================================================================================
void mqSleep(int millisec);
void uqSleep(int microsec);
// =============================================================================================











// =============================================================================================
void mqSleep(int millisec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = millisec * 1000000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void uqSleep(int microsec) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = microsec * 1000L;
    nanosleep(&req, (struct timespec *)NULL);
}
// ===================================================================================
void sqSleep(int sec) {
	int s = sec * 100;
    while (s-- > 0)
	{
		mqSleep(10);
	}
}
// ===================================================================================
int processMessage(char * topicName, char * payLoad)
// ===================================================================================
// è arrivato un messaggio MQTT (p.es. da homeassistant)
// input: topic e payload           
// output: se necessario topic di stato
//         coda di esecuzione knx  schedule_b
// ===================================================================================
{
  char dev[5];
  char *ch;
  char devtype = 0;
  int  device = 0xFF;
  int  from = 1;
  char reply = 0;
  char command = 0xFF;
  char value = 0;
  char rtopic[24];
  char rpayload[24];
        
  strcpy(rpayload, payLoad);

// --------------------------------------- SWITCHES -------------------------------------------
  if (memcmp(topicName, SWITCH_SET, sizeof(SWITCH_SET) - 1) == 0)
  {
    devtype = 1; // switch
    strcpy(rtopic, SWITCH_STATE);
    reply = 1;
    dev[0] = *(topicName + sizeof(SWITCH_SET) - 1);
    dev[1] = *(topicName + sizeof(SWITCH_SET));
    dev[2] = *(topicName + sizeof(SWITCH_SET)+1);
    dev[3] = *(topicName + sizeof(SWITCH_SET)+2);
    dev[4] = 0;
    device = (int)strtoul(dev, &ch, 16);
    if (memcmp(payLoad, "ON", 2) == 0)
      command = 0x81;
    else if (memcmp(payLoad, "OFF", 3) == 0)
      command = 0x80;

	if (mqVerbose) fprintf(stderr,"switch %s  cmd %x \n", dev, command);
  }
  // ----------------------------------------------------------------------------------------------
  else
    // --------------------------------------- LIGHTS DIMM ------------------------------------------
  if (memcmp(topicName, BRIGHT_SET, sizeof(BRIGHT_SET) - 1) == 0)
  {
    devtype = 4; // dimmer light
    strcpy(rtopic, BRIGHT_STATE);
    reply = 1;
    dev[0] = *(topicName + sizeof(BRIGHT_SET) - 1);
    dev[1] = *(topicName + sizeof(BRIGHT_SET));
    dev[2] = *(topicName + sizeof(BRIGHT_SET)+1);
    dev[3] = *(topicName + sizeof(BRIGHT_SET)+2);
    dev[4] = 0;
    device = (int)strtoul(dev, &ch, 16);
//    if (memcmp(payLoad, "ON", 2) == 0)
//      command = 0x80;
//    else if (memcmp(payLoad, "OFF", 3) == 0)
//      command = 0x81;
//   else
      {
	    command = 0xFF;
        value = atoi(payLoad);    // percentuale
		if (value == 0xFF) value = 0xFE;
		if (mqVerbose) fprintf(stderr,"light %s  cmd %x val %d\n", dev, command, value);
      }
    }
  // ----------------------------------------------------------------------------------------------
    else
  // --------------------------------------- COVER ------------------------------------------------
  if (memcmp(topicName, COVER_SET, sizeof(COVER_SET) - 1) == 0)
  {
    devtype = 8; // cover
    strcpy(rtopic, COVER_STATE);
    reply = 1;
    dev[0] = *(topicName + sizeof(COVER_SET) - 1);
    dev[1] = *(topicName + sizeof(COVER_SET));
    dev[2] = *(topicName + sizeof(COVER_SET)+1);
    dev[3] = *(topicName + sizeof(COVER_SET)+2);
    dev[4] = 0;
    device = (int)strtoul(dev, &ch, 16);

    if (memcmp(payLoad, "STOP", 4) == 0)
	{
		command = 0x80;
// su STOP indirizzo deve essere base 
    }
    else
    {
// su OPEN/CLOSE indirizzo deve essere base+1
		device++;      // porta a base+1
		devtype = 18; // cover
		if (memcmp(payLoad, "ON", 2) == 0)
		  command = 0x81;
		else if (memcmp(payLoad, "CLOSE", 5) == 0)
		{
		  command = 0x81;
		  if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
			  strcpy(rpayload, "closed");
		}
		else if (memcmp(payLoad, "OFF", 3) == 0)
		  command = 0x80;
		else if (memcmp(payLoad, "OPEN", 4) == 0)
		{
		  command = 0x80;
		  if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
			  strcpy(rpayload, "open");
		}
	}

	if (mqVerbose) fprintf(stderr,"cover %s  cmd %x \n", dev, command);
  }
  // ----------------------------------------------------------------------------------------------
    else
  // --------------------------------------- COVERPCT ---------------------------------------------
  if (memcmp(topicName, COVERPCT_SET, sizeof(COVERPCT_SET) - 1) == 0)
  {
    devtype = 9; // cover
    strcpy(rtopic, COVERPCT_STATE);
    reply = 0;
    dev[0] = *(topicName + sizeof(COVERPCT_SET) - 1);
    dev[1] = *(topicName + sizeof(COVERPCT_SET));
    dev[2] = *(topicName + sizeof(COVERPCT_SET)+1);
    dev[3] = *(topicName + sizeof(COVERPCT_SET)+2);
    dev[4] = 0;
    device = (int)strtoul(dev, &ch, 16);

    if (memcmp(payLoad, "STOP", 4) == 0)
	{
		command = 0x80;
	}
	else 
	{	
// su OPEN/CLOSE indirizzo deve essere base+1
		device++;      // porta a base+1
		devtype = 19; // cover
		if (memcmp(payLoad, "ON", 2) == 0)
		{
	      command = 0x81;
		}
		else if (memcmp(payLoad, "CLOSE", 5) == 0)
		{
		  command = 0x81;
		  if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
			  strcpy(rpayload, "closed");
		}

		else if (memcmp(payLoad, "OFF", 3) == 0)
		{
	      command = 0x80;
		}
		else if (memcmp(payLoad, "OPEN", 4) == 0)
		{
		  command = 0x80;
		  if ((domoticMode == 'h') || (domoticMode == 'H')) // home assistant
			  strcpy(rpayload, "open");
		}
		else
		{
          device--;      // riporta a base
		  command = 0xFF;
		  value = atoi(payLoad);    // percentuale
		}
	}
	if (mqVerbose) fprintf(stderr,"cover %s  cmd %x \n", dev, command);

  }
  // ----------------------------------------------------------------------------------------------
    else
  // --------------------------------------- GENERIC ----------------------------------------------
  if (memcmp(topicName, GENERIC_SET, sizeof(GENERIC_SET) - 1) == 0)
  {
    devtype = 11; // generic
//  strcpy(rtopic, GENERIC_STATE);
    reply = 0;
    dev[0] = *(topicName + sizeof(GENERIC_SET) - 1);
    dev[1] = *(topicName + sizeof(GENERIC_SET));
    dev[2] = *(topicName + sizeof(GENERIC_SET)+1);
    dev[3] = *(topicName + sizeof(GENERIC_SET)+2);
    dev[4] = 0;
    device = (int)strtoul(dev, &ch, 16);

    *(payLoad+6) = 0;        // packet: ffffcc ->  <from><command>
    command = (char)strtoul(payLoad+4, &ch, 16);
    *(payLoad+4) = 0;
    from = (int)strtoul(payLoad, &ch, 16);
  }

  // ----------------------------------------------------------------------------------------------
  if ((reply == 1) && (device != 0) && (devtype != 0))
  { // device valido 
	  strcat(rtopic, dev);
	  if (mqVerbose) 	fprintf(stderr,"pub WR\n");
	  publish(rtopic, rpayload, 1);
	  if (mqVerbose) 	fprintf(stderr,"pub OK\n");
  }       // device valido
  // ----------------------------------------------------------------------------------------------

  if ((device != 0) && (devtype != 0))
  {
	// schedulare azione (_command) su devices (id)  con valore (value)
	  bus_knx_queue schedule;

	  schedule.busid = device;
	  schedule.bustype = devtype;
	  schedule.buscommand = command;
	  schedule.busvalue = value;
	  schedule.busfrom = from;
	// push
	  _schedule_b.push_back(schedule);
  }
  return 0;
}
// ===================================================================================



// ===================================================================================
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
// ===================================================================================
// è arrivato un messaggio MQTT - processo
// ===================================================================================
{
	(void) context;
	(void) topicLen;

    int i;
    char* payloadptr;
    char* payLoad;

	if (mqVerbose>1) printf("Message arrived - topic: %s   message: ", topicName);

    payloadptr = (char*)message->payload;
    payLoad = (char*)message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        if (mqVerbose) putchar(*payloadptr);
        payloadptr++;
    }
	*payloadptr = 0;
    if (mqVerbose) putchar('\n');

	processMessage(topicName, payLoad);

	MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}
// ===================================================================================
void connlost(void *context, char *cause)
// ===================================================================================
// connessione persa - hangup broker
// ===================================================================================
{
	(void) context;
    if (mqVerbose) printf("\nConnection lost - cause: %s\n", cause);
	mqttopen = 1;
}
// ===================================================================================
int MQTTconnect(char * broker, char * user, char * password, char verbose)
// ===================================================================================
// connessione broker MQTT e sottoscrizione topics
// ===================================================================================
{
    int rc;
	if (*broker == 0) return 0;

	mqVerbose = verbose;
	strcpy(mqttAddress, broker);

    if (mqVerbose) fprintf(stderr,"MQTT connection at %s ...\n", mqttAddress);
	mqttopen = 1;

	MQTTClient_create(&client, mqttAddress, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);

	conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
	conn_opts.username = (const char *) user;
	conn_opts.password = (const char *) password;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        if (mqVerbose) fprintf(stderr,"MQTT - Failed to connect, return code %d\n", rc);
		return 0;
    }
	mqttopen = 2;

	MQTTClient_subscribe(client, SUBSCRIBE1, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE2, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE3, QOS);

    if (mqVerbose) fprintf(stderr,"MQTT connected\n");
	mqttopen = 3;
	return 1;
}
// ===================================================================================
void MQTTreconnect(void)
// ===================================================================================
// tentativo di riconnessione broker MQTT e sottoscrizione topics
// ===================================================================================
{
    int rc;
    if (mqVerbose) fprintf(stderr,"MQTT reconnection at %s ...\n", mqttAddress);

	conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        if (mqVerbose) fprintf(stderr,"MQTT - Failed to reconnect, return code %d\n", rc);
        return;
    }

	MQTTClient_subscribe(client, SUBSCRIBE1, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE2, QOS);
	MQTTClient_subscribe(client, SUBSCRIBE3, QOS);

    if (mqVerbose) fprintf(stderr,"MQTT - reconnected\n");
	mqttopen = 3;
}
// ===================================================================================
void publish(char * pTopic, char * pPayload, int retain)
// ===================================================================================
// pubblicazione topic/payload
// ===================================================================================
{
	if (mqttopen != 3)  
		return;

	if (mqVerbose>1) printf("topic: %s    payload: %s \n",pTopic,pPayload);

	pubmsg.payload = pPayload;
    pubmsg.payloadlen = (int)strlen(pPayload);
    pubmsg.qos = QOS;
    pubmsg.retained = retain;
    MQTTClient_publishMessage(client, pTopic, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, TIMEOUT);
}
// ===================================================================================
void MQTTverify(void)
// ===================================================================================
// verifica connessione broker - se disconnesso tenta la riconnessione
// ===================================================================================
{
	if ((mqttopen) && (mqttopen != 3))  
	{ 
		mqttTimer++;
		mqSleep(10);	// 10msecs wait
		if (mqttTimer > 1000)  // 10 sec
		{
			MQTTreconnect();
			mqttTimer = 0;
		}
	}
	else
		mqttTimer = 0;
}
// ===================================================================================
void MQTTstop(void)
// ===================================================================================
// ferma connessione al broker
// ===================================================================================
{
	if (mqttopen == 3)  
	{
		MQTTClient_unsubscribe(client, SUBSCRIBE1);
		MQTTClient_unsubscribe(client, SUBSCRIBE2);
		MQTTClient_unsubscribe(client, SUBSCRIBE3);

		MQTTClient_disconnect(client, 10000);
		MQTTClient_destroy(&client);
	}
}
// ===================================================================================
char MQTTrequest(bus_knx_queue * busdata)
// ===================================================================================
// è arrivato un messaggio KNX -> pubblicare lo STATO
// input: dati del messaggio e tipo dispositivo censito
// output: se necessario topic di stato
// return: tipo dispositivo individuato
// ===================================================================================
{
// ---------------------------------------------------------------------------------------------------------------------------------
	if (mqttopen != 3)	return 0xFF;


				// 01:switch        04:dimmer   
				// 08:tapparella    09:tapparella pct   
				// 11:generic		12:generic 
				// 14:allarme		15:termostato
				// 18 n.a.          19 n.a.
				// 0x30-0x37:rele i2c
				// 0x40-0x47:pulsanti i2c


 // START pubblicazione stato device        [0xF5] [y] 32 00 12 01
	int  device = 0;
	char devtype = 0;
	char action;
	char topic[32];
	char payload[64];
	char nomeDevice[5];

	device = busdata->busid;
	action = busdata->buscommand;
	if ((busdata->bustype > 17) && (busdata->bustype < 30))
		sprintf(nomeDevice, "%04X", device-1);  // to
	else
		sprintf(nomeDevice, "%04X", device);  // to

	printf("MQTTR %04x c:%02x v:%d\n",busdata->busid,busdata->buscommand,busdata->busvalue);


// ================================ INTERPRETAZIONE STATO ===========================================

	// ----------------------------------pubblicazione stati GENERIC device KNX (to & from)------------------------
	switch (busdata->bustype)
	{
	case 11:  
		          // device generic censito
  // knx ridotto  [0xF5] [y] 29 0B 65 81
  // knx ridotto  [0xF6] [y] 01 0F 01 81 01
		devtype = 11;
		strcpy(topic, GENERIC_TO);
//		strcat(topic, nomeDevice);	// in topic		nomedevice			0b65	0F01	
      	sprintf(payload, "%04X%02X", busdata->busfrom, busdata->buscommand);
		break;
	case 12:  
		devtype = 12;
		sprintf(nomeDevice, "%04X", busdata->busfrom);  // to
		strcpy(topic, GENERIC_FROM);
		sprintf(payload, "%04X%02X", busdata->busid, busdata->buscommand);
		break;

	case 1:  // switch
    // knx ridotto  [0xF5] [y] 29 0B 65 81
		switch (action)
		{
		  case 0x81:
			strcpy(payload,"ON");
			strcpy(topic, SWITCH_STATE);
			devtype = 1;
			break;
		  case 0x80:
			strcpy(payload,"OFF");
			strcpy(topic, SWITCH_STATE);
			devtype = 1;
			break;
		}
		break;

	case 4:  // dimmer
    // knx ridotto  [0xF5] [y] 29 0B 65 81
		switch (action)
		{
		  case 0x81:
			strcpy(payload,"ON");
			strcpy(topic, SWITCH_STATE);
			devtype = 4;
			break;
		  case 0x80:
			strcpy(payload,"OFF");
			strcpy(topic, SWITCH_STATE);
			devtype = 4;
			break;
		  case 'm':
// ---------------------u-posizione dimmer %---------------------------------------------------------------------
//    [F4] m <address> <position%>
			devtype = 4;
			sprintf(payload, "%03u", busdata->busvalue);     // position
			strcpy(topic, BRIGHT_STATE);
			break;
		}
		break;

	case 24:  // dimmer
    // knx ridotto  [0xF5] [y] 29 0B 65 81
		switch (action)
		{
		  case 0x81:
			strcpy(payload,"-DOWN START");
			strcpy(topic, SWITCH_STATE);
			devtype = 4;
			break;
		  case 0x80:
			strcpy(payload,"-DOWN END");
			strcpy(topic, SWITCH_STATE);
			devtype = 4;
			break;
		  case 0x89:
			strcpy(payload,"-UP START");
			strcpy(topic, SWITCH_STATE);
			devtype = 4;
			break;
		  case 0x88:
			strcpy(payload,"-UP END");
			strcpy(topic, SWITCH_STATE);
			devtype = 4;
			break;
		  case 'm':
// ---------------------u-posizione dimmer %---------------------------------------------------------------------
//    [F4] m <address> <position%>
			devtype = 4;
			sprintf(payload, "%03u", busdata->busvalue);     // position
			strcpy(topic, BRIGHT_STATE);
			break;
		}
		break;
	case 8:  // cover (INDIRIZZO BASE = STOP)
	case 9:
    // knx ridotto  [0xF5] [y] 29 0B 65 81
		if ((action == 0x80) || (action == 0x81))
		{
			strcpy(payload,"STOP");
			strcpy(topic, COVER_STATE);
			devtype = 8;
		}
		else
		if (action == 'u')
		{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F4] u <address> <position%>
			devtype = 9;
			sprintf(payload, "%03u", busdata->busvalue);     // position
			strcpy(topic, COVERPCT_STATE);
		}
		break;
	case 18:  // cover (INDIRIZZO BASE+1 = UP/DOWN)
	case 19: 
    // knx ridotto  [0xF5] [y] 29 0B 65 81
		switch (action)
		{
		  case 0x80:
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"open");
			else
				strcpy(payload,"OFF");
			strcpy(topic, COVER_STATE);
			devtype = 8;
			break;
		  case 0x81:
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"closed");
			else
				strcpy(payload,"ON");
			strcpy(topic, COVER_STATE);
			devtype = 8;
			break;
		  case 'u':
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F4] u <address> <position%>
			devtype = 9;
			sprintf(payload, "%03u", busdata->busvalue);     // position
			strcpy(topic, COVERPCT_STATE);
			break;
		}
		break;
	}	// fine 	switch (busdata->bustype)


	if ((device != 0) && (devtype != 0))
    { // device valido 
		strcat(topic, nomeDevice);
		if ((payload[0] != '-') || (mqVerbose))
			publish(topic, payload, 1);
		printf("%s -> %s\n",topic,payload);
    }       // device valido
	else
	// ----------------------------------------------------------------------------------------------------
	if (devtype == 0)
	{
		strcpy(topic, "NO_TOPIC");
		sprintf(payload, "to %04X, from %04X, cmd %02X, val %d", busdata->busid, busdata->busfrom, busdata->buscommand, busdata->busvalue);
		publish(topic, payload, 0); 
		printf("%s -> %s\n",topic,payload);
	} 
//	printf("end mqttrequest\n");
	return devtype;
}
// ===================================================================================


// ===================================================================================
char MQTTcommand(bus_knx_queue * busdata)
// ===================================================================================
// richiesta di pubblicazione COMANDO in mqtt (per esempio da alexa)
// input: dati del messaggio e tipo dispositivo censito
// output: topic di comando
// return: tipo dispositivo individuato
// ===================================================================================
{
// ---------------------------------------------------------------------------------------------------------------------------------
	if (mqttopen != 3)	return 0xFF;

 // START pubblicazione comando device        [0xF5] [y] 32 00 12 01
	int  device = 0;
	char devtype = 0;
	char action;
	char topic[32];
	char payload[64];
	char nomeDevice[5];

	device = busdata->busid;
	action = busdata->buscommand;
	if (action == 0)
		action = 0x80;
	else
	if (action == 1)
		action = 0x80;
	else
	if (action == 2)
		action = 0x81;

	sprintf(nomeDevice, "%04X", device);  // to

	printf("MQTTC %04x t:%02x c:%02x v:%d\n",busdata->busid,busdata->bustype,busdata->buscommand,busdata->busvalue);


// ================================ PUBBLICAZIONE COMANDO ===========================================

  // ----------------------------------pubblicazione comandi GENERIC device KNX (to & from)------------------------
	switch (busdata->bustype) 
	{
	case 11:  
	    // device generic censito
		devtype = 11;
		strcpy(topic, GENERIC_SET);
		strcat(topic, nomeDevice);
      	sprintf(payload, "%04X%02X", busdata->busfrom,action);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
		break;

	case 12:  
		devtype = 12;
		sprintf(nomeDevice, "%04X", busdata->busfrom);  // to
		strcpy(topic, GENERIC_SET);
		strcat(topic, nomeDevice);
		sprintf(payload, "%04X%02X", busdata->busid, action);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
		break;

	case 1: // switch
		switch (action)
		{
		  case 0x81:
			strcpy(payload,"ON");
			strcpy(topic, SWITCH_SET);
			devtype = 1;
			break;
		  case 0x80:
			strcpy(payload,"OFF");
			strcpy(topic, SWITCH_SET);
			devtype = 1;
			break;
		}
		break;

	case 4:  // dimmer
		switch (action)
		{
		  case 0x81:
			strcpy(payload,"ON");
			strcpy(topic, SWITCH_SET);
			devtype = 3;
			break;
		  case 0x80:
			strcpy(payload,"OFF");
			strcpy(topic, SWITCH_SET);
			devtype = 3;
			break;
		  case 2:
		  case 3:
		  case 4:
			sprintf(payload, "%02u", action);
			strcpy(topic, BRIGHT_SET);
			devtype = 3;
			break;
		  default:
			if (action > 0x7F)
			{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F4] u <address> <position%>
				devtype = 3;
				sprintf(payload, "%03u", busdata->busvalue);     // position
				strcpy(topic, BRIGHT_SET);
				strcat(topic, nomeDevice);
				publish(topic, payload, 0);
				printf("%s -> %s\n",topic,payload);
			}
			break;
		}
		break;

	case 8:  // cover (INDIRIZZO BASE)
    // knx ridotto  [0xF5] [y] 29 0B 65 81
		if ((action == 0x80) || (action == 0x81))
		{
			devtype = 8;
			strcpy(payload,"STOP");
			strcpy(topic, COVER_SET);
		}
		else
		if (action == 11)
		{
			devtype = 18;
			strcpy(topic, COVER_SET);
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"OPEN");
			else
				strcpy(payload,"OFF");
		}
		else
		if (action == 12)
		{
			devtype = 18;
			strcpy(topic, COVER_SET);
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"CLOSE");
			else
				strcpy(payload,"ON");
		}
		break;

	case 9:
		if ((action == 0x80) || (action == 0x81))
		{
			devtype = 9;
			strcpy(payload,"STOP");
			strcpy(topic, COVER_SET);
		}
		else
		if (action == 11)
		{
			devtype = 19;
			strcpy(topic, COVER_SET);
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"OPEN");
			else
				strcpy(payload,"OFF");
		}
		else
		if (action == 12)
		{
			devtype = 19;
			strcpy(topic, COVER_SET);
			if ((domoticMode == 'h') || (domoticMode == 'H'))
				strcpy(payload,"CLOSE");
			else
				strcpy(payload,"ON");
		}
		else
		if (action > 0x7F)
		{
// ---------------------u-posizione tapparelle o dimmer %---------------------------------------------------------------------
//    [F4] u <address> <position%>
			devtype = 19;
			sprintf(payload, "%03u", busdata->busvalue);     // position
			strcpy(topic, COVERPCT_SET);
			strcat(topic, nomeDevice);
			publish(topic, payload, 0);
			printf("%s -> %s\n",topic,payload);
		}
		break;
	} // 	switch (busdata->bustype) ----------------------------

    if ((device != 0) && (devtype != 0))
    { // device valido 
		strcat(topic, nomeDevice);
		publish(topic, payload, 0);
		printf("%s -> %s\n",topic,payload);
    }       // device valido
	// ----------------------------------------------------------------------------------------------------
	else
		printf("no action %02x\n",action);
	
	
	// ----------------------------------------------------------------------------------------------------
	if (devtype == 0)
	{
		strcpy(topic, "NO_TOPIC");
		sprintf(payload, "to %04X, from %02X, cmd %02X, val %d", busdata->busid, busdata->busfrom, busdata->buscommand, busdata->busvalue);
		publish(topic, payload, 0); 
		printf("%s -> %s\n",topic,payload);
	} 
//	printf("end mqttrequest\n");
	return devtype;
}
// ===================================================================================
