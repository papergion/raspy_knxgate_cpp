/* ---------------------------------------------------------------------------
 * modulo per la emulazione HUE (alexa)  con KNXgate
 * ---------------------------------------------------------------------------
*/
// v 3.2.1 - risorse esterne - come hue 1
// v 3.2.0 - risorse esterne - originale
// v 3.1.1 - ssdp:discover
//
#define HUE_1

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

#include "knx_hue.h"


// =============================================================================================
using namespace std;
// =============================================================================================

#ifdef HUE_0
#include "knx_hue_resource_0.h"
#endif

#ifdef HUE_1
#include "knx_hue_resource_1.h"
#endif



// =============================================================================================
char	hueverbose = 0;	// 1=hueverbose     2=hueverbose+      3=debug
// =============================================================================================
    TCPServer hueServer;
    UDPServer udpServer;
    char my_ipaddress[NI_MAXHOST];
	char my_macaddress[13];
	char my_bridgeid[20];
	char my_shortaddress[5];
// =============================================================================================
unsigned char udpanswer = 0;
unsigned char tcppost = 0;

unsigned char iDevice = 0;
unsigned char iDiscovered = 0;

std::vector<hue_device_t> _devices;
volatile char cache_id = 0xff;
volatile char cache_state;
volatile char cache_value;

extern std::vector<hue_knx_queue> _schedule;
// =============================================================================================
void mac_eth0(char * interf)
{
    #define HWADDR_len 6
    int s,i,j;
    struct ifreq ifr;
    s = socket(AF_INET, SOCK_DGRAM, 0);
    strcpy(ifr.ifr_name, interf);
    ioctl(s, SIOCGIFHWADDR, &ifr);
	j=0;
    for (i=0; i<HWADDR_len; i++)
	{
        sprintf(&my_macaddress[i*2],"%02x",((char*)ifr.ifr_hwaddr.sa_data)[i]);
        sprintf(&my_bridgeid[j*2],"%02X",((char*)ifr.ifr_hwaddr.sa_data)[i]);
		j++;
		if (j == 3)
		{
			my_bridgeid[6] = 'F';
			my_bridgeid[7] = 'F';
			my_bridgeid[8] = 'F';
			my_bridgeid[9] = 'E';
			j = 5;
		}
	}
//	char my_bridgeid[20];
    my_macaddress[12]='\0';
    my_bridgeid[16]='\0';
    strcpy (my_shortaddress, &my_macaddress[9]);
    close(s);
}
// ===================================================================================
void tryMyIp(void)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;

    if (getifaddrs(&ifaddr) == -1) {
	perror("getifaddrs");
	exit(EXIT_FAILURE);
    }



