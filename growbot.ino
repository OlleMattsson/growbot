/*
    GROW BOT 0.1
*/

// WIFI
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
const char *ssid = "";
const char *password = "";

// DHT11
#include "DHT.h"
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 27          // Data wire For Temp Probes
OneWire oneWire(ONE_WIRE_BUS);// Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);// Pass our oneWire reference to Dallas Temperature.
DeviceAddress temp01 = { 0x28, 0x11, 0x81, 0x79, 0x97, 0x04, 0x03, 0xC4 }; // tank
DeviceAddress temp02 = { 0x28, 0xC4, 0xE2, 0x79, 0x97, 0x04, 0x03, 0xB4 }; // canopy
float tankTemp= 0;
float canopyTemp = 0;

// PH
const int phPin = A0;
float calibration = 23.3; //change this value to calibrate
int sensorValue = 0;
unsigned long int avgValue;
int buf[10], temp;


// SERVER
#include <WebServer.h>
WebServer server(80);

// EC
int R1= 1000;
int Ra=25; //Resistance of powering Pins
float PPMconversion=0.7;
float TemperatureCoef = 0.019; //this changes depending on what chemical we are measuring
float K=1.2;
const int TempProbePossitive = 8;   //Temp Probe power connected to pin 9
const int TempProbeNegative = 9;    //Temp Probe Negative connected to pin 8
float EC=0;
float EC25 =0;
int ppm =0;
float raw= 0;
float Vin= 3.3;
float Vdrop= 0;
float Rc= 0;
float buffer=0;
int ECPin= A7; //A0;
int ECGround= A3; //A1;
int ECPower = A6; //A2;

const int led = 13;

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);


  // Establish Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }


  /*
   * ROUTES
  */
  server.on("/", dataView );
  server.on("/test.svg", drawGraph);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);

  server.on("/data", dataView);
  
  server.begin();
  Serial.println("HTTP server started");

  // DHT
  dht.begin();

  // DS18B20
  sensors.begin();
  sensors.setResolution(temp01, 10);
  sensors.setResolution(temp02, 10);
  delay(100);

  // EC
  pinMode(TempProbeNegative , OUTPUT ); //seting ground pin as output for tmp probe
  digitalWrite(TempProbeNegative , LOW );//Seting it to ground so it can sink current
  pinMode(TempProbePossitive , OUTPUT );//ditto but for positive
  digitalWrite(TempProbePossitive , HIGH );
  pinMode(ECPin,INPUT);
  pinMode(ECPower,OUTPUT);//Setting pin for sourcing current
  pinMode(ECGround,OUTPUT);//setting pin for sinking current
  digitalWrite(ECGround,LOW);//We can leave the ground connected permanantly

  analogSetWidth(10);                           // 10Bit resolution
  analogSetAttenuation((adc_attenuation_t)2);   // -6dB range

  delay(1000);
}



void loop(void) {
  server.handleClient();
}


/*
    DATA VIEW
*/

void dataView() {
  char temp[1000];

  // DTH11
  float humidity = dht.readHumidity();
  float atemp = dht.readTemperature();

  // DS18B20
  sensors.requestTemperatures();// Send the command to get temperatures
  canopyTemp = sensors.getTempC(temp02);
  tankTemp = sensors.getTempC(temp01);

  // ph
  float ph = getPh();

  // EC
  GetEC(); 
  
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  Serial.print("Ambient temp: ");
  Serial.print(atemp); 
  Serial.println(" *C");
  
  Serial.print("Canapy temp: ");
  Serial.print(canopyTemp);
  Serial.println(" *C");
  
  Serial.print("Tank temp: ");
  Serial.print(tankTemp);
  Serial.println(" *C");

  Serial.print("pH: ");
  Serial.println(ph);

  Serial.print("Conductivity: ");
  Serial.print(EC25 * 1000);
  Serial.println(" uS/cm");
  Serial.print("TDS: ");
  Serial.print(ppm);
  Serial.print(" ppm @ ");
  Serial.println(PPMconversion);
  

  snprintf(temp, 1000,
   "<html>\
    <head>\
      <meta http-equiv='refresh' content='5'/>\
      <title>Growbot</title>\
      <style>\
        body { background-color: #fff; font-family: Helvetica, Sans-Serif; Color: #3d3d3ds; }\
      </style>\
    </head>\
    <body>\
      <h1>Growbot</h1>\
      <table>\
      <tr>\
        <td>humidity</td>\
        <td>%d %c </td>\
      </tr>\    
      <tr>\
        <td>ambient temperature</td>\
        <td>%d \xB0\C </td>\
      </tr>\
      <tr>\
        <td>canopy temperature</td>\
        <td>%d \xB0\C</td>\
      </tr>\
      <tr>\
        <td>tank temperature</td>\
        <td>%d \xB0\C</td>\
      </tr>\     
      <tr>\
        <td>pH</td>\
        <td>%f </td>\
      </tr>\    
      <tr>\
        <td>Conductivity</td>\
        <td>%d uS/cm (%f)</td>\
      </tr>\ 
      <tr>\
        <td>TDS</td>\
        <td>%d ppm @ %f</td>\
      </tr>\               
      </table>\
    </body>\
  </html>",
    int(humidity), 
    37, 
    int(atemp), 
    int(canopyTemp), 
    int(tankTemp), 
    ph, 
    int(EC25 * 1000),
    K, 
    int(ppm), 
    PPMconversion
   );
   
  server.send(200, "text/html", temp);
}



/*
    PH
*/
float getPh() {
  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(phPin);
    delay(30);
  }
  
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  
  avgValue = 0;
  
  for (int i = 2; i < 8; i++) {
    avgValue += buf[i];
  }

  avgValue = avgValue / 6;

  float pinVoltage = 3.3;
  int AdcResolution = 1023;
  float phPinVoltage = avgValue * (pinVoltage / AdcResolution);
  float phValue = -12 * phPinVoltage + calibration;
  return phValue;
 }


/*
    EC
*/

 void GetEC() {
  // read solution temp
  sensors.requestTemperatures();// Send the command to get temperatures
  tankTemp = sensors.getTempC(temp01);  
 
  // Estimate Resistance of Liquid
  digitalWrite(ECPower,HIGH);
  raw = analogRead(ECPin);
  raw = analogRead(ECPin);// This is not a mistake, First reading will be low beause if charged a capacitor
  digitalWrite(ECPower,LOW);
 
  //Convert to EC
  Vdrop= (Vin*raw)/1024.0;
  Rc=(Vdrop*R1)/(Vin-Vdrop);
  Rc=Rc-Ra; //acounting for Digital Pin Resitance
  EC = 1000/(Rc*K);
   
  //Compensating For Temperaure
  EC25 = EC/ (1 + TemperatureCoef * (tankTemp - 25.0));
  ppm = (EC25) * (PPMconversion * 1000);  
}



/*
    Misc
*/


void handleRoot() {
  digitalWrite(led, 1);
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400,
  
 "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>Growbot</title>\
    <style>\
      body { background-color: #fff; font-family: Helvetica, Sans-Serif; Color: #3d3d3ds; }\
    </style>\
  </head>\
  <body>\
    <h1>Growbot</h1>\
    <h2>0.1</h2>\
    <h2>Uptime: %0sd:%02d:%02d</h2>\
  </body>\
</html>",

           hr, min % 60, sec % 60
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}


void drawGraph() {
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}
