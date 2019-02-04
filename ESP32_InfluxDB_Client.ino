//#include <WiFi.h>         //for ESP32
//#include <WiFiNINA.h>   //for MKR1010
#include <ESP8266WiFi.h>

// Application Config
const int n_measurements = 3 ;
int update_ms = 10000 ;

// Hotspot
const char* wifi_ssid = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";              //your wifi name
const char* wifi_password = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";         //your wifi password 

// InfluxDB Server
const char* host = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";                               //your server IP
const int port = XXXX;                                                              //port
const char* influx_database = "XXXXXXXXXXXXXXXXXXXX";                               //name of the InfluxDB database

// InfluxDB Client
const char* influx_username = "XXXXXXXXXXXXXXXXX";                                  //username on InfluxDB  
const char* influx_password = "XXXXXXXXXXXXXXXXXXXXXX";                             //password on InfluxDB

// InfluxDB Measurements
char* influx_measurement_and_tags[n_measurements];
const char* influx_field_key = "value";
float influx_field_value[n_measurements];
float last_influx_field_value[n_measurements];
bool influx_field_value_changed[n_measurements];
bool any_changement = true;

/***************************************************************************/
/**************************SENSORS CONFIGURATION****************************/
/***************************************************************************/
//DHT11
#include <dht11.h>
dht11 DHT11;
#define DHT11PIN 18
/**************************************/
//DS18B20
#include "DS18B20.h"
DS18B20  ds(15, 1);
//I'll need a temporary variable to check the value
float temp_DS18B20;

/***************************************************************************/
/*********************************SETUP*************************************/
/***************************************************************************/

void setup() {

  /***************************************************************************/
  /*******************influxDB MEASUREMENTS CONFIGURATION*********************/
  /***************************************************************************/
  
influx_measurement_and_tags[0] = "temperature,device=ESP32,sensor=DHT11,location=bathroom";
  influx_measurement_and_tags[1] = "humidity,device=ESP32,sensor=DHT11,location=bathroom";
  influx_measurement_and_tags[2] = "temperature,device=ESP32,sensor=DS18B20,location=bathroom";

  // SERIAL SETUP
  Serial.begin(115200);
  delay(10);


  // WiFi SETUP
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(wifi_ssid);
  //If you don's put the following command ESP32 will work also as AP
  WiFi.mode(WIFI_STA);		//comment for MKR1010
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  /***************************************************************************/
  /*****************************SENSORS SETUP*********************************/
  /***************************************************************************/
  //DHT11
  Serial.print("DHT11 LIBRARY VERSION: ");
  Serial.println(DHT11LIB_VERSION);
  /***************************************************************************/
  //DS18B20 - Check if sensor connected
  Serial.print(ds.getNumber());
  Serial.println(" DS18B20 device(s) connected");

}

/***************************************************************************/
/********************************LOOP***************************************/
/***************************************************************************/


void loop() {

  /***************************************************************************/
  /*****************************SENSORS CHECK*********************************/
  /***************************************************************************/
  Serial.println("____________________________________________________________________________________________________");

  //DHT11
  int DHT11_check = DHT11.read(DHT11PIN);

  Serial.print("Read sensor: ");
  switch (DHT11_check)
  {
    case DHTLIB_OK:
      Serial.println("OK");
      break;
    case DHTLIB_ERROR_CHECKSUM:
      Serial.println("Checksum error");
      break;
    case DHTLIB_ERROR_TIMEOUT:
      Serial.println("Time out error");
      break;
    default:
      Serial.println("Unknown error");
      break;
  }
  /***************************************************************************/
  //DS18B20
  ds.start();
  delay(200);
  float temp_value;
  temp_value = ds.get(0);
  if (temp_value < 200) {
    temp_DS18B20 = temp_value ;
  } else {
    temp_value = ds.get(0);
    if (temp_value < 200) {
      temp_DS18B20 = temp_value ;
    } else {
      temp_value = ds.get(0);
      if (temp_value < 200) {
        temp_DS18B20 = temp_value ;
      }
    }
  }
  //try 3 times to read the value otherwise the value won't be updated

  /***************************************************************************/
  /*****************SENSORS TO InflxuDB FIELD ASSIGNMENT**********************/
  /***************************************************************************/

  // assign sensor value to measurements
  influx_field_value[0] = (float)DHT11.temperature;
  influx_field_value[1] = (float)DHT11.humidity;
  influx_field_value[2] = temp_DS18B20;




  /***************************************************************************/
  /*****************NO MORE CONFIGURATION FROM HERE TO END********************/
  /***************************************************************************/


  // check for changements
  any_changement = false;
  for (int i = 0; i < n_measurements; i++) {
    Serial.print(String(influx_measurement_and_tags[i]) + "\t" + influx_field_key + "=" + influx_field_value[i]);

    if (influx_field_value[i] != last_influx_field_value[i]) {
      influx_field_value_changed[i] = true;
      Serial.println("\tValue changed");
      any_changement = true;
    } else {
      influx_field_value_changed[i] = false;
      Serial.println("\tValue has not changed");
    }
  }

  if (any_changement) {

    Serial.println("\n\n\n\________________________________________CONNECTION__________________________________________________ ");

    // Send data to InfluxDB
    Serial.print("Connecting to ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);

    // Make a new client to connect to the server with
    WiFiClient client;
    if (!client.connect(host, port)) {
      Serial.println("connection failed");
      return;
    }

    // Build the request URI with the database, username and password
    String url = "/write?db=";
    url += influx_database;
    url += "&u=";
    url += influx_username;
    url += "&p=";
    url += influx_password;
    Serial.print("Requesting URL: ");
    Serial.println(url);


    // Build the full POST request
    client.println(String("POST ") + url + " HTTP/1.1");
    client.println(String("Host: ") + host);
    client.println("Connection: close");
    client.print("Content-Length: ");

    //**************************************************************************************************

    // Make the POST body containing the data
    String body;
    for (int i = 0; i < n_measurements; i++) {
      if (influx_field_value_changed[i]) {
        body = body + String(influx_measurement_and_tags[i]) + " " + influx_field_key + "=" + influx_field_value[i] + "\n" ;
      }
    }
    Serial.println("Body: ");
    Serial.println(body);
    client.println(body.length());
    client.println();
    client.print(body);

    //**************************************************************************************************


    // Wait for the response
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Client timed out!");
        client.stop();
        return;
      }
    }

    // Read the reply and print it out
    Serial.println("Got response:");
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println();
  }

  //save last values
  for (int i = 0; i < n_measurements; i++) {
    last_influx_field_value[i] = influx_field_value[i] ;
  }


  delay(update_ms);

}
