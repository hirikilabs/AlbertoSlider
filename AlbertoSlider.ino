#include <EEPROM.h>
#include <LiquidCrystal.h>


// defines
#define DEBUG       1

#define CABLE_SHOOT 1   // select one of these, camera activated by cable
#undef CANON_IR         // or Canon IR (940nm IR LED)
#undef NIKON_IR         // or Nikon IR

#define SHUTTER_PULSE_TIME  500

#define ENDSTOP_PIN 11
#define BUTTONS_PIN A0
#define CAM_PIN     12
#define STEP_PIN    2
#define DIR_PIN     3

#define EEPROM_INIT 0
#define EEPROM_DIST 1
#define EEPROM_EXP  3
#define EEPROM_TIME 5
#define EEPROM_PICS 7

#define BTN_RIGHT   0
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_LEFT    3
#define BTN_SELECT  4
#define BTN_NONE    5

#define DIR_FWD     1
#define DIR_BKW     0

#define MAX_DISTANCE  900     // mm
#define MIN_DISTANCE  10      // mm
#define MAX_TIME      5*3600  // seg
#define MIN_TIME      10      // seg
#define MAX_EXP_TIME  30      // seg
#define MIN_EXP_TIME  1 // the exposure time is controlled by the camera, but
// we need to stop, take the picture and continue,
// so 1s min pause
#define MIN_PICS      10
#define MAX_PICS      1000

#define MOTOR_STEPS   200
#define MICROSTEPING  8
#define STEPS_REV     MOTOR_STEPS * MICROSTEPING
#define STEPS_MM      STEPS_REV / 20 / 2          // 20 teeth pulley, GT2


// objects
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// variables
uint16_t distance_mm = 900;
uint16_t time_s = 300;
uint16_t num_pics = 100;
uint16_t exp_time_s = 10;
uint16_t buttons = BTN_NONE;
int32_t moving_time;
uint32_t time_between_pictures, steps_between_pictures;
uint32_t step_mtime;

int read_LCD_buttons()
{
  // read the value from the sensor
  int adc_key_in = analogRead(BUTTONS_PIN);

  if (adc_key_in > 1000) return BTN_NONE;
  if (adc_key_in < 50)   return BTN_RIGHT;
  if (adc_key_in < 250)  return BTN_UP;
  if (adc_key_in < 450)  return BTN_DOWN;
  if (adc_key_in < 650)  return BTN_LEFT;
  if (adc_key_in < 850)  return BTN_SELECT;

  // default
  return BTN_NONE;
}

void stepper_step(uint16_t delay_ms) {
  digitalWrite(STEP_PIN, HIGH);
  delay(delay_ms);
  digitalWrite(STEP_PIN, LOW);
}
void stepper_direction(int dir) {
  digitalWrite(DIR_PIN, dir);
}


void take_picture() {
#ifdef CABLE_SHOOT
  digitalWrite(CAM_PIN, HIGH);
  delay(SHUTTER_PULSE_TIME);
  digitalWrite(CAM_PIN, LOW);
#elif defined(CANON_IR)
  tone(CAM_PIN, 33000);
  delayMicroseconds(220);
  noTone(CAM_PIN);
  delayMicroseconds(7000);
  tone(CAM_PIN, 33000);
  delayMicroseconds(220);
  noTone(CAM_PIN);
#elif defined(NIKON_IR)
  int i;
  for (i = 0; i < 76; i++) {
    digitalWrite(CAM_PIN, HIGH);
    delayMicroseconds(7);
    digitalWrite(CAM_PIN, LOW);
    delayMicroseconds(7);
  }
  delay(27);
  delayMicroseconds(810);
  for (i = 0; i < 16; i++) {
    digitalWrite(CAM_PIN, HIGH);
    delayMicroseconds(7);
    digitalWrite(CAM_PIN, LOW);
    delayMicroseconds(7);
  }
  delayMicroseconds(1540);
  for (i = 0; i < 16; i++) {
    digitalWrite(CAM_PIN, HIGH);
    delayMicroseconds(7);
    digitalWrite(CAM_PIN, LOW);
    delayMicroseconds(7);
  }
  delayMicroseconds(3545);
  for (i = 0; i < 16; i++) {
    digitalWrite(CAM_PIN, HIGH);
    delayMicroseconds(7);
    digitalWrite(CAM_PIN, LOW);
    delayMicroseconds(7);
  }
#endif

}

