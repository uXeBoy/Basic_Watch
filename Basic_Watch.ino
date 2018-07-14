#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"

#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349

#define OLED_DC     4
#define OLED_CS     12
#define OLED_RESET  6

#define SSD1306_LCDHEIGHT 64

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

Adafruit_SSD1306 display(OLED_DC, OLED_RESET, OLED_CS);
RTC_DS3231 rtc;

// A variable for the padded time
char timeBuffer[6];
char secondBuffer[6];

// Pin definitions
#define Button1 8
#define Button2 A0
#define Button3 A3
#define buzzer 5

unsigned long currentMillis;
unsigned long previousMillis = 0;
uint8_t previousSecond, deciSecond;

// Tracking watch state
// I plan to use an integer to track what state the watch is in
// this should make it more effecient so we can check the state
// before running routines that do not need to be run.
// It can also be used to see what menu or item the watch is currently on.
// Initially these will be as follows.
// 0 = Watchface
// 1 = Menu 1
// 2 = MM option 1
// 3 = MM option 2
// etc
int MenuOption = 1;
int Menu = 0;
int MaxMenu;
int MinMenu;

// Watch state
// 0 = Screen off
// 1 = Screen on
// 3 = Sleep?
int WatchState = 1;

// time to limit multiple presses.
int debounce = 300;

// Variables to store manual date and time values
int year;
int month;
int day;
int hour;
int minute;
int second;

// Button press tones 
int KeyPressTone = 15;  // enabled
const int KeyPressToneTime = 15;  // short chirp 10 ms
// Hourly beeps
int HourlyTone = 0;  // disabled
const int HourlyToneTime = 100;  // 100 ms

void setup()
{
  // Set up the buttons as inputs
  pinMode(Button1, INPUT_PULLUP);
  pinMode(Button2, INPUT_PULLUP);
  pinMode(Button3, INPUT_PULLUP);

  // Arduboy's 2nd speaker pin
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  if (KeyPressTone > 0)
  {
    tone(buzzer,1000,KeyPressTone);
  }

  rtc.begin();

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
  // init done

  // Clear the buffer.
  display.clearDisplay();

  // Set the watch time (when uploading sketch)
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //rtc.adjust(DateTime(2018,07,02,23,23,0));

  SetDateTimeVar();

  Watchface();
}

void(* resetFunc) (void) = 0; // declare reset function at address 0

// The main loop Primarily used to monitor button presses and exceute actions.
void loop()
{
  if (HourlyTone > 0) // if hourly tones are enabled, check the time
  {
    DateTime now = rtc.now();

    // Double beep on the hour <-- todo make this occur once so we don't need the delay on the end
    if (now.minute() == 00 && now.second() == 00)
    {
      tone(buzzer, 2000, HourlyTone);
      delay(300);
      tone(buzzer, 2000, HourlyTone);
      delay(800);
      noTone(buzzer);
    }
    // Single beep on the half hour <-- todo make this occur once so we don't need the delay on the end
    if (now.minute() == 30 && now.second() == 00)
    {
      tone(buzzer, 2000, HourlyTone);
      delay(1200);
      noTone(buzzer);
    }
  }

  // manage button presses.
  if (digitalRead(Button1) == 0) // B Button
  {
    if (KeyPressTone > 0)
    {
      tone(buzzer, NOTE_C7, KeyPressTone);
    }
    if (digitalRead(Button3) == 0)
    {
      resetFunc();  // hold buttons 1 & 3 to reset watch
    }
    if (WatchState == 0)
    {
      Watchface();
    }
    else if (WatchState == 1 && Menu == 0)
    {
      MainMenu();
    }
    else if (WatchState == 1 && Menu > 0)
    {
      ExecuteAction(MenuOption);
    }

    delay(debounce);
  }
  
  if (digitalRead(Button2) == 0) // UP 
  {
    if (KeyPressTone > 0)
    {
      tone(buzzer, NOTE_D7, KeyPressTone);
    }
    if (WatchState == 0)
    {
      if (Menu == 0)
      {
        Watchface();
      }
    }
    else if (Menu > 0)
    {
      if (MenuOption > MinMenu)
      {
        MenuOption --;
      }
      else
      {
        MenuOption = MaxMenu;  // Wrap
      }
      // not needed?
      if (Menu == 1)
      {
        MainMenu();
      }
      else if (Menu == 2)
      {
        SetTimeMenu();
      }
      else if (Menu == 3)
      {
        TonesMenu();
      }
    }

    delay(debounce);
  } // button 2

  if (digitalRead(Button3) == 0) // DOWN
  {
    if (KeyPressTone > 0)
    {
      tone(buzzer, NOTE_B6, KeyPressTone);
    }
    if (WatchState == 0)
    {
      if (Menu == 0)
      {
        Watchface();
      }
    }
    else if (Menu > 0)
    {
      if (MenuOption < MaxMenu)
      {
        MenuOption ++;
      }
      else
      {
        MenuOption = 1;  // wrap around
      }
      if (Menu == 1)
      {
        MainMenu();
      }
      else if (Menu == 2)
      {
        SetTimeMenu();
      }
      else if (Menu == 3)
      {
        TonesMenu();
      }
    }

    delay(debounce);
  } // button 3

  if (Menu == 0)
  {
    Watchface();
  }
}

