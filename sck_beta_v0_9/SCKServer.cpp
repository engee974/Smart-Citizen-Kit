/*

  SCKServer.cpp

*/
//#include "SCKAmbient.h"
#include "SCKServer.h"
#include <Wire.h>
#include <EEPROM.h>



//SCKBase base__;
//SCKAmbient ambient__;

SCKServer::SCKServer(SCKBase& base)
{
  _base = base;
}

boolean SCKServer::time(char *time_)
{
  boolean ok = false;
  uint8_t count = 0;
  byte retry = 0;
  while ((retry < 5) && (!ok)) {
    retry++;
    if (_base.enterCommandMode()) {
      if (_base.open(WEB[0], 80)) {
        for (byte i = 0; i < 3; i++) Serial1.print(WEBTIME[i]); //Requests to the server time
        if (_base.findInResponse("UTC:", 2000))
        {
          char newChar;
          byte offset = 0;
          unsigned long time = millis();
          while (offset < TIME_BUFFER_SIZE) {
            if (Serial1.available())
            {
              newChar = Serial1.read();
              time = millis();
              if (newChar == '#') {
                ok = true;
                time_[offset] = '\x00';
                break;
              }
              else if (newChar != -1) {
                if (newChar == ','){
                  if (count < 2) time_[offset] = '-';
                  else if (count > 2) time_[offset] = ':';
                  else time_[offset] = ' ';
                  count++;
                }
                else time_[offset] = newChar;
                offset++;
              }
            }
            else if ((millis() - time) > 1000){
              ok = false;
              break;
            }
          }
        }
        else {
#if debugServer
          Serial.println("FAIL:(");
#endif
        }
        _base.close();
      }
    }
  }
  if (!ok){
    time_[0] = '#';
    time_[1] = 0x00;
  }
  _base.exitCommandMode();
  return ok;
}

boolean SCKServer::RTCupdate(char *time_)
{
  byte retry = 0;
  if (_base.checkRTC()) {
    if (time(time_)) {
      while (retry < 5) {
        retry++;
        if (_base.RTCadjust(time_)) {
          return true;
        }
      }
    }
  }
  return false;
}

void SCKServer::json_update(uint16_t updates, long *value, char *time, boolean isMultipart)
{
#if debugServer
  Serial.print(F("["));
#endif
  Serial1.print(F("["));
  for (int i = 0; i < updates; i++) {
    readFIFO();
    if ((i < (updates - 1)) || (isMultipart)) {
      Serial1.print(F(","));
#if debugServer
      Serial.print(F(","));
#endif
    }
  }

  if (isMultipart) {
    byte i;
    for (i = 0; i < 9; i++) {
      Serial1.print(SERVER[i]);
      Serial1.print(value[i]);
    }
    Serial1.print(SERVER[i]);
    Serial1.print(time);
    Serial1.print(SERVER[i + 1]);
    Serial1.println(F("]"));
    Serial1.println();

#if debugServer
    for (i = 0; i < 9; i++) {
      Serial.print(SERVER[i]);
      Serial.print(value[i]);
    }
    Serial.print(SERVER[i]);
    Serial.print(time);
    Serial.print(SERVER[i + 1]);
#endif
  }
  Serial1.println(F("]"));
  Serial1.println();
#if debugServer
  Serial.println(F("]"));
#endif
}

void SCKServer::addFIFO(long *value, char *time)
{
  uint16_t updates = (_base.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL) - _base.readData(EE_ADDR_NUMBER_READ_MEASURE, INTERNAL)) / ((SENSORS) * 4 + TIME_BUFFER_SIZE);
  if (updates < MAX_MEMORY) {
    int eeaddress = _base.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL);
    int i = 0;
    for (i = 0; i < 9; i++) {
      _base.writeData(eeaddress + i * 4, value[i], EXTERNAL);
    }
    _base.writeData(eeaddress + i * 4, 0, time, EXTERNAL);
    eeaddress = eeaddress + (SENSORS) * 4 + TIME_BUFFER_SIZE;
    _base.writeData(EE_ADDR_NUMBER_WRITE_MEASURE, eeaddress, INTERNAL);
  }
  else {
#if debugEnabled
    if (!_base.getDebugState()) Serial.println(F("Memory limit exceeded!!"));
#endif
  }
}

void SCKServer::readFIFO()
{
  int i = 0;
  int eeaddress = _base.readData(EE_ADDR_NUMBER_READ_MEASURE, INTERNAL);
  for (i = 0; i < 9; i++) {
    Serial1.print(SERVER[i]);
    Serial1.print(_base.readData(eeaddress + i * 4, EXTERNAL)); //SENSORS
  }
  Serial1.print(SERVER[i]);
  Serial1.print(_base.readData(eeaddress + i * 4, 0, EXTERNAL)); //TIME
  Serial1.print(SERVER[i + 1]);

#if debugServer
  for (i = 0; i < 9; i++) {
    Serial.print(SERVER[i]);
    Serial.print(_base.readData(eeaddress + i * 4, EXTERNAL)); //SENSORS
  }
  Serial.print(SERVER[i]);
  Serial.print(_base.readData(eeaddress + i * 4, 0, EXTERNAL)); //TIME
  Serial.print(SERVER[i + 1]);
#endif

  eeaddress = eeaddress + (SENSORS) * 4 + TIME_BUFFER_SIZE;
  if (eeaddress == _base.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL)) {
    _base.writeData(EE_ADDR_NUMBER_WRITE_MEASURE, 0, INTERNAL);
    _base.writeData(EE_ADDR_NUMBER_READ_MEASURE, 0, INTERNAL);
  }
  else _base.writeData(EE_ADDR_NUMBER_READ_MEASURE, eeaddress, INTERNAL);
}

