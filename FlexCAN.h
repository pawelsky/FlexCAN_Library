// -------------------------------------------------------------
// a simple Arduino Teensy 3.1/3.2/3.5/3.6 CAN driver
// by teachop
// dual CAN support for MK66FX1M0 and updates for MK64FX512 by Pawelsky
//
#ifndef __FLEXCAN_H__
#define __FLEXCAN_H__

#include <Arduino.h>

typedef struct CAN_message_t {
  uint32_t id; // can identifier
  uint8_t ext; // identifier is extended
  uint8_t len; // length of data
  uint16_t timeout; // milliseconds, zero will disable waiting
  uint8_t buf[8];
} CAN_message_t;

typedef struct CAN_filter_t {
  uint8_t rtr;
  uint8_t ext;
  uint32_t id;
} CAN_filter_t;

// -------------------------------------------------------------
class FlexCAN
{
private:
  struct CAN_filter_t defaultMask;
  uint32_t flexcanBase;

public:
  FlexCAN(uint32_t baud = 125000, uint8_t id = 0, uint8_t txAlt = 0, uint8_t rxAlt = 0);
  void begin(const CAN_filter_t &mask);
  inline void begin()
  {
    begin(defaultMask);
  }
  void setFilter(const CAN_filter_t &filter, uint8_t n);
  void end(void);
  int available(void);
  int write(const CAN_message_t &msg);
  int read(CAN_message_t &msg);

};

#endif // __FLEXCAN_H__
