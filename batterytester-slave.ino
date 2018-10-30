/* Timer.h https://github.com/JChristensen/Timer
 *  
 *  Slave battery tester. A testing system that can be daisy chained in up to 100 units and sends all data to master unit. It can do dischar, charge and IR tests. Configureable from the master
 *  
 *  Functions:
 *  
 *  
 *  Cost:
 *  Per cell cost need to be lower than Opus tester
 *  
 *  
 *  More infor:
  */



#include <Timer.h>
#include <EEPROM.h>
#include "custom.h"

// Constant Variables
const byte cells = 1; // Number of cells to monitor
const bool debug = true;

bool runningTest = false; //If we have anything running and being tested



struct stru{
  int unitID = 0;
  byte field2;
  char name[10];
};
stru configEE;


static const uint8_t batteryVoltagePins[] =     {A0,A3,A6};
static const uint8_t batteryVoltageDropPins[] = {A1,A4,A7};
static const uint8_t batteryVoltageDropPins2[] = {A2,A5};
static const uint8_t chargeMosfetPins[] =       {2,5};
static const uint8_t chargeLedPins[] =          {3,6};
static const uint8_t dischargeMosfetPins[] =    {4,7};


int  serIn;             // var that will hold the bytes-in read from the serialBuffer
char serInString[100];  // array that will hold the different bytes  100=100characters;
                        // -> you must state how long the array will be else it won't work.
int  serInIndx  = 0;    // index of serInString[] in which to insert the next incoming byte
int  serOutIndx = 0;    // index of the outgoing serInString[] array;
bool newData = false; // variable to hold if we have data or not

// Initialization
unsigned long longMilliSecondsCleared[cells];
int intSeconds[cells];
int intMinutes[cells];
int intHours[cells];


// Check Battery Voltage
byte batteryDetectedCount[cells];

// Charge / Recharge Battery
float batteryInitialVoltage[cells];
byte batteryChargedCount[cells];

// Discharge Battery
int intMilliSecondsCount[cells];
unsigned long longMilliSecondsPreviousCount[cells];
unsigned long longMilliSecondsPrevious[cells];
unsigned long longMilliSecondsPassed[cells];
float dischargeMilliamps[cells];
float dischargeVoltage[cells];
float dischargeAmps[cells];
float batteryCutOffVoltage[cells];
bool dischargeCompleted[cells];
bool dischargeUploadCompleted[cells];

int dischargeMinutes[cells];
bool pendingDischargeRecord[cells];

// Check Battery Milli Ohms
byte batteryMilliOhmsCount[cells];
float tempMilliOhmsValue[cells];
float milliOhmsValue[cells];

byte cycleState[cells];
byte cycleStateLast = 0;

byte mosfetSwitchState[cells]; // Is it idle, discharging or charging?
float batteryVoltage[cells];
float batteryLastVoltage[cells];
byte batteryFaultCode[cells];
byte batteryInitialTemp[cells];
byte batteryHighestTemp[cells];
byte batteryCurrentTemp[cells];
int batteryCapacity[cells];
int batteryTimeCharge[cells];
int batteryTimeDisCharge[cells];
int batteryIr[cells];
byte tempCount[cells];

Timer timerObject;
void setup() {
// inite serial to handle the daisy chain
Serial.begin(9600);

//Fetch config from EEPROM
EEPROM.get(0, configEE);

//If screen init


// Setup pins 
for(byte i = 0; i < cells; i++)
  { 
    pinMode(chargeLedPins[i], INPUT_PULLUP);
    pinMode(chargeMosfetPins[i], OUTPUT);
    pinMode(dischargeMosfetPins[i], OUTPUT);
    digitalWrite(chargeMosfetPins[i], LOW);
    digitalWrite(dischargeMosfetPins[i], LOW);
  }

timerObject.every(2, cycleSerial);  // Read data from serial to buffer
timerObject.every(2, cycleSerialBuffer);  //Process data from buffer
timerObject.every(2000, cycleValues);  // Process battery data
timerObject.every(4000, cycleValuesOut);  // Process battery data


timerObject.every(10000, cycleAlive);
}

