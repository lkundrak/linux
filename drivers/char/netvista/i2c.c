#include <linux/types.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include "seeprom.h"
#include <linux/pci.h>
#include <linux/interrupt.h>
#include "other.h"

#define ENABLE	0x00
#define CLOCK	0x08
#define SDIO	0x04

static void SCLK(uchar state);
static void SDOUT(uchar state);
static uchar SDIN(void);
static void I2C_start(void);
static void I2C_stop(void);
static void I2C_writeByte(uchar Data);
static uchar I2C_readByte(void);
static void I2C_sendAck(void);
static void I2C_sendNack(void);
static int I2C_waitOnAck(void);


/*
===============================================================================
  INPUT:
    state
      If state is != 0, then SCLK is set high, else set to 0.
===============================================================================
*/
static void SCLK(uchar state)
{
  uchar outByte = ENABLE;     /* Default is SDCLK low */

  if (state > 0)
  {
    outByte |= CLOCK;
  }

// This sucks because I have to protect SDAT otherwise when I clock
// the line might be munged, I also have be be sure that SCLK is an output

  PCI_Write_CFG_Reg(PCI_ISA_CONFIG,
                    0x90,
                     0x08 | (0xFB & PCI_Read_CFG_Reg(PCI_ISA_CONFIG,
                                                   0x90,
                                                    1)),
                    1);

  PCI_Write_CFG_Reg(PCI_ISA_CONFIG,
                    0x91,
                    outByte | (0xF7 & PCI_Read_CFG_Reg(PCI_ISA_CONFIG,
                                                       0x91,
                                                       1)),
                    1);

//  PCI_Write_CFG_Reg(PCI_ISA_CONFIG,
//                    0x6D,
//                    outByte | (0xF7 & PCI_Read_CFG_Reg(PCI_ISA_CONFIG,
//                                                       0x6D,
//                                                       1)),
//                    1);
}

/*
===============================================================================
  INPUT:
    state
      If state is != 0, then SDOUT is set high, else set to 0.
===============================================================================
*/
static void SDOUT(uchar state)
{
  uchar outByte = ENABLE;     /* Default is SDOUT low */

  if (state != 0)
  {
    outByte |= SDIO;          /* SDOUT is high */
  }

// SDAT might be set as an input, therefore I have to change it
// I also have to protect the clock

  PCI_Write_CFG_Reg(PCI_ISA_CONFIG,
                    0x90,
                     0x04 | (0xF7 & PCI_Read_CFG_Reg(PCI_ISA_CONFIG,
                                                   0x90,
                                                    1)),
                    1);

  PCI_Write_CFG_Reg(PCI_ISA_CONFIG,
                    0x91,
                    outByte | (0xFB & PCI_Read_CFG_Reg(PCI_ISA_CONFIG,
                                                       0x91,
                                                       1)),
                    1);

//  PCI_Write_CFG_Reg(PCI_ISA_CONFIG,
//                    0x6D,
//                    outByte | (0xEF & PCI_Read_CFG_Reg(PCI_ISA_CONFIG,
//                                                       0x6D,
//                                                       1)),
//                    1);
}

/*
===============================================================================
  OUTPUT:
    return
      State of SDIN is returned in least significant bit.
===============================================================================
*/
static uchar SDIN(void)
{
  return (PCI_Read_CFG_Reg(PCI_ISA_CONFIG, 0x91, 1) >> 2) & 0x01;
//  return (PCI_Read_CFG_Reg(PCI_ISA_CONFIG, 0x6D, 1) >> 4) & 0x01;
}

#define DELAY(usecs) Wait_uSecs(usecs)
//#define DELAY(usecs) delayuSecs(usecs)

/*
===============================================================================
  Start is a high to low transition of SDOUT while SCLK is high.

  SDA   1:-----7--\__5______
  SCLK  :-----7-----5-\__7__
===============================================================================
*/
static void I2C_start(void)
{

  DELAY(2);
  SDOUT(1);
  DELAY(1);
  SCLK(1);
  DELAY(7);
  SDOUT(0);
  DELAY(5);
  SCLK(0);
  DELAY(7);
  SDOUT(1);
}