void setup() {
  // GPIO
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(CAM_PIN, OUTPUT);
  pinMode(ENDSTOP_PIN, INPUT);
  digitalWrite(ENDSTOP_PIN, HIGH); // pullup


  // begin serial and LCD
  Serial.begin(115200);
  lcd.begin(16, 2);

  // try to get values from eeprom
  if (EEPROM.read(EEPROM_INIT) == 0xA5) {
    EEPROM.get(EEPROM_DIST, distance_mm);
    EEPROM.get(EEPROM_EXP, exp_time_s);
    EEPROM.get(EEPROM_TIME, time_s);
    EEPROM.get(EEPROM_PICS, num_pics);
  }

  // init screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    WELCOME!    ");
  delay(1000);
  lcd.setCursor(0, 0);
  lcd.print("    MEDIALAB    ");
  lcd.setCursor(0, 1);
  lcd.print(" Camera  SLIDER ");
  delay(1000);
  lcd.clear();

  // homing
  lcd.setCursor(0, 0);
  lcd.print("Homing...");
  stepper_direction(DIR_BKW);
  while (digitalRead(ENDSTOP_PIN)) {
    stepper_step(2);
  }

  // get initial values
  // distance
  lcd.setCursor(0, 0);
  lcd.print("Distance:");
  lcd.setCursor(0, 1);
  lcd.print(distance_mm);
  lcd.print("mm");
  do {
    buttons = read_LCD_buttons();
    if (buttons == BTN_UP) {
      distance_mm += 10;
      if (distance_mm > MAX_DISTANCE)
        distance_mm = MAX_DISTANCE;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("          ");
      lcd.setCursor(0, 1);
      lcd.print(distance_mm);
      lcd.print("mm");
    }
    if (buttons == BTN_DOWN) {
      distance_mm -= 10;
      if (distance_mm < MIN_DISTANCE)
        distance_mm = MIN_DISTANCE;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("          ");
      lcd.setCursor(0, 1);
      lcd.print(distance_mm);
      lcd.print("mm");
    }
  } while (buttons != BTN_SELECT);
  delay(250);
  lcd.clear();

  // exp time
  lcd.setCursor(0, 0);
  lcd.print("EXP time:");
  lcd.setCursor(0, 1);
  lcd.print(exp_time_s);
  lcd.print(" s");
  do {
    buttons = read_LCD_buttons();
    if (buttons == BTN_UP) {
      exp_time_s += 1;
      if (exp_time_s > MAX_EXP_TIME)
        exp_time_s = MAX_EXP_TIME;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("          ");
      lcd.setCursor(0, 1);
      lcd.print(exp_time_s);
      lcd.print(" s");
    }
    if (buttons == BTN_DOWN) {
      exp_time_s -= 1;
      if (exp_time_s < MIN_EXP_TIME)
        exp_time_s = MIN_EXP_TIME;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("          ");
      lcd.setCursor(0, 1);
      lcd.print(exp_time_s);
      lcd.print(" s");
    }
  } while (buttons != BTN_SELECT);
  delay(250);
  lcd.clear();

  // total time
  lcd.setCursor(0, 0);
  lcd.print("Total time:");
  lcd.setCursor(0, 1);
  lcd.print(time_s);
  lcd.print(" s");
  do {
    buttons = read_LCD_buttons();
    if (buttons == BTN_UP) {
      time_s += 1;
      if (time_s > MAX_TIME)
        time_s = MAX_TIME;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(time_s);
      lcd.print("s");
    }
    if (buttons == BTN_DOWN) {
      time_s -= 1;
      if (time_s < MIN_TIME)
        time_s = MIN_TIME;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(time_s);
      lcd.print("s");
    }
    if (buttons == BTN_RIGHT) {
      time_s += 60;
      if (time_s > MAX_TIME)
        time_s = MAX_TIME;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(time_s);
      lcd.print("s");
    }
    if (buttons == BTN_LEFT) {
      time_s -= 60;
      if (time_s < MIN_TIME)
        time_s = MIN_TIME;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(time_s);
      lcd.print("s");
    }

    // show hours and minutes
    lcd.setCursor(8, 1);
    lcd.print(" ");
    lcd.print(time_s / 3600);
    lcd.print(":");
    lcd.print((time_s / 60) % 60);
    lcd.print(":");
    lcd.print(time_s % 60);
  } while (buttons != BTN_SELECT);
  delay(250);
  lcd.clear();

  // number of pics
  lcd.setCursor(0, 0);
  lcd.print("Number of pics:");
  lcd.setCursor(0, 1);
  lcd.print(num_pics);
  do {
    buttons = read_LCD_buttons();
    if (buttons == BTN_UP) {
      num_pics += 1;
      if (num_pics > MAX_PICS)
        num_pics = MAX_PICS;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(num_pics);
    }
    if (buttons == BTN_DOWN) {
      num_pics -= 1;
      if (num_pics < MIN_PICS)
        num_pics = MIN_PICS;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(num_pics);
    }
    if (buttons == BTN_RIGHT) {
      num_pics += 10;
      if (num_pics > MAX_PICS)
        num_pics = MAX_PICS;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(num_pics);
    }
    if (buttons == BTN_LEFT) {
      num_pics -= 10;
      if (num_pics < MIN_PICS)
        num_pics = MIN_PICS;
      delay(100);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(num_pics);
    }
  } while (buttons != BTN_SELECT);
  delay(250);
  lcd.clear();

  // make calculations
  moving_time = (int32_t) time_s - (exp_time_s * num_pics);
  if (moving_time <= 0) {
    lcd.setCursor(0, 0);
    lcd.print("WRONG TIME VALUE");
    lcd.setCursor(0, 1);
    lcd.print("PRESS RESET");
    while (1);
  }

  time_between_pictures = moving_time / num_pics;
  steps_between_pictures = ((uint32_t)distance_mm * STEPS_MM) / num_pics;
  step_mtime = (time_between_pictures * 1000) / steps_between_pictures; // ms

#ifdef DEBUG
  Serial.print("Steps per mm: ");
  Serial.println(STEPS_MM);
  Serial.print("Total steps: ");
  Serial.println((uint32_t)distance_mm * STEPS_MM);
  Serial.print("Time moving(s): ");
  Serial.println(moving_time);
  Serial.print("Time between pictures(s): ");
  Serial.println(time_between_pictures);
  Serial.print("Steps between pictures: ");
  Serial.println(steps_between_pictures);
  Serial.print("Time between steps(ms): ");
  Serial.println(step_mtime);
#endif

  // store values in EEPROM
  EEPROM.put(EEPROM_DIST, distance_mm);
  EEPROM.put(EEPROM_EXP, exp_time_s);
  EEPROM.put(EEPROM_TIME, time_s);
  EEPROM.put(EEPROM_PICS, num_pics);
  EEPROM.write(EEPROM_INIT, 0xA5);

  // ok, show values and wait
  lcd.setCursor(0, 0);
  lcd.print("SELECT to start");
  lcd.setCursor(0, 1);
  lcd.print(distance_mm);
  lcd.print(" ");
  lcd.print(exp_time_s);
  lcd.print(" ");
  lcd.print(time_s);
  lcd.print(" ");
  lcd.print(num_pics);

  do {
    buttons = read_LCD_buttons();
  } while (buttons != BTN_SELECT);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Running");
  lcd.setCursor(10, 0);
  lcd.print(num_pics);
  lcd.setCursor(0, 1);
  lcd.print("RST to stop");
  stepper_direction(DIR_FWD);
}

void loop() {
  while (num_pics > 0) {
    // take picture
    take_picture();
    // wait exp time
    delay((exp_time_s * 1000) - 100);
    num_pics--;
    // update display
    lcd.setCursor(10, 0);
    lcd.print("      ");
    lcd.setCursor(10, 0);
    lcd.print(num_pics);
    // advance steps_between_pictures in time_between_pictures
    for (int i = 0; i < steps_between_pictures; i++) {
      stepper_step(step_mtime);
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Done!");
  lcd.setCursor(0, 1);
  lcd.print("RST to start again");
  while (1);
}
