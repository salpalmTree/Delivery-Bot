#include <Arduino.h>
#include <QTRSensors.h>
#include <Romi32U4.h>
#include <Romi32U4Motors.h>
#include <Rangefinder.h>
#include <Chassis.h>
#include <ir_codes.h>
#include <servo32u4.h>

#define LED_PIN 13
#define k_p 0.12
#define k_d 0.320 //was 0.315
#define k_i 0.0001 //was .0001
#define MAX_SPEED 150
#define BASE_SPEED 100
#define SERVO_UP 475 //was SERVO DOWN = 450 normal ground
#define SERVO_DOWN 1230 //was SERVO UP = 1300~1200 normal ground 750 for 8cm

// Sets up the IR receiver/decoder object
//const uint8_t IR_DETECTOR_PIN = 1;
//IRDecoder decoder(IR_DETECTOR_PIN);
QTRSensors qtr;
const uint8_t SensorCount = 6;
uint16_t sensorValues[SensorCount];
Rangefinder rangefinder(11, 4); 
Servo32U4 servo; 

volatile bool calibrated = false;
bool zone = false; 
int lefthigh = 0; 
int righthigh = 0; 
int sense = 600; 
volatile bool grab = false; 
volatile bool height = false; 
float kp = k_p, kd = k_d, ki = k_i;
float distance = 0.0;   
String uartMessage = "";  
int amountR = 0; 


enum STATE {NOTRDY, READY, FOLLOW, PATH, DELIVERY, PICK, DROP}; 
STATE robotState = NOTRDY; 

typedef struct
{
  char name; 
  char direct[30]; 
}ROUTE;
/*
  'l' -> left turn
  'r' -> right turn
  'f' -> forward
  'b' -> bust 180 (go back/turn around)
  'E' -> end of run
*/
ROUTE testRun = {'T',{'p', 'l', 'r', 'r', 'l', 'l', 'd', 'r', 'r', 'l', 'l', 'r', 'b', 'E'}}; //Test run at home {'T',{'p', 'l', 'r', 'r', 'l', 'l', 'd', 'r', 'r', 'l', 'l', 'r', 'b', 'E'}}; 
ROUTE Run = {'R', {'b', 'l', 'b', 's', 'r', 'b', 'r', 'b', 'r', 'r', 'b', 'r', 'b', 'r', 'l', 'b', 's', 'b', 'r', 'r', 'b', 'r', 'b', 'r', 'r', 'b', 'r', 'b', 'E'}}; 
ROUTE A = {'A', {'p', 'r', 'r', 'd', 'l', 'l', 'b', 'E'}}; 
ROUTE B = {'B', {'p', 'r', 's', 'd', 's', 'l', 'b', 'E'}}; 
ROUTE C = {'C', {'p', 'r', 'l', 'r', 'd', 'l', 'r', 'l', 'b', 'E'}}; 
ROUTE D = {'D', {'p', 'r', 'l', 's', 'd', 's', 'r', 'l', 'b', 'E'}}; 
ROUTE E = {'E', {'p', 'r', 'l', 'l', 'l', 'd', 'r', 'r', 'r', 'l', 'b', 'E'}}; 
ROUTE F = {'F', {'p', 'r', 'l', 'l', 'r', 'd', 'l', 'r', 'r', 'l', 'b', 'E'}}; 
ROUTE G = {'G', {'p', 'r', 'l', 'l', 's', 'r', 'd', 'l', 's', 'r', 'r', 'l', 'b', 'E'}}; 
ROUTE H = {'H', {'p', 'r', 'l', 'l', 's', 's', 'd', 's', 's', 'r', 'r', 'l', 'b', 'E'}}; 
ROUTE I = {'I', {'p', 'l', 'd', 'r', 'b', 'E'}}; 
ROUTE Z = {'Z', {'u', 'd', 'E'}}; 

ROUTE currPath[3]; 

