/**
 * @example TCPServer.ino
 * @brief The TCPServer demo of library WeeESP8266. 
 * @author Wu Pengfei<pengfei.wu@itead.cc> 
 * @date 2015.02
 * 
 * @par Copyright:
 * Copyright (c) 2015 ITEAD Intelligent Systems Co., Ltd. \n\n
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version. \n\n
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
 

#include <ESP8266.h>
#include <SoftwareSerial.h>
#include <HX711.h>
#include <ArduinoJson.h>
#include <EEPROMex.h>

// set to true for memory monitoring
#define MEMORY false

SoftwareSerial ESPserial(7, 8); // RX | TX
ESP8266 wifi(ESPserial, 38400);

// setting for distance sensor
const int trigPin = 2;
const int echoPin = 3;

//EEPROM data storage
char WiFiName[32] = "ddfk";
char WiFiPass[32] = "lepsinezdefault";
int MobilePort = 8090;
char ServerAddress[32] = "";
char ServerAuth[32] = "";
long machineID = 1; 
int UpperLimit = 10;
int UpperTrshld = 10;
int BottomTrshld = 10;
int BottomLimit = 10;

//EEPROM addresses
int adrWiFiName = 0; 
int adrWiFiPass = 0;
int adrMobilePort = 0;
int adrServerAddress = 0;
int adrServerAuth = 0;
int adrmachineID = 0;

int TCPcounter = 0;
uint8_t mux_id = 0;
bool startPosWeight = false;

// setting for weight measurement
HX711 scale(A1, A0);  //HX711.PD_SCK pin #A0, HX711.DOUT pin #A1

void setup(void)
{
    Serial.begin(38400);
    Serial.print("setup begin\r\n");
    
    //read the settings from EEPROM
    getAdr_EEPROM();
    read_EEPROM();
    
    // start the wifi connection and TCP server
    setup_wifi();    
    start_TCP();
    
    
    Serial.print("Weight measurement setup\r\n");
    scale.set_scale(-20152.8f);                      // this value is obtained by calibrating the scale with known weights; see the README for details
    scale.tare();				        // reset the scale to 0
    
    Serial.print("setup end\r\n");
}
 
void loop(void)
{
    if(MEMORY)
    {
      Serial.print("Free memory beginning: ");
      Serial.println(freeRam());
    }
    
    String command = receiveMsg();
    
    

    if (command == "")
    {
      //no message
      //Serial.print("No command received");
    }else{ 
      if (command.indexOf("StartPosWeight") >= 0)
      {
        Serial.println("StartPosWeight command received"); 
        startPosWeight = true;               
      }
      if (command.indexOf("StopPosWeight") >= 0)
      {
        Serial.println("StopPosWeight command received");
        startPosWeight = false;    
        sendStopConfirmation();    
      }
      if (command.indexOf("GetPositionCalibration") >= 0)
      {
        Serial.println("GetPositionCalibration command received");
        startPosWeight = false;    
        sendPositionCalibration();    
      }
    }  
            
    if (startPosWeight)
    {
      sendPosWeight();
    }    
      
    
    
    
    if(MEMORY)
    {
      Serial.print("Free memory ending: ");
      Serial.println(freeRam());
    }
}

String receiveMsg()
{   
    //receive data
    String s;
    uint8_t buffer[128] = {0};
    uint32_t len = wifi.recv(&mux_id, buffer, sizeof(buffer), 100);
    if (len > 0) {  
      
        Serial.print("Received from :");
        Serial.print(mux_id);
        Serial.print("[");
        for(uint32_t i = 0; i < (len-0); i++) {
            Serial.print((char)buffer[i]);
            s += (char)buffer[i];
        }
        Serial.print("]\r\n");         
    }
    return s;
}

void  sendPositionCalibration()
{
   //prepare data string
    char buf[75] = "";
  // create JSON message
    StaticJsonBuffer<JSON_OBJECT_SIZE(4)> jsonBuffer;  //+ JSON_ARRAY_SIZE(2)
    JsonObject& message = jsonBuffer.createObject();
      message["UpperLimit"] = UpperLimit;
      message["UpperTrshld"] = UpperTrshld;
      message["BottomTrshld"] = BottomTrshld;
      message["BottomLimit"] = BottomLimit;
    message.printTo(buf, sizeof(buf));    
          
    // try to send the data
    int sendCount = 0;
    bool sent = false;
    do {
      if(wifi.send(mux_id, (const uint8_t*)buf, strlen(buf))) {
         Serial.print(buf);
         Serial.println("Position calibration data send");
         sent = true;    
      }else{           
        Serial.print("Calibration data send error\r\n");
        Serial.println(sendCount);
        sendCount++;
      }
    }while(!sent || sendCount < 10 );
}

void sendStopConfirmation()
{
    //prepare data string
    char buf[30] = "MeasurementStopped";
    
     // try to send the data
    if(wifi.send(mux_id, (const uint8_t*)buf, strlen(buf))) {
       Serial.print(buf);    
    }else{           
      Serial.print("Send error\r\n");
    }
}

void sendPosWeight()
{        
    //measure distance
    int cm = measure_distance();
    
    //measure weight
    float weight = scale.get_units();
    
    //prepare data string
    char buf[100] = "";
    
    // create JSON message
    StaticJsonBuffer<JSON_OBJECT_SIZE(3)> jsonBuffer;  //+ JSON_ARRAY_SIZE(2)
    JsonObject& message = jsonBuffer.createObject();
      message["machineID"] = machineID;
      message["position"] = cm;
      message["weight"] = weight;
    message.printTo(buf, sizeof(buf));
    
    // add enter (to delete later)    
    char ch[5] = " \r\n";
    for (int i=0; i < 5; i++)
    {
      append(buf, 100, ch[i]);
    }  
        
    // try to send the data
    if(wifi.send(mux_id, (const uint8_t*)buf, strlen(buf))) {
       Serial.print(buf);    
    }else{           
      Serial.print("Send error\r\n");
    }
}

void stopTCP()
{   
  if (wifi.releaseTCP(mux_id)) {
            Serial.print("release tcp ");
            Serial.print(mux_id);
            Serial.println(" ok");
        } else {
            Serial.print("release tcp");
            Serial.print(mux_id);
            Serial.println(" err");
        }
        
        Serial.print("Status:[");
        Serial.print(wifi.getIPStatus().c_str());
        Serial.println("]");
  
}

void setup_wifi()
{  
    wifi.restart();
    Serial.print("FW Version:");
    Serial.println(wifi.getVersion().c_str());
      
    if (wifi.setOprToStationSoftAP()) {
        Serial.print("to station + softap ok\r\n");
    } else {
        Serial.print("to station + softap err\r\n");
    }
 
    if (wifi.joinAP(WiFiName, WiFiPass)) {
        Serial.print("Join AP success\r\n");
        Serial.print("IP: ");
        Serial.println(wifi.getLocalIP().c_str());    
    } else {
        Serial.print("Join AP failure\r\n");
    }
    
    if (wifi.enableMUX()) {
        Serial.print("multiple ok\r\n");
    } else {
        Serial.print("multiple err\r\n");
    }  
}

void start_TCP()
{
   if (wifi.startTCPServer(MobilePort)) {
        Serial.print("start tcp server ok\r\n");
    } else {
        Serial.print("start tcp server err\r\n");
    }    
    if (wifi.setTCPServerTimeout(10)) { 
        Serial.print("set tcp server timout 10 seconds\r\n");
    } else {
        Serial.print("set tcp server timout err\r\n");
    }
 }
 

void write_EEPROM()
{
   EEPROM.setMaxAllowedWrites(180);
   
   EEPROM.updateBlock(adrWiFiName, WiFiName, 32);
   EEPROM.updateBlock(adrWiFiPass, WiFiPass, 32);
   EEPROM.updateInt(adrMobilePort, MobilePort);
   //EEPROM.updateBlock(adrServerAddress, ServerAddress, 32);
   //EEPROM.updateBlock(adrServerAuth, ServerAuth, 32);
   EEPROM.updateLong(adrmachineID, machineID);
}

void read_EEPROM()
{
  Serial.println("Data read from EEPROM:");
  
  EEPROM.readBlock(adrWiFiName, WiFiName, 32);
  Serial.print("WiFiName: ");
  Serial.println(WiFiName);
  
  EEPROM.readBlock(adrWiFiPass, WiFiPass, 32);
  Serial.print("WiFiPass: ");
  Serial.println(WiFiPass);
  
  MobilePort = EEPROM.readInt(adrMobilePort);
  Serial.print("MobilePort: ");
  Serial.println(MobilePort);
  
  /*
  EEPROM.readBlock(adrServerAddress, ServerAddress, 32);
  Serial.print("ServerAddress: ");
  Serial.println(ServerAddress);
  
  EEPROM.readBlock(adrServerAuth, ServerAuth, 32);
  Serial.print("ServerAuth: ");
  Serial.println(ServerAuth);
  */
  
  machineID = EEPROM.readLong(adrmachineID);
  Serial.print("MachineID: ");
  Serial.println(machineID);
}

