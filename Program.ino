// Include the Arduino Stepper Library
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Stepper.h>
#include <RTC.h>
#include <EEPROM.h>
#include <avr/wdt.h>

class View;
class Controller;
class Model;

unsigned long millisButtonLastPush = 0;
unsigned long lcdLastUpdate = 0;

const unsigned long millisButtonPushInterval = 1000;
const unsigned long lcdUpdateInterval = 1000;

const byte button1 = 2;
const byte button2 = 3;
const byte buttonMenuLeft = 4;
const byte buttonMenuRight = 5;

// Number of steps per output rotation
const int stepsPerRevolution = 200;

// Create Instance of Stepper library
Stepper myStepper{stepsPerRevolution, 8, 9, 10, 11};

class Model
{
  private:

  byte previousSecond = 0;
  
  unsigned secondsTillNextRotation;

  bool feedingSkipped = false;

  //Settings and statistics
  
  const byte rotationsAddress = 0;
  byte totalRotations;

  const byte outagesAddress = 1;
  byte totalOutages;

  const byte unitsAddress = 2; 
  byte unitsToWait;

  const byte feedMinInterval = 1;
  const byte feedMaxInterval = 12;
  
  void setTotalRotations(const byte value)
  {
    totalRotations = value;
    EEPROM.write(rotationsAddress, totalRotations); 
  }

  void setTotalOutages(const byte value)
  {
    totalOutages = value;
    EEPROM.write(outagesAddress, totalOutages); 
  }

  public:

  unsigned getSecondsTillNextRotation()
  {
    return secondsTillNextRotation;
  }

  void resetStatistics()
  {
    setTotalRotations(0);
    setTotalOutages(0);  
  }
  
  bool timeHasPassed()
  {
    return secondsTillNextRotation <= 0;
  }

  void initialize()
  {
    totalRotations = EEPROM.read(rotationsAddress);  
    totalOutages = EEPROM.read(outagesAddress);
    unitsToWait = EEPROM.read(unitsAddress);

    secondsTillNextRotation = unitsToWait * 60;
  }

  void increaseUnitsToWait()
  {
    if (unitsToWait < feedMaxInterval)
    {
      ++unitsToWait;  
      EEPROM.write(unitsAddress, unitsToWait);
    }
  }

  void decreaseUnitsToWait()
  {
    if (unitsToWait > feedMinInterval)
    {
      --unitsToWait;
      EEPROM.write(unitsAddress, unitsToWait);
    }
  }

  byte getPreviousSecond()
  {
    return previousSecond;  
  }

  void setPreviousSecond(const byte value)
  {
    previousSecond = value;  
  }

  byte getTotalRotations()
  {
    return totalRotations;  
  }

  byte getTotalOutages()
  {
    return totalOutages;  
  }

  void registerPowerOutage()
  {
    setTotalOutages(totalOutages + 1);  
  }

  void updateRotationStatistics()
  {
      ++totalRotations;
      setTotalRotations(totalRotations);
  }

  void setFeedingSkipped(const bool value)
  {
    feedingSkipped = value;
  }

  bool getFeedingSkipped()
  {
    return feedingSkipped;  
  }

  byte getUnitsToWait()
  {
     return unitsToWait;  
  }

  void decreaseCounter()
  {
    --secondsTillNextRotation;  
  }

  void resetTime()
  {

    secondsTillNextRotation = unitsToWait * 60;  
  }

  
};

class Controller
{
private:

  bool powerState;

  PCF8563 RTC;

  View* view;
  Model* model;
  const int alanogPin = A0;

public:

   void initialize()
   {
      powerState = powerIsOn();
      RTC.begin();
   } 

   void setView(View* view)
   {
      this->view = view; 
   }

   void setModel(Model* model)
   {
      this->model = model; 
   }

  bool getPowerState()
  {
    return powerState;
  }

  void setPowerState(const bool state)
  {
    this->powerState = state;  
  }

  byte getCurrentMinutes()
  {
    return RTC.getMinutes(); 
  }

  byte getCurrentSeconds()
  {
    return RTC.getSeconds();  
  }
 
   bool powerIsOn()
   {
      return analogRead(alanogPin)*(5.0 / 1023.0) >= 4;  
   }

   byte getFeedingInterval()
   {
      return model->getUnitsToWait(); 
   }

   byte getUnitsToWait()
   {
      return model->getUnitsToWait(); 
   }

   byte getSecondsTillNextRotation()
   {
      return model->getSecondsTillNextRotation(); 
   }

   byte getTotalRotations()
   {
      return model->getTotalRotations();
   }

   byte getTotalOutages()
   {
      return model->getTotalOutages(); 
   }

   bool getFeedingSkipped()
   {
      return model->getFeedingSkipped(); 
   }
};

class View
{
private:

