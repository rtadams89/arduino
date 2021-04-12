/*
 *
 * Alarm sketch
 * Ryan Adams
 * 5/19/2012
 *  
 */
#define sketchVersion "1.0.0"

//#define DEBUG

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
//#include <MemoryStats.h>
#include <DebugUtils.h>

#define zone1Name "Front Door"
#define zone2Name "Patio Door"
#define zone3Name "Bedroom Window"
#define zone4Name "Office Window"
#define zone5Name "Bedroom PIR" //These must be motion zones
#define zone6Name "Motion 2" //These must be motion zones

#define sirenPin 7
#define LEDPin 6
#define buttonPin 5

#define sensorThreshold 200

#define localUdpPort 8888
#define syslogPort 514

#define sysRAM 2048

#define webpass "V8atR5aP"

// Configure other variables
byte alarmTripCount = 0;
boolean breached = 0;
unsigned long sirenOnTimestamp = 0;
boolean sirenState = 0;
byte armedState = 1;  //0=off, 1=away, 2=home
char line1[100];
EthernetServer server(80);
String randomNum = String(random(1000,10000));
unsigned long sirenDuration = 180000;
unsigned long lastHeartbeatSignal = 0;

byte zoneStatusArray[6] = {
  0, 0, 0, 0, 0, 0};

byte previousZoneStatusArray[6] = {
  0, 0, 0, 0, 0, 0};

int zoneBreachCountArray[6] = {
  0, 0, 0, 0, 0, 0};

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 10, 102);
IPAddress syslogServer(192, 168, 10, 101); 

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup()
{
  Serial.begin(9600);
  DEBUG_PRINT("Debugging output on");

  //Disables SD card to avoid SPI problems
  pinMode(4,OUTPUT);
  digitalWrite(4, HIGH); 

  Ethernet.begin(mac,ip);
  delay(1000); //Wait 1 sec for Ethernet
  Udp.begin(localUdpPort);

  // Set pullup on zone input pins
  pinMode(A0, INPUT);
  digitalWrite(A0, HIGH);
  pinMode(A1, INPUT);
  digitalWrite(A1, HIGH);
  pinMode(A2, INPUT);
  digitalWrite(A2, HIGH);
  pinMode(A3, INPUT);
  digitalWrite(A3, HIGH);
  pinMode(A4, INPUT);
  digitalWrite(A4, HIGH);
  pinMode(A5, INPUT);
  digitalWrite(A5, HIGH);

  pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, HIGH);

  pinMode(sirenPin, OUTPUT);
  digitalWrite(sirenPin, HIGH);

  pinMode(LEDPin, OUTPUT);
  digitalWrite(LEDPin, HIGH);

  // Start HTTP server
  server.begin();

  delay(3000); //wait for PIR sensors to calm

  sendSyslogMessage(1, "Boot complete");
}

