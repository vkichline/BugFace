#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <M5Stack.h>
#include <BugComm.h>
#include <JoyFace.h>

#define   BG_COLOR          NAVY
#define   FG_COLOR          LIGHTGREY
#define   SCREEN_WIDTH      320
#define   SCREEN_H_CENTER   160
#define   DISPLAY_LINE1     20
#define   DISPLAY_LINE2     70
#define   DISPLAY_LINE3     105
#define   DISPLAY_LINE4     150
#define   DISPLAY_LINE5     200
#define   INDENT            10


BugComm   bug_comm;                     // From NowComm template class
JoyFace   joy;
bool      comp_mode   = false;          // Competition mode: manually select a channel


// Display the mac address of the device, and if connected, of its paired device.
// Also show the channel in use.
// Red indicates no ESP-Now connection, Green indicates connection established.
//
void print_mac_address(uint16_t color) {
  M5.Lcd.setTextColor(color, BLACK);
  M5.Lcd.drawCentreString("BugNow Controller", SCREEN_H_CENTER, DISPLAY_LINE1, 4);

  String mac = WiFi.macAddress();
  //mac.replace(":", " ");
  M5.Lcd.drawString("Ctrl:", INDENT, DISPLAY_LINE2, 4);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(mac, SCREEN_WIDTH-INDENT, DISPLAY_LINE2, 4);
  M5.Lcd.setTextDatum(TL_DATUM);

  if(bug_comm.is_connected()) {
    char  buffer[32];
    uint8_t*  pa = bug_comm.get_peer_address();
    //sprintf(buffer, "%02X %02X %02X %02X %02X %02X", pa[0], pa[1], pa[2], pa[3], pa[4], pa[5]);
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", pa[0], pa[1], pa[2], pa[3], pa[4], pa[5]);
    M5.Lcd.drawString("Rcvr:", INDENT, DISPLAY_LINE3, 4);
    M5.Lcd.setTextDatum(TR_DATUM);
    M5.Lcd.drawString(buffer, SCREEN_WIDTH-INDENT, DISPLAY_LINE3, 4);
    M5.Lcd.setTextDatum(TL_DATUM);
  }

  String chan = "Chan " + String(bug_comm.get_channel());
  M5.Lcd.drawCentreString(chan, SCREEN_H_CENTER, DISPLAY_LINE5, 4);
}


// Broadcast a BugComm_Discovery until someone responds with a valid packet
//
bool pair_with_receiver() {
  if(comp_mode) {
    M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
    M5.Lcd.fillScreen(BG_COLOR);
    M5.Lcd.drawCentreString("Waiting for Pairing", SCREEN_H_CENTER, DISPLAY_LINE2, 4);
    M5.Lcd.drawCentreString("on Channel " + String(bug_comm.get_channel()), SCREEN_H_CENTER, DISPLAY_LINE3, 4);
  }
  else {
    M5.Lcd.fillScreen(TFT_BLACK);
    print_mac_address(TFT_RED);
  }
  while(!bug_comm.is_connected()) {
    bug_comm.send_discovery();
    delay(500);
    if(bug_comm.is_data_ready() && NOWCOMM_KIND_DISCOVERY == bug_comm.get_msg_kind()) {
      Serial.println("Discovery response received.");
      if(bug_comm.process_discovery_response()) {
        print_mac_address(TFT_GREEN);
      }
    }
  }
  return true;
}


// Display the values being transmitted on the screen
//
void display_reading(JF_Reading& data) {
  M5.Lcd.drawCentreString(String("      ") + String(data.y) + "      ", SCREEN_H_CENTER - 55, DISPLAY_LINE4, 4);
  M5.Lcd.drawCentreString(String("      ") + String(data.x) + "      ", SCREEN_H_CENTER + 50, DISPLAY_LINE4, 4);
}


// Select the ESP-Now channel (1 - 14) to operate on
//
uint8_t select_channel() {
  uint8_t chan = random(14) + 1;
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.clearDisplay(BG_COLOR);
  M5.Lcd.drawCentreString("Select Channel", SCREEN_H_CENTER, DISPLAY_LINE1, 4);
  M5.Lcd.drawCentreString(String(chan), SCREEN_H_CENTER, DISPLAY_LINE2, 8);
  M5.Lcd.drawCentreString("+",       62, DISPLAY_LINE5, 4);
  M5.Lcd.drawCentreString("Select", 160, DISPLAY_LINE5, 4);
  M5.Lcd.drawCentreString("-",      256, DISPLAY_LINE5, 4);
  while(true) {
    M5.update();
    if(M5.BtnA.wasReleased()) {
      if(14 < ++chan) chan = 1;
      M5.Lcd.drawCentreString(String("  ") + String(chan) + "  ", SCREEN_H_CENTER, DISPLAY_LINE2, 8);
    }
    if(M5.BtnB.wasReleased()) {
      break;
    }
    if(M5.BtnC.wasReleased()) {
      if(1 > --chan) chan = 14;
      M5.Lcd.drawCentreString(String("  ") + String(chan) + "  ", SCREEN_H_CENTER, DISPLAY_LINE2, 8);
    }
  }
  return chan;
}


// On startup, select simple or competition mode
//
void select_mode() {
  M5.Lcd.setTextColor(FG_COLOR, BG_COLOR);
  M5.Lcd.clearDisplay(BG_COLOR);
  M5.Lcd.drawCentreString("BugFace Controller", SCREEN_H_CENTER, DISPLAY_LINE1, 4);
  M5.Lcd.drawCentreString("Select Mode", SCREEN_H_CENTER, DISPLAY_LINE2, 4);
  M5.Lcd.drawCentreString("Simple",       62, DISPLAY_LINE5, 4);
  M5.Lcd.drawCentreString("Selective",   256, DISPLAY_LINE5, 4);
  while(true) {
    M5.update();
    if(M5.BtnA.wasReleased()) {
      comp_mode = false;
      return;
    }
    if(M5.BtnC.wasReleased()) {
      comp_mode = true;
      return;
    }
  }
}


void setup() {
  M5.begin();
  joy.begin();
  select_mode();
  uint8_t channel = comp_mode ? select_channel() : 1;
  M5.Lcd.clear();
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.drawCentreString("BugFace Controller", 160, 12, 4);
  bug_comm.begin(NOWCOMM_MODE_CONTROLLER, channel);
  pair_with_receiver();
  M5.Lcd.fillScreen(BLACK);
  print_mac_address(TFT_GREEN);
}


void loop() {
  JF_Reading  reading;

  M5.update();
  if(M5.BtnA.wasReleased()) {
    // do stuff
  }
  if(M5.BtnB.wasReleased()) {
    // do stuff
  }
  if(M5.BtnC.wasReleased()) {
    // do stuff
  }
  if(joy.read(reading)) {
    // do stuff
  }

  if(joy.read(reading)) {
    bug_comm.send_command(reading.y, -reading.x, reading.button);
    display_reading(reading);
  }

  delay(20);
}
