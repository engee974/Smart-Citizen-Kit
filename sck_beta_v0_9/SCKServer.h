/*

  SCKServer.h

*/


#ifndef __SCKSERVER_H__
#define __SCKSERVER_H__

#include "Constants.h"
#include "SCKBase.h"
#include <Arduino.h>

class SCKServer {
  public:
    SCKServer(SCKBase& base);
    boolean time(char *time);
    void json_update(uint16_t updates, long *value, char *time, boolean isMultipart);
    void send(boolean sleep, boolean *wait_moment, long *value, char *time, boolean instant);
    boolean update(long *value, char *time_);
    boolean connect();
    void addFIFO(long *value, char *time);
    void readFIFO();
    boolean RTCupdate(char *time);

  private:
    SCKBase& _base;

};
#endif