void PickUp(); //pick up object
void Drop(); //set down object
void Turn(float degrees, float speed); //turn 90 degrees or however much it needs
void Straight(); //literally go straight for a bit, just enought to get over the intersection
void Bust(); //full turn around, bust a 180
void freezone(uint16_t *sensorValues, float distance, int& m1speed, int& m2speed); 

bool checkIntersectionEvent(int left, int right);
void lineFollow(uint16_t *sensorValues, int& m1speed, int& m2speed, int position); 
bool obstruct(float distance, bool& obstruction);
void cruiseControl(float distance, int& m1speed, int& m2speed, float turnEff);
float giveTurnEffort(int position, float kp, float kd, float ki); 
void checkBounds(int& m1speed, int& m2speed); 
void getCurrentPath(ROUTE *currPath); 
void printPath(ROUTE *currPath); 
void takePath(ROUTE *currPath, uint16_t *sensorValues, int& m1speed, int& m2speed, int position);

void sendState(STATE robotState)
{
  Serial1.println(robotState); 
}
void setLED(bool value)
{
  digitalWrite(LED_PIN, value);
}

void handleUart()
{
  while(Serial1.available() > 0)
  {
    static char uMessage[16]; 
    static uint8_t pos = 0;
    char byte = Serial1.read(); 
    if(byte != '\n')
    {
      uMessage[pos] = byte; 
      pos++; 
    }
    else
    {
      uMessage[pos] = '\0'; 
      pos = 0; 
      uartMessage = uMessage; 
      Serial.println(uartMessage); 
    }
  }
}
void handleKeyPress(int16_t key)
{
  Serial.println("Key detected:" + String(key)); 
  if(key == ENTER_SAVE)
  {
    setLED(true); 
    robotState = FOLLOW; 
    Serial.println("Robot set to Line follow"); 
  }
  else if(key == STOP_MODE)
  {
    setLED(false); 
    robotState = READY; 
    chassis.idle(); 
    Serial.println("Robot set to READY state"); 
  }
  else if(key == REWIND)
  {
    robotState = PATH; 
    chassis.idle();
    Serial.println("Ready for path intake"); 
  }
  switch(robotState)
  {
    case PATH:
    //getCurrentPath(currPath);
      break; 
    default: 
      break; 
  }
}

void handleCurrentState(STATE robotState)
{
  if(robotState == READY)
  {
    setLED(false); 
    chassis.idle(); 
  }
  else if(robotState == PATH)
  {
    setLED(true); 
    chassis.idle(); 
    getCurrentPath(currPath); 
  }
}

Romi32U4ButtonA buttonA;
Romi32U4ButtonB buttonB;

Romi32U4Motors motors;

Chassis chassis;

bool calibration(void);

void setup()
{
  Serial.begin(115200); //A7 A2 A3 A4 A0 A11
  Serial1.begin(14400);
  chassis.init(); 
  //decoder.init(); 
  qtr.setTypeAnalog();                //right             left
  qtr.setSensorPins((const uint8_t[]){A7, A0, A4, A3, A2, A11}, SensorCount);
  //took off A11 on the right and A7 on the left
  //A7 -> 0
  motors.allowTurbo(true);
  motors.setSpeeds(0, 0);
  rangefinder.init(); 

  servo.attach(); 
  servo.setMinMaxMicroseconds(SERVO_UP, SERVO_DOWN); 
  servo.writeMicroseconds(SERVO_DOWN); 
}

