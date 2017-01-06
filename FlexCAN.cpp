// -------------------------------------------------------------
// a simple Arduino Teensy 3.1/3.2/3.5/3.6 CAN driver
// by teachop
// dual CAN support for MK66FX1M0 and updates for MK64FX512 by Pawelsky
//
#include "FlexCAN.h"
#include "kinetis_flexcan.h"

static const int txb = 8; // with default settings, all buffers before this are consumed by the FIFO
static const int txBuffers = 8;
static const int rxb = 0;

#define FLEXCANb_MCR(b)                   (*(vuint32_t*)(b))
#define FLEXCANb_CTRL1(b)                 (*(vuint32_t*)(b+4))
#define FLEXCANb_RXMGMASK(b)              (*(vuint32_t*)(b+0x10))
#define FLEXCANb_IFLAG1(b)                (*(vuint32_t*)(b+0x30))
#define FLEXCANb_RXFGMASK(b)              (*(vuint32_t*)(b+0x48))
#define FLEXCANb_MBn_CS(b, n)             (*(vuint32_t*)(b+0x80+n*0x10))
#define FLEXCANb_MBn_ID(b, n)             (*(vuint32_t*)(b+0x84+n*0x10))
#define FLEXCANb_MBn_WORD0(b, n)          (*(vuint32_t*)(b+0x88+n*0x10))
#define FLEXCANb_MBn_WORD1(b, n)          (*(vuint32_t*)(b+0x8C+n*0x10))
#define FLEXCANb_IDFLT_TAB(b, n)          (*(vuint32_t*)(b+0xE0+(n*4)))

// -------------------------------------------------------------
FlexCAN::FlexCAN(uint32_t baud, uint8_t id, uint8_t txAlt, uint8_t rxAlt)
{
  flexcanBase = FLEXCAN0_BASE;
#ifdef __MK66FX1M0__
  if(id > 0) flexcanBase = FLEXCAN1_BASE;
#endif

  // set up the pins
  if(flexcanBase == FLEXCAN0_BASE)
  {
#if defined(__MK66FX1M0__) || defined(__MK64FX512__)
    //  3=PTA12=CAN0_TX,  4=PTA13=CAN0_RX (default)
    // 29=PTB18=CAN0_TX, 30=PTB19=CAN0_RX (alternative)
    if(txAlt == 1) CORE_PIN29_CONFIG = PORT_PCR_MUX(2); else CORE_PIN3_CONFIG = PORT_PCR_MUX(2); 
    if(rxAlt == 1) CORE_PIN30_CONFIG = PORT_PCR_MUX(2); else CORE_PIN4_CONFIG = PORT_PCR_MUX(2);// | PORT_PCR_PE | PORT_PCR_PS; 
#else   
    //  3=PTA12=CAN0_TX,  4=PTA13=CAN0_RX (default)
    // 32=PTB18=CAN0_TX, 25=PTB19=CAN0_RX (alternative)
    if(txAlt == 1) CORE_PIN32_CONFIG = PORT_PCR_MUX(2); else CORE_PIN3_CONFIG = PORT_PCR_MUX(2); 
    if(rxAlt == 1) CORE_PIN25_CONFIG = PORT_PCR_MUX(2); else CORE_PIN4_CONFIG = PORT_PCR_MUX(2);// | PORT_PCR_PE | PORT_PCR_PS;
#endif
  }
#ifdef __MK66FX1M0__
  else if(flexcanBase == FLEXCAN1_BASE)
  {
    // 33=PTE24=CAN1_TX, 34=PTE25=CAN1_RX (default)
    // NOTE: Alternative CAN1 pins are not broken out on Teensy 3.6
    CORE_PIN33_CONFIG = PORT_PCR_MUX(2);
    CORE_PIN34_CONFIG = PORT_PCR_MUX(2);// | PORT_PCR_PE | PORT_PCR_PS;
  }
#endif

  // select clock source 16MHz xtal
  OSC0_CR |= OSC_ERCLKEN;
  if(flexcanBase == FLEXCAN0_BASE) SIM_SCGC6 |=  SIM_SCGC6_FLEXCAN0;
#ifdef __MK66FX1M0__
  else if(flexcanBase == FLEXCAN1_BASE) SIM_SCGC3 |=  SIM_SCGC3_FLEXCAN1;
#endif
  FLEXCANb_CTRL1(flexcanBase) &= ~FLEXCAN_CTRL_CLK_SRC;

  // enable CAN
  FLEXCANb_MCR(flexcanBase) |=  FLEXCAN_MCR_FRZ;
  FLEXCANb_MCR(flexcanBase) &= ~FLEXCAN_MCR_MDIS;
  while(FLEXCANb_MCR(flexcanBase) & FLEXCAN_MCR_LPM_ACK)
    ;
  // soft reset
  FLEXCANb_MCR(flexcanBase) ^=  FLEXCAN_MCR_SOFT_RST;
  while(FLEXCANb_MCR(flexcanBase) & FLEXCAN_MCR_SOFT_RST)
    ;
  // wait for freeze ack
  while(!(FLEXCANb_MCR(flexcanBase) & FLEXCAN_MCR_FRZ_ACK))
    ;
  // disable self-reception
  FLEXCANb_MCR(flexcanBase) |= FLEXCAN_MCR_SRX_DIS;

  //enable RX FIFO
  FLEXCANb_MCR(flexcanBase) |= FLEXCAN_MCR_FEN;

  // segment splits and clock divisor based on baud rate
    if ( 50000 == baud ) {
    FLEXCANb_CTRL1(flexcanBase) = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                                | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(19));
  } else if ( 100000 == baud ) {
    FLEXCANb_CTRL1(flexcanBase) = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                                | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(9));
  } else if ( 250000 == baud ) {
    FLEXCANb_CTRL1(flexcanBase) = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                                | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(3));
  } else if ( 500000 == baud ) {
    FLEXCANb_CTRL1(flexcanBase) = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                                | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(1));
  } else if ( 1000000 == baud ) {
    FLEXCANb_CTRL1(flexcanBase) = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(0)
                                | FLEXCAN_CTRL_PSEG1(1) | FLEXCAN_CTRL_PSEG2(1) | FLEXCAN_CTRL_PRESDIV(1));
  } else { // 125000
    FLEXCANb_CTRL1(flexcanBase) = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                                | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(7));
  }

  // Default mask is allow everything
  defaultMask.rtr = 0;
  defaultMask.ext = 0;
  defaultMask.id = 0;
}