/* Walk through linked list, maintaining head pointer so we
   can free list later */

    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) 
	{
		if (ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

		/* Display interface name and family (including symbolic
		   form of the latter for the common families) */
	/*
		printf("%-8s %s (%d)\n",
			   ifa->ifa_name,
			   (family == AF_PACKET) ? "AF_PACKET" :
			   (family == AF_INET) ? "AF_INET" :
			   (family == AF_INET6) ? "AF_INET6" : "???",
			   family);
	*/
		/* For an AF_INET* interface address, display the address */

		if (family == AF_INET) // || family == AF_INET6) 
		{
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6),
					my_ipaddress, NI_MAXHOST,
					NULL, 0, NI_NUMERICHOST);
			if (s != 0) 
			{
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				exit(EXIT_FAILURE);
			}
			mac_eth0(ifa->ifa_name);
			if(strcmp(ifa->ifa_name,"wlan0")==0) // &&(ifa->ifa_addr->sa_family==AF_INET))
				printf("%-8s  address: <%s>    mac <%s>\n", ifa->ifa_name, my_ipaddress, my_macaddress);
			else
			if(strcmp(ifa->ifa_name,"eth0")==0) // &&(ifa->ifa_addr->sa_family==AF_INET))
			{
				printf("%-8s  address: <%s>    mac <%s>\n", ifa->ifa_name, my_ipaddress, my_macaddress);
				freeifaddrs(ifaddr);
				return;
			}

		}
		/*
		else if (family == AF_PACKET && ifa->ifa_data != NULL) 
		{
			struct rtnl_link_stats *stats = ifa->ifa_data;

			printf("\t\ttx_packets = %10u; rx_packets = %10u\n"
			   "\t\ttx_bytes   = %10u; rx_bytes   = %10u\n",
			   stats->tx_packets, stats->rx_packets,
			   stats->tx_bytes, stats->rx_bytes);
		}
	*/
    }
    freeifaddrs(ifaddr);
}
// ===================================================================================
void setState(char id, char state, char value) {
  if (id < _devices.size()) {
    if (state != 0xFF) _devices[(int)id].state = state;
    if (value)         _devices[(int)id].value = value;
	cache_state = _devices[(int)id].state;
	cache_value = _devices[(int)id].value;
	cache_id = id;
	if (hueverbose>1) printf("cache id %d value %d\n",cache_id,cache_value);
  }
}
// ===================================================================================
char addDevice(const char * device_name, unsigned char initvalue, int busaddr) {

  hue_device_t device;
  unsigned int device_id = _devices.size();

  // init properties
  device.name = strdup(device_name);
  device.state = 0;
  device.value = initvalue;
  device.busaddress = busaddr;

  // Attach
  _devices.push_back(device);
  return device_id;
}
// ===================================================================================
char addDevice(const char * device_name, int busaddr) {
  return addDevice(device_name, 128, busaddr);
}
// ===================================================================================
void _sendTCPResponse1(TCPSocket *client, const char * code, char * body, const char * mime) 
{
  (void) *client;
  (void) *mime;
  (void) *code;

  char headers[strlen(HUE_TCP_HEADER1) + 32];
//  sprintf((char *)headers,HUE_TCP_HEADER1,code);
  sprintf((char *)headers,HUE_TCP_HEADER1);

  if (hueverbose>2) printf("--> response for alexa \n%s%s\n",headers,body);

//  uSleep(100);

  if ((client->Send((string)headers + (string)body)) < 0)
		printf("\n\nERRORE IN TCP SEND...\n\n");
}
// ===================================================================================
void _sendTCPResponse0(TCPSocket *client, const char * code, char * body, const char * mime) 
{
  (void) *client;
  (void) *code;

  char headers[strlen(HUE_TCP_HEADER0) + 32];
//  sprintf((char *)headers,HUE_TCP_HEADER0,code,mime,strlen(body));
  sprintf((char *)headers,HUE_TCP_HEADER0,mime,strlen(body));

  if (hueverbose>2) printf("--> response for alexa \n%s%s\n",headers,body);

//  uSleep(100);

  if ((client->Send((string)headers + (string)body)) < 0)
		printf("\n\nERRORE IN TCP SEND...\n\n");
}
// ===================================================================================
void _sendTCPResponse(TCPSocket *client, const char * code, char * body, const char * mime) 
{
  (void) *client;
  (void) *code;

  char headers[strlen(HUE_TCP_HEADERS) + 32];
//  sprintf((char *)headers,HUE_TCP_HEADERS,code,mime,strlen(body));
  sprintf((char *)headers,HUE_TCP_HEADERS,mime,strlen(body));

  if (hueverbose>2) printf("--> response for alexa \n%s%s\n",headers,body);

//  uSleep(100);

  if ((client->Send((string)headers + (string)body)) < 0)
		printf("\n\nERRORE IN TCP SEND...\n\n");
}
// ===================================================================================
void _sendTCPok(TCPSocket *client) 
{
  (void) *client;

  char headers[strlen(HUE_TCP_OK) + 8];
  sprintf((char *)headers,HUE_TCP_OK);

  if (hueverbose>2) printf("--> response 200 OK for alexa \n");

//  uSleep(100);

  if (client->Send((string)headers) < 0)
		printf("\n\nERRORE IN TCP SEND...\n\n");
}
// ===================================================================================
void _onTCPDescription(TCPSocket *client, string url, string body) 
{
  (void) *client;
  (void) url;
  (void) body;

  _sendTCPok(client); 

  char response[1536];
#ifdef HUE_0
  sprintf((char *)response, HUE_DESCRIPTION_TEMPLATE, my_ipaddress, HUE_TCP_PORT, my_ipaddress, HUE_TCP_PORT, my_macaddress, my_macaddress  );
#endif

#ifdef HUE_1
  sprintf((char *)response, HUE_DESCRIPTION_TEMPLATE, my_ipaddress, HUE_TCP_PORT, my_ipaddress, my_macaddress, my_macaddress  );
#endif

  _sendTCPResponse0(client, "200 OK", response, "text/xml");
}
// ===================================================================================
string _deviceJson_first(unsigned char id) 
{

// json per dichiarativa iniziale - nel buffer (2920 bytes) ci stanno così fino a 37 dispositivi
// se il buffer e' 1000 bytes) ci stanno fino a 12 dispositivi

  char buffer[strlen(HUE_DEVICE_JSON_TEMPLATE_FIRST) + 64];

  if (id >= _devices.size()) return "{}";

  hue_device_t device = _devices[id];
  sprintf(buffer,HUE_DEVICE_JSON_TEMPLATE_FIRST,
#ifdef UNIQUE_MACADDRESS
    device.name, my_macaddress, id + 1
#endif
#ifdef SHORT_MACADDRESS
    device.name, my_shortaddress, id + 1
#endif
#ifdef UNIQUE_MY
    device.name, id + 1	
#endif
  );

  return (string)buffer;
}