void loop() {
  // Only timer objects in here. 
  timerObject.update(); // Timer Triggers
}


//Send keep alive message if no testing is done
void cycleAlive() {
  if (!runningTest) {
    Serial.print(configEE.unitID);
    Serial.println(" Status: running");
  }
}

// This cycles through all values to read from the battery
void cycleValues() {
  byte tempProcessed = 0;
  for(byte i = 0; i < cells; i++)
  { 
    switch (cycleState[i]) 
    {
    case 0: // Check Battery Voltage
      //digitalWrite(chargeMosfetPins[i], LOW);
      //digitalWrite(dischargeMosfetPins[i], LOW);
      if(batteryCheck(i)) batteryDetectedCount[i]++;
      if (batteryDetectedCount[i] == 5)
      {
        
      debugPrint(" Status: Found battery on " + String(i));
       // batteryCurrentTemp[i] = getTemperature(i);
       // batteryInitialTemp[i] = batteryCurrentTemp[i];
       // batteryHighestTemp[i] = batteryCurrentTemp[i];
        // Re-Initialization
        batteryDetectedCount[i] = 0;
        //strcpy(batteryBarcode[i], "");
        batteryChargedCount[i] = 0;
        batteryMilliOhmsCount[i] = 0;
        tempMilliOhmsValue[i] = 0;
        milliOhmsValue[i] = 0;
        intMilliSecondsCount[i] = 0;
        longMilliSecondsPreviousCount[i] = 0;
        longMilliSecondsPrevious[i] = 0;
        longMilliSecondsPassed[i] = 0;
        dischargeMilliamps[i] = 0.0;
        dischargeVoltage[i] = 0.00;
        dischargeAmps[i] = 0.00;
        dischargeCompleted[i] = false;
        batteryFaultCode[i] = 0;
        clearSecondsTimer(i);
        getBatteryVoltage(i); // Get battery voltage for Charge Cycle
        batteryInitialVoltage[i] = batteryVoltage[i];
        cycleState[i] = 2; // Check Battery Voltage Completed set cycleState to Get Battery Barcode   
  
      }
      break;
      
      case 2: // Charge Battery
        if(!batteryCheck(i)) {
          debugPrint("Status: Battery pulled out of bank while charging" + String(i));
          batteryFaultCode[i] = 9; // Error code for battery pulled out?
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          cycleState[i] = 0; // 
          break;    
        }
        getBatteryVoltage(i);
        //batteryCurrentTemp[i] = getTemperature(i);
        //tempProcessed = processTemperature(i, batteryCurrentTemp[i]);
        if (tempProcessed == 2) 
        {
          //Battery Temperature is >= MAX Threshold considered faulty
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          batteryFaultCode[i] = 7; // Set the Battery Fault Code to 7 High Temperature
          cycleState[i] = 7; // Temperature is to high. Battery is considered faulty set cycleState to Completed  
          debugPrint("Status: To high temperature on " + String(i));      
        } else {
          debugPrint("Status: Charging running on cell " + String(i));
          digitalWrite(chargeMosfetPins[i], HIGH); // Turn on TP4056
          batteryChargedCount[i] = batteryChargedCount[i] + chargeCycle(i,false);
          if (batteryChargedCount[i] == 5)
          {
            digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
            //clearSecondsTimer(i);
            batteryChargedCount[i] = 0;
            cycleState[i] = 3; // Charge Battery Completed set cycleState to Check Battery Milli Ohms should be 3
          } 
        }
        if (intHours[i] == chargingTimeout) // Charging has reached Timeout period. Either battery will not hold charge, has high capacity or the TP4056 is faulty
        {
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          batteryFaultCode[i] = 9; // Set the Battery Fault Code to 7 Charging Timeout
          cycleState[i] = 7; // Charging Timeout. Battery is considered faulty set cycleState to Completed
        }
      break;

    case 3: // Check Battery Milli Ohm
      batteryMilliOhmsCount[i] = batteryMilliOhmsCount[i] + milliOhms(i);
      tempMilliOhmsValue[i] = tempMilliOhmsValue[i] + milliOhmsValue[i];
      if (batteryMilliOhmsCount[i] == 4)
      {
        milliOhmsValue[i] = tempMilliOhmsValue[i] / 4;
        debugPrint("Status: Battery IR: " + String(milliOhmsValue[i]) + " on cell: " + String(i));
        if (milliOhmsValue[i] > highMilliOhms) // Check if Milli Ohms is greater than the set high Milli Ohms value
        {
          batteryFaultCode[i] = 3; // Set the Battery Fault Code to 3 High Milli Ohms
          cycleState[i] = 7; // Milli Ohms is high battery is considered faulty set cycleState to Completed
        } else {
          if (intMinutes[i] <= 1) // No need to rest the battery if it is already charged
          { 
            cycleState[i] = 5; // Check Battery Milli Ohms Completed set cycleState to Discharge Battery
          } else {
            cycleState[i] = 4; // Check Battery Milli Ohms Completed set cycleState to Rest Battery   
          }
          clearSecondsTimer(i);  
        }
      }
      break;
      
      case 4: // Rest Battery
      getBatteryVoltage(i);
      //batteryCurrentTemp[i] = getTemperature(i);
      if (intMinutes[i] == restTimeMinutes) // Rest time
      {
        clearSecondsTimer(i);
        intMilliSecondsCount[i] = 0;
        longMilliSecondsPreviousCount[i] = 0;
        cycleState[i] = 5; // Rest Battery Completed set cycleState to Discharge Battery    
      }
      break;
      
     case 5: // Discharge Battery
     //batteryCurrentTemp[i] = getTemperature(i);
      //tempProcessed = processTemperature(i, batteryCurrentTemp[i]);
        if(!batteryCheck(i)) {
          debugPrint("Status: Battery pulled out of bank discharging" + String(i));
          digitalWrite(dischargeMosfetPins[i], LOW);
          batteryFaultCode[i] = 9; // Error code for battery pulled out?
          cycleState[i] = 0; // Temperature is to high. Battery is considered faulty set cycleState to Completed
          break;    
        }
      
      if (tempProcessed == 2)
      {
        //Battery Temperature is >= MAX Threshold considered faulty
        digitalWrite(dischargeMosfetPins[i], LOW); 
        batteryFaultCode[i] = 7; // Set the Battery Fault Code to 7 High Temperature
        cycleState[i] = 7; // Temperature is high. Battery is considered faulty set cycleState to Completed
      } else {
        if (dischargeCompleted[i] == true)
        {
          //Set Variables for Web Post
          dischargeMinutes[i] = intMinutes[i] + (intHours[i] * 60);
          pendingDischargeRecord[i] = true;
          if (dischargeMilliamps[i] < lowMilliamps) // No need to recharge the battery if it has low Milliamps
          { 
            batteryFaultCode[i] = 5; // Set the Battery Fault Code to 5 Low Milliamps
            cycleState[i] = 7; // Discharge Battery Completed set cycleState to Completed
          } else {
            getBatteryVoltage(i); // Get battery voltage for Recharge Cycle
            batteryInitialVoltage[i] = batteryVoltage[i]; // Reset Initial voltage
            cycleState[i] = 6; // Discharge Battery Completed set cycleState to Recharge Battery
          }
          //clearSecondsTimer(i);           
        } else {
          if(dischargeCycle(i)) dischargeCompleted[i] = true;
        }
      }
      break; 
      
      case 6: // Recharge Battery
        digitalWrite(chargeMosfetPins[i], HIGH); // Turn on TP4056
        //batteryCurrentTemp[i] = getTemperature(i);
        //tempProcessed = processTemperature(i, batteryCurrentTemp[i]);
        if (tempProcessed == 2)
        {
          //Battery Temperature is >= MAX Threshold considered faulty
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          batteryFaultCode[i] = 7; // Set the Battery Fault Code to 7 High Temperature
          cycleState[i] = 7; // Temperature is high. Battery is considered faulty set cycleState to Completed
        } else {
          batteryChargedCount[i] = batteryChargedCount[i] + chargeCycle(i,true);
          if (batteryChargedCount[i] == 5)
          {
            digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
            batteryChargedCount[i] = 0;
            clearSecondsTimer(i);
            cycleState[i] = 7; // Recharge Battery Completed set cycleState to Completed
          } 
        }
        if (intHours[i] == chargingTimeout) // Charging has reached Timeout period. Either battery will not hold charge, has high capacity or the TP4056 is faulty
        {
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          digitalWrite(dischargeMosfetPins[i], LOW); // Turn off Discharge Mosfet (Just incase)
          batteryFaultCode[i] = 9; // Set the Battery Fault Code to 9 Charging Timeout
          cycleState[i] = 7; // Charging Timeout. Battery is considered faulty set cycleState to Completed
        }
        getBatteryVoltage(i);
      break;

      case 7: // Completed
        if (!batteryCheck(i)) batteryDetectedCount[i]++;
        if (batteryDetectedCount[i] == 2) 
        {
          batteryDetectedCount[i] = 0;
          cycleState[i] = 0; // Completed and Battery Removed set cycleState to Check Battery Voltage
        }
      break;
  
    }
  }
}