void loop()
{
  handleUart();
  distance = rangefinder.getDistance(); 
  Serial.print(distance); 
  Serial.print("cm\n");  
  if (buttonA.getSingleDebouncedPress())
  {
    //Serial1.print("0\n");
    sendState(robotState);  
    Serial.println("Calibration Called");
    ledRed(1);
    calibrated = calibration();
    delay(500); 
    robotState = PATH; 
    sendState(robotState); 
  }
  else if(calibrated && robotState == PATH)
  {
    getCurrentPath(currPath); 
  }
  else if (calibrated && robotState != PATH)
  {  
    //int16_t key = decoder.getKeyCode(false);  
    //if(key >= 0) handleKeyPress(key);

    if(robotState > 0) handleCurrentState(robotState); 
    int position = qtr.readLineBlack(sensorValues);
    int m1Speed = 0; 
    int m2Speed = 0;

    // read raw sensor values
    qtr.read(sensorValues);
    for (uint8_t i = 0; i < SensorCount; i++)
    {
      Serial.print(sensorValues[i]);
      Serial.print("  ");
    }
    Serial.println();
    Serial.println();

    switch(robotState)
    {
      case FOLLOW:
        lineFollow(sensorValues, m1Speed, m2Speed, position);
        motors.setSpeeds(m1Speed, m2Speed);
        if(checkIntersectionEvent(sensorValues[0] + sensorValues[1] + sensorValues[2], sensorValues[3] + sensorValues[4] + sensorValues[5])) chassis.idle();
        break;  
      case DELIVERY:
        //printPath(currPath); 
        takePath(currPath, sensorValues, m1Speed, m2Speed, position);
        break; 
      case PICK:
        PickUp();
        break; 
      case DROP:
        Drop(); 
        break; 
      default: 
        break; 
    }  
  } 
}
void freezone(uint16_t *sensorValues, float distance, int& m1speed, int& m2speed)
{
  static int angleTurned = 0; 
  if(distance < 50 && grab == false)
  {
    Serial.println("something close"); 
    Serial.println(distance); 
    chassis.driveFor(1, 10, true); 
    if(distance < 8.0)
    {
      chassis.idle(); 
      delay(500); 
      static int i = SERVO_DOWN; 
      for(i = SERVO_DOWN; i > SERVO_UP; i--)
      {
        delay(1); 
        servo.writeMicroseconds(i); 
      }
      grab = true; 
      chassis.turnFor((265 - angleTurned), 80, true); 
    }
  } 
  else if(grab == true)
  {
    if(sensorValues[3] < 200 || sensorValues[2] < 200)
    {
      Serial.println("Must go back to line"); 
      chassis.driveFor(2, 30, true); 
      chassis.idle();
    }
    else
    {
      chassis.driveFor(5.0, 20, true); 
      chassis.turnFor(90, 60, true); 
      chassis.idle(); 
      delay(500); 
      angleTurned = 0; 
      zone = false; 
      grab = false;  
    }  
  }
  else
  {
    Serial.println(distance);
    chassis.turnFor(10, 60, true); 
    angleTurned += 10; 
    delay(250);
  }
}
void PickUp()
{ 
  delay(500); 
  static int i = SERVO_DOWN; 
  for(i = SERVO_DOWN; i > SERVO_UP; i--)
  {
    delay(1); 
    servo.writeMicroseconds(i); 
  }
  delay(500);
  Bust();  
  //grab = true; 
  robotState = DELIVERY; 
  sendState(robotState); 
}
void Drop()
{
  static int i;
  static int j;  
  if(height == true)
  {
    j = 750;
  }
  else
  {
    j = SERVO_DOWN;
  } 
  delay(500); 
  for(i = SERVO_UP; i < j; i++)
  {
    delay(2); 
    servo.writeMicroseconds(i); 
  } 
  delay(500);
  chassis.driveFor(-5.0, 20, true); 
  Bust(); 
  servo.writeMicroseconds(SERVO_DOWN);
  delay(100); 
  //grab = false; 
  robotState = DELIVERY; 
  sendState(robotState); 
}
void Bust()
{
  chassis.turnFor(180, 200, true); 
}
void Turn(float degrees, float speed)
{
  chassis.driveFor(6.2, 50, true); 
  chassis.turnFor(degrees, speed, true);
}
void Straight()
{
  chassis.driveFor(4.5, 50, true); 
}
void checkBounds(int& m1speed, int& m2speed)
{
  if(m1speed < 0)
  {
    m1speed = 0;
  }
  if(m2speed < 0)
  {
    m2speed = 0;
  }
  if(m1speed > MAX_SPEED)
  {
    m1speed = MAX_SPEED;
  }
  if(m2speed > MAX_SPEED)
  {
    m2speed = MAX_SPEED;
  }
}
bool obstruct(float distance)
{
  if(distance < 20.0)
  {
    return true; 
  }
  else
  {
    return false; 
  } 
}
void cruiseControl(float distance, int& m1speed, int& m2speed, float turnEff)
{
  height = obstruct(distance); 
  if(height == true)
  {
    m1speed = 10*(distance) + turnEff; 
    m2speed = 10*(distance) - turnEff; 
  }
}
bool checkIntersectionEvent(int left, int right)
{ 
  if ((left >= (lefthigh - sense)) && (right >= (righthigh - sense)))
  {
    return true; 
  }
  else
  {
    return false; 
  } 
}
bool calibration()
{
  for (uint16_t i = 0; i < 400; i++)
  {
    qtr.calibrate();
  }

  // print the calibration minimum values measured when emitters were on
  //Serial.begin(9600);
  for (uint8_t i = 0; i < SensorCount; i++)
  {
    Serial.print(qtr.calibrationOn.minimum[i]);
    Serial.print(' ');
  }
  Serial.println();

  // print the calibration maximum values measured when emitters were on
  for (uint8_t i = 0; i < SensorCount; i++)
  {
    Serial.print(qtr.calibrationOn.maximum[i]);
    if(i < 3)
    {
      righthigh += qtr.calibrationOn.maximum[i]; //first 3 are right side sensors
    }
    else
    {
      lefthigh += qtr.calibrationOn.maximum[i]; 
    }
    Serial.print(' ');
  }
  Serial.println("CALIBRATION COMPLETE"); 
  Serial.print("Left side high: "); 
  Serial.println(lefthigh); 
  Serial.print("Right side high: "); 
  Serial.println(righthigh);  
  ledRed(0);
  ledGreen(1); 
  robotState = READY;
  sendState(robotState); 
  //delay(3000); 
  //robotState = PATH; 
  //sendState(robotState); 
  return true;
}
float giveTurnEffort(int position, float kp, float kd, float ki)
{
  static int lastErr = 0;
  static int I = 0;
  int error = 2500 - position;
  I = I + error;
  if (I > 10000)
    I = 10000;
  float turnEffort = (kp * error) + (ki * I) + (kd * (error - lastErr));
  lastErr = error;

  return turnEffort; 
}
void lineFollow(uint16_t *sensorValues, int& m1speed, int& m2speed, int position)
{
  static float turnEff = 0; 
  static int16_t speedInc = 0; 
  static int16_t samp1 = 0; 
  static int16_t samp2 = 0; 
  static int16_t samp3 = 0; 

  turnEff = giveTurnEffort(position, kp, kd, ki); 

  if((sensorValues[2] > (sensorValues[0] + sensorValues[1])) && (sensorValues[3] > (sensorValues[4] + sensorValues[5])))
  {
    samp1++;
    if (samp1 > 2)
    {
      speedInc += 1;
      samp1 = 0;
    }
  }
  else if((sensorValues[0] + sensorValues[1]) > (sensorValues[3] + sensorValues[4] + sensorValues[5]) || (sensorValues[5] + sensorValues[4]) > (sensorValues[0]) + sensorValues[1] + sensorValues[2])
  {
    samp2++;
    if (samp2 > 4)
    {
      speedInc -= 1;
      samp2 = 0; 
      if (speedInc < 0)
      {
        speedInc = 0;
      }
    }
  }
  else
  {
    samp3++; 
    if(samp3 > 8)
    {
      speedInc -= 2;
      samp3 = 0;  
      if(speedInc < 0)
      {
        speedInc = 0; 
      }
    }
  } 
  m1speed = (BASE_SPEED + speedInc) + turnEff;
  m2speed = (BASE_SPEED + speedInc) - turnEff;

  cruiseControl(distance, m1speed, m2speed, turnEff);
  checkBounds(m1speed, m2speed); 
}
void getCurrentPath(ROUTE *currPath)
{
  //delay(700); 
  static uint8_t cursor = 0; 
  if(uartMessage != "")
  {
    Serial.println(uartMessage); 
    int amountRoutes = (uartMessage.charAt(0)) - 48; 
    amountR = amountRoutes; 
    Serial.println(amountRoutes); 
    if(cursor + 1 <= amountRoutes)
    {
      if(uartMessage.charAt(cursor + 1) == 'a')
      {
        currPath[cursor] = A;
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'b')
      {
        currPath[cursor] = B; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'c')
      {
        currPath[cursor] = C; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'd')
      {
        currPath[cursor] = D; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'e')
      {
        currPath[cursor] = E; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'f')
      {
        currPath[cursor] = F; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'g')
      {
        currPath[cursor] = G; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'h')
      {
        currPath[cursor] = H; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'i')
      {
        currPath[cursor] = I; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 't')
      {
        currPath[cursor] = testRun; 
        cursor++; 
      }
      else if(uartMessage.charAt(cursor + 1) == 'z')
      {
        currPath[cursor] = Z; 
        cursor++; 
      }
      else
      {
        Serial.println("Error"); 
        cursor++; 
      }
    }
    else
    {
      cursor = 0; 
      uartMessage = ""; 
      Serial.println("Path received"); 
      //printPath(currPath); 
      robotState = DELIVERY; 
      sendState(robotState); 
    }
  }
} 
void printPath(ROUTE *currPath)
{
  Serial.println("Now printing path!"); 
  uint8_t i = 0; 
  uint8_t j = 0;
  for(i = 0; i < amountR; i++)
  {
    Serial.println(currPath[i].name);
    for(j = 0; j < 15; j++)
    {
      if(currPath[i].direct[j] == 'E')
      {
        break; 
      }
      else
      {
        Serial.println(currPath[i].direct[j]);
      }
    }
  } 
}
void takePath(ROUTE *currPath, uint16_t *sensorValues, int& m1speed, int& m2speed, int position)
{
  static int route = 0; 
  static int dir = 0; 
  if(route < amountR)
  {
    if(zone == true)
    {
      freezone(sensorValues, distance, m1speed, m2speed); 
    }
    else
    {
      Serial.println("Doing LINE FOLLOWING"); 
      lineFollow(sensorValues, m1speed, m2speed, position);
      motors.setSpeeds(m1speed, m2speed);
      if(checkIntersectionEvent(sensorValues[3] + sensorValues[4] + sensorValues[5], sensorValues[0] + sensorValues[1] + sensorValues[2]))
      {
        Serial.println("Intersection detected"); 
        chassis.idle();
        if(dir < 30)
        {
          if(currPath[route].direct[dir] == 'E')
          {
            dir = 0; 
            route++;
            Bust();
            delay(2000); 
            Serial.println("Doing next ROUTE");
          }
          else
          {
            Serial.println("Doing motion"); 
            switch(currPath[route].direct[dir])
            {
              case 'l':
                Turn(90, 150);
                Serial.println("Turning left"); 
                break; 
              case 'r': 
                Turn(-90, 150);  
                Serial.println("Turning right"); 
                break; 
              case 's':
                Straight(); 
                Serial.println("Going stright"); 
                break; 
              case 'b':
                Bust(); 
                Serial.println("Busting"); 
                break; 
              case 'p':
                robotState = PICK; 
                sendState(robotState); 
                Serial.println("Lifting"); 
                break; 
              case 'd':
                robotState = DROP; 
                sendState(robotState); 
                Serial.println("Delivering"); 
                break; 
              case 'u': 
                chassis.driveFor(-10.0, 20, true);
                chassis.turnFor(90, 100, true); 
                chassis.driveFor(50, 30, true); 
                chassis.turnFor(-90, 100, true); 
                chassis.idle(); 
                zone = true; 
                Serial.println("Freezone"); 
              default:
                break; 
            }
            dir++; 
            Serial.println("Next direction"); 
          }
        } 
      }
    }
  }
  else
  {
    chassis.idle(); 
    Serial.println("Paths are complete"); 
    route = 0; 
    robotState = PATH; 
    sendState(robotState); 
  }
}