// -------------------------------------------------------------
void FlexCAN::end(void)
{
  // enter freeze mode
  FLEXCANb_MCR(flexcanBase) |= (FLEXCAN_MCR_HALT);
  while(!(FLEXCANb_MCR(flexcanBase) & FLEXCAN_MCR_FRZ_ACK))
    ;
}


// -------------------------------------------------------------
void FlexCAN::begin(const CAN_filter_t &mask)
{
  FLEXCANb_RXMGMASK(flexcanBase) = 0;

  //enable reception of all messages that fit the mask
  if (mask.ext) {
    FLEXCANb_RXFGMASK(flexcanBase) = ((mask.rtr?1:0) << 31) | ((mask.ext?1:0) << 30) | ((mask.id & FLEXCAN_MB_ID_EXT_MASK) << 1);
  } else {
    FLEXCANb_RXFGMASK(flexcanBase) = ((mask.rtr?1:0) << 31) | ((mask.ext?1:0) << 30) | (FLEXCAN_MB_ID_IDSTD(mask.id) << 1);
  }

  // start the CAN
  FLEXCANb_MCR(flexcanBase) &= ~(FLEXCAN_MCR_HALT);
  // wait till exit of freeze mode
  while(FLEXCANb_MCR(flexcanBase) & FLEXCAN_MCR_FRZ_ACK);

  // wait till ready
  while(FLEXCANb_MCR(flexcanBase) & FLEXCAN_MCR_NOT_RDY);

  //set tx buffers to inactive
  for (int i = txb; i < txb + txBuffers; i++) {
    FLEXCANb_MBn_CS(flexcanBase, i) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_INACTIVE);
  }
}


// -------------------------------------------------------------
void FlexCAN::setFilter(const CAN_filter_t &filter, uint8_t n)
{
  if ( 8 > n ) {
    if (filter.ext) {
      FLEXCANb_IDFLT_TAB(flexcanBase, n) = ((filter.rtr?1:0) << 31) | ((filter.ext?1:0) << 30) | ((filter.id & FLEXCAN_MB_ID_EXT_MASK) << 1);
    } else {
      FLEXCANb_IDFLT_TAB(flexcanBase, n) = ((filter.rtr?1:0) << 31) | ((filter.ext?1:0) << 30) | (FLEXCAN_MB_ID_IDSTD(filter.id) << 1);
    }
  }
}