void loop()
{
//  minFreeMemoryPercent(sysRAM);
  if ((millis() - lastHeartbeatSignal) >= 600000)
  {
    sendSyslogMessage(6, "Heartbeat Signal");
    lastHeartbeatSignal = millis();
  }

  //Look for button push
  if ((digitalRead(buttonPin) == HIGH) && (!armedState))
  {
    toggleArmSystem(1);
  }

  // Determine status of each normally LOW zone. 1 = closed, 0 = open
  for (int i=0; i <= 3; i++)
  {
    previousZoneStatusArray[i] = zoneStatusArray[i];
    if (analogRead(i) < sensorThreshold)
    {
      zoneStatusArray[i] = 1;
    } 
    else 
    {
      zoneStatusArray[i] = 0;
    }  
  }

  // Determine status of each normally HIGH zone. Typically PIR sensors 1 = closed, 0 = open
  for (int i=4; i <= 5; i++)
  {
    previousZoneStatusArray[i] = zoneStatusArray[i];
    if (analogRead(i) > sensorThreshold)
    {
      zoneStatusArray[i] = 1;
    } 
    else 
    {
      zoneStatusArray[i] = 0;
      delay(500); //This reduces the number of breaches detected by the PIR sensor
    }  
  }

  breached = 0;
  for (int i=0; i <= 5; i++)
  {
    if (zoneStatusArray[i] < previousZoneStatusArray[i])
    {
      if ((armedState == 1) || ((armedState == 2) && (i <= 3)))
      {
        breached = 1;
      }
      zoneBreachCountArray[i]++;
    }
  }

  if ((breached) && (armedState) && (!sirenState))
  {
    toggleSiren(1);
  } 
  else if ((armedState) && (sirenState) && (breached))
  {
    sirenOnTimestamp = millis();
  } 
  else if ((sirenState) && (((millis() - sirenOnTimestamp) >= sirenDuration) || (!armedState)))
  {
    toggleSiren(0);
  } 
  else 
  {
    // Do nothing for now
  }

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) 
  {
    while (client.connected()) 
    {
      readHeader(client);

      if ((getUrlParam("action") == "DISARM") && (getUrlParam("password") == webpass) && (getUrlParam("random") == randomNum) && (armedState))
      {
        toggleArmSystem(0);
      } 
      else if ((getUrlParam("action") == "AWAY") && (getUrlParam("password") == webpass) && (getUrlParam("random") == randomNum) && (armedState != 1))
      {
        toggleArmSystem(1);
      } 
      else if ((getUrlParam("action") == "HOME") && (getUrlParam("password") == webpass) && (getUrlParam("random") == randomNum) && (armedState != 2))
      {
        toggleArmSystem(2);
      } 

      printProgStr(client, F("HTTP/1.1 200 OK\nContent-Type: text/html\n"));
      client.println();

      // send the body
      printProgStr(client, F("<html>\n<head>\n<title>Alarm Interface</title><meta name='viewport' content='width=device-width, initial-scale=1.0' />\n</head>\n<body>\n<H1>Alarm Interface</H1>\n<br />\n"));

      if (getUrlParam("password") == webpass)
      {      
        printProgStr(client, F("<table border='1' id='zones'>\n<tr>\n"));
        makeTableCell(client, zone1Name, zoneStatusArray[0], "red", "lime");
        makeTableCell(client, zone2Name, zoneStatusArray[1], "red", "lime");
        makeTableCell(client, zone3Name, zoneStatusArray[2], "red", "lime");
        makeTableCell(client, zone4Name, zoneStatusArray[3], "red", "lime");
        makeTableCell(client, zone5Name, zoneStatusArray[4], "red", "lime");
        //        makeTableCell(client, zone6Name, zoneStatusArray[5], "red", "lime");
        printProgStr(client, F("</tr><tr>"));
        makeTableCell(client, returnZoneBreachCount(0), zoneBreachCountArray[0], "white", "white");
        makeTableCell(client, returnZoneBreachCount(1), zoneBreachCountArray[1], "white", "white");
        makeTableCell(client, returnZoneBreachCount(2), zoneBreachCountArray[2], "white", "white");
        makeTableCell(client, returnZoneBreachCount(3), zoneBreachCountArray[3], "white", "white");
        makeTableCell(client, returnZoneBreachCount(4), zoneBreachCountArray[3], "white", "white");
        //        makeTableCell(client, returnZoneBreachCount(5), zoneBreachCountArray[3], "white", "white");
        printProgStr(client, F("</tr></table>\n<br />\n<table border='1' id='system'>\n<tr>\n"));
        makeTableCell(client, "Armed State", armedState, "white", "lime");
        printProgStr(client, F("</tr>\n<tr>\n"));
        makeTableCell(client, "Siren State", sirenState, "white", "red");
        printProgStr(client, F("</tr>\n<tr>\n<td align='center'>Alarms: "));
        client.print(alarmTripCount);
        printProgStr(client, F("</td>\n</tr>\n</table>\n<br />\n"));
        printProgStr(client, F("<form method='GET'>\n<input name='random' type='hidden' value='")); 
        client.print(randomNum);
        printProgStr(client, F("'>\n<input name='password' type='hidden' value='"));
        client.print(getUrlParam("password"));
        printProgStr(client, F("'>\n<input type='submit' name='action' value='AWAY'/>\n<input type='submit' name='action' value='HOME'/>\n<input type='submit' name='action' value='DISARM'/>\n<input type='submit' name='action' value='REFRESH'/>\n</form>\n<br />Version: "));
        client.write(sketchVersion);
    //    printProgStr(client, F("\n<br />Min RAM free: "));
    //    client.print(minFreeMemoryPercent(sysRAM));
    //    printProgStr(client, F("% \n"));
        printProgStr(client, F("\n<br />Uptime: "));
        client.print(millis()/1000);
        printProgStr(client, F("s \n"));
      } 
      else 
      {
        printProgStr(client, F("<form method='GET'>\nEnter Password :<br />\n<input name='password' type='password' size='20' value='"));
        client.print(getUrlParam("password"));
        printProgStr(client, F("'>\n<br />\n<input type='submit' name='submit' value='Login'/>\n</form>\n"));
      }

      printProgStr(client, F("</body>\n</html>"));

      client.flush();
      client.stop();   
    }
  }

}

