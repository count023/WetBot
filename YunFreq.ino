/*
YunFreq 0.3a by Tacuma, count0
06. Juni 2015
basiert auf:
 
 * FreqCount - Example with serial output
 * http://www.pjrc.com/teensy/td_libs_FreqCount.html
 *
 * This example code is in the public domain.

 Sensors Input auf Pin 12 / Arduino Yún
 Sensors VCC   auf Pin 14 - 19 / Arduino Yún
 
 Fuer die Lauffaehigkeit ist in der FreqCount.cpp oder so
 eine zeile 556 oder so auskommentiert. (irgendwas mit Timer2 oder so)
 31.05.2015 / modified by tq


Vielen Dank an die Arduino-Community und alle OpenSourc@s in the World!

 */
#include <FreqCount.h>
#include <Process.h>
#include <Time.h>
#include <Wire.h>

#define TIME_HEADER  'T'   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 
const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

                               // || Zustand            || count0 || Tacuma ||
const int LEDdusty = 4;        // |  staubig         => |  rot    |  rot2   |
const int LEDdry = 5;          // |  trocken         => |  gelb   |  rot1   |
const int LEDhumid = 6;        // |  feucht          => |  gruen  |  gelb   |
const int LEDwet = 7;          // |  nass            => |  blau   |  gruen  |
const int LEDjustWatered = 8;  // |  frisch gegossen => |  weiss  |  blau   |

const int LEDs[5]= { LEDjustWatered, LEDwet, LEDhumid, LEDdry, LEDdusty };

int previouslyHighlightedLedNumber = -1;
int currentlyHighlightedLedNumber = -1;


const int SENSOR_1  = A0;
const int SENSOR_2  = A1;
const int SENSOR_3  = A2;
const int SENSOR_4  = A3;
const int SENSOR_5  = A4;
const int SENSOR_6  = A5;
// const int SENSORs[6] = { SENSOR_1, SENSOR_2, SENSOR_3, SENSOR_4, SENSOR_5, SENSOR_6, SENSOR_6 };
const int SENSORs[2] = { SENSOR_1, SENSOR_2 };

int previousSensorNumber = -1;
int currentSensorNumber = -1;

const long waitIntervallForRead = 30000; // half a minute

unsigned long currentFrequency;

int i = 0; // global iteration counter variable (take care, it's global ;) )



void setup() {

  Bridge.begin();	// Initialize the Bridge
  Serial.begin(9600);	// Initialize the Serial
  pinMode(13, OUTPUT); // initialize the LED pin as an output

  // waiting for serial just for debugging purposes
  while (!Serial) {
    // wait for serial port to connect. Needed for Leonardo only
    digitalWrite(13, HIGH); // wait for Serial to connect.
  }
  Serial.println("Serial is available.");
  digitalWrite(13, LOW); // Serial now is available, switching of the led

  Serial.println("***  YunFreq 0.3a  ***");
  Serial.println("***  by tq, count0 6/2015 *** ");
  Serial.println("***  18.326 Bytes  *** ");
  Serial.println("");

  delay(200);

  Serial.println("will initialize the time");
  initAndSyncClock();

  Serial.println("will initialize the LEDs");
  initLeds();

  Serial.println("will initialize the SENSORs");
  initSensors();

  Serial.println("Messung beginnt.");
  FreqCount.begin(1000);
  delay(1000);
  
} // end void setup


void loop() {

  if (FreqCount.available()) {
    currentFrequency = FreqCount.read();
    Serial.print("Sensor: ");
    Serial.print((1 + previousSensorNumber));
    Serial.print(" -> frequency: ");
    Serial.print(currentFrequency);
    Serial.print(" Hz @ ");
    digitalClockDisplay();
  }

  // After reading the Frequency after the delay of this loop function we switch to the next SENSOR
  currentSensorNumber = getNextSensorNumber();
  if (previousSensorNumber > -1) {
    digitalWrite(SENSORs[previousSensorNumber], LOW);
  }
  digitalWrite(SENSORs[currentSensorNumber], HIGH);

  if (currentFrequency == 0) {

    Serial.println("Error: no signal.");
    digitalWrite(LEDs[previouslyHighlightedLedNumber], LOW);

  } else {

    currentlyHighlightedLedNumber = getGradeOfDrynessByFrequency(currentFrequency);

    if (currentlyHighlightedLedNumber != previouslyHighlightedLedNumber) {

      if (previouslyHighlightedLedNumber > -1) {
        digitalWrite(LEDs[previouslyHighlightedLedNumber], LOW);
      }

      Serial.print("####--- ! Dryness changed for Sensor: ");
      Serial.print((1 + previousSensorNumber));
      Serial.print(" to: ");
      Serial.println(currentlyHighlightedLedNumber);
      digitalWrite(LEDs[currentlyHighlightedLedNumber], HIGH);
      
      // saving current led number for next iteration
      previouslyHighlightedLedNumber = currentlyHighlightedLedNumber;
    }
  }

  // saving current sensor number for next iteration
  previousSensorNumber = currentSensorNumber;

  delay(waitIntervallForRead);
} // end void loop



