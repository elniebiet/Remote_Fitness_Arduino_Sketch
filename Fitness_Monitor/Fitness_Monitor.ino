#include <SoftwareSerial.h>
#define USE_ARDUINO_INTERRUPTS true    // Set-up low-level interrupts for most acurate BPM math.
#include <PulseSensorPlayground.h>     // Includes the PulseSensorPlayground Library.   

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
float threshold = 178;
float xval[11] = {0};
float yval[11] = {0};
float zval[11] = {0};
float xavg, yavg, zavg;
long int steps = 0;
float totvect[11] = {0};
float totave[11] = {0};
float xaccl[11] = {0};
float yaccl[11] = {0};
float zaccl[11] = {0};

String readings("");

int counter = 0;
int updateReceived = 0; //update received from mobile app
int receivedData = 0; //data received from mobile app

void setup() {
  Serial.begin(9600);
  pinMode(tempPower, OUTPUT);
  digitalWrite(tempPower, HIGH); //power supply temp sensor
  // Configure the PulseSensor object, by assigning our variables to it. 
  pulseSensor.analogInput(PulseWire);   
  pulseSensor.blinkOnPulse(LED13);       //auto-magically blink Arduino's LED with heartbeat.
  pulseSensor.setThreshold(Threshold);

  pulseSensor.begin();
  pulseSensor.pause(); //pause the pulse sensor
  calibrate(); //get initial readings for adxl335

}

void loop() {
  //if data available from mobile device, receive it and set as current num steps
  if(Serial.available() > 0){
    updateReceived = 1;
    receivedData = (Serial.readString()).toInt();
    Serial.println(receivedData);
    steps = receivedData;
  }
  //do nothing until update is received
  if(updateReceived == 0)
  ;
  else {
    
    counter++; //loop counter 40 iterations, 20 for pulse sensor, 20 for temp sensor
    Serial.println(counter);
    //Read Accelerometer in every loop
    for (int a = 1; a < 11; a++){
      xaccl[a] = float(analogRead(xpin));
      delay(1);
      yaccl[a] = float(analogRead(ypin));
      delay(1);
      zaccl[a] = float(analogRead(zpin));
      delay(1);
      totvect[a] = sqrt(((xaccl[a] - xavg) * (xaccl[a] - xavg)) + ((yaccl[a] - yavg) * (yaccl[a] - yavg)) + ((zval[a] - zavg) * (zval[a] - zavg)));
      totave[a] = (totvect[a] + totvect[a - 1]) / 2 ;
  
      if (totave[a] > threshold)
      {
        steps = steps + 1;
      }
  
      if (steps < 0) {
        steps = 0;
      }
      
      delay(10);
    }
    delay(10);

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
        readings += myBPM;
        readings += "|";
        readings += steps;
        Serial.println(readings);
        readings = "";
        
      }
      
    } 
   
    delay(10);
  }
}

void calibrate()
{
  float sum = 0;
  float sum1 = 0;
  float sum2 = 0;
  for (int i = 0; i < 10; i++) {
    xval[i] = float(analogRead(xpin));
    sum = xval[i] + sum;
  }
  delay(10);
  xavg = sum / 10.0;
//  Serial.println(xavg);
  for (int j = 0; j < 10; j++)
  {
    yval[j] = float(analogRead(ypin));
    sum1 = yval[j] + sum1;
  }
  yavg = sum1 / 10.0;
//  Serial.println(yavg);
  delay(10);
  for (int q = 0; q < 10; q++)
  {
    zval[q] = float(analogRead(zpin));
    sum2 = zval[q] + sum2;
  }
  zavg = sum2 / 10.0;
  delay(10);
//  Serial.println(zavg);
}
