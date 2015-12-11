#include <SPI.h>
#include <Adafruit_CC3000.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

#define MS_PROXY                "u614vn.messaging.internetofthings.ibmcloud.com"
#define MQTT_PORT               1883
#define MQTT_MAX_PACKET_SIZE    100
#define MQTT_CLIENT_ID          "d:u614vn:iotsample-arduino:00ffbbccde02"               // d:org_id:device_type:device_id, d - device, <org_id>, <device_type>, <device_id>，必须与设备注册信息一致
#define MQTT_TOPIC              "iot-2/evt/status/fmt/json"
#define AUTHMETHOD              "use-token-auth"
#define AUTHTOKEN               "password"
byte mac[] = { 0x00, 0xFF, 0xBB, 0xCC, 0xDE, 0x02 };                                    // consistent with <device_id>

//For Arduino Yun, instantiate a YunClient and use the instance to declare
//an IPStack ipstack(c) instead of EthernetStack with c being the YunClient
// e.g. YunClient c;
// IPStack ipstack(c);
// EthernetClient c;            // replace by a YunClient if running on a Yun
Adafruit_CC3000_Client c;
IPStack ipstack(c);

MQTT::Client<IPStack, Countdown, 100, 1> client = MQTT::Client<IPStack, Countdown, 100, 1>(ipstack);

// Wifi/WLAN
#define WLAN_SSID       "Wifi-SSID"
#define WLAN_PASS       "Wifi-Password"
#define WLAN_SECURITY   WLAN_SEC_WPA2                           // Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2

// CC3000
#define ADAFRUIT_CC3000_IRQ     3                               // MUST be an interrupt pin!
#define ADAFRUIT_CC3000_VBAT    5                               // Can be any pin
#define ADAFRUIT_CC3000_CS      10                              // Can be any pin
// Use hardware SPI for the remaining pins, for UNO, SCK = 13, MISO = 12, MOSI = 11
Adafruit_CC3000                 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER); // you can change this clock speed but DI


void setup() {
  Serial.begin(9600);

  if (!cc3000.begin())
  {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while (1);
  }
  displayMACAddress();

  // connect to Wifi
  cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);
  while (!cc3000.checkDHCP())                                   // wait until get DHCP address
    delay(100);
  displayConnectionDetails();
}


void loop() {
  int rc = -1;
  if (!client.isConnected()) {
    Serial.println("Connecting to IoT Foundation for publishing Temperature");
    while (rc != 0) {
      rc = ipstack.connect(MS_PROXY, MQTT_PORT);
    }
    Serial.println("TCP connected");

    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.MQTTVersion = 3;
    options.clientID.cstring = MQTT_CLIENT_ID;
    options.username.cstring = AUTHMETHOD;
    options.password.cstring = AUTHTOKEN;
    options.keepAliveInterval = 10;
    rc = -1;
    while ((rc = client.connect(options)) != 0)
    {
      Serial.print("rc=");
      Serial.println(rc);
      delay (2000);
    }
    Serial.println("MQTT connected");

    Serial.println("Connected successfully\n");
    Serial.println("Temperature(in C)\tDevice Event (JSON)");
    Serial.println("____________________________________________________________________________");
  }

  MQTT::Message message;
  message.qos = MQTT::QOS0;
  message.retained = false;

  char json[56] = "{\"d\":{\"myName\":\"Arduino Uno\",\"temperature\":";
  float tempValue = getTemp();
  dtostrf(tempValue,1,2, &json[43]);
  json[48] = '}';
  json[49] = '}';
  json[50] = '\0';
  Serial.print("\t");
  Serial.print(tempValue);
  Serial.print("\t\t");
  Serial.println(json);
  message.payload = json;
  message.payloadlen = strlen(json);

  rc = client.publish(MQTT_TOPIC, message);
  if (rc != 0) {
    Serial.print("return code from publish was ");
    Serial.println(rc);
  }
  client.yield(1000);
}


/*
This function is reproduced as is from Arduino site => http://playground.arduino.cc/Main/InternalTemperatureSensor
*/
double getTemp(void) {
  unsigned int wADC;
  double t;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN); // enable the ADC
  delay(20); // wait for voltages to become stable.
  ADCSRA |= _BV(ADSC); // Start the ADC
  // Detect end-of-conversion
  while (bit_is_set(ADCSRA,ADSC));
  // Reading register "ADCW" takes care of how to read ADCL and ADCH.
  wADC = ADCW;
  // The offset of 324.31 could be wrong. It is just an indication.
  t = (wADC - 324.31 ) / 1.22;
  // The returned temperature is in degrees Celcius.
  return (t);
}


/**************************************************************************/
/*!
    @brief  Tries to read the 6-byte MAC address of the CC3000 module
*/
/**************************************************************************/
void displayMACAddress(void)
{
  uint8_t macAddress[6];

  if(!cc3000.getMacAddress(macAddress))
  {
    Serial.println(F("Unable to retrieve MAC Address!\r\n"));
  }
  else
  {
    Serial.print(F("MAC Address : "));
    cc3000.printHex((byte*)&macAddress, 6);
  }
}


/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;

  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println("\n");
    return true;
  }
}
