#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <SoftwareSerial.h>

//OLED libs
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//OLED defines
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//ADXL335 defines
#define ADX_ADC_REF_VOLT 5  //5V adc ref voltage
#define ADX_ADC_AMPLITUDE 1024 //max amplitude 1024
#define ADX_ADC_SENSE 0.25 //default adc sensitivity = 0.25 per g
#define ZERO_X  1.22 //accleration of X-AXIS is 0g, the voltage of X-AXIS is 1.22v
#define ZERO_Y  1.22 //
#define ZERO_Z  1.25 //

//SRF02
int reading = 0;

//MAX30105
MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;

//LM35
//LM35DZ Temperature sensor variables          
float tempSignal = 0.f;
int tempSignals[100]; //getting mode pf 40 values
int tempCnt = 0; 
int tempOccurrence[100];
int currentHighestTemp = 0;

//ADXL335 Accelerometer variables
const int xpin = A1;
const int ypin = A2;
const int zpin = A3;
int trueSteps = 0;
float currentAcc = 0.0;
float previousAcc = 0.0;
int tenIters = 0;
boolean countedStep = false;
int presumedSteps = 0;
int checkWalkingIters = 0;
int startStepCount = 0;


String readings("");
int counter = 0;
int socialDistancingEnable = -1; //-1 for off, -2 for on
int updateReceived = 0; //update received from mobile device 1 for received
int receivedData = 0;
int dontSendUpdate = 0; //dont send when object detected, variables will be unstable due to delay
void setup()
{
  Wire.begin();
  Serial.begin(9600);
//  Serial.println("Initializing...");

  //MAX30105
  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
//    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
//  Serial.println("Place your index finger on the sensor with steady pressure.");
  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

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
//  Serial.print("Difference Threshold is: ");
//  Serial.println(differenceThreshold);
}