// This function displays the main watchface
// Todo seperate non watchface calcs into a seperate function eg. battery calcs
void Watchface()
{
  // Screen stuff
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(WHITE);

  DateTime now = rtc.now();
  currentMillis = millis();
  
  if (previousSecond != now.second())
  {
    deciSecond = 0;
    previousSecond = now.second();
  }
  else if (currentMillis > (previousMillis + 100) && deciSecond < 9)
  {
    deciSecond++;
    previousMillis = currentMillis;
  }
  
  sprintf(secondBuffer, "%02u.%u", now.second(), deciSecond);

  if (now.second() % 2) sprintf(timeBuffer, "%02u:%02u", now.hour(), now.minute());
  else                  sprintf(timeBuffer, "%02u %02u", now.hour(), now.minute());

  // Show Time
  display.setCursor(4, 15);
  display.print(timeBuffer);  
  display.setTextSize(1);
  display.setCursor(50, 55);
  display.print(secondBuffer);

  // Show date
  display.setCursor(40, 1);
  display.print(now.day(), DEC);
  display.print("-");
  display.print(now.month(), DEC);
  display.print("-");
  display.print(now.year(), DEC);
  display.display();
}

// This function displays the main menu
void MainMenu()
{
  Menu = 1;
  MaxMenu = 3;
  MinMenu = 1;

  display.clearDisplay();
  display.setTextSize(1);
  // display.setCursor(0, 0);
  // display.print("sel");
  display.setCursor(40, 0);
  display.print("Main Menu");
  display.setCursor(5, 12);
  display.print("Exit");
  display.setCursor(5, 22);
  display.print("Set Time");
  display.setCursor(5, 32);
  display.print("Tones");
  // display.setCursor(115, 0);
  // display.print("up");
  // display.setCursor(115, 57);
  // display.print("dn");

  if (MenuOption == 1)
  {
    display.setCursor(0, 12);
    display.print(">");
  }
  if (MenuOption == 2)
  {
    display.setCursor(0, 22);
    display.print(">");
  }
  if (MenuOption == 3)
  {
    display.setCursor(0, 32);
    display.print(">");
  }

  display.display();
  delay(debounce);
}

// This function is the exit menu option
void Exit()
{
  Watchface();
  Menu = 0;
}

// Execute a selected menu option
void ExecuteAction(int option)
{
  // Menu 1 Actions
  if (Menu == 1)
  {
    if (option == 1)
    {
      Exit();
    }
    else if (option == 2)
    {
      MenuOption = 1;
      SetTimeMenu();
    }
    else if (option == 3)
    {
      MenuOption = 1;
      TonesMenu();
    }
  }

  // Menu 2 Actions
  else if (Menu == 2)
  {
    if (option == 1)
    {
      year++;

      if (year > 2020)
      {
        year = 2018;
      }

      SetTimeMenu();
    }
    else if (option == 2)
    {
      month++;

      if (month > 12)
      {
        month = 1;
      }

      SetTimeMenu();
    }
    else if (option == 3)
    {
      day++;

      // this needs to be smarter as some months have less days
      if (day > 31)
      {
        day = 1;
      }

      SetTimeMenu();
    }
    else if (option == 4)
    {
      hour++;

      if (hour > 23)
      {
        hour = 0;
      }

      SetTimeMenu();
    }
    else if (option == 5)
    {
      minute++;

      if (minute > 59)
      {
        minute = 0;
      }

      SetTimeMenu();
    }
    else if (option == 6)
    {
      SetTime();
      MenuOption = 1;
      MainMenu();
    }
    else if (option == 7)
    {
      MenuOption = 1;
      SetDateTimeVar();
      MainMenu();
    }
  }

  // Menu 3 actions
  else if (Menu == 3)
  {
    if (option == 1)
    {
      MenuOption = 1;
      MainMenu();
    }
    else if (option == 2)
    {
      if (KeyPressTone == 0)
      {
        KeyPressTone = KeyPressToneTime;  // enable key press tones
      }
      else
      {
        KeyPressTone = 0;  // off
      }

      TonesMenu();
    }
    else if (option == 3)
    {
      if (HourlyTone == 0)
      {
        HourlyTone = HourlyToneTime;  // enable hourly tones
        tone(buzzer,2000,100);
      }
      else
      {
        HourlyTone = 0;
      }

      TonesMenu();
    }    
  }
}

