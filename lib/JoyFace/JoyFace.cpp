#include <Wire.h>
#include <Preferences.h>
#include <M5Stack.h>
#include "JoyFace.h"

#define FACE_JOY_ADDR   0x5e
#define JF_NAMESPACE    "JF_CalDat"
#define ZERO_THRESHOLD  3


// Must be called before othe functions.
//
void JoyFace::begin(bool show_leds) {
  this->show_leds = show_leds;
  Wire.begin();                 // Ensure this is started
  go_dark();                    // unconditionally turn leds off
  load_calibration_from_nvs();  // calibrate if we have the data
}


// Read the X/Y and button data from the I2C interface.
// If show_leds is true, light the corresponding LEDs on Face.
//
bool JoyFace::read(JF_Reading& reading) {
  Wire.requestFrom(FACE_JOY_ADDR, 5);
  if (Wire.available()) {
    calibrating = false;
    // Acquire the joystick data
    uint8_t y_low   = Wire.read();
    uint8_t y_high  = Wire.read();
    uint8_t x_low   = Wire.read();
    uint8_t x_high  = Wire.read();
    reading.button  = 0 == Wire.read();  // 1 = released, 0 = pressed
    reading.x       = x_high << 8 | x_low;
    reading.y       = y_high << 8 | y_low;
    // provide feedback by lighting the corresponding LEDs
    if(show_leds) {
      if(reading.button) {
        set_led(0, 50, 0, 0);
        set_led(1, 50, 0, 0);
        set_led(2, 50, 0, 0);
        set_led(3, 50, 0, 0);
      }
      else {
        if     (reading.x > 600) { set_led(2, 0, 0, 50); set_led(0, 0, 0, 0); }
        else if(reading.x < 400) { set_led(0, 0, 0, 50); set_led(2, 0, 0, 0); }
        else                     { set_led(0, 0, 0,  0); set_led(2, 0, 0, 0); }
        if     (reading.y > 600) { set_led(3, 0, 0, 50); set_led(1, 0, 0, 0); }
        else if(reading.y < 400) { set_led(1, 0, 0, 50); set_led(3, 0, 0, 0); }
        else                     { set_led(1, 0, 0,  0); set_led(3, 0, 0, 0); }
      }
    }
    // scale the reading if the joystick is calibrated
    if(calibrated) {
      // Scale the reading down
      reading.x = (reading.x - cal_data.center_x) * cal_data.x_scale;
      reading.y = (reading.y - cal_data.center_y) * cal_data.y_scale;
      // Enforce limitations
      if( JF_SCALE_TO < reading.x) reading.x =  JF_SCALE_TO;
      if(-JF_SCALE_TO > reading.x) reading.x = -JF_SCALE_TO;
      if( JF_SCALE_TO < reading.y) reading.y =  JF_SCALE_TO;
      if(-JF_SCALE_TO > reading.y) reading.y = -JF_SCALE_TO;
      // Make 0/0 easier to hit
      if(abs(reading.x) <= ZERO_THRESHOLD && abs(reading.y) <= ZERO_THRESHOLD) {
        reading.x = 0;
        reading.y = 0;
      }
    }
    return true;
  }
  return false;
}


// Turn off all the LEDs
//
void JoyFace::go_dark() {
  set_led(0, 0, 0, 0);
  set_led(1, 0, 0, 0);
  set_led(2, 0, 0, 0);
  set_led(3, 0, 0, 0);
}


// Index is 0 - 3. It looks like they are arrainged W, S, E, N
//
void JoyFace::set_led(int index, int r, int g, int b) {
  Wire.beginTransmission(FACE_JOY_ADDR);
  Wire.write(index);
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
  Wire.endTransmission();
}


// Flash the LEDs rapidly 4 times in random colors.
// Used to signal that calibration is complete.
//
void JoyFace::flash_leds() {
  for (int i = 0; i < 256; i++) {
    Wire.beginTransmission(FACE_JOY_ADDR);
    Wire.write(i % 4);
    Wire.write(random(256) * (256 - i) / 256);
    Wire.write(random(256) * (256 - i) / 256);
    Wire.write(random(256) * (256 - i) / 256);
    Wire.endTransmission();
    delay(2);
  }
  go_dark();
}