// ===================================================================================
string _deviceJson(unsigned char id) 
{

// json per stato di dettaglio del dispositivo

/*
            "Tasmota": {"state": {"on": False, "bri": 200, "hue": 0, "sat": 0, "xy": [0.0, 0.0], "alert": "none", "effect": "none", "colormode": "xy", "reachable": True}, "type": "Extended color light", "swversion": "1.29.0_r21169"}, 
            "LCT015": {"state": {"on": False, "bri": 200, "hue": 0, "sat": 0, "xy": [0.0, 0.0], "ct": 461, "alert": "none", "effect": "none", "colormode": "ct", "reachable": True}, "type": "Extended color light", "swversion": "1.29.0_r21169"}, 
            "LST002": {"state": {"on": False, "bri": 200, "hue": 0, "sat": 0, "xy": [0.0, 0.0], "ct": 461, "alert": "none", "effect": "none", "colormode": "ct", "reachable": True}, "type": "Color light", "swversion": "5.105.0.21169"}, 
            "LWB010": {"state": {"on": False, "bri": 254,"alert": "none", "reachable": True}, "type": "Dimmable light", "swversion": "1.15.0_r18729"}, 
            "LTW001": {"state": {"on": False, "colormode": "ct", "alert": "none", "reachable": True, "bri": 254, "ct": 230}, "type": "Color temperature light", "swversion": "5.50.1.19085"}, 
            "Plug 01": {"state": {"on": False, "alert": "none", "reachable": True}, "type": "On/Off plug-in unit", "swversion": "V1.04.12"}}
*/
//	printf("\njsonDev\n");

  if (id >= _devices.size()) return "{}";

  hue_device_t device = _devices[id];
  if (hueverbose>1) printf("...........cacheid %d  id %d\n",cache_id,id);
  if (cache_id == id)
  {
	device.value = cache_value;
	device.state = cache_state;
  }
  char buffer[strlen(HUE_DEVICE_JSON_TEMPLATE) + 64];


#ifdef HUE_0
  sprintf(buffer, HUE_DEVICE_JSON_TEMPLATE,	// nome, mac, progmac, state, bright
#ifdef UNIQUE_MACADDRESS
    device.name, my_macaddress, id + 1,
#endif
#ifdef SHORT_MACADDRESS
    device.name, my_shortaddress, id + 1,
#endif
#ifdef UNIQUE_MY
    device.name, id + 1,
#endif
    (device.state & 0x01) ? "true" : "false",
    device.value
  );
#endif


#ifdef HUE_1
  sprintf(buffer, HUE_DEVICE_JSON_TEMPLATE,	// state, bright, nome, mac, progmac 
    (device.state & 0x01) ? "true" : "false",
    device.value,
#ifdef UNIQUE_MACADDRESS
    device.name, my_macaddress, id + 1
#endif
#ifdef SHORT_MACADDRESS
    device.name, my_shortaddress, id + 1
#endif
#ifdef UNIQUE_MY
    device.name, id + 1
#endif
  );
#endif





  return (string)buffer;
}
// ===================================================================================
void _onTCPList(TCPSocket *client, char * url, char * body) 
{
//  (void) client;
//  (void) url;
  (void) body;

  _sendTCPok(client); 

  // Get the index
  char * lights;
  lights = (strstr(url, "lights")); 
  if (lights == NULL) 
	 return;


  // Get the id
  int id;
  long ret;
  char *ptr;
  ret = strtoul(lights+7, &ptr, 10);
  id = (int) ret;

//  printf("\nid=%d\n",id);

  // This will hold the response string
  string response;

  char first = 0;
  // Client is requesting all devices - limit is bufsize (2920 bytes)
  if (0 == id)
  {
    //      Serial.print("[FAUXMO] Handling ALL DEVICES list <=========================\r\n");
//	printf("\ndevsize=%d\n",_devices.size());
    if (iDevice >= _devices.size()) iDevice = 0;
    if (iDevice < _devices.size())
    {
		response += "{";
//      while ((response.length() < 928) && (iDevice < _devices.size()))
      while ((response.length() < (HUE_TCP_PACKETSIZE - (strlen(HUE_DEVICE_JSON_TEMPLATE) + 64))) && (iDevice < _devices.size()))
      {
		if (first++ > 0) response += ",";
		auto s = std::to_string(iDevice + 1);
#ifdef SHORT_DECLARE
        response += "\"" + s + "\":" + _deviceJson_first(iDevice); // reduced  string
#else
        response += "\"" + s + "\":" + _deviceJson(iDevice);  // original string
#endif
        iDiscovered++;
		iDevice++;
      }
      response += "}";
//	  printf("\nresponse\n");
      _sendTCPResponse1(client, "200 OK", (char *) response.c_str(), "application/json");
    }
  } 
  else 
  {
    response = _deviceJson(id - 1);
    _sendTCPResponse1(client, "200 OK", (char *) response.c_str(), "application/json");

    _devices[id-1].state &= 0x8F; // cover da 0xC0 a 0x80
    if ((_devices[id-1].state & 0x80) == 0x80)
    {
		setState(id-1, 1, 128);
//       _devices[id-1].state = 1;   // cover 
//       _devices[id-1].value = 128; // cover 
    }

  }
}
// ===================================================================================
void _onTCPControl(TCPSocket *client, char * url, char * body) 
{
  (void) client;
  (void) url;
  (void) body;

  _sendTCPok(client); 

  // "devicetype" request
  if (strstr(body,"devicetype") != NULL) 
  {

	if (tcppost == 0)
	{
		tcppost++;
	    _sendTCPResponse1(client, "200 OK", (char *) "[{\"success\":{\"username\": " USERNAME "}}]", "application/json");
	}
	else
	{
	    _sendTCPResponse1(client, "200 OK", (char *) "[{\"error\":{\"type\":101,\"address\":\"\",\"description\":\"link button not pressed\"}}]", "application/json");
	}
    return;
  }


  // "state" request
  if ((strstr(url,"state") != NULL) && (*body != 0))
  {

	  // Get the index
	  char * found;
	  found = (strstr(url, "lights")); 
	  if (found == NULL) 
		 return;


	  // Get the id
	  int id;
	  char *ptr;
//	  long ret;
	  id = (int) strtoul(found+7, &ptr, 10);

//	  printf("\nid=%d\n",id);

	  if (id > 0) 
	  {
		  --id;

		  char _command = 0;
		  int oldvalue = _devices[id].value;
		  int value100 = 0;
		  int value255 = 0;

		  // command is a char (1=ON 2=OFF    3=bright equal  4=bright+   5=bright- )

		  // Brightness
		  found = (strstr(body, "bri")); 
		  if (found != NULL) 
		  {
			value255 = (int) strtoul(found+5, &ptr, 10); // voce su 100, riportare a 255 su devices.value, lasciare su 100 per knx/mqtt
			value100 = ((value255 * 100) + 128) / 255;

			if (value255 > oldvalue)
			  _command = 4;			// ALZA
			else if (value255 < oldvalue)
			  _command = 5;			// ABBASSA
			else
			  _command = 3;			// IMPOSTA

//			_devices[id].value = value255;
			setState(id, 0xff, value255);
		  }
		  else
		  {
			  found = (strstr(body, "false")); 
			  if (found != NULL) 
			  {
				_command = 2;		// SPEGNI
				_devices[id].state &= 0xFE;
				value255 = _devices[id].value;
				value100 = ((value255 * 100)+128) / 255;
		      }
			  else
			  {
				  found = (strstr(body, "true")); 
				  if (found != NULL) 
			      {
					_command = 1;	// ACCENDI
					_devices[id].state |= 0x01;
					if (0 == _devices[id].value) 
					{
//						_devices[id].value = 255;
						setState(id, 0xff, 255);
					}
					value255 = _devices[id].value;
					value100 = ((value255 * 100)+128) / 255;
				  }
			  }
		  }

		  const char * azione[5] = {
			 "accendi",
			 "spegni",
			 "ferma",
			 "alza",
			 "abbassa"
		};

		  if (hueverbose) printf("-> hue cmd: %d %s id %d  value %d%% (%d from %d)\n",_command,azione[(int)_command-1],id,value100,value255,oldvalue);

		  const char HUE_TCP_STATE_RESPONSE[] = "["
				"{\"success\":{\"/lights/%d/state/on\":%s}}]";

		  const char HUE_TCP_VALUE_RESPONSE[] = "["
				"{\"success\":{\"/lights/%d/state/bri\":%d}}]";

		  char response[strlen(HUE_TCP_VALUE_RESPONSE) + 10];
		  if (_command < 3) // on-off      
		  {
			sprintf(response, HUE_TCP_STATE_RESPONSE,id + 1, (_devices[id].state & 0x01)? "true" : "false");
		  }
		  else
		  {
			sprintf(response, HUE_TCP_VALUE_RESPONSE,id + 1, _devices[id].value );
		  }
      
		  _sendTCPResponse(client, "200 OK", response, "text/xml");

	   // schedulare azione (_command) su devices (id)  con valore (value)
		  hue_knx_queue schedule;

		  schedule.hueid = id;
		  schedule.huecommand = _command;
		  schedule.pctvalue = value100;
		  schedule.huevalue = value255;
	  // push
		  _schedule.push_back(schedule);
    }
  }
}
// ===================================================================================
int HUE_start(char verb)
{
	int rc = 1;
	hueverbose = verb;
    // Initialize server socket..
	tryMyIp();

    // When a new client connected:
    hueServer.onNewConnection = [&](TCPSocket *hueClient) 
	{
        if (hueverbose) cout << "\nTCP client " << hueClient->remoteAddress() << ":" << hueClient->remotePort() << "\n" << endl;

        hueClient->onRawMessageReceived = [hueClient](const char* data, int len) 
		{
              if (hueverbose>1) cout << "\n\nTCP RAW request from " << hueClient->remoteAddress() << ":" << hueClient->remotePort() << " => \n" << data << endl;
			
			// parse TCP packet ----------------------------------------------------------------------------

			  char * p = (char *) data;
			  p[len] = 0;
			  p[len++] = ' ';
			  p[len++] = 0;

			  // Method is the first word of the request
			  char * method = p;

			  while (*p != ' ') p++;
			  *p = 0;
			  p++;

			  // Split word and flag start of url
			  char * url = p;

			  // Find next space
			  while (*p != ' ') p++;
			  *p = 0;
			  p++;

			  // Find double line feed
			  unsigned char c = 0;
			  while ((*p != 0) && (c < 2)) {
				if (*p != '\r') {
				  c = (*p == '\n') ? c + 1 : 0;
				}
				p++;
			  }
			  char * body = p;

			  bool isGet = (strncmp(method, "GET", 3) == 0);


			// packet response

			  if (strncmp(url,"/description.xml",16) == 0)
			  {
//				  cout << "\nurl: " << " description.xml \n" << endl;
				  _onTCPDescription(hueClient, url, body);
				  if (hueverbose>2) printf("<--\n");
			  }
			  else
			  if (strncmp(url,"/api",4) == 0) 
			  {
			//  if (url.startsWith("/api")) {
				if (isGet) 
					// GET api ------------------------------------------------------------------------
				{
//					cout << "\n get /api - tcp list \n" << endl;
					_onTCPList(hueClient, url, body);
				} else 
					// POST api -----------------------------------------------------------------------
				{
//					cout << "\n /api - tcp control \n" << endl;
					_onTCPControl(hueClient, url, body);
				}
			  }
//			hueClient->Send("404 KO");
        };
        
        hueClient->onSocketClosed = [hueClient]() 
		{
            if (hueverbose>1) cout << "\nSocket closed:" << hueClient->remoteAddress() << ":" << hueClient->remotePort() << endl;
			if (hueverbose>2) printf(" ...\n");
        };
    };

    // Bind the server to a port.
    hueServer.Bind(HUE_TCP_PORT, [](int errorCode, string errorMessage)	
	{
        // BINDING FAILED:
        cout << errorCode << " :---> " << errorMessage << endl;
    });

    // Start Listening the server.
    hueServer.Listen([](int errorCode, string errorMessage) 
	{
        // LISTENING FAILED:
        cout << errorCode << " :-> " << errorMessage << endl;
    });

	// non rispondere se presente stringa  Windows 

unsigned char udpanswer = 0;

    // If you want to use raw byte arrays:
    
    udpServer.onRawMessageReceived = [&](const char* message, int length, string ipv4, uint16_t port) 
	{
	  if (length > 0) 
	  {
		std::string request = (char *) message;
		if (hueverbose>2) printf("\nUDP request: \n %s\n",message);
		if (request.find("M-SEARCH") != std::string::npos) 
		{
			if ((request.find("Windows") != std::string::npos))
			{
			}
			else
			if ((request.find("upnp:rootdevice") != std::string::npos) || (request.find("ssdp:discover") != std::string::npos) || (request.find("device:basic:1") != std::string::npos) || (request.find("ssdp:all") != std::string::npos))
			{
				if (hueverbose == 2) printf ("\n\nudp request for alexa ssdp:all - answered");

  			    char response[1024];
				if (udpanswer == 0)
				{
					sprintf(response, HUE_UDP_RESPONSE_TEMPLATE, my_ipaddress, HUE_TCP_PORT, my_bridgeid, my_macaddress  );
				}
				else
				if (udpanswer == 1)
				{
					sprintf(response, HUE_UDP_RESPONSE_TEMPLATE_1, my_ipaddress, HUE_TCP_PORT, my_bridgeid, my_macaddress, my_macaddress  );
				}
				else
				if (udpanswer == 2)
				{
					sprintf(response, HUE_UDP_RESPONSE_TEMPLATE_2, my_ipaddress, HUE_TCP_PORT, my_bridgeid, my_macaddress );
					tcppost = 0;
				}
				if (hueverbose>2) printf("\nresponse for UDP alexa: \n%s\n",response);
				udpServer.SendTo(response, ipv4, port);
				udpanswer++;
				if (udpanswer > 2) udpanswer = 0;
			}
		}
	  }
    };

    // Bind the server to a port.
    udpServer.BindMulticast(HUE_UDP_MULTICAST_PORT,HUE_UDP_MULTICAST_MASK, [](int errorCode, string errorMessage) 
	{
        cout << errorCode << " : --> " << errorMessage << endl;
//		rc=-1;
    });

	printf("HUE tcp/udp OK\n");
	return rc;
}
// ===================================================================================
void HUE_stop(void)
{
    hueServer.Close();
    udpServer.Close();
}
// ===================================================================================



