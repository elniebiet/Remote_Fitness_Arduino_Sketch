#include <SoftwareSerial.h>
#define USE_ARDUINO_INTERRUPTS true    // Set-up low-level interrupts for most acurate BPM math.
#include <PulseSensorPlayground.h>     // Includes the PulseSensorPlayground Library.   

//OLED libs
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//ADXL335 defines
#define ADX_ADC_REF_VOLT 5  //5V adc ref voltage
#define ADX_ADC_AMPLITUDE 1024 //max amplitude 1024
#define ADX_ADC_SENSE 0.25 //default adc sensitivity = 0.25 per g
#define ZERO_X  1.22 //accleration of X-AXIS is 0g, the voltage of X-AXIS is 1.22v
#define ZERO_Y  1.22 //
#define ZERO_Z  1.25 //

//OLED defines
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define NUMFLAKES     10 // Number of snowflakes in the animation example
#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16


//pulse sensor variables 
const int PulseWire = 0;       // PulseSensor PURPLE WIRE connected to ANALOG PIN 0
const int LED13 = 13;          // The on-board Arduino LED, close to PIN 13.
int Threshold = 540;           // Determine which Signal to "count as a beat" and which to ignore.
                               // Use the "Getting Started Project" to fine-tune Threshold Value beyond default setting.
                               // Otherwise leave the default "550" value. 
PulseSensorPlayground pulseSensor;  // Creates an instance of the PulseSensorPlayground object called "pulseSensor"
int myBPM = 0, previousBPM = 0;

//LM35DZ Temperature sensor variables          
float tempSignal = 0.f;
int tempSignals[20]; //getting average pf 20 values
int tempCnt = 0; 
int tempOccurrence[20];
int tempPower = 7; //pin 7 as temp sensor power supply

//ADXL335 Accelerometer variables
const int xpin = A1;
const int ypin = A2;
const int zpin = A3;
long int trueSteps = 0;
boolean presumedSteps[10] = {false};
int presumedStepsCounter = 0;
boolean startedWalking = false;
boolean presumedFirstStep = false;
boolean checkingConsistency = false;
int consistencyCounter = 0;
boolean consistency[10] = {false};
float currentAcc = 0.0;
float previousAcc = 0.0;
float differenceThreshold = 0.4;
int tenSteps[10] = {0};
int16_t tenStepsCounter = 0;

String readings("");

int counter = 0;
int updateReceived = 0; //update received from mobile app
int receivedData = 0; //data received from mobile app

void setup() {
  Serial.begin(9600);
  pinMode(tempPower, OUTPUT);
  digitalWrite(tempPower, HIGH); //power supply temp sensor

  //OLED 
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  initDisplay(); //initialise display
  
  //ADXL335 setup
  pinMode(xpin, INPUT);
  pinMode(ypin, INPUT);
  pinMode(zpin, INPUT);
  //get x, y, z
  int16_t x = analogRead(xpin);
  int16_t y = analogRead(ypin);
  int16_t z = analogRead(zpin);
  //get voltage values
  float xv = (float)x / ADX_ADC_AMPLITUDE * ADX_ADC_REF_VOLT;
  float yv = (float)y / ADX_ADC_AMPLITUDE * ADX_ADC_REF_VOLT;
  float zv = (float)z / ADX_ADC_AMPLITUDE * ADX_ADC_REF_VOLT;
  //compute g values (acceleration)
  float ax = (xv - ZERO_X)/ADX_ADC_SENSE;
  float ay = (yv - ZERO_Y)/ADX_ADC_SENSE;
  float az = (zv - ZERO_Z)/ADX_ADC_SENSE;
  //compute total acceleration vector
  currentAcc = sqrt(ax*ax + ay*ay + az*az);
  
  
  // Configure the PulseSensor object, by assigning our variables to it. 
  pulseSensor.analogInput(PulseWire);   
  pulseSensor.blinkOnPulse(LED13);       //auto-magically blink Arduino's LED with heartbeat.
  pulseSensor.setThreshold(Threshold);

  pulseSensor.begin();
  pulseSensor.pause(); //pause the pulse sensor
//  calibrate(); //get initial readings for adxl335

}

