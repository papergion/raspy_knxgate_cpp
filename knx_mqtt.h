/* ---------------------------------------------------------------------------
 * modulo per la gestione di mqtt  con knxgate
 * ---------------------------------------------------------------------------
*/
//
//
#ifndef _KNX_MQTT_H
#define _KNX_MQTT_H

#define CLIENTID    "Raspy_KNX_1"
#define QOS         1
#define TIMEOUT     50L	// millisecondi

// =============================================================================================
#define		_MODO "KNX"
#define		_modo "knx"
#define		ID_MY "interfaccia knx"
#define MYPFX _modo
#define     SWITCH_SET   MYPFX "/switch/set/"
#define     SWITCH_STATE MYPFX "/switch/state/"
#define     BRIGHT_SET   MYPFX "/switch/setlevel/"
#define     BRIGHT_STATE MYPFX "/switch/value/"
#define     COVER_SET    MYPFX "/cover/set/"
#define     COVER_STATE  MYPFX "/cover/state/"
#define     COVERPCT_SET    MYPFX "/cover/setposition/"
#define     COVERPCT_STATE  MYPFX "/cover/value/"
#define     SENSOR_TEMP_STATE MYPFX "/sensor/temp/state/"
#define     SENSOR_HUMI_STATE MYPFX "/sensor/humi/state/"
#define     SENSOR_PRES_STATE MYPFX "/sensor/pres/state/"

// generic:
//  knx/generic/set/<to>      <from><type><cmd>   command to send
//  knx/generic/from/<from>   <to><type><cmd>	  received
//  knx/generic/to/<to>       <from><type><cmd>   received

#define     GENERIC_SET   MYPFX "/generic/set/"
#define     GENERIC_FROM  MYPFX "/generic/from/"
#define     GENERIC_TO    MYPFX "/generic/to/"
// =============================================================================================
#define		SUBSCRIBE1 MYPFX "/+/set/+"
#define		SUBSCRIBE2 MYPFX "/+/setlevel/+"
#define		SUBSCRIBE3 MYPFX "/+/setposition/+"
// =============================================================================================
typedef struct {
    char topic[24];
    char payload[8];
    char retain;
} publish_queue;
// =============================================================================================
typedef struct {
    uint16_t  busid;
    char bustype;
    char buscommand;
    char busvalue;
    uint16_t  busfrom;
	char busbuffer[16];
//	char baseaddress;
} bus_knx_queue;
// =============================================================================================
int  MQTTconnect(char * broker, char * user, char * password, char verbose);
void MQTTverify(void);
void MQTTstop(void);
char MQTTrequest(bus_knx_queue * busdata);
char MQTTcommand(bus_knx_queue * busdata);
// =============================================================================================
void publish(char * pTopic, char * pPayload, int retain);
void publish_dequeue(void);
// ===================================================================================
#endif