// so to say, private methods ;)
void initLeds() {
  Serial.print("LED-Tests: ");
  delay(800);
  setupLeds();
  delay(300);
  testLedsInOrder(300);
  Serial.println(" Done!\n");
}

void setupLeds() {
  for (i = 0; i < (sizeof(LEDs)/sizeof(int)); i++) {

    pinMode( LEDs[i], OUTPUT );
    
    digitalWrite(LEDs[i], HIGH);
    delay(500);
    digitalWrite(LEDs[i], LOW);
  }
}

void testLedsInOrder(int interruptionTime) {
  int lastPin = -1;
  for (i = 0; i <= (sizeof(LEDs)/sizeof(int)); i++) {
    if ( lastPin != -1 ) {
      digitalWrite(lastPin, LOW);
    }
    if ( i != (sizeof(LEDs)/sizeof(int))) { // not the last iteration...
      digitalWrite( LEDs[i], HIGH );
      lastPin = LEDs[i];
    }
    delay(interruptionTime);
  }
}

/**
 * Formel generiert von www.arndt-bruenner.de/mathe/scripts/regr.htm
 * anhand der Dtaenreihe (5 LEDs):
 *  0       0
 *  15000   1
 *  50000   2
 *  100000  3
 *  200000  4
 *     => f(x) = -0 * x^2 + 0,000036 * x + 0,163636
 *  Für die Datenreihe (4 LEDs):
 *  0       0
 *  50000   1
 *  100000  2
 *  200000  3 
 *     => f(x) = -0 * x^2 + 0,000022 * x + 0,206379 
 */
int getGradeOfDrynessByFrequency(float freq) {
  //Serial.print("getGradeOfDrynessByFrequency: with freq: ");
  //Serial.print(freq);
  int gradeOfDryness = round( ( 0.000022 * freq ) + 0.163636 );
  //Serial.print(" and rounded result: ");
  //Serial.print(gradeOfDryness);
  if (gradeOfDryness >= (sizeof(LEDs)/sizeof(int))) {
    //Serial.println(" WARN: formula calculated incompatible result !!!!");
    gradeOfDryness = (sizeof(LEDs)/sizeof(int)) - 1;
  }
  //Serial.println("");

  return gradeOfDryness;
}

void initSensors() {

  for (i = 0; i < (sizeof(SENSORs)/sizeof(int)); i++) {
    pinMode( SENSORs[i], OUTPUT );
  }

  // switching to the first sensor:
  previousSensorNumber = 0;
  digitalWrite(SENSORs[previousSensorNumber], HIGH);
}

int getNextSensorNumber() {
  // int previousSensorNumber = -1;
  // int currentSensorNumber = -1;

  if (previousSensorNumber >= (sizeof(SENSORs)/sizeof(int)) - 1) {
    return 0;
  }
  return 1 + previousSensorNumber;
}


void initAndSyncClock() {
  
  // thie makes this function clallable just once at the very beginning
  setSyncProvider( requestTimeSyncFromYunSide );  //set function to call when sync required
  setSyncInterval( (uint32_t)(24 * 60 * 59) ); // choosed 84960 secs; a day has 86400  // argument is maximal: 4.294.967.295
}

time_t requestTimeSyncFromYunSide() {
  unsigned long pctime;
  char pctimeCharBuf[11] = "";
  Process p;

  p.runShellCommand("/bin/date +%s");
  
  // do nothing until the process finishes, so you get the whole output:
  while (p.running());

// Read command output. runShellCommand() should have passed "Signal: xx&":
  int i = 0;
  while (p.available() > 0) {
    char c = p.read();
    if (isdigit(c)) {
      pctimeCharBuf[i] = c;
    }
    //Serial.print(c);
    i += 1;
  }
  //Serial.println(;
  //Serial.print("pcCharBuf: ");
  //Serial.println(pctimeCharBuf);

  char *junk;
  pctime = strtol(pctimeCharBuf, &junk, 10);
  if (strlen(junk) > 0) { // systemcall response from yun side contains unexpected characters
    pctime = DEFAULT_TIME; // fall back to defined const fallback @see above
  }
  Serial.print("requestTimeSyncFromYunSide => read pctime is: ");
  Serial.println(pctime);

  return pctime;
}

void digitalClockDisplay() {
  // digital clock display of the time
  printDigits(day(), false);
  Serial.print(".");
  printDigits(month(), false);
  Serial.print(".");
  Serial.print(year()); 
  Serial.print(" ");
  printDigits(hour(), false);
  printDigits(minute(), true);
  printDigits(second(), true);
  Serial.print(" UTC");

  Serial.println(); 
}

void printDigits(int digits, bool colon){
  // utility function for digital clock display: prints preceding colon and leading 0
  if (colon) {
    Serial.print(":");
  }
  if (digits < 10) {
    Serial.print('0');
  }
  Serial.print(digits);
}