  byte feedingIntervalDisplayed = -1;
  bool powerSupplyDisplayed;
  bool feedingSkippedDisplayed;
  byte rotationsDisplayed = -1;
  byte outagesDisplayed = -1;
  
  Controller* controller;

  const byte rowLength = 20;

  static const byte rowsNumber = 4;
  static const byte numberOfScreens = 4;

  // Define LCD pinout
  const byte  en = 2, rw = 1, rs = 0, d4 = 4, d5 = 5, d6 = 6, d7 = 7, bl = 3;
 
  // Define I2C Address
  const byte i2c_addr = 0x27;

  LiquidCrystal_I2C lcd {i2c_addr, en, rw, rs, d4, d5, d6, d7, bl, POSITIVE};
  
  byte currentMenu = 0;
  
  String strings[numberOfScreens][rowsNumber]  = {{"      Settings      ", "Feeding interval:", "1) Increase", "2) Decrease"},
                                          {"Feeding in: ", "Power supply: ", "1) Feed now", "2) Reset the clock"},
                                          {"   Bowl position   ", "Press to rotate", "1) Clockwise", "2) Counterclockwise"},
                                          {"     Statistics     ", "Rotations today: ", "Power outages: ", ""}};

  void printSettings()
  {
    this->setRow(0);
    lcd.print(strings[0][0]);
    this->setRow(1);
    lcd.print(strings[0][1]);
    lcd.print(controller->getUnitsToWait());
    this->setRow(2);
    lcd.print(strings[0][2]);
    this->setRow(3);
    lcd.print(strings[0][3]);
  }

  void printNextFeeding()
  {
    updateNextFeeding();
    this->setRow(1);
    lcd.print(strings[1][1]);
    lcd.print(powerStateToString(controller->powerIsOn()));
    this->setRow(2);
    lcd.print(strings[1][2]);
    this->setRow(3);
    lcd.print(strings[1][3]);
  }

  void printManual()
  {
    for (size_t i = 0; i < rowsNumber; ++i)
    {
      this->setRow(i);
      lcd.print(strings[2][i]);  
    }
  }

  void printStatistics()
  {
      this->setRow(0);
      lcd.print(strings[3][0]);
      this->setRow(1);
      lcd.print(strings[3][1]);
      lcd.print(controller->getTotalRotations());
      this->setRow(2);
      lcd.print(strings[3][2]);
      lcd.print(controller->getTotalOutages());
      this->setRow(3);
      lcd.print(strings[3][3]);
  }

  void printSeconds(const byte seconds)
  { 
    if (seconds < 10)
    {
      this->print("0");
    }
    this->print(seconds);
  }

public:

  void setController(Controller* controller)
  {
      this->controller = controller;  
  }

  void initialize()
  {
    powerSupplyDisplayed = controller->powerIsOn();
    lcd.begin(rowLength, rowsNumber);
    lcd.setCursor(0, 1);
    lcd.print("     Feeder 1.0     ");
    delay(2000);
    lcd.clear();
    this->printCurrentScreen();
  }

  void setRow(const byte row)
  {
    lcd.setCursor(0, row);  
  }

  void clearView()
  {
    lcd.clear();
  }

  void clearRow(const byte rowNumber)
  {
    this->setRow(rowNumber);
    for (size_t i = 0; i < rowLength; ++i)
    {
      this->print(' ');  
    }
    this->setRow(rowNumber);
  }

  String powerStateToString(const bool state)
  {
    if (state)
    {
      return "on";  
    }  
    return "off";
  }

  void print(unsigned n)
  {
    lcd.print(n);  
  }

  void print(const char c)
  {
    lcd.print(c);  
  }

  void print(const String s)
  {
    lcd.print(s);  
  }

  void print(const byte b)
  {
    lcd.print(b);  
  }

  void setNextScreen()
  {
    currentMenu = ++currentMenu % numberOfScreens;
  }

  void setPreviousScreen()
  {
    currentMenu = --currentMenu % numberOfScreens;
  }

  byte getCurrentMenu()
  {
    return currentMenu;
  }
  
  void printCurrentScreen()
  {
      switch(currentMenu)
      {
        case 0:  
          printSettings();
          break;
        case 1:
          printNextFeeding();
          break;
        case 2:
          printManual();
          break;
        case 3:
          printStatistics();
          break;
      }
  } 

  void updateSettings()
  {
    byte currentInterval = controller->getFeedingInterval();
    if (currentInterval != feedingIntervalDisplayed)
    {
       this->clearRow(1);
       this->print(strings[0][1]);
       this->print(currentInterval);
       feedingIntervalDisplayed = currentInterval;
    }
  }

