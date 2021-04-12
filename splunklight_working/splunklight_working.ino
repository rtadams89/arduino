#define sketchName "SplunkLight"
#define sketchVersion "1.0.0"
#define sketchAuthor "Ryan Adams"

/***********
 * Notes on use:
 * 
 * http://ip/?password=TRAFFIC&color=RED&duration=10
 * http://ip/?password=TRAFFIC&command=RESET
 * http://ip/?password=TRAFFIC&command=DISCO
 * 
 ***********/

//#define DEBUG //Uncomment to enable debugging output
#include <DebugUtils.h>
#include <SPI.h>
#include <Ethernet.h>
#include <avr/wdt.h>

//Define output pin mappings
#define greenPin A5
#define yellowPin A4
#define redPin A3
#define strobePin A2

//Define input pin mappings
#define buttonPin A1

#define webpass "TRAFFIC" //Password required to activate outputs. Must be < 20 chars long. Do not use "?", "&", or " "

unsigned long newDuration = 1; //Variable for submitted duration value

//Variables for current duration values
unsigned long strobeDuration = 1; 
unsigned long redDuration = 1;
unsigned long yellowDuration = 1;

//Variables for last time outputs were activated
unsigned long lastStrobe = 0;
unsigned long lastRed = 0;
unsigned long lastYellow = 0;

//Variables for current state of outputs
boolean greenState = 0;
boolean yellowState = 0;
boolean redState = 0;
boolean strobeState = 0;

char line1[80]; //Holds HTTP header received

byte mac[] = {
  0x00, 0x90, 0x40, 0x37, 0x69, 0xCD}; //MAC address
EthernetServer server(80); //HTTP server port
void setup()
{
  Serial.begin(9600);
  Serial.println(sketchName " " sketchVersion);
  Serial.println("Created by " sketchAuthor);
  Serial.println("/?password=" webpass "&color=RED&duration=10");
  Serial.println("/?password=" webpass "&command=RESET");
  Serial.println("/?password=" webpass "&command=DISCO");

  // Seed RNG from analog port.
  randomSeed(analogRead(0));


  //Disables SD card to avoid SPI problems
  pinMode(4,OUTPUT);
  digitalWrite(4, HIGH);

  if (Ethernet.begin(mac) == 0) {
    DEBUG_PRINT("DHCP - General Error");
  }
  else
  {
    // print your local IP address:
    DEBUG_PRINT(Ethernet.localIP());
  }

  //Configure output pins
  pinMode(strobePin, OUTPUT);
  digitalWrite(strobePin, HIGH);
  pinMode(greenPin, OUTPUT);
  digitalWrite(greenPin, HIGH);
  pinMode(yellowPin, OUTPUT);
  digitalWrite(yellowPin, HIGH);
  pinMode(redPin, OUTPUT);
  digitalWrite(redPin, HIGH);

  //Set pullup on input pin
  pinMode(buttonPin, INPUT);
  digitalWrite(buttonPin, HIGH);

  // Start HTTP server
  server.begin();

  wdt_enable (WDTO_2S);
}

void loop()
{
  wdt_reset ();
  //Look for button push
  if (digitalRead(buttonPin) == LOW) //Button normally open, closes to GND
  {
    DEBUG_PRINT("Button pushed");
    lastStrobe = millis();
    strobeDuration = 1000;
  }

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) 
  {
    DEBUG_PRINT("Client connected");
    while (client.connected()) 
    {
      readHeader(client); //Stores header received in "line1" variable

      if ((getUrlParam("password") == webpass)) //Verify password
      {
        DEBUG_PRINT("Password accepted");
        printProgStr(client, F("HTTP/1.1 200 OK\nContent-Type: text/html\n")); //Send back HTTP 200 code
        client.println();

        if ((getUrlParam("command") == "RESET"))
        {
          printProgStr(client, F("\nResetting..."));
          disconnectClient(client);
          //powerOnTest();
          while (1)
          {
          }
        }
        else if ((getUrlParam("command") == "DISCO"))
        {
          printProgStr(client, F("\nDisco mode on."));
          disconnectClient(client);
          discoMode();
        }
        else
        {
          if ((stringObjectToInt(getUrlParam("duration")) >= 1) && (stringObjectToInt(getUrlParam("duration")) <= 7200)) //Verifies duration is between 1 and 7200 seconds
          {          
            newDuration = stringObjectToInt(getUrlParam("duration"));
            newDuration = newDuration * 1000; //Converts duration passed to milliseconds

            if (getUrlParam("color") == "STROBE")
            {
              if (!(lastStrobe) || (strobeDuration - (millis() - lastStrobe) < newDuration)) //Only continue if new duration will keep output on for longer
              {
                strobeDuration = newDuration;
                lastStrobe = millis();
                printProgStr(client, F("\nStrobe updated."));
              }
              else
              {
                printProgStr(client, F("\nStrobe update ignored."));
              }
            }
            else if (getUrlParam("color") == "RED")
            {
              if (!(lastRed) || (redDuration - (millis() - lastRed) < newDuration)) //Only continue if new duration will keep output on for longer
              {
                redDuration = newDuration;
                lastRed = millis();
                printProgStr(client, F("\nRed updated."));
              }
              else
              {
                printProgStr(client, F("\nRed update ignored."));
              }
            } 
            else if (getUrlParam("color") == "YELLOW")
            {
              if (!(lastYellow) || (yellowDuration - (millis() - lastYellow) < newDuration)) //Only continue if new duration will keep output on for longer
              {
                yellowDuration = newDuration;
                lastYellow = millis();
                printProgStr(client, F("\nYellow updated."));
              }
              else
              {
                printProgStr(client, F("\nYellow update ignored."));
              }
            }
            else
            {
              printProgStr(client, F("\nInvalid color value supplied."));
            }
          } 
          else
          {
            printProgStr(client, F("\nInvalid duration value supplied."));
          }
          disconnectClient(client);
        }
      }
      else
      {
        //printProgStr(client, F("\nInvalid password value supplied."));
        DEBUG_PRINT("Password invalid");
        printProgStr(client, F("HTTP/1.1 403 Forbidden\nContent-Type: text/html\n")); //Send back HTTP 403 code
        client.println();
        disconnectClient(client);
      }   
    }
  }


  //This section checks the specified duration and the actual on time of outputs
  if ((millis() - lastYellow > yellowDuration) && (lastYellow))
  {
    lastYellow = 0;
    DEBUG_PRINT("lastYellow reset");
  }
  if ((millis() - lastRed > redDuration) && (lastRed))
  {
    lastRed = 0;
    DEBUG_PRINT("lastRed reset");
  }
  if ((millis() - lastStrobe > strobeDuration) && (lastStrobe))
  {
    lastStrobe = 0;
    DEBUG_PRINT("lastStrobe reset");
  }

  if (lastStrobe) //If "lastStrobe" is not zero, output should be on
  {
    if (!strobeState) //If the output isn't already on, turn it on
    {
      allOff();       
      digitalWrite(strobePin, LOW); //Output on
      strobeState = 1;
      DEBUG_PRINT("Strobe on");
    }
  }
  else if (lastRed)
  {
    if (!redState)
    {
      allOff();       
      digitalWrite(redPin, LOW); //Output on
      redState = 1;
      DEBUG_PRINT("Red on");
    }
  }
  else if (lastYellow)
  {
    if (!yellowState)
    {
      allOff();       
      digitalWrite(yellowPin, LOW); //Output on
      yellowState = 1;
      DEBUG_PRINT("Yellow on");
    }
  }
  else //If no other outputs are on, the green output should be on
  {
    if (!greenState)
    {
      allOff();       
      digitalWrite(greenPin, LOW); //Output on
      greenState = 1;
      DEBUG_PRINT("Green on");
    }
  }

  //Renew DHCP
  switch(Ethernet.maintain())
  {
  case 0:
    break;
  case 1:
    DEBUG_PRINT("DHCP - Renew failed");
    delay(5000);
    break;
  case 2:
    DEBUG_PRINT("DHCP - Renew success");
    break;
  default:
    DEBUG_PRINT("DHCP - General Error");
    delay(5000);
    break;
  }
}