// This cycles through all values to read from the battery
void cycleValuesOut() {

  // Lets send out the values on the grid


  Serial.print(configEE.unitID);
  Serial.println(" Data........");
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
    
    /*String data = "";
    for(serOutIndx=0; serOutIndx < serInIndx; serOutIndx++) {
      data = data + serInString[serOutIndx] ;    //Lets temporary store the data in a string.....
    }*/
    if (serInString[0] == 's') {
      char command = serInString[1];
      switch (command) {
        case 'r':
          Serial.print("sr");
          configEE.unitID = serInString[2] - '0';
          Serial.print(configEE.unitID+1);
          Serial.println();
          EEPROM.put(0, configEE);
          debugPrint("Config: Changed ID to:" + String(configEE.unitID) );
          break;
  
        case 'u':
          if (serInString[1] - '0' == configEE.unitID) {  // If the command sent is for this unit....
            debugPrint("This unit was notified lets configure: ");
          }
          break;
  
        default:  // If the data wasnt for me lets ship it to next one
          break;
          
        
        }
      } else {
        for(serOutIndx=0; serOutIndx < serInIndx; serOutIndx++) {
          Serial.write( serInString[serOutIndx] );    //print out the byte at the specified index
        }
      }
      

      //reset all the functions to be able to fill the string back with content
      serOutIndx = 0;
      serInIndx  = 0;
      newData = false;
  }
  
}


