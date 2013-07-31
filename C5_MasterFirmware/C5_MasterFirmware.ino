// C5 v1.00 PRODUCTION FIRMWARE
// ----------------------------------------------------------------------------
// Version:     1.0.5 [Tested]
// Date:        March 17, 2013
// Authors:     John and Nick @ JDS Labs, Inc.
// Requires:    Arduino Bootloader, Arduino 1.0.1
// License:     Creative Commons Attribution-ShareAlike 3.0 Unported
//              http://creativecommons.org/licenses/by-sa/3.0/deed.en_US

#include <Wire.h>
#include <EEPROM.h>

// PINOUTS
int upButton = 2;       // Attenuation UP pushbutton
int downButton = 5;     // Attenuation DOWN pushbutton
int centerButton = 0;   // Center pushbutton
int PWRVOLLED = 4;      // PCB LED1 (bi-color); Green LED is wired active-low
int ENPOSREG = 6;       // Pin to enable +7V LDO
int ENNEGREG = 7;       // Pin to enable -7V LDO
int GAIN = A1;          // Pin to control gain
int POTPOWER = A2;      // 5V to DS1882 IC logic
int PREBOOST = A7;      // USB or battery voltage, 2.7-5.5VDC

// Temporary variables
int uptemp = 0;
int downtemp = 0;
int centertemp = 0;                               // NOTE: Attenuation & bass should be stored in non-volatile memory later. Pots remember their value; MCU does not.
byte gainstate = 0;                               // Stores current gain, where 1 = low, 0 = high
byte lastStoredGainState = 0;                     // Last stored EEPROM gain
byte attenuation = 62;                            // Attenuation value of volume potentiometer gangs. Initially set to minimum volume.
byte tempattenuation = 0;                         // Holds attenuation value for second pot bitwise operation
byte lastStoredAttenuation = 0;                   // Last stored EEPROM volume
float BattVoltage = 5;                            // Temp variable to read battery voltage
float LowVoltageThreshold = 3.51;                 // 'Low' hysteresis threshold (voltage at which low battery flashing begins). 3.50V should yield 45-90 minute warning.
float HighVoltageThreshold = 3.55;                // 'High' hysteresis threshold
int flashState = LOW;                             // Flashing power LED during low battery
unsigned long lastMillis = 0;                     // Time of last battery check
unsigned long volumeChangeTimer = 0;              // Time of Last Volume Change
unsigned long gainChangeTimer = 0;                // Time of Last Gain Change
unsigned long startTime = 0;                // Time of Last timer record


boolean lowBatt = false;                          // Low Battery Value

void setup()
{
  delay(50);                                      // Wait for power to stabilize, then setup I/O pins

  pinMode(POTPOWER, OUTPUT);
  pinMode(ENNEGREG, OUTPUT);
  pinMode(ENPOSREG, OUTPUT);
  pinMode(GAIN, OUTPUT);
  pinMode(PWRVOLLED, OUTPUT);
  pinMode(upButton, INPUT);                       // Declare pushbuttons as inputs
  pinMode(downButton, INPUT);
  pinMode(centerButton, INPUT);

  digitalWrite(ENPOSREG, HIGH);                  // Enable +7V LDO
  digitalWrite(ENNEGREG, HIGH);                  // Enable -7V LDO
  delay(200);                                     // Wait for power to stabilize
  digitalWrite(PWRVOLLED, LOW);                  // Turn Power LED on, turn gain and bass LEDs off
  digitalWrite(POTPOWER, HIGH);                  // Enable DS1882 IC
  delay(50); 
  digitalWrite(GAIN, LOW);                      // Set gain HIGH
  
  
  Wire.begin();                                  // Join the I2C bus as master device
 
  // Sets DS1882 registers to nonvolatile memory, zero-crossing enabled, and 64 tap positions (pg 10 of datasheet)
  // FUTURE: This only needs to be performed once--read register and only write if necessary!
  tempattenuation = B10000110;
  Wire.beginTransmission(40);                    // Set parameters of volume potentiometers
  Wire.write(tempattenuation);
  Wire.endTransmission();
  delay(25);
  
  // Retrieve volume and gain values from EEPROM. Set default values if EEPROM is new or corrupted.
  attenuation = EEPROM.read(0);
  delay(4);
  gainstate = EEPROM.read(1);
  
  if(gainstate > 1)
  {
    gainstate = 1;
  }
  changeGain();

  if(attenuation > 63)
  {
    attenuation = 62;
  }
  changeVolume();
  
}