/*
===============================================================================
  Stop is a low to high transition of SDOUT while SCLK is high.

  SDA   \___5_/--
  SCLK   /--5----
===============================================================================
*/
static void I2C_stop(void)
{
  SDOUT(0);
  DELAY(2);
  SCLK(1);
  DELAY(5);
  SDOUT(1);
  DELAY(2);
}

/*
===============================================================================
  INPUT:
    Data
      Byte to output.

  SDA   ::::::DDDDDDDDD:DDDDDDDDD:DDDD:  ::::::::::
  SCLK  :_______/---\_____/---\_____/--  :::--\____
===============================================================================
*/
static void I2C_writeByte(uchar Data)
{
  int i;
  uchar mask = 0x80;

  SCLK(0);
  DELAY(7);
  for (i = 0; i < 8; i++)
  {
    DELAY(2);   /* Tdh */
    SDOUT(0 != (Data & mask));
    mask = mask >> 1;
    DELAY(5);   /* Remainder of Tlow  */
    SCLK(1);
    DELAY(5);   /* Thigh */
    SCLK(0);
  }
  SDOUT(1);
  DELAY(7);     /* Remainder of Tlow */
}

/*
===============================================================================
  SDA   :::::/-----
  SCLK  ::::\__________
===============================================================================
*/
static uchar I2C_readByte(void)
{
  int i;
  uchar outByte = 0;

  SDOUT(1);
  for (i = 0; i < 8; i++)
  {
    DELAY(6);
    SCLK(1);
    outByte = (outByte << 1) | SDIN();
    DELAY(5);
    SCLK(0);
    DELAY(1);
    SDOUT(1);
  }
  DELAY(6);
  return outByte;
}

/*
===============================================================================
  SDA   :_____2_/-7--
  SCLK   1:-5-\______
===============================================================================
*/
static void I2C_sendAck(void)
{
  SDOUT(0);
  DELAY(1);
  SCLK(1);
  DELAY(5);
  SCLK(0);
  DELAY(2);
  SDOUT(1);
  DELAY(7);
}

/*
===============================================================================
  SDA   :___________
  SCLK  1:--5-\__7__
===============================================================================
*/
static void I2C_sendNack(void)
{
  DELAY(5);
  SDOUT(1);
  DELAY(1);
  SCLK(1);
  DELAY(5);
  SCLK(0);
  DELAY(7);
  SDOUT(0);
}

/*
===============================================================================
  SDA   :-1-:::::::::
  SCLK  :::::----\_____
===============================================================================
*/
static int I2C_waitOnAck(void)
{
  int i;
  int retVal = -1;

  SDOUT(1);
  DELAY(1);
  SCLK(1);
  for (i = 0; i < 1500; i++)
  {
    DELAY(10);
    if (SDIN() == 0)
    {
      retVal = 0;
      break;
    }
  }
  SCLK(0);
  DELAY(1);
  SDOUT(1);
  DELAY(4);
  return retVal;
}

/*
===============================================================================
  DESCRIPTION:
    This is equivalent to a Current or Sequential Read.  If nbyte = 1, then
    is a Current Address Read.  Otherwise it is a Sequential Read from the
    current address.

  INPUT:
    buff
      Pointer to where bytes are to be copied.
    devAddr
    nBytes
      The number of bytes to be read.
  OUTPUT:
    return
      If <0 then error in read.
===============================================================================
*/
int i2cReadCurrentBytes(uchar* buff, uchar devAddr, int nBytes)
{
  int i;

  I2C_start();
  I2C_writeByte(devAddr | 0x01);
  if (I2C_waitOnAck() != 0)
  {
    return -1;
  }
  *buff++ = I2C_readByte();
  for (i = 1; i < nBytes; i++)
  {
    I2C_sendAck();
    *buff++ = I2C_readByte();
  }
  I2C_sendNack();
  I2C_stop();
  return 0;
}