void getBatteryVoltage(byte j)
{
  float batterySampleVoltage = 0.00;
  for(byte i = 0; i < 50; i++)
  {
    batterySampleVoltage = batterySampleVoltage + analogRead(batteryVoltagePins[j]);
  }
  batterySampleVoltage = batterySampleVoltage / 50; 
  batteryVoltage[j] = batterySampleVoltage * referenceVoltage / 1024; 
  Serial.print("Battery voltage: ");
  Serial.println(batteryVoltage[j]);
}

bool batteryCheck(byte j)
{
  getBatteryVoltage(j);
  
  if (batteryLastVoltage[j] - batteryVoltage[j] >= 0.05 || batteryLastVoltage[j] - batteryVoltage[j] <= 0.05) 
  {
    //digitalWrite(chargeMosfetPins[j], HIGH); // Turn on TP4056
    //digitalWrite(chargeMosfetPins[j], LOW); // Turn off TP4056
    getBatteryVoltage(j);
  }
  if (batteryVoltage[j] <= batteryVolatgeLeak) 
  {
    return false;
  } else {
    return true;
  }
  batteryLastVoltage[j] = batteryVoltage[j];
}

byte chargeCycle(byte j, bool storage)
{
  if (storage) {
    getBatteryVoltage(j);
    if (batteryVoltage[j] > 3.8) {
      debugPrint("Storage is done so need to count up");
      return 1;
    }
  }
  digitalWrite(chargeMosfetPins[j], HIGH); // Turn on TP4056
  if (digitalRead(chargeLedPins[j]) == HIGH)
  {
    debugPrint("Charging is done so need to count up");
    return 1;
  } else {
    return 0;
  }
}