#define numbers_retry 5

boolean SCKServer::update(long *value, char *time_)
{
  value[8] = _base.scan();  //Wifi Nets
  byte retry = 0;
  if (time(time_)) {
    //Update server time
    if (_base.checkRTC()) {
      while (!_base.RTCadjust(time_) && (retry < numbers_retry)) retry++;
    }
  }
  else if (_base.checkRTC()) {
    _base.RTCtime(time_);
#if debugEnabled
    if (!_base.getDebugState()) Serial.println(F("Fail server time!!"));
#endif
  }
  else {
    time_ = "#";
    return false;
  }
  return true;
}

boolean SCKServer::connect()
{
  int retry = 0;
  while (true) {
    if (_base.open(WEB[0], 80)) break;
    else {
      retry++;
      if (retry >= numbers_retry) return false;
    }
  }
  for (byte i = 1; i < 5; i++) Serial1.print(WEB[i]);
  Serial1.println(_base.readData(EE_ADDR_MAC, 0, INTERNAL)); //MAC ADDRESS
  Serial1.print(WEB[5]);
  Serial1.println(_base.readData(EE_ADDR_APIKEY, 0, INTERNAL)); //Apikey
  Serial1.print(WEB[6]);
  Serial1.println(FirmWare); //Firmware version
  Serial1.print(WEB[7]);
  return true;
}


void SCKServer::send(boolean sleep, boolean *wait_moment, long *value, char *time, boolean instant)
{
  *wait_moment = true;
  if (_base.checkRTC()) _base.RTCtime(time);
  char tmpTime[19];
  strncpy(tmpTime, time, 20);
  uint16_t updates = (_base.readData(EE_ADDR_NUMBER_WRITE_MEASURE, INTERNAL) - _base.readData(EE_ADDR_NUMBER_READ_MEASURE, INTERNAL)) / ((SENSORS) * 4 + TIME_BUFFER_SIZE);
  uint16_t NumUpdates = _base.readData(EE_ADDR_NUMBER_UPDATES, INTERNAL); // Number of readings before batch update
  if (updates >= (NumUpdates - 1) || instant) {
    if (sleep) {
#if debugEnabled
      if (!_base.getDebugState()) Serial.println(F("SCK Waking up..."));
#endif
      digitalWrite(AWAKE, HIGH);
    }
    if (_base.connect()) {
      //Wifi connect
#if debugEnabled
      if (!_base.getDebugState()) Serial.println(F("SCK Connected to Wi-Fi!!"));
#endif
      if (update(value, time)) {
        //Update time and nets
#if debugEnabled
        if (!_base.getDebugState()) {
          Serial.print(F("updates = "));
          Serial.println(updates + 1);
        }
#endif
        int num_post = updates;
        int cycles = cycles = updates / POST_MAX;;
        if (updates > POST_MAX) {
          for (int i = 0; i < cycles; i++) {
            connect();
            json_update(POST_MAX, value, tmpTime, false);
          }
          num_post = updates - cycles * POST_MAX;
        }
        connect();
        json_update(num_post, value, tmpTime, true);
#if debugEnabled
        if (!_base.getDebugState()) Serial.println(F("Posted to Server!"));
#endif

      }
      else {
#if debugEnabled
        if (!_base.getDebugState()) {
          Serial.println(F("Error updating time Server..!"));
        }
#endif
      }
#if debugEnabled
      if (!_base.getDebugState()) Serial.println(F("Old connection active. Closing..."));
#endif
      _base.close();
    }
    else {
      //No connect
      if (_base.checkRTC()) _base.RTCtime(time);
      else time = "#";
      addFIFO(value, time);
#if debugEnabled
      if (!_base.getDebugState()) {
        Serial.print(F("Error in connection!!"));
        Serial.println(F(" Data saved in memory"));
        Serial.print(F("Pending updates: "));
        Serial.println(updates + 1);
      }
#endif
    }
    if (sleep) {
      _base.sleep();
#if debugEnabled
      if (!_base.getDebugState()) {
        Serial.println(F("SCK Sleeping"));
      }
#endif
      digitalWrite(AWAKE, LOW);
    }
  }
  else {
    if (_base.checkRTC()) _base.RTCtime(time);
    else time = "#";
    addFIFO(value, time);
#if debugEnabled
    if (!_base.getDebugState()) Serial.println(F("Saved in memory!!"));
#endif
  }
  *wait_moment = false;
}

