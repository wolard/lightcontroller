#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "8080";
char blynk_token[34] = "YOUR_BLYNK_TOKEN";
char callback_topic[100];
char mqtt_topic[100];
//flag for saving data
bool shouldSaveConfig = false;
WiFiClient espClient;
PubSubClient client(espClient);
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
void callback(char* topic, byte* payload, int length) {
  char msg[50];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
 if (strcmp(topic,"/query")==0){
    // whatever you want for this topic
   StaticJsonBuffer<200> jsonBuffer;
   JsonObject& json = jsonBuffer.createObject();
   json["type"] = "lightswitch";
  json["topic"] = mqtt_topic;
char MQTTmessageBuffer[100];
json.printTo(MQTTmessageBuffer, sizeof(MQTTmessageBuffer));
 
 }
if (strcmp(topic,mqtt_topic)==0){
 if ((char)payload[0] == '1') {
    digitalWrite(D1, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
     
     
     
     snprintf (msg, 50, "lights on");
      client.publish(callback_topic, msg);
  } else {
    digitalWrite(D1, HIGH);  // Turn the LED off by making the voltage HIGH
     
    snprintf (msg, 50, "lights off");
     client.publish(callback_topic, msg);
  }

}


}
boolean reconnect() {
 String clientId = "ESP8266Client-";
 
    clientId += String(random(0xffff), HEX);
 
 
  if (client.connect(clientId.c_str())) {
    // Once connected, publish an announcement...
     Serial.println("mqttenabled");
   // client.publish(mqtt_topic,"wifimanager");
    // ... and resubscribe
    client.subscribe(mqtt_topic);
    client.subscribe("/query");
  }
  return client.connected();
}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println();

  //clean FS, for testing
 //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
         Serial.println(size);
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
     // char buf[100];
        configFile.readBytes(buf.get(),size);
    
       DynamicJsonBuffer jsonBuffer;
       
  
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
       if (json.success()) {
          Serial.println("\nparsed json");

         strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);
          strcpy(callback_topic, json["callback_topic"]);

       
     
      }
      else {
          Serial.println("failed to load json config");
        }
           configFile.close();
    }
  }
  }
   else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_callback_topic("callback", "callback topic", callback_topic, 100);
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 100);


  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_callback_topic);
    wifiManager.addParameter(&custom_mqtt_topic);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(callback_topic, custom_callback_topic.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
     
     
    
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["callback_topic"] = callback_topic;
    json["mqtt_topic"] = mqtt_topic;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(mqtt_server);
  Serial.println(mqtt_topic);
   Serial.println(callback_topic);
client.setServer(mqtt_server, 1883);
client.setCallback(callback);
}

void loop() {
  // put your main code here, to run repeatedly:
if (!client.connected()) {
    reconnect();
  }
 
    client.loop();
 
}