/*
===============================================================================
  This is equivalent to a Random Read or a Random Read followed by a Sequential
  Read.  If nbytes = 1, then this is a Random Read.  If nbytes > 1, then this
  is equivalent to a Random Read followed by a Sequential Read.

  INPUT:
    buff
      Pointer to where bytes are to be copied.
    devAddr
    startAddr
    nBytes
      The number of bytes to be read.

  OUTPUT:
    return
      If <0 then error in read.
-------------------------------------------------------------------------------
*/
int i2cReadBytes(uchar* buff, uchar devAddr, uchar startAddr, int nBytes)
{
  int retVal;
  unsigned long flags;

  local_save_flags(flags);
  local_irq_disable();

//  enableSEEPROM;
  I2C_start();
  I2C_writeByte(devAddr & 0xFE);
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }
  I2C_writeByte(startAddr);
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }
  retVal = i2cReadCurrentBytes(buff, devAddr, nBytes);

  local_irq_restore(flags);
  return retVal;
}

/*
===============================================================================
  This is equivalent to a Random Read.

  INPUT:
    devAddr
    startAddr

  OUTPUT:
    return
      If <0 then error in read, else returns byte read.
-------------------------------------------------------------------------------
*/
int i2cReadByte(uchar devAddr, uchar startAddr)
{
  int retVal;
  uchar tmp;

  retVal = i2cReadBytes(&tmp, devAddr, startAddr, 1);

  if (retVal == 0)
  {
    return tmp;
  }
  return retVal;
}

/*
===============================================================================
  DESCRIPTION:
    This is equivalent to a Byte Write or a Page Write.  If nBytes = 1, then
    this a Byte Write, otherwise it is a Page Write.

  INPUT:
    buff
      Pointer to data to be written.
    devAddr
      Device address
    startAddr
      Offset into device of first byte to be written.
    nBytes
      Number of bytes to be written

  OUTPUT:
    return
      If <0 then error in write.
-------------------------------------------------------------------------------
*/
int i2cWriteBytes(uchar* buff, uchar devAddr, uchar startAddr, int nBytes)
{
  int i;
  unsigned long flags;

  local_save_flags(flags);
  local_irq_disable();

//  enableSEEPROM;
  I2C_start();
  I2C_writeByte(devAddr & 0xFE);
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }
/* Output start address */
  I2C_writeByte(startAddr);
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }

/* Output data */
  for (i = 0; i < nBytes; i++)
  {
    I2C_writeByte(*buff++);
    if (I2C_waitOnAck() != 0)
    {
      local_irq_restore(flags);
      return -1;
    }
  }
  I2C_stop();
/* !!!! Disable I2C???? */
  local_irq_restore(flags);
  return 0;
}

/*
===============================================================================
  DESCRIPTION:
    This is equivalent to a Byte Write or a Page Write.  If nBytes = 1, then
    this a Byte Write, otherwise it is a Page Write.

  INPUT:
    devAddr
      Device address
    startAddr
      Offset into device of first byte to be written.
    data
      Byte to be written.

  OUTPUT:
    return
      If <0 then error in write.
-------------------------------------------------------------------------------
*/
int i2cWriteByte(uchar devAddr, uchar startAddr, uchar data)
{
  uchar tmp = data;

  return i2cWriteBytes(&tmp, devAddr, startAddr, 1);
}

/*
===============================================================================
  This is a direct read to the i2c bus.

  INPUT:
    devAddr

  OUTPUT:
    information that was read
-------------------------------------------------------------------------------
*/
int i2cReadDirectByte(uchar devAddr)
{
  uchar tmp;
  unsigned long flags;

  local_save_flags(flags);
  local_irq_disable();

  I2C_start();
  I2C_writeByte(devAddr | 0x01);
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }                          
  tmp = I2C_readByte();
  I2C_stop();
  local_irq_restore(flags);
  return tmp;
}

/*
===============================================================================
  This is a direct write to the i2c bus.

  INPUT:
    devAddr

  OUTPUT:
    return 0
-------------------------------------------------------------------------------
*/
int i2cWriteDirectByte(uchar devAddr, uchar data)
{
  unsigned long flags;
 
  local_save_flags(flags);
  local_irq_disable(); 
  I2C_start();
  I2C_writeByte(devAddr & 0xFE);     
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }
  I2C_writeByte(data);
  if (I2C_waitOnAck() != 0)
  {
    local_irq_restore(flags);
    return -1;
  }
  I2C_stop();
  local_irq_restore(flags);
  return 0;
}                     

