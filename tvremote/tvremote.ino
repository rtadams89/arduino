/*
 TVRemote
 Version: 0.1.1
 */

#include <IRremote.h>

IRsend irsend;
int RECV_PIN = 11;
IRrecv irrecv(RECV_PIN);
decode_results results;

void setup()
{
  Serial.begin(9600);
  irrecv.enableIRIn(); // Start the receiver
}

long unsigned int irCode;

void loop() {
  if (irrecv.decode(&results)) {
    
      irCode = results.value;
      Serial.print("Code received: ");
      Serial.println(irCode);


    if (irCode == 16191727)
    {
      for (int i = 0; i < 4; i++) {
        irsend.sendNEC(0x20DF40BF, 32); // Vol up
      }
      Serial.println("Vol up code sent");
    }
    else if (irCode == 16218757)
    {
      for (int i = 0; i < 4; i++) {
        irsend.sendNEC(0x20DFC03F, 32); // Vol down
      }
      Serial.println("Vol down code sent");
    }

    else if (irCode == 16235077)
    {
      for (int i = 0; i < 2; i++) {
        irsend.sendNEC(0x20DF906F, 32); // Mute
      } 
      Serial.println("Mute code sent");
    }

    else if (irCode == 16222327)
    {
      for (int i = 0; i < 2; i++) {
        irsend.sendNEC(0x20DF10EF, 32); // Power
      }
      Serial.println("Power code sent");
    }
    
    irrecv.enableIRIn();
    irrecv.resume(); // Receive the next value
  }
}




