// =============================================================================================
#define HUE_UDP_MULTICAST_MASK   "239.255.255.250"
#define HUE_UDP_MULTICAST_PORT   1900
#define HUE_TCP_PORT 80
#define HUE_TCP_PACKETSIZE 2700
#define SHORT_DECLARE
//#define UNIQUE_MY "00:17:88:01:00:f0:"
//#define UNIQUE_MY "00:17:88:b4:d6:c7:"
#define SHORT_MACADDRESS
//#define UNIQUE_MACADDRESS

//#define USERNAME "\"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\""
#define USERNAME "\"U8RV5Aj2fAUpChl1vdDwOVqyTThnQGTh9smG32tn\""	// nuovo da hue 1

// =============================================================================================
const char HUE_DEVICE_JSON_TEMPLATE[] = "{"	// nome, mac, progmac, state, bright
    "\"type\":\"Extended Color Light\","
    "\"name\":\"%s\","
    "\"uniqueid\":\""
#ifdef UNIQUE_MACADDRESS
    "%s"
    "-%d\","
#endif
#ifdef SHORT_MACADDRESS
	"00:17:88:"
	"%s"
	"%02x:e9-0b\","
#endif
#ifdef UNIQUE_MY
    UNIQUE_MY
    "%02x:e9-0b\","
#endif
    "\"modelid\":\"LCT007\","
    "\"state\":{"
        "\"on\":%s,\"bri\":%d,\"xy\":[0,0],\"reachable\": true"
    "},"
    "\"capabilities\":{"
        "\"certified\":false,"
        "\"streaming\":{\"renderer\":true,\"proxy\":false}"
    "},"
    "\"swversion\":\"5.105.0.21169\""
"}";
// =============================================================================================
const char HUE_TCP_HEADERS[] =
"HTTP/1.1 %s\r\n"
"Content-Type: %s\r\n"
"Content-Length: %d\r\n"
"Connection: close\r\n\r\n";
// ===================================================================================
const char HUE_DESCRIPTION_TEMPLATE[] =		// var: address, port, address, port, mac, mac
"<?xml version=\"1.0\" ?>" 
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">" 
    "<specVersion><major>1</major><minor>0</minor></specVersion>" 
    "<URLBase>http://%s:%d/</URLBase>" 
    "<device>" 
        "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>" 
        "<friendlyName>Philips hue (%s:%d)</friendlyName>" 
        "<manufacturer>Royal Philips Electronics</manufacturer>" 
        "<manufacturerURL>http://www.philips.com</manufacturerURL>" 
        "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>" 
        "<modelName>Philips hue bridge 2012</modelName>" 
        "<modelNumber>929000226503</modelNumber>" 
        "<modelURL>http://www.meethue.com</modelURL>" 
        "<serialNumber>%s</serialNumber>" 
        "<UDN>uuid:2f402f80-da50-11e1-9b23-%s</UDN>" 
        "<presentationURL>index.html</presentationURL>" 
    "</device>" 
"</root>";
// ===================================================================================
const char HUE_DEVICE_JSON_TEMPLATE_FIRST[] = "{"
    "\"name\":\"%s\","    // max name length 20 bytes
    "\"uniqueid\":\""
#ifdef UNIQUE_MACADDRESS
    "%s"
    "-%d\","
#endif
#ifdef SHORT_MACADDRESS
	"00:17:88:"
	"%s"
	"%02x:e9-0b\","
#endif
#ifdef UNIQUE_MY
    UNIQUE_MY
    "%02x:e9-0b\","
#endif
	"\"capabilities\":{}"
"}";
// ===================================================================================
const char HUE_TCP_STATE_RESPONSE[] = "["
	"{\"success\":{\"/lights/%d/state/on\":%s}}]";
const char HUE_TCP_VALUE_RESPONSE[] = "["
	"{\"success\":{\"/lights/%d/state/bri\":%d}}]";
const char HUE_TCP_KO_RESPONSE[] = "["
	"[{\"error\":{\"type\":101,\"address\":\"\",\"description\":\"link button not pressed\"}}]";
// ===================================================================================
/*
const char HUE_UDP_RESPONSE_TEMPLATE[] =  // ipaddress, port, mac, mac
	"HTTP/1.1 200 OK\r\n"
	"EXT:\r\n"
	"CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
	"LOCATION: http://%s:%d/description.xml\r\n"
	"SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n"		// _modelName, _modelNumber - 1.16.0 ?
	"hue-bridgeid: %s\r\n"
	"ST: urn:schemas-upnp-org:device:basic:1\r\n"				// _deviceType
	"USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n" // _uuid::_deviceType
	"\r\n";
	*/
const char HUE_UDP_RESPONSE_TEMPLATE[] =	// ipaddress, port, bridgeid, mac
	"HTTP/1.1 200 OK\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"EXT:\r\n"
	"CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
	"LOCATION: http://%s:%d/description.xml\r\n"
	"SERVER: FreeRTOS/7.4.2, UPnP/1.0, IpBridge/1.16.0\r\n"		// _modelName, _modelNumber - 1.16.0 ?
	"hue-bridgeid: %s\r\n"
	"ST: upnp:rootdevice\r\n"
	"USN: uuid:2f402f80-da50-11e1-9b23-%s::upnp:rootdevice\r\n" // _uuid::_deviceType
	"\r\n";
// =============================================================================================
const char HUE_UDP_RESPONSE_TEMPLATE_1[] =	// ipaddress, port, bridgeid, mac, mac
	"HTTP/1.1 200 OK\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"EXT:\r\n"
	"CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
	"LOCATION: http://%s:%d/description.xml\r\n"
	"SERVER: FreeRTOS/7.4.2, UPnP/1.0, IpBridge/1.16.0\r\n"		// _modelName, _modelNumber - 1.16.0 ?
	"hue-bridgeid: %s\r\n"
	"ST: uuid:2f402f80-da50-11e1-9b23-%s\r\n"
	"USN: uuid:2f402f80-da50-11e1-9b23-%s\r\n" // _uuid::_deviceType
	"\r\n";
// =============================================================================================
const char HUE_UDP_RESPONSE_TEMPLATE_2[] =	// ipaddress, port, bridgeid, mac
	"HTTP/1.1 200 OK\r\n"
	"HOST: 239.255.255.250:1900\r\n"
	"EXT:\r\n"
	"CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
	"LOCATION: http://%s:%d/description.xml\r\n"
	"SERVER: FreeRTOS/7.4.2, UPnP/1.0, IpBridge/1.16.0\r\n"		// _modelName, _modelNumber - 1.16.0 ?
	"hue-bridgeid: %s\r\n"
	"ST: urn:schemas-upnp-org:device:basic:1\r\n"
	"USN: uuid:2f402f80-da50-11e1-9b23-%s\r\n" // _uuid::_deviceType
	"\r\n";
// =============================================================================================

// =============================================================================================
