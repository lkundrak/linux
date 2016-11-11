/* Stuff Taken from the bootcode to allow this module to work */

#include "seeprom.h"
#include "other.h"



/*
===============================================================================
  PROCEDURE:
    ulong    PCI_Read_CFG_Reg(int BusDevFunc, int Reg, int Width);

  Description:
               Read a PCI configuration register

  Inputs:
               BusDevFunc      PCI Bus+Device+Function number
               Reg             Configuration register number
               Width           Number of bytes to read (1, 2, or 4)

  Return value:
               (ulong)  Value of the configuration register read.
                       For reads shorter than 4 bytes, return value
                       is LSB-justified
-------------------------------------------------------------------------------
*/

unsigned long PCI_Read_CFG_Reg(int BusDevFunc, int Reg, int Width)
{
  unsigned long    RegAddr;

  RegAddr = 0x80000000 | ((Reg|BusDevFunc) & 0xFFFFFFFC);

  outpl(PCI_CONFIG_ADDR, RegAddr);

  switch (Width)
  {
    case 1:
      return ((ulong) inpb(PCI_CONFIG_DATA | (Reg & 3)));

    case 2:
      return ((ulong) inps(PCI_CONFIG_DATA | (Reg & 3)));

    case 4:
      return (inpl(PCI_CONFIG_DATA | (Reg & 3)));
  }
  return -1;
}


/*
===============================================================================
  PROCEDURE:
    int PCI_Write_CFG_Reg(int BusDevFunc, int Reg, ulong Value, int Width);

  DESCRIPTION:
    Write a PCI configuration register.

  INPUTS:
    BusDevFunc      PCI Bus+Device+Function number
    Reg             Configuration register number
    Value           Configuration register value
    Width           Number of bytes to write (1, 2, or 4)

  Return value:
    0       Successful
    != 0    Unsuccessful
-------------------------------------------------------------------------------
*/
int     PCI_Write_CFG_Reg(int BusDevFunc, int Reg, ulong Value, int Width)
{
  ulong    RegAddr;

  RegAddr = 0x80000000 | ((Reg|BusDevFunc) & 0xFFFFFFFC);

  outpl(PCI_CONFIG_ADDR, RegAddr);

  switch (Width)
  {
    case 1:
      outpb(PCI_CONFIG_DATA | (Reg & 3), (uchar) (Value & 0xFF));
      break;

    case 2:
      outps(PCI_CONFIG_DATA | (Reg & 3), (ushort) (Value & 0xFFFF));
      break;

    case 4:
      outpl(PCI_CONFIG_DATA | (Reg & 3), Value);
      break;
  }

  return (0);
}