  void updateNextFeeding()
  {
    bool feedingSkipped = controller->getFeedingSkipped();
    if (feedingSkippedDisplayed != feedingSkipped)
    {
      this->clearRow(0);
      if (feedingSkipped)
      {
        this->print("Waiting for power!"); 
      }
      feedingSkippedDisplayed = feedingSkipped;
    }
    else if(!feedingSkipped)
    {
      this->setRow(0);
      this->print(strings[1][0]);
      unsigned seconds = controller->getSecondsTillNextRotation();
      this->print(seconds / 60);
      this->print(':');
      this->printSeconds(seconds % 60);
    }
    bool powerIsOn = controller->powerIsOn();
    if (powerIsOn != powerSupplyDisplayed)
    {
        this->clearRow(1);
        lcd.print(strings[1][1]);
        lcd.print(powerStateToString(powerIsOn));
        powerSupplyDisplayed = powerIsOn;
    }
  }

  void updateStatistics()
  {
    byte currentRotations = controller->getTotalRotations();
    if (currentRotations != rotationsDisplayed)
    {
      this->clearRow(1);
      
      this->print(strings[3][1]);
      this->print(currentRotations);
      rotationsDisplayed = currentRotations;
    }
    byte currentOutages = controller->getTotalOutages();
    if (currentOutages != outagesDisplayed)
    {
      this->clearRow(2);
      this->print(strings[3][2]);
      this->print(currentOutages);
      outagesDisplayed = currentOutages;
    }
  }

  void updateCurrentScreen()
  {
       switch(currentMenu)
       {
        case 0:  
          updateSettings();
          break;
        case 1:
          updateNextFeeding();
          break;
        case 2:
          break;
        case 3:
          updateStatistics();
          break;
       }
  }
};

View view;
Controller controller;
Model model;

void setup()
{
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(buttonMenuLeft, INPUT_PULLUP);
  pinMode(buttonMenuRight, INPUT_PULLUP);
  pinMode(7, INPUT);
  
  controller.setModel(&model);
  controller.setView(&view);
  view.setController(&controller);
  view.initialize();
  model.initialize(); 
  // set the speed at 60 rpm:
  myStepper.setSpeed(120);
  wdt_enable(WDTO_4S);
}

void processMainMenuInput()
{
    if (digitalRead(button1) == LOW)
    {
        model.increaseUnitsToWait();
    }
    else if (digitalRead(button2) == LOW)
    {
        model.decreaseUnitsToWait();
    }
}

void processNextFeedingInput()
{
    if (digitalRead(button1) == LOW)
    {
       rotate();
    } else if (digitalRead(button2) == LOW)
    {
       model.resetTime();
    }
}

void processManualRotationInput()
{
    if (digitalRead(button1) == LOW)
    {
        rotateClockwise(15);
    }
    else if (digitalRead(button2) == LOW)
    {
        rotateCounterclockwise(15);
    }
}

void processStatisticsInput()
{
  if (digitalRead(button1) == LOW)
    {
        model.resetStatistics();
    }
}

void processButtonInput()
{
    unsigned long current = millis();
    if (current - millisButtonLastPush >= millisButtonPushInterval)
    {
      millisButtonLastPush = current;
      if (digitalRead(buttonMenuLeft) == LOW)
      {
        view.setPreviousScreen();
        view.clearView();
        view.printCurrentScreen();
      }
      else if (digitalRead(buttonMenuRight) == LOW)
      {
        view.setNextScreen();  
        view.clearView();
        view.printCurrentScreen();
      }
      else {
        switch(view.getCurrentMenu())
        {
          case 0:
              processMainMenuInput();
              break;
          case 1:
              processNextFeedingInput();
              break;
          case 2:
              processManualRotationInput();
              break;
          case 3:
              processStatisticsInput();
              break;
        }
      }
    }
}

   
  void rotateClockwise(byte steps)
  {
    
    myStepper.step(steps);
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    digitalWrite(11, LOW); 
    
  }

  void rotateCounterclockwise(byte steps)
  {
    
    myStepper.step(-steps);
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    digitalWrite(11, LOW); 
    
  }
  void rotate()
  {
    myStepper.step(stepsPerRevolution / 2);
    
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    digitalWrite(11, LOW);
    model.updateRotationStatistics();
  }

void loop() 
{
  bool currentPowerState = controller.powerIsOn();
  bool previousPowerState = controller.getPowerState();
  if (previousPowerState != currentPowerState)
  {
      controller.setPowerState(currentPowerState);
      if (previousPowerState)
      {
        model.registerPowerOutage();
      }
  }
    
  if (model.getFeedingSkipped() && controller.powerIsOn())
  {
    rotate();
    model.resetTime();
    model.setFeedingSkipped(false);  
  }

  byte seconds = controller.getCurrentSeconds();
  if (model.getPreviousSecond() != seconds)
  {
    model.setPreviousSecond(seconds);
    model.decreaseCounter();
    if (model.timeHasPassed())
    {
      if (controller.powerIsOn())
      {
        rotate();
        model.resetTime();
      }
      else
      {
        model.setFeedingSkipped(true);
      }
    } 
  }
  view.updateCurrentScreen();
  processButtonInput();
  wdt_reset();
}
