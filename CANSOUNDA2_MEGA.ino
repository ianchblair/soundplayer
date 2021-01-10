#include <DFRobotDFPlayerMini.h>

/*
Usim FLIM mode teach on/off events.
It implements all automatic configuration, including learning events.
This node uses 500 bytes of EPROM to store events and the other information.
See MemoryManagement.h for memory configuration
To clear the memory, press pushbutton1 while reseting the arduino

Interface for a DFPlayer Mini via the standard serial port
Hardware requirements are an MCP2515 based CBUS interface

The CBUS library by amaurail can be downloaded from github at
https://github.com/amaurial/mergCanBus
The DFPlayer library is here
https://github.com/DFRobot/DFPlayer-Mini-mp3

Pins used for interface chip

           Nano/Uno  Mega
INT         D2       D49
SCK   SCK   D13      D52
SI    MISO  D11      D50
SO    MOSI  D12      D51
CS          D10      D53
GND         0V       0V
VCC         5V       5V


*/

#define MEGA    1 //set to 1 for MEGA 0 for UNO/NAN0
#define DEBUG   0 // set to 0 for no debug messages, 1 for messages to console

// Load up required libraries
#include <Arduino.h>
#include <SPI.h> //required by the CBUS library
#include <MergCBUS.h>
#include <Message.h>
#include <EEPROM.h> //required by the CBUS library
//#include <DFPlayer_Mini_Mp3.h>  //for the sound card
#include "DFRobotDFPlayerMini.h"  //for the sound card

//Module definitions

#define debounceDelay 50  //for any switches if used

#define TRUE    1
#define FALSE   0
#define ON      1
#define OFF     0

/**
 * Definitions of the flags bits
 */

//For Sound
// Input pins - analog
//int pot = A6; int busy = A7;  //NOT used in this sketch

//variables
int n;         // track being played
int volume;    // current volume level
boolean muteflag = false;  // mutes sound if true

//CBUS definitions
#if MEGA
//  #define GREEN_LED 27                  //merg green led port
//  #define YELLOW_LED 26                 //merg yellow led port
//  #define PUSH_BUTTON 25                //std merg push button
//  #define PUSH_BUTTON1 28               //debug push button
  #define GREEN_LED 5                  //merg green led port
  #define YELLOW_LED 4                 //merg yellow led port
  #define PUSH_BUTTON 6                //std merg push button
  #define PUSH_BUTTON1 7               //debug push button
#else  //Nano or Uno
  #define GREEN_LED 5                  //merg green led port
  #define YELLOW_LED 4                 //merg yellow led port
  #define PUSH_BUTTON 6                //std merg push button
  #define PUSH_BUTTON1 7               //debug push button
#endif

#define NODE_VARS 4      //sets up number of NVs for module to store variables
//  Only one currently in use
//  NV 1 default volume level

#define NODE_EVENTS 20     //max number of events in case of teaching short events
#define EVENTS_VARS 3   //number of variables per event  1 ON sound 2 OFF Sound 3 Volume
#define DEVICE_NUMBERS 1  //number of device numbers. 

//arduino mega has 4K, so it is ok.


//create the merg object
MergCBUS cbus=MergCBUS(NODE_VARS,NODE_EVENTS,EVENTS_VARS,DEVICE_NUMBERS);

//Create the DFPlayer object
DFRobotDFPlayerMini DFPlayer1;

void setup(){

  pinMode(PUSH_BUTTON1,INPUT_PULLUP);//debug push button
  //Serial.begin(115200);  Serial port is used for MP3 card at 9600 so debug cannot be used
 

  //pinMode(busy,INPUT);  pinMode(pot,INPUT);  Again not used in this sketch

  //Configuration CBUS data for the node
  cbus.getNodeId()->setNodeName("CANSND2",7);  //node name shows in FCU when first detected set your own name for each module
  cbus.getNodeId()->setModuleId(172);            //module number
  cbus.getNodeId()->setManufacturerId(0xA5);    //merg code
  cbus.getNodeId()->setMinCodeVersion(1);       //Version 1
  cbus.getNodeId()->setMaxCodeVersion(0);
  cbus.getNodeId()->setProducerNode(true);
  cbus.getNodeId()->setConsumerNode(true);
  cbus.setStdNN(999); //standard node number

  if (digitalRead(PUSH_BUTTON1)==LOW){
    //Serial.println("Setup new memory");
    cbus.setUpNewMemory();
    cbus.setSlimMode();
    cbus.saveNodeFlags();
  }
  cbus.setLeds(GREEN_LED,YELLOW_LED);//set the led ports
  cbus.setPushButton(PUSH_BUTTON);//set the push button ports
  cbus.setUserHandlerFunction(&myUserFunc);//function that implements the node logic
  #if MEGA
      cbus.initCanBus(53,CAN_125KBPS,MCP_8MHz,10,200);  //initiate the transport layer. pin=53, rate=125Kbps,10 tries,200 millis between each try
  #else
      cbus.initCanBus(10,CAN_125KBPS,MCP_8MHz,10,200);  //initiate the transport layer. pin=10, rate=125Kbps,10 tries,200 millis between each try
  #endif
  //Note the clock speed 8Mhz for the ebay module may be 16Mhz for others please check your speed

  //Setup Sound Card
  //get base volume from NVs
   volume=cbus.getNodeVar(1);
   if (volume == 0) volume =20;  //NV not set so give it a default of 20 in a scale of 0-30
   
   Serial.begin (9600);  //Standard port used for the sound card as in the original
   DFPlayer1.begin(Serial);
   //mp3_set_serial (Serial); 
   DFPlayer1.reset();
   DFPlayer1.volume(volume); 
   // DFPlayer1.play(1);      
}

void play(int n, int Tvolume) {
  if (Tvolume == 0) Tvolume = volume;
  DFPlayer1.volume(volume); 
  DFPlayer1.play(n);
  //while (analogRead(busy)< 200)  {delay(100);  adjustVolume();}  Ignore this to concentrate of reading CBUS message Therefore new sounds will override old ones still playing    
}

void loop (){
  cbus.cbusRead();  //Check CBUS buffers
  cbus.run();//do all logic
  
  if (cbus.getNodeState()==NORMAL){
    //nothing to do in this sketch events are processed later 
    
     }

  //debug memory
  if (digitalRead(PUSH_BUTTON1)==LOW){
    cbus.dumpMemory();  //Used to clear the Arduino Memory
  }
  
  
}

//user defined function. contains the module logic.called every time run() is called.
void myUserFunc(Message *msg,MergCBUS *mcbus){
   byte CBUSOpc = msg->getOpc();
 
   if (mcbus->eventMatch()){  //The recived event has been thaut this module
        //retrive the values entered when teaching the event to the modules these are stored on the module
        int v0=mcbus->getEventVar(msg,1);  //track to play for ON
        int v1=mcbus->getEventVar(msg,2);  //track to play for OFF
        int v2=mcbus->getEventVar(msg,3);  //sound level if zero use default
     if ((mcbus->isAccOn()==true)&&(v0 != 0)){  //No sound file no play
           
          processOnEvent(v0,v2); //Pass the event on the ON event handler   
     }

     if ((mcbus->isAccOff()==true)&&(v1 != 0)){  //No sound file no play
          
          processOffEvent(v1,v2);    //Pass the event on the OFF event handler 
     }
     
   }   


}