// -------------------------------------------------------------
int FlexCAN::available(void)
{
  //In FIFO mode, the following interrupt flag signals availability of a frame
  return (FLEXCANb_IFLAG1(flexcanBase) & FLEXCAN_IMASK1_BUF5M)? 1:0;
}


// -------------------------------------------------------------
int FlexCAN::read(CAN_message_t &msg)
{
  unsigned long int startMillis;

  startMillis = msg.timeout? millis() : 0;

  while( !available() ) {
    if ( !msg.timeout || (msg.timeout<=(millis()-startMillis)) ) {
      // early EXIT nothing here
      return 0;
    }
    yield();
  }

  // get identifier and dlc
  msg.len = FLEXCAN_get_length(FLEXCANb_MBn_CS(flexcanBase, rxb));
  msg.ext = (FLEXCANb_MBn_CS(flexcanBase, rxb) & FLEXCAN_MB_CS_IDE)? 1:0;
  msg.id  = (FLEXCANb_MBn_ID(flexcanBase, rxb) & FLEXCAN_MB_ID_EXT_MASK);
  if(!msg.ext) {
    msg.id >>= FLEXCAN_MB_ID_STD_BIT_NO;
  }

  // copy out message
  uint32_t dataIn = FLEXCANb_MBn_WORD0(flexcanBase, rxb);
  msg.buf[3] = dataIn;
  dataIn >>=8;
  msg.buf[2] = dataIn;
  dataIn >>=8;
  msg.buf[1] = dataIn;
  dataIn >>=8;
  msg.buf[0] = dataIn;
  if ( 4 < msg.len ) {
    dataIn = FLEXCANb_MBn_WORD1(flexcanBase, rxb);
    msg.buf[7] = dataIn;
    dataIn >>=8;
    msg.buf[6] = dataIn;
    dataIn >>=8;
    msg.buf[5] = dataIn;
    dataIn >>=8;
    msg.buf[4] = dataIn;
  }
  for( int loop=msg.len; loop<8; ++loop ) {
    msg.buf[loop] = 0;
  }

  //notify FIFO that message has been read
  FLEXCANb_IFLAG1(flexcanBase) = FLEXCAN_IMASK1_BUF5M;

  return 1;
}


// -------------------------------------------------------------
int FlexCAN::write(const CAN_message_t &msg)
{
  unsigned long int startMillis;

  startMillis = msg.timeout? millis() : 0;

  // find an available buffer
  int buffer = -1;
  for ( int index = txb; ; ) {
    if ((FLEXCANb_MBn_CS(flexcanBase, index) & FLEXCAN_MB_CS_CODE_MASK) == FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_INACTIVE)) {
      buffer = index;
      break;// found one
    }
    if ( !msg.timeout ) {
      if ( ++index >= (txb+txBuffers) ) {
        return 0;// early EXIT no buffers available
      }
    } else {
      // blocking mode, only 1 txb used to guarantee frames in order
      if ( msg.timeout <= (millis()-startMillis) ) {
        return 0;// timed out
      }
      yield();
    }
  }

  // transmit the frame
  FLEXCANb_MBn_CS(flexcanBase, buffer) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_INACTIVE);
  if(msg.ext) {
    FLEXCANb_MBn_ID(flexcanBase, buffer) = (msg.id & FLEXCAN_MB_ID_EXT_MASK);
  } else {
    FLEXCANb_MBn_ID(flexcanBase, buffer) = FLEXCAN_MB_ID_IDSTD(msg.id);
  }
  FLEXCANb_MBn_WORD0(flexcanBase, buffer) = (msg.buf[0]<<24)|(msg.buf[1]<<16)|(msg.buf[2]<<8)|msg.buf[3];
  FLEXCANb_MBn_WORD1(flexcanBase, buffer) = (msg.buf[4]<<24)|(msg.buf[5]<<16)|(msg.buf[6]<<8)|msg.buf[7];
  if(msg.ext) {
    FLEXCANb_MBn_CS(flexcanBase, buffer) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ONCE)
                                         | FLEXCAN_MB_CS_LENGTH(msg.len) | FLEXCAN_MB_CS_SRR | FLEXCAN_MB_CS_IDE;
  } else {
    FLEXCANb_MBn_CS(flexcanBase, buffer) = FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ONCE)
                                         | FLEXCAN_MB_CS_LENGTH(msg.len);
  }

  return 1;
}