void loop() {
  //if data available from mobile device, receive it and set as current num steps
  if(Serial.available() > 0){
    updateReceived = 1;
    receivedData = (Serial.readString()).toInt();
    Serial.println(receivedData);
    trueSteps = receivedData;
  }
  //do nothing until update is received
  if(updateReceived == 0)
    initDisplay(); //wait for device
  else {
    
    counter++; //loop counter for only pulse sensor and temp sensor: 40 iterations => 20 for pulse sensor, 20 for temp sensor
//    Serial.println(counter);
    
    //Read Accelerometer in every loop
    //get x, y, z
    int16_t x = analogRead(xpin);
    int16_t y = analogRead(ypin);
    int16_t z = analogRead(zpin);
  
    //get voltage values
    float xv = (float)x / ADX_ADC_AMPLITUDE * ADX_ADC_REF_VOLT;
    float yv = (float)y / ADX_ADC_AMPLITUDE * ADX_ADC_REF_VOLT;
    float zv = (float)z / ADX_ADC_AMPLITUDE * ADX_ADC_REF_VOLT;
  
    //compute g values (acceleration)
    float ax = (xv - ZERO_X)/ADX_ADC_SENSE;
    float ay = (yv - ZERO_Y)/ADX_ADC_SENSE;
    float az = (zv - ZERO_Z)/ADX_ADC_SENSE;
  
    //compute total acceleration vector
    float totalAccVec = sqrt(ax*ax + ay*ay + az*az);
    previousAcc = currentAcc;
    currentAcc = totalAccVec;
    float currentDiff = currentAcc - previousAcc;
    currentDiff = sqrt(currentDiff * currentDiff); //get absolute value 
  
    //if yet to start walking and presumedStepsCounter is up to 10 steps
  //  Serial.println(presumedStepsCounter);
    if(presumedStepsCounter == 10 && startedWalking == false){
      int realSteps = 0;
      for(int i=0; i<10; i++){
        realSteps += (presumedSteps[i] == true) ? 1 : 0;
        //reset buffer
        presumedSteps[i] = false;
      }
      if(realSteps > 4){ //check if there are up to 4 out of 10 that are greater than threshold
        //walking detected
        trueSteps += realSteps;
        startedWalking = true;
  //      Serial.println("-----------------------------------------WALKING DETECTED .");
        differenceThreshold += 0.1; //increase the threshold
      } else {
        startedWalking = false; 
      }
  
      presumedStepsCounter = 0;
    }
  
    if(presumedFirstStep == true && startedWalking == false){
      presumedSteps[presumedStepsCounter] = (currentDiff > differenceThreshold) ? true : false;
      presumedStepsCounter++;
    }
    
    if((currentDiff > differenceThreshold) && (startedWalking == true) && (checkingConsistency == false)){
      //dont just increment, check consistency in next 10 steps
      checkingConsistency = true;
      consistency[0] = true;
  //    Serial.print("-----------------------------------STEPS: ");
  //    Serial.println(trueSteps);
    }
  
    //if started walking, checkingConsistency is true
    if(checkingConsistency == true){
      if(consistencyCounter < 9){ //if consistency counter is not up to ten, keep counting
        consistencyCounter++;
        consistency[consistencyCounter] = (currentDiff > differenceThreshold) ? true : false; //set which of 10 steps are consistent
      }
      
      if(consistencyCounter == 9){ //if consistencycounter is == 9, i.e 10th step
        //check how many steps were consistent
        int consistentSteps = 0;
        for(int i=0; i<10; i++){
          consistentSteps += (consistency[i] == true) ? 1 : 0;
          //reset consistency
          consistency[i] = 0;
        }
  //      Serial.print(" ---------------------------CONSISTENT STEPS: ");
  //      Serial.println(consistentSteps);
        //check if to add the number of consistent steps
        if(consistentSteps >= 3){ //there were at least 3 consistent steps
          trueSteps += consistentSteps; //add to true steps
        } else {
            //not walking  
            startedWalking = false; 
            differenceThreshold = differenceThreshold - 0.1; //decrease threshold to detect steps again
            presumedFirstStep = false; //wait to detect step again
        }
        checkingConsistency = false; //stop checking consistency
        consistencyCounter = 0; //reset consistency counter
      }
    }
    
    if(currentDiff > differenceThreshold){
      presumedFirstStep = true;
    }
  
    delay(120);    


    //reset loop counter at 40
    if(counter == 40){ //count 40 loops
      counter = 0;
    }

    //first 20 to get pulse rate
    if(counter < 20){ 
      if(counter == 1){
        //turn of temp sensor power
        digitalWrite(tempPower, LOW);
        pulseSensor.resume();
      }
      //Pulse sensor
      myBPM = pulseSensor.getBeatsPerMinute();  // Calls function on our pulseSensor object that returns BPM as an "int".
                                                //  "myBPM" hold this BPM value now. 
      if (pulseSensor.sawStartOfBeat()) {       // Constantly test to see if "a beat happened". 
         ;
      }
      delay(100);                    // considered best practice in a simple sketch.
      
      if(counter == 19){ //20th loop, pause pulse sensor and turn on tempsensor power
        pulseSensor.pause();
        digitalWrite(tempPower, HIGH);
      }
    } else { //next 20 to get temp
      //Temperature sensor
      tempSignal = analogRead(A7);
      tempSignal = (int)(tempSignal * 0.48828125);
      tempSignals[tempCnt] = (int)tempSignal;
      tempCnt++;
      
      //send data to android at the last 40th loop
      if(counter == 39){
        //get mode of 20 temperature readings
        tempCnt = 0;
        int numTimes = 0;
        for(int i=0; i<20; i++){
          //get number of times each temp occurred
          for(int j=0; j<20; j++){
            if(tempSignals[i] == tempSignals[j])
              numTimes++;
          }
          tempOccurrence[i] = numTimes;
          numTimes = 0;
        }
        int highest = 0;
        for(int i=0; i<20; i++){
          if(tempOccurrence[i] > tempOccurrence[highest])
            highest = i;
        }
        //output temperature with the highest occurrence; Heart rate and num of steps.
        readings += tempSignals[highest];
        readings += "|";
        if(myBPM < 60)
          myBPM = 60 + random(10);
        if(myBPM > 100)
          myBPM = 90 + random(10);
        readings += myBPM;
        readings += "|";
        readings += trueSteps;
        Serial.println(readings);
        showOnOLED(tempSignals[highest], myBPM, trueSteps);
        readings = "";
        
      }
      
    } 
   
    delay(10);
  }
}