// This function displays the menu for setting the time and date
void SetTimeMenu()
{
  MaxMenu = 7;
  MinMenu = 1;
  Menu = 2;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(40, 0);
  display.print("Set Time");
  // display.setCursor(0, 0);
  // display.print("sel");
  // display.setCursor(115, 0);
  // display.print("up");
  // display.setCursor(115, 57);
  // display.print("dn");

  display.setCursor(5, 10);
  display.print("Year  :");
  display.setCursor(50, 10);
  display.print(year);
  display.setCursor(5, 18);
  display.print("Month :");
  display.setCursor(50, 18);
  display.print(month);
  display.setCursor(5, 26);
  display.print("Day   :");
  display.setCursor(50, 26);
  display.print(day);
  display.setCursor(5, 34);
  display.print("Hour  :");
  display.setCursor(50, 34);
  display.print(hour);
  display.setCursor(5, 42);
  display.print("Minute:");
  display.setCursor(50, 42);
  display.print(minute);
  display.setCursor(5, 50);
  display.print("Save");
  display.setCursor(80, 50);
  display.print("Cancel");

  if (MenuOption == 1)
  {
    display.setCursor(0, 10);
    display.print(">");
  }
  else if (MenuOption == 2)
  {
    display.setCursor(0, 18);
    display.print(">");
  }
  else if (MenuOption == 3)
  {
    display.setCursor(0, 26);
    display.print(">");
  }
  else if (MenuOption == 4)
  {
    display.setCursor(0, 34);
    display.print(">");
  }
  else if (MenuOption == 5)
  {
    display.setCursor(0, 42);
    display.print(">");
  }
  else if (MenuOption == 6)
  {
    display.setCursor(0, 50);
    display.print(">");
  }
  else if (MenuOption == 7)
  {
    display.setCursor(75, 50);
    display.print(">");
  }

  display.display();
}

// This function actually sets the time
void SetTime()
{
  rtc.adjust(DateTime(year, month, day, hour, minute, 0));
}

// This fucntion prepares some variables to display when setting the time
void SetDateTimeVar()
{
  DateTime noW = rtc.now();
  year = noW.year();
  month = noW.month();
  day = noW.day();
  hour = noW.hour();
  minute = noW.minute();
  second = noW.second();
}

// Menu for setting the tones
void TonesMenu()
{
  MaxMenu = 3;
  MinMenu = 1;
  Menu = 3;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(28, 0);
  display.print("Tone Settings");
  // display.setCursor(0, 0);
  // display.print("sel");
  // display.setCursor(115, 0);
  // display.print("up");
  // display.setCursor(115, 57);
  // display.print("dn");
 
  display.setCursor(5, 10);
  display.print("Exit");
  
  display.setCursor(5, 18);
  display.print("Key Tones   :");
  display.setCursor(85, 18);
  if (KeyPressTone > 0)
  {
    display.print("On");
  }
  else
  {
    display.print("Off");
  }

  display.setCursor(5, 26);
  display.print("Hourly Tones:");
  display.setCursor(85, 26);
  
  if (HourlyTone > 0)
  {
    display.print("On");
  }
  else
  {
    display.print("Off");
  }

  if (MenuOption == 1)
  {
    display.setCursor(0, 10);
    display.print(">");
  }
  else if (MenuOption == 2)
  {
    display.setCursor(0, 18);
    display.print(">");
  }
    else if (MenuOption == 3)
  {
    display.setCursor(0, 26);
    display.print(">");
  }

  display.display();
}