bool JoyFace::calibrate() {
  // Test to see if we are just beginning calibration
  if(!calibrating) {
    Serial.println("Beginning calibration");
    cal_info.clear();
    calibrating = true;
    calibrated  = false;
  }
  // Read data from the I2C interface
  Wire.requestFrom(FACE_JOY_ADDR, 5);
  if (Wire.available()) {
    // Acquire the joystick data
    uint8_t y_low   = Wire.read();
    uint8_t y_high  = Wire.read();
    uint8_t x_low   = Wire.read();
    uint8_t x_high  = Wire.read();
                      Wire.read();  // burn the button
    uint16_t raw_x  = x_high << 8 | x_low;
    uint16_t raw_y  = y_high << 8 | y_low;

    // When the joystick is at rest in the center, add to the center sums
    if(raw_x > 400 && raw_x < 600 && raw_y > 400 && raw_y < 600) {
      if(CALIBRATION_SAMPLES > cal_info.center_count) {
        cal_info.x_center += raw_x;
        cal_info.y_center += raw_y;
        Serial.printf("Calibration center point #%d: raw_x = %d, raw_y = %d, x_center = %d, y_center = %d\n",
                      cal_info.center_count, raw_x, raw_y, cal_info.x_center, cal_info.y_center);
        cal_info.center_count++;
      }
    }
    else if(CALIBRATION_SAMPLES > cal_info.xy_count) {
      // Capture minimum and maximum values when user is making circles
      if(raw_x > cal_info.max_x) cal_info.max_x = raw_x;
      if(raw_x < cal_info.min_x) cal_info.min_x = raw_x;
      if(raw_y > cal_info.max_y) cal_info.max_y = raw_y;
      if(raw_y < cal_info.min_y) cal_info.min_y = raw_y;
      Serial.printf("Calibartion min/max: #%d: raw_x = %d, raw_y = %d, min_x = %d, max_x = %d, min_y = %d, max_y = %d\n",
                    cal_info.xy_count, raw_x, raw_y, cal_info.min_x, cal_info.max_x, cal_info.min_y, cal_info.max_y);
      cal_info.xy_count++;
    }

    // If we've collected all the data we need, calibration is complete
    if(CALIBRATION_SAMPLES-1 <= cal_info.center_count && CALIBRATION_SAMPLES-1 <= cal_info.xy_count) {
      // Handle the actual calibration here
      cal_data.center_x = cal_info.x_center / cal_info.center_count;
      cal_data.center_y = cal_info.y_center / cal_info.center_count;
      cal_data.x_scale = (double(JF_SCALE_TO * 2) / double(cal_info.max_x - cal_info.min_x)) + .01; // Overscale a little
      cal_data.y_scale = (double(JF_SCALE_TO * 2) / double(cal_info.max_y - cal_info.min_y)) + .01;
      Serial.printf("Center: %d/%d. Min: %d/%d. Max: %d/%d. Scale: %.2f/%.2f\n",
                    cal_data.center_x, cal_data.center_y, cal_info.min_x, cal_info.min_y,
                    cal_info.max_x, cal_info.max_y, cal_data.x_scale, cal_data.y_scale);
      calibrating = false;
      calibrated  = true;
    }
    return calibrated;
  }
  return false;
}


bool JoyFace::load_calibration_from_nvs() {
  Preferences prefs;
  JF_CalDat   temp;
  if(prefs.begin(JF_NAMESPACE, true)) { // read-only
    temp.center_x = prefs.getUShort("center_x", 0xFFFF);  // Defaults are impossible values
    temp.center_y = prefs.getUShort("center_y", 0xFFFF);
    temp.x_scale  = prefs.getDouble("x_scale",  10.0);
    temp.y_scale  = prefs.getDouble("y_scale",  10.0);
    prefs.end();
    if(0xFFFF  == temp.center_x || 0xFFFF == temp.center_y || 10.0 == temp.x_scale || 10.0 == temp.y_scale) {
      Serial.println("ERROR: failed to read calibration data");
      return false;
    }
    cal_data    = temp;    // assign entire structure
    calibrated  = true;
    Serial.printf("Succeeded reading calibration data from NVS: %d/%d, %.4f/%.5f\n",
              cal_data.center_x, cal_data.center_y, cal_data.x_scale, cal_data.y_scale);
    return true;
  }
  Serial.println("ERROR: prefs.begin(true) failed in load_calibration_from_nvs().");
  return false;
}


bool JoyFace::save_calibration_to_nvs() {
  Preferences prefs;
  if(prefs.begin(JF_NAMESPACE, false)) { // read-write
    prefs.putUShort("center_x", cal_data.center_x);  // Defaults are impossible values
    prefs.putUShort("center_y", cal_data.center_y);
    prefs.putDouble("x_scale", cal_data.x_scale);
    prefs.putDouble("y_scale", cal_data.y_scale);
    prefs.end();
    Serial.printf("Succeeded writing calibration data to NVS: %d/%d, %.4f/%.5f\n",
            cal_data.center_x, cal_data.center_y, cal_data.x_scale, cal_data.y_scale);
    return true;
  }
  Serial.println("ERROR: prefs.begin(false) failed in save_calibration_to_nvs().");
  return false;
}


bool JoyFace::delete_calibration_from_nvs() {
  Preferences prefs;
  if(prefs.begin(JF_NAMESPACE, false)) { // read-write
    prefs.clear();
    prefs.end();
    Serial.println("JoyFace NVS settings cleared");
    return true;
  }
  Serial.println("ERROR: prefs.begin(false) failed in delete_calibration_from_nvs().");
  return false;
}