//Reads header sent, char by char into "line1" variable until end of line is reached
void readHeader(EthernetClient client)
{
  DEBUG_PRINT("Start readHeader");
  memset(line1,0,sizeof(line1));
  char ch = '0';
  int i = 0;
  while ((ch != '\n') && (i < 78)) //"i" must be less than the size of "line1"
  {
    if (client.available())
    {
      ch = client.read();
      line1[i] = ch;
      i ++;
    }
  }
  line1[i] = '\0'; 
  DEBUG_PRINT("End readHeader");
}

//Searches "line1" for parameter passed and returns value between "=" and "&" or end of line
String getUrlParam(String param)
{
  DEBUG_PRINT("Start getUrlParam");
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
  DEBUG_PRINT("End getUrlParam");
  return String(url.substring(startOfVar, endOfVar));  
}

//Turns all outputs off
void allOff()
{
  DEBUG_PRINT("Start alloff");
  digitalWrite(greenPin, HIGH); //Green off
  digitalWrite(yellowPin, HIGH); //Yellow off
  digitalWrite(redPin, HIGH); //Red off
  digitalWrite(strobePin, HIGH); //Strobe off
  greenState = 0;
  yellowState = 0;
  redState = 0 ;
  strobeState = 0;
  DEBUG_PRINT("End allOff");
}

//This beauty writes Strings stored in flash in such a way as to not send them byte by byte
void printProgStr (EthernetClient client, __FlashStringHelper const * const str)
{
  DEBUG_PRINT("Start printProgStr");
  char * p = (char *) str;
  if (!p) return;
  char buf [strlen_P (p) + 1];
  byte i = 0;
  char c;
  while ((c = pgm_read_byte(p++)))
    buf [i++] = c;
  buf [i] = 0;

  client.write(buf);
  DEBUG_PRINT("End printProgStr");
}

//Converts String objects to ints
int stringObjectToInt(String input)
{
  DEBUG_PRINT("Start stringObjectToInt");
  char this_char[input.length() + 1];
  input.toCharArray(this_char, sizeof(this_char));
  int my_integer_data = atoi(this_char);
  DEBUG_PRINT("End stringObjectToInt");
  return my_integer_data;
}

//Disco Mode!
void discoMode()
{
  int randomDelay = 0;
  byte randomAction;
  unsigned long discoStart = millis(); 

  DEBUG_PRINT("DISCO!");
  allOff();
  delay(1000); 
  digitalWrite(strobePin, LOW);

  while(millis()-discoStart < 60000)
  {  
    wdt_reset ();
    // Generate random delay time
    randomDelay=random(100,500);
    // Generate random action
    if(random(2)==1)
    {
      randomAction=LOW;
    }
    else
    {
      randomAction=HIGH;
    }

    switch (random(1,4))

    {
    case 1:
      digitalWrite(greenPin, randomAction);
      break;
    case 2:
      digitalWrite(yellowPin, randomAction);
      break;
    case 3:
      digitalWrite(redPin, randomAction);
      break;
    }

    delay(randomDelay);

  }
  allOff();
  DEBUG_PRINT("End discoMode"); 
}

void disconnectClient(EthernetClient client)
{
  //Finish the session
  client.flush();
  client.stop();   
  DEBUG_PRINT("Client disconnected");
}