void loop()
{
  //if data available from mobile device, receive it and check what was sent
  //if steps, update steps, if -1 or -2, set socialDistancingEnable
  if(Serial.available() > 0){
    receivedData = (Serial.readString()).toInt();
    //set socialDistancingEnable
    if(receivedData == -1 || receivedData == -2){
      socialDistancingEnable = receivedData;
    } else {
      updateReceived = 1;
      trueSteps = receivedData;
    }
  }

  if(updateReceived == 1){
    if(counter == 200){
      counter = 0;
    } else {
      counter++;
    }
    
    /*ADXL335 starts*/
    //check current loop count
  //  Serial.print("--------------------------LOOP--- : ");
  //  Serial.println(checkWalkingIters);
    //only count step once in 20 iters
    if(tenIters == 20){
      tenIters = 0;
      countedStep = false;
    } else {
      tenIters++;
    }
  
    //check for walking, at least 10 steps in 250 iters before adding to true steps
    if(checkWalkingIters == 0){
  
      startStepCount = trueSteps;
      checkWalkingIters++;
      
    } else if(checkWalkingIters == 250){
      
      checkWalkingIters = 0;
      if((presumedSteps - startStepCount) > 5){ //if not dont add, reset presumed steps
        trueSteps  = presumedSteps;
      } else {
        presumedSteps = trueSteps;
      }
  
    } else {
      
      checkWalkingIters++;
      
    }
    
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
//    Serial.println(currentDiff);
    if(currentDiff > 0.4){
     if(countedStep == false){
        presumedSteps++; 
        countedStep = true;
//        Serial.print("---------------------------------------------Presumed : ");
//        Serial.println(presumedSteps);
//        Serial.print("---------------------------------------------Truestep : ");
//        Serial.println(trueSteps);
     }   
    }
    
  //  delay(5);
    /*ADXL335 ends*/
    
    /*LM35 starts*/
    tempSignal = analogRead(A0);
    tempSignal = (int)(tempSignal * 0.45);
    tempSignals[tempCnt] = (int)tempSignal;
    tempCnt++;
    
    if(tempCnt == 100){
      //get mode of 100 temperature readings
      tempCnt = 0;
      int numTimes = 0;
      for(int i=0; i<100; i++){
        //get number of times each temp occurred
        for(int j=0; j<100; j++){
          if(tempSignals[i] == tempSignals[j])
            numTimes++;
        }
        tempOccurrence[i] = numTimes;
        numTimes = 0;
      }
      int highest = 0;
      for(int i=0; i<100; i++){
        if(tempOccurrence[i] > tempOccurrence[highest])
          highest = i;
      }
      //output temperature with the highest occurrence; Heart rate and num of steps.
      currentHighestTemp = tempSignals[highest];
  //    readings += tempSignals[highest];
    }
    /*LM35 ends*/
  
    /*MAX30105 starts*/
    long irValue = particleSensor.getIR();
  
    if (checkForBeat(irValue) == true)
    {
      //We sensed a beat!
      long delta = millis() - lastBeat;
      lastBeat = millis();
  
      beatsPerMinute = 60 / (delta / 1000.0);
  
      if (beatsPerMinute < 255 && beatsPerMinute > 20)
      {
        rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
        rateSpot %= RATE_SIZE; //Wrap variable
  
        //Take average of readings
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }
  
  //  Serial.print("IR=");
  //  Serial.print(irValue);
  //  Serial.print(", BPM=");
  //  Serial.print(beatsPerMinute);
  //  Serial.print(", Avg BPM=");
  //  Serial.print(beatAvg);
  
    if (irValue < 50000)
      ;//Serial.print(" No finger?");
  
  //  Serial.println();
    /*MAX30105 ends*/
    
    /*SRF02 starts*/
    // step 1: instruct sensor to read echoes
    Wire.beginTransmission(112); // transmit to device #112 (0x70)
    // the address specified in the datasheet is 224 (0xE0)
    // but i2c adressing uses the high 7 bits so it's 112
    Wire.write(byte(0x00));      // sets register pointer to the command register (0x00)
    Wire.write(byte(0x50));      // command sensor to measure in "inches" (0x50)
    // use 0x51 for centimeters
    // use 0x52 for ping microseconds
    Wire.endTransmission();      // stop transmitting
  
    // step 2: wait for readings to happen
    delay(5);                   // datasheet suggests at least 65 milliseconds
  
    // step 3: instruct sensor to return a particular echo reading
    Wire.beginTransmission(112); // transmit to device #112
    Wire.write(byte(0x02));      // sets register pointer to echo #1 register (0x02)
    Wire.endTransmission();      // stop transmitting
  
    // step 4: request reading from sensor
    Wire.requestFrom(112, 2);    // request 2 bytes from slave device #112
  
    // step 5: receive reading from sensor
    if (2 <= Wire.available()) { // if two bytes were received
      reading = Wire.read();  // receive high byte (overwrites previous reading)
      reading = reading << 8;    // shift high byte to be high 8 bits
      reading |= Wire.read(); // receive low byte as lower 8 bits
      //if social distancing is enabled and distance is less than 1meter report to mobile
      if(reading > 10 && reading < 39 && socialDistancingEnable == -2){
        //send reading in same format to device first reading is a warning message i.e -1
        readings = "-1"; //sending -1 means object detected within 1m
        readings += "|";
        readings += beatAvg;
        readings += "|";
        readings += trueSteps;
        Serial.println(readings);
        readings = "";
        delay(1000);
        dontSendUpdate = 1; 
      }
//      Serial.println(reading);   // print the reading
    }

    if(dontSendUpdate == 0){
      delay(5);                  // wait a bit since people have to read the output :) 
      /*SRF02 ends*/
      
      /*OLED display*/
      if(counter == 200){
        readings += currentHighestTemp;
        readings += "|";
        readings += beatAvg;
        readings += "|";
        readings += trueSteps;
        Serial.println(readings);
        readings = "";
        showOnOLED(currentHighestTemp, beatAvg, trueSteps); 
      }
    }

    dontSendUpdate = 0;
  
  
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
  char* txtPulseRate = "Pulse Rate: ";
  for(int i=0; i<12; i++){
    display.write(*(txtPulseRate+i));
  }

  if(pulseRate < 100){
    if(pulseRate/100 != 0){
      display.write(pulseRate/100 + 48); //first digit if available
    }
    display.write((pulseRate % 100) / 10 + 48); //second digit
    display.write((pulseRate % 100) % 10 + 48); //last digit
  } else {
    display.write('1');
    display.write('0');
    display.write('0');
    display.write('+');
  }
  
  display.write(' ');
  display.write('B');
  display.write('P');
  display.write('M');
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
  display.write(" ");
  char* txtLoading = "Waiting for device..";
  for(int i=0; i<22; i++){
    display.write(*(txtLoading+i));  
  }

  display.display();
}
