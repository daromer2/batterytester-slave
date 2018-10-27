/* Timer.h https://github.com/JChristensen/Timer
 *  
 *  Slave battery tester. A testing system that can be daisy chained in up to 100 units and sends all data to master unit. It can do dischar, charge and IR tests. Configureable from the master
 *  
 *  Functions:
 *  
 *  
 *  Cost:
 *  
 *  
 *  More infor:
  */



#include "Timer.h"

bool runningTest = false; //If we have anything running and being tested

int  serIn;             // var that will hold the bytes-in read from the serialBuffer
char serInString[100];  // array that will hold the different bytes  100=100characters;
                        // -> you must state how long the array will be else it won't work.
int  serInIndx  = 0;    // index of serInString[] in which to insert the next incoming byte
int  serOutIndx = 0;    // index of the outgoing serInString[] array;


void setup() {
// inite serial to handle the daisy chain
Serial.begin(9600);


//If screen init


timerObject.every(2, cycleSerial);  // Read data from serial to buffer
timerObject.every(2, cycleSerialBuffer);  //Process data from buffer
timerObject.every(2000, cycleValues);  // Process battery data


timerObject.every(10000, cycleAlive);
}

void loop() {
  // Only timer objects in here. 
  timerObject.update(); // Timer Triggers
}


//Send keep alive message if no testing is done
void cycleAlive() {
  if (!runningTest) {
    Serial.println("Im alive. Dont kill me");
  }
}
// This cycles through all values to read from the battery
void cycleValues() {
  
}
//Reads all data from the serial port into buffer
void cycleSerial() {
      char sb;   
      char endMarker = '\n';
      
    if(Serial.available()) { 
       while (Serial.available() && newData == false ){ 
          sb = Serial.read();             
          serInString[serInIndx] = sb;
          serInIndx++;
       }
        if (sb == endMarker) {
          newData = true;
        }
    }
}
       
//Process buffer when its ready
// Todo: Currently it only forwards sort so it reads "settings" and set those
void cycleSerialBuffer() {
  if (newData) {
      for(serOutIndx=0; serOutIndx < serInIndx; serOutIndx++) {
          Serial.print( serInString[serOutIndx] );    //print out the byte at the specified index
      }        
      //reset all the functions to be able to fill the string back with content
      serOutIndx = 0;
      serInIndx  = 0;
      newData = false;
      Serial.println();
  }
  
}



