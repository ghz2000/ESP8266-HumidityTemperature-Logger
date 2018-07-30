#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include "Ambient.h"

#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include "FS.h"
#include "NTP.h"

#include "webserver.c"

ESP8266WebServer server ( 80 );
Ambient ambient;
WiFiClient client;

#define FILE_NAME "/th2.txt"

#define DEBUG 0
#if DEBUG
#else
#endif

const char* ssid[] = {"", ""};
const char* password[] = {"", ""};

const unsigned int channelId = ;
const char* writeKey = "";

void setup() {
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);

  delay(1000);
  Serial.println("");

  Wire.begin();

  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid[0], password[0]);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("SSID0 Connection Failed! next SSID1");
    WiFi.begin(ssid[1], password[1]);
  }
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");


  //NTPのため
  // 2390 はローカルのUDPポート。空いている番号なら何番でもいいです。
  ntp_begin(2390);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());



  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/log1", handleLog1);
  server.on("/log2", handleLog2);
  server.on("/glaph", handleGlaph);
  server.on("/data.js", handleDatajs);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");


  SPIFFS.begin();

  FSInfo fsinfo;
  SPIFFS.info(fsinfo);
  Serial.print("totalBytes =");
  Serial.println(fsinfo.totalBytes);
  Serial.print("usedBytes =");
  Serial.println(fsinfo.usedBytes);
  Serial.print("blockSize =");
  Serial.println(fsinfo.blockSize);
  Serial.print("pageSize =");
  Serial.println(fsinfo.pageSize);
  Serial.print("maxOpenFiles =");
  Serial.println(fsinfo.maxOpenFiles);
  Serial.print("maxPathLength =");
  Serial.println(fsinfo.maxPathLength);


  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.print(dir.fileName());
    File f = dir.openFile("r");
    Serial.println(f.size());
  }

  SPIFFS.end();
}


void display(char *s) {
  time_t n = now();
  time_t t;

  const char* format = "%04d-%02d-%02d %02d:%02d:%02d";

  // JST
  t = localtime(n, 9);
  sprintf(s, format, year(t), month(t), day(t), hour(t), minute(t), second(t));


  //anbient
  bool res;
  res = ambient.begin(channelId, writeKey, &client);

  Serial.print("begin:");
  Serial.println( res ? "Success" : "Failed" );
}



//#############################  ここから Loop #########################
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  float hum, tempe;
  if (!getAM2320(&hum, &tempe)) return;
  if(hum ==0 && tempe == 0) return;

  SPIFFS.begin();
  File fd = SPIFFS.open(FILE_NAME, "a");
  if (!fd) {
    Serial.println("open error");
  }

  char s[20];
  display(s);
  Serial.print(s);
  fd.print(s);

  Serial.print("  Humidity=");
  Serial.print(hum);
  Serial.print("   ");
  Serial.print("  Temperature=");
  Serial.println(tempe);

  fd.print(",");
  fd.print(hum);
  fd.print(",");
  fd.println(tempe);

  fd.close();
  SPIFFS.end();

  {
    ambient.set(1, hum);
    ambient.set(2, tempe);

    bool res;
    res = ambient.send();

    Serial.println( res ? "send:Success" : "send:Failed" );
  }

  for (int i = 0; i < 60; i++) {
    delay(1000);
    ArduinoOTA.handle();
    server.handleClient();
  }
}


bool getAM2320(float *hum, float *tempe) {
  Wire.beginTransmission(0x5c);     // address(0x5c)  sensor(AM2320)
  Wire.write(0x03); //Arduino read senser
  Wire.write(0x00); //address of Humidity
  Wire.write(0x04); //The number of address
  //(Humidity1,Humidity2,Temperature1,Temperature2)
  Wire.endTransmission();//
  delay(1000);
  int ans = Wire.requestFrom(0x5c, 6); // request 6 bytes from sensor(AM2320)

  bool result = 0;

  while (Wire.available() != 0) {
    int H1, H2, T1, T2 = 0;
    float h, t = 0;
    for (int i = 1; i <  ans + 1; i++) {
      int c = Wire.read();
      switch (i) {
        case 5:
          T1 = c;
          break;
        case 6:
          T2 = c;
          break;
        case 3:
          H1 = c;
          break;
        case 4:
          H2 = c;
          break;
        default:
          break;
      }
    }
    h = (H1 * 256 + H2) / 10.0;
    t = (T1 * 256 + T2) / 10.0;
    *hum = h;
    *tempe = t;
    result = 1;
  }
  return result;
}


