// =============================================================================================
#define HUE_UDP_MULTICAST_MASK   "239.255.255.250"
#define HUE_UDP_MULTICAST_PORT   1900
#define HUE_TCP_PORT 80
#define HUE_TCP_PACKETSIZE 2700
//#define SHORT_DECLARE
#define UNIQUE_MY "00:17:88:01:00:f0:"
//#define SHORT_MACADDRESS
//#define UNIQUE_MACADDRESS
#define USERNAME "\"U8RV5Aj2fAUpChl1vdDwOVqyTThnQGTh9smG32tn\""
// =============================================================================================
const char HUE_DEVICE_JSON_TEMPLATE[] = // state, bright, nome, mac, progmac
"{\"state\":{\"on\":%s,\"bri\":%d,\"hue\":0,\"sat\":0,\"effect\":\"none\",\"xy\":[0.0000,0.0000],\"ct\":0,\"alert\":\"none\",\"colormode\":\"hs\",\"reachable\":true}," // era false
"\"type\":\"Extended color light\",\"name\":\"%s\",\"modelid\":\"LCT003\",\"manufacturername\":\"Philips\",\"uniqueid\":\""
#ifdef UNIQUE_MACADDRESS
    "%s"
#endif
#ifdef SHORT_MACADDRESS
    "%s"
#endif
#ifdef UNIQUE_MY
    UNIQUE_MY
#endif
	"%02x:e9-0b\",\"swversion\":\"5.11.0.10732\"}";
// ===================================================================================
const char HUE_TCP_OK[] =
"HTTP/1.1 200 OK\r\n";
// ===================================================================================
const char HUE_TCP_HEADERS[] =
//"HTTP/1.1 %s\r\n"
"Content-Type: %s\r\n"
"Content-Length: %d\r\n"
"Connection: close\r\n\r\n";	
// ===================================================================================
const char HUE_TCP_HEADER0[] =
//"HTTP/1.1 %s\r\n"
"Content-Type: %s\r\n"
"Content-Length: %d\r\n"
"Connection: Keep-Alive\r\n\r\n";
// ===================================================================================
const char HUE_TCP_HEADER1[] =
//"HTTP/1.1 %s\r\n"
"Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n"
"Pragma: no-cache\r\n"
"Expires: Mon, 1 Aug 2011 09:00:00 GMT\r\n"
"Connection: close\r\n"
"Access-Control-Max-Age: 3600\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Allow-Credentials: true\r\n"
"Access-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE, HEAD\r\n"
"Access-Control-Allow-Headers: Content-Type\r\n"
"Content-type: application/json\r\n\r\n";
// ===================================================================================
  const char HUE_DESCRIPTION_TEMPLATE[] = // var: address, port, address, , mac, mac
"<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
    "<specVersion><major>1</major><minor>0</minor></specVersion>\n"
    "<URLBase>http://%s:%d/</URLBase>\n"
    "<device>\n"
        "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\n"
        "<friendlyName>Philips hue (%s)</friendlyName>\n"	// NON SERVE LA PORTA
        "<manufacturer>Royal Philips Electronics</manufacturer>\n"
        "<manufacturerURL>http://www.philips.com</manufacturerURL>\n"
        "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>\n"
        "<modelName>Philips hue bridge 2012</modelName>\n"
        "<modelNumber>929000226503</modelNumber>\n"
        "<modelURL>http://www.meethue.com</modelURL>\n"
        "<serialNumber>%s</serialNumber>\n"
        "<UDN>uuid:2f402f80-da50-11e1-9b23-%s</UDN>\n"
        "<presentationURL>index.html</presentationURL>\n"
"<iconList>\n"
"<icon>\n"
"<mimetype>image/png</mimetype>\n"
"<height>48</height>\n"
"<width>48</width>\n"
"<depth>24</depth>\n"
"<url>hue_logo_0.png</url>\n"
"</icon>\n"
"</iconList>\n"
    "</device>\n"
"</root>\n";
// ===================================================================================
const char HUE_DEVICE_JSON_TEMPLATE_FIRST[] = "{"	// DA RIVEDERE <------------------------
    "\"name\":\"%s\","    // max name length 20 bytes
    "\"uniqueid\":\""
#ifdef UNIQUE_MACADDRESS
    "%s"
#endif
#ifdef SHORT_MACADDRESS
    "%s"
#endif
#ifdef UNIQUE_MY
    UNIQUE_MY
#endif
    "%02x:e9-0b\","
	"\"capabilities\":{}"
"}";
// ===================================================================================
// new - risposta dopo get api lights e risposta 200ok o post api o get description (prima del resto)
const char HUE_TCP_RESPONSE_TEMPLATE[] = 
"Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n"
"Pragma: no-cache\r\n"
"Expires: Mon, 1 Aug 2011 09:00:00 GMT\r\n"
"Connection: close\r\n"
"Access-Control-Max-Age: 3600\r\n"
"Access-Control-Allow-Origin: *\r\n"
"Access-Control-Allow-Credentials: true\r\n"
"Access-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE, HEAD\r\n"
"Access-Control-Allow-Headers: Content-Type\r\n"
"Content-type: application/json\r\n";
// ===================================================================================
const char HUE_TCP_STATE_RESPONSE[] = "["
	"{\"success\":{\"/lights/%d/state/on\":%s}}]";
const char HUE_TCP_VALUE_RESPONSE[] = "["
	"{\"success\":{\"/lights/%d/state/bri\":%d}}]";
const char HUE_TCP_KO_RESPONSE[] = "["
	"[{\"error\":{\"type\":101,\"address\":\"\",\"description\":\"link button not pressed\"}}]";
// ===================================================================================
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