void getAdr_EEPROM()
{
   adrWiFiName = EEPROM.getAddress(sizeof(char)*32); 
   adrWiFiPass = EEPROM.getAddress(sizeof(char)*32); 
   adrMobilePort = EEPROM.getAddress(sizeof(int)); 
   adrServerAddress = EEPROM.getAddress(sizeof(char)*32); 
   adrServerAuth = EEPROM.getAddress(sizeof(char)*32); 
   adrmachineID = EEPROM.getAddress(sizeof(long)); 
}

int measure_distance()
{
  long duration, inches;
  int cm;
 
  // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
 
  // Read the signal from the sensor: a HIGH pulse whose
  // duration is the time (in microseconds) from the sending
  // of the ping to the reception of its echo off of an object.
  pinMode(echoPin, INPUT);
  duration = pulseIn(echoPin, HIGH);
 
  // convert the time into a distance 
  cm = microsecondsToCentimeters(duration);  

  return cm;
}
      
long microsecondsToCentimeters(long microseconds)
{
  // The speed of sound is 340 m/s or 29 microseconds per centimeter.
  // The ping travels out and back, so to find the distance of the
  // object we take half of the distance travelled.
  return microseconds / 29 / 2;
}  

//returns 1 if failed, 0 if succeeded 
int  append(char*s, size_t size, char c) {
     if(strlen(s) + 1 >= size) {
          return 1;
     }
     int len = strlen(s);
     s[len] = c;
     s[len+1] = '\0';
     return 0;
}

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