bool dischargeCycle(byte j)
{
  float batteryShuntVoltage = 0.00;
  String d;
  intMilliSecondsCount[j] = intMilliSecondsCount[j] + (millis() - longMilliSecondsPreviousCount[j]);
  longMilliSecondsPreviousCount[j] = millis();
  if (intMilliSecondsCount[j] >= 5000 || dischargeAmps[j] == 0) // Get reading every 5+ seconds or if dischargeAmps = 0 then it is first run
  {
    dischargeVoltage[j] = analogRead(batteryVoltagePins[j]) * referenceVoltage / 1023.0;
    batteryShuntVoltage = analogRead(batteryVoltageDropPins[j]) * referenceVoltage / 1023.0; 
    d = "V: ";
    d = d + String(dischargeVoltage[j]) + " V2: ";
    d = d + String(batteryShuntVoltage) + " A: ";
    if(dischargeVoltage[j] >= defaultBatteryCutOffVoltage)
    {
      digitalWrite(dischargeMosfetPins[j], HIGH);
      dischargeAmps[j] = (dischargeVoltage[j] - batteryShuntVoltage) / shuntResistor;
      longMilliSecondsPassed[j] = millis() - longMilliSecondsPrevious[j];
      dischargeMilliamps[j] = dischargeMilliamps[j] + (dischargeAmps[j] * 1000.0) * (longMilliSecondsPassed[j] / 3600000.0);
      longMilliSecondsPrevious[j] = millis();
      d = d + String(dischargeAmps[j]) + " mAh: "+ String(dischargeMilliamps[j]);
    }
    debugPrint(d);  
    intMilliSecondsCount[j] = 0;
    if(dischargeVoltage[j] < defaultBatteryCutOffVoltage)
    {
      digitalWrite(dischargeMosfetPins[j], LOW);
      return true;
    } 
  } 
  return false;
}

byte milliOhms(byte j)
{
  float resistanceAmps = 0.00;
  float voltageDrop = 0.00;
  float batteryVoltageInput = 0.00;
  float batteryShuntVoltage = 0.00;
  digitalWrite(dischargeMosfetPins[j], LOW);
  getBatteryVoltage(j);
  batteryVoltageInput = batteryVoltage[j];
  digitalWrite(dischargeMosfetPins[j], HIGH);
  delay(2);
  getBatteryVoltage(j);
  batteryShuntVoltage = batteryVoltage[j];
  digitalWrite(dischargeMosfetPins[j], LOW);
  resistanceAmps = batteryShuntVoltage / shuntResistor;
  voltageDrop = batteryVoltageInput - batteryShuntVoltage;
  milliOhmsValue[j] = ((voltageDrop / resistanceAmps) * 1000) + offsetMilliOhms; // The Drain-Source On-State Resistance of the IRF504's
  if (milliOhmsValue[j] > 9999) milliOhmsValue[j] = 9999;
  return 1;
}

void secondsTimer(byte j)
{
  unsigned long longMilliSecondsCount = millis() - longMilliSecondsCleared[j];
  intHours[j] = longMilliSecondsCount / (1000L * 60L * 60L);
  intMinutes[j] = (longMilliSecondsCount % (1000L * 60L * 60L)) / (1000L * 60L);
  intSeconds[j] = (longMilliSecondsCount % (1000L * 60L * 60L) % (1000L * 60L)) / 1000;
}

void clearSecondsTimer(byte j)
{
  longMilliSecondsCleared[j] = millis();
  intSeconds[j] = 0;
  intMinutes[j] = 0;
  intHours[j] = 0;
}

void debugPrint(String str) {
  if (debug) {
    Serial.print(configEE.unitID);
    Serial.print(" - ");
    Serial.println(str);
  }
}