void loop()
{
  uptemp = digitalRead(upButton);                  // Read the up pushbutton
  downtemp = digitalRead(downButton);              // Read the down pushbutton
  centertemp = digitalRead(centerButton);          // Read the center pushbutton


 // User pressed center pushbutton
  if (centertemp == HIGH) {
    delay(50);                                     // Simple debounce: wait and see if user actually pushed the button
    if (centertemp == HIGH) {                      // If user pressed center pushbutton, toggle gain
        if(gainstate < 1){
          gainstate = 1;
        }
        else{
          gainstate = 0;
        }
        
        changeGain();
        changeLEDs();
        delay(333);                                // Wait for 1/3 of a second to prevent additional button presses
    }
  }
  
   // --------------Volume Control---------------
    // First IF statement LOWERS volume. DS1808 has a range of 0-33; DS1882 has range of 0-63. So, for DS1882:
    //       attenuation = 0 = max volume
    //       attenuation = 62 = minimum volume
    //       attenuation = 63 = mute
    if (uptemp == HIGH || downtemp == HIGH) {
      delay(55);                                      // Simple debounce: wait and see if user actually pushed the button
      
      uptemp = digitalRead(upButton);                  // Update the up pushbutton
      downtemp = digitalRead(downButton);              // Update the down pushbutton
  
    //Here we begin checking if button was really pressed and dealing with the direction    
      if ((uptemp == HIGH) && (attenuation < 63)) {   // If user pressed button and volume isn't already at min.
          attenuation++;                              // Increase the potentiometer attenuation value
      }

      if ((downtemp == HIGH) && (attenuation > 0)) {  // If user pressed button and volume isn't already at max.
          attenuation--;                 // Decrease the potentiometer attenuation value
      }
      //make initial volume change
          changeVolume();   
     
      //set starting point for the 1 second delay    
     startTime = millis();
     
     while((millis() - startTime) < 375){
        //check to see if temp changed
          uptemp = digitalRead(upButton);                  // Update the up pushbutton
          downtemp = digitalRead(downButton);              // Update the down pushbutton
          
          //if temp changed, meaning button was let go, then break out and do nothing
          if ((uptemp != HIGH) && (downtemp != HIGH)){
            break;
          }
       }
      
      //if button was not let go then go fast!
      //volume up
      if ((downtemp == HIGH) && (attenuation > 0)) {
          do{
            downtemp = digitalRead(downButton);              // Update the down pushbutton
            delay(55);                          //delay 55 milliseconds between each step
            attenuation--;                      //increment by one
            changeVolume();                     //call volume change funtion
          }while((downtemp == HIGH) && (attenuation > 0));    //checking to make sure number is within valid range
      }
      
      //if button was not let go then go fast!
      //volume down
      if ((uptemp == HIGH) && (attenuation > 0)) {
          do{
            uptemp = digitalRead(upButton);                  // Update the up pushbutton
            delay(55);                          //delay 55 milliseconds between each step
            attenuation++;                      //decrement by one
            changeVolume();                     //call volume change funtion
          }while((uptemp == HIGH) && (attenuation < 63));    //checking to make sure number is within valid range
      }
    }
    // -------------End Volume Control-------------
    
  if ((millis() - lastMillis) > 500)                  // Checks battery voltage every 500mS
  {      checkBattery();
        lastMillis = millis();
  }
  
  if ((millis() - volumeChangeTimer > 2000) || (millis() - gainChangeTimer > 2000)){
         if (lastStoredAttenuation != attenuation){
             EEPROM.write(0, attenuation);            // Store attenuation value in EEPROM location 0, wait 4ms for write to complete
             delay(6);
             lastStoredAttenuation = attenuation;
             volumeChangeTimer = millis();
         }
         else if (lastStoredGainState != gainstate){    
             EEPROM.write(1, gainstate);              // Store gain value in EEPROM location 1        
             delay(6);  
             lastStoredGainState = gainstate;
             gainChangeTimer = millis();  
              
       }
     }
}

void changeVolume()
{
      // Update first potentiometer gang, where pot address is first two bits of attenuation: 00 ######
      Wire.beginTransmission(40);                 // Transmit to DS1808, device #40 (0x28) = 0101 000, LSB is auto sent by Arduino Wire
      Wire.write(attenuation);                    // Write wiper value
      Wire.endTransmission();                     // Stop transmitting    
      
      // Update second potentiometer gang. Same process, where pot address is now: 01 ######.
      // (each I2C transmission should be started stopped individually)
      tempattenuation = B01000000 | attenuation;          // Attenuation value is bitwise OR'ed with address byte fpr second pot gang
      Wire.beginTransmission(40);
      Wire.write(tempattenuation);
      Wire.endTransmission();
      volumeChangeTimer = millis();       
}

void changeGain()
{
  digitalWrite(GAIN, gainstate);
  changeLEDs();
  gainChangeTimer = millis();  

}

void changeLEDs()
{
    if(flashState == LOW && lowBatt == true){       // If the battery is low, toggle the LED according to flashState
      digitalWrite(PWRVOLLED, flashState); 
    }
    
    else if(lowBatt == true && flashState == HIGH){
        digitalWrite(PWRVOLLED, flashState); 
    }
    
    else{                                         // Activate LED as usual if the battery isn't low
        digitalWrite(PWRVOLLED, LOW); 
    }
    
}

void checkBattery()
{
    BattVoltage = (float)analogRead(PREBOOST)*4.95/1023;   // Note: 4.95V is imperical value of c5 5V rail
    
    if(BattVoltage > HighVoltageThreshold){                // Use of hysteresis to avoid erratic LED toggling
      flashState = LOW;                                    // flashState toggles if voltage is below the hysteresis threshold,
      lowBatt = false;                                     // lowBatt prevents flashState from toggling when above high threshold
    }
    else if(BattVoltage < LowVoltageThreshold) {          // lowbatt toggles 
      lowBatt = true;
    }
  
    if(lowBatt == true && flashState == LOW){            // If the battery is low, determine next state of LED
      flashState = HIGH;
    }
    else{
      flashState = LOW;
    }
    
    changeLEDs();                                        // Call LED function to perform toggle
}