void readHeader(EthernetClient client)
{
  memset(line1,0,sizeof(line1));
  // read first line of header
  char ch;
  int i = 0;
  while (ch != '\n')
  {
    if (client.available())
    {
      ch = client.read();
      line1[i] = ch;
      i ++;
    }
  }
  line1[i] = '\0'; 
//  minFreeMemoryPercent(sysRAM);
}

void sendSyslogMessage(int severity, String message)
{
  /*
   0 Emergency: system is unusable 
   1 Alert: action must be taken immediately 
   2 Critical: critical conditions 
   3 Error: error conditions 
   4 Warning: warning conditions 
   5 Notice: normal but significant condition 
   6 Informational: informational messages 
   
   Only severity levels 0, 1, and 2 will trigger an email alert
   */

  byte pri = (32 + severity);
  String priString = String(pri, DEC);
  String buffer = "<" + priString + ">" + "AlarmSystem " + message;
  int bufferLength = buffer.length();
  char char1[bufferLength+1];
  for(int i=0; i<bufferLength; i++)
  {
    char1[i]=buffer.charAt(i);
  }

  char1[bufferLength] = '\0';
  Udp.beginPacket(syslogServer, syslogPort);
  Udp.write(char1);
  Udp.endPacket(); //this is slow 
//  minFreeMemoryPercent(sysRAM);
}

String getUrlParam(String param)
{
  String url = line1;
  param += "=";

  int startOfVar = url.indexOf(param) + param.length();
  int endOfVar = (url.indexOf("&", startOfVar));
  if (endOfVar == -1)
  {
    endOfVar = (url.indexOf(" ", startOfVar));
  }

  if (url.indexOf(param) == -1){
    return "";
  }

  if (endOfVar == -1){
    endOfVar = url.length();
  }
//  minFreeMemoryPercent(sysRAM);
  return String(url.substring(startOfVar, endOfVar));  
}

void makeTableCell(EthernetClient client, char text[], byte value, char zeroVal[], char nonZeroVal[])
{
  printProgStr(client, F("<td align='center' bgcolor='"));
  if (value == 0)
  {
    client.write(zeroVal);
  } 
  else
  {
    client.write(nonZeroVal);
  }
  printProgStr(client, F("'>"));
  client.write(text);
  printProgStr(client, F("</td>"));  
//  minFreeMemoryPercent(sysRAM);
}

void toggleArmSystem(byte toggle)
{
  if (toggle)
  {
    armedState = toggle;
    digitalWrite(LEDPin, HIGH);
    sendSyslogMessage(2, "System armed");
    memset(zoneBreachCountArray,0,sizeof(zoneBreachCountArray));
  } 
  else
  {
    armedState = 0;
    digitalWrite(LEDPin, LOW);
    sendSyslogMessage(2, "System disarmed");
    alarmTripCount=0;
    memset(zoneBreachCountArray,0,sizeof(zoneBreachCountArray));
  }
  randomNum = String(random(1000,10000));
//  minFreeMemoryPercent(sysRAM);
}

void toggleSiren(boolean toggle)
{
  if (toggle)
  {
    digitalWrite(sirenPin, LOW); 
    sirenOnTimestamp = millis();
    sirenState = 1;
    sendSyslogMessage(0, "Siren activated");
    alarmTripCount++;
  } 
  else
  {
    digitalWrite(sirenPin, HIGH); 
    sirenOnTimestamp = 0;
    sirenState = 0;
    sendSyslogMessage(1, "Siren deactivated");
  }
//  minFreeMemoryPercent(sysRAM);
}

void printProgStr (EthernetClient client, __FlashStringHelper const * const str)
{
  char * p = (char *) str;
  if (!p) return;
  char buf [strlen_P (p) + 1];
  byte i = 0;
  char c;
  while ((c = pgm_read_byte(p++)))
    buf [i++] = c;
  buf [i] = 0;

  client.write(buf);
//  minFreeMemoryPercent(sysRAM);
}

char * returnZoneBreachCount(byte zoneIndex)
{
  if (zoneBreachCountArray[zoneIndex] > 999)
  {
    zoneBreachCountArray[zoneIndex] = 999;
  }
  char buf[11];
  itoa(zoneBreachCountArray[zoneIndex], buf, 10);
//  minFreeMemoryPercent(sysRAM);
  return buf;
}