//##########################  WEB
void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleLog() {
  SPIFFS.begin();
  File fd = SPIFFS.open(FILE_NAME, "r");
  if (!fd) {
    Serial.println("open error");
  }
  size_t totalSize = fd.size();
  delay(1);
  Serial.print("size=");
  Serial.print(String(totalSize) + "\n");
  server.streamFile(fd, "text/plain");
  //  client.write(fd, HTTP_DOWNLOAD_UNIT_SIZE);  // send a binary file

  fd.close();
  SPIFFS.end();
}

void handleLog2() {
  SPIFFS.begin();
  File fd = SPIFFS.open("/th.txt", "r");
  if (!fd) {
    Serial.println("open error");
  }
  size_t totalSize = fd.size();
  delay(1);
  Serial.print("size=");
  Serial.print(String(totalSize) + "\n");
  server.streamFile(fd, "text/plain");
  //  client.write(fd, HTTP_DOWNLOAD_UNIT_SIZE);  // send a binary file

  fd.close();
  SPIFFS.end();
}

void handleLog1() {
  SPIFFS.begin();
  File fd = SPIFFS.open("/log1.txt", "r");
  if (!fd) {
    Serial.println("open error");
  }
  size_t totalSize = fd.size();
  delay(1);
  Serial.print("size=");
  Serial.print(String(totalSize) + "\n");
  server.streamFile(fd, "text/plain");
  //  client.write(fd, HTTP_DOWNLOAD_UNIT_SIZE);  // send a binary file

  fd.close();
  SPIFFS.end();
}

static int vstart;
static int vend;

void handleGlaph() {
  char temp[1300];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  String tmp;
  tmp = server.arg("START");
  vstart = tmp.toInt();
  tmp = server.arg("END");
  vend = tmp.toInt();

  if (server.method() != HTTP_GET) {
    server.send(204, "");
  }

  snprintf ( temp, 1300,

             "<!doctype html>\
 <html>\
  <head>\
    <meta http-equiv='refresh' content='1500'/>\
    <title>Glaph Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    \
    <form method='get' action='./onoff'>\
    <input type='radio' name='LED' value='on' checked='checked' />ON\
    <input type='radio' name='LED' value='off' />OFF\
    <input type='submit' value='送信'>\
    </button></form>\
    \
    <form method='get' action='./glaph'>\
    <p>開始:<input type='text' name='START' value='0' size='10' />\
    終了:<input type='text' name='END' size='10' /></p>\
    <input type='submit' value='送信'>\
    </button></form>\
    \
    <div id=\"chartContainer\"></div>\
    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/canvasjs/1.7.0/canvasjs.min.js\"></script>\
    <script src=\"data.js\"></script>\
  </body>\
</html>",

             hr, min % 60, sec % 60
           );
  server.send ( 200, "text/html", temp );
}

void handleDatajs() {
  String out = "";
  char temp[100];

  if (vend < 500 ) vend = 500;

  out += "var dataPlot = [\n";

  SPIFFS.begin();
  File fd = SPIFFS.open(FILE_NAME, "r");
  if (!fd) {
    Serial.println("open error");
  }

  //    out +="{ x: new Date(2012, 00, 1, 01, 02, 33), y: 13.52 },\n";
  //    out +="{ x: new Date(2016, 11, 7, 01, 01, 43), y: 15.14 },\n";

  for (int i = 0; i < vend; i++) {
    String line = fd.readStringUntil('\n');
    if (!line.compareTo("")) break;
    if (i < vstart)continue;
    if (i % (vend / 500))continue;
    //    line = trim(line);


    out += "{ x: new Date(";
    out += line.substring(0, 4);
    out += ", ";
    out += line.substring(5, 7);
    out += " -1, ";
    out += line.substring(8, 10);
    out += ", ";
    out += line.substring(11, 13);
    out += ", ";
    out += line.substring(14, 16);
    out += ", ";
    out += line.substring(17, 19);
    out += "), y: ";
    out += line.substring(20, 25);
    out += " },\n";



    //    sprintf(temp, "  { x: %d,   y: %d },\n", i, rand()%10 );
    //    out += temp;
  }

  fd.close();
  SPIFFS.end();


  out += "];\n\n\n";

  out += "var chart = new CanvasJS.Chart(\"chartContainer\", {\n";
  out += "    data: [{\n";
  out += "        type: 'spline',\n";
  out += "        dataPoints:dataPlot\n";
  out += "    }]\n";
  out += "});\n";
  out += "chart.render();\n";
  out += "//";


  server.send ( 200, "application/x-javascript", out);
}


void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}