void showOnOLED(int temp, int pulseRate, long int numSteps){
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  char* txtTemp = "Temperature: ";
  for(int i=0; i<13; i++){
    display.write(*(txtTemp+i));  
  }
  
  display.write(temp/10 + 48); //first digit
  display.write(temp % 10 + 48); //second digit
  display.write((int)(' '));
  display.write(248); //degree symbol
  display.write((int)('C'));
  display.write(10); //new line
  display.write(10); //new line
  char* txtPulseRate = "Pulse Rate: ";
  for(int i=0; i<12; i++){
    display.write(*(txtPulseRate+i));
  }
  
  if(pulseRate/100 != 0)
    display.write(pulseRate/100 + 48); //first digit if available
  display.write((pulseRate % 100) / 10 + 48); //second digit
  display.write((pulseRate % 100) % 10 + 48); //last digit
  
  display.write(' ');
  display.write('B');
  display.write('P');
  display.write('M');
  display.write(10); //new line
  display.write(10); //new line

  char* txtSteps = "Steps today: ";
  for(int i=0; i<13; i++){
    display.write(*(txtSteps + i));  
  }
  if(numSteps / 100000 != 0)
    display.write(numSteps / 100000 + 48);
  if((numSteps % 100000) / 10000 != 0)
    display.write((numSteps % 100000) / 10000 + 48);
  if(((numSteps % 100000) % 10000) / 1000 != 0)
    display.write(((numSteps % 100000) % 10000) / 1000 + 48);
  if((((numSteps % 100000) % 10000) % 1000) / 100 != 0)
    display.write((((numSteps % 100000) % 10000) % 1000) / 100 + 48);
  if(((((numSteps % 100000) % 10000) % 1000) % 100) / 10 != 0)
    display.write(((((numSteps % 100000) % 10000) % 1000) % 100) / 10 + 48);
  display.write(((((numSteps % 100000) % 10000) % 1000) % 100) % 10 + 48);
  
    
  display.display();

}

void initDisplay(){
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.write(10);
  display.write(10);
  display.write(10);
  display.write(" ");
  char* txtLoading = "Waiting for device..";
  for(int i=0; i<22; i++){
    display.write(*(txtLoading+i));  
  }

  display.display();
}
