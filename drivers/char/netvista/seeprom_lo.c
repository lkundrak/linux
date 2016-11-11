#include <linux/types.h>
#include "seeprom.h"
#include <linux/delay.h>
#include <linux/interrupt.h>
#include "i2c.h"
#include "other.h"

int smbus_byte_data_read(u_char deviceAddr, u_char addr);
int smbus_byte_data_write(u_char deviceAddr, u_char addr, u_char data);


/*
===============================================================================
  Functions to read and write the SEEPROM.
===============================================================================
 */

/*
===============================================================================
  DESCRIPTION:
    Write the data the seeprom.

  INPUT:
    data
      Byte to written.

  NOTES:
    Supports 4K-bit 8-bit wide SEEPROM.

    Need to add error handling.
-------------------------------------------------------------------------------
*/
int write_byte_seeprom(ushort addr, u_char data)
{
    unsigned long flags;

    local_save_flags(flags);
    local_irq_disable();
    if (addr > (ushort)0xFF)
    {
        udelay(3000);
        i2cWriteByte(SEEPROM_ADDRESS | 0x02, 0x0FF & addr, data);
    }
    else
    {
        udelay(3000);
        i2cWriteByte(SEEPROM_ADDRESS, 0x0FF & addr, data);
    }
    local_irq_restore(flags);
    return 0;
}

/*
===============================================================================
  DESCRIPTION:
    Write the data passed to the System Clock.

  INPUT:
    data
      Pointer to data to be written to System Clock.

  NOTES:
    Supports 4K-bit 8-bit wide SEEPROM.

    Need to add error handling.
-------------------------------------------------------------------------------
*/
int read_byte_seeprom(ushort addr)
{
    int ret_val;
    unsigned long flags;

    local_save_flags(flags);
    local_irq_disable();
    if (addr > (ushort)0xFF)
    {
        ret_val = i2cReadByte(SEEPROM_ADDRESS | 0x02, 0x0FF & addr);
    }
    else
    {
        ret_val = i2cReadByte(SEEPROM_ADDRESS, 0x0FF & addr);
    }
    local_irq_restore(flags);
    return ret_val;
}

/*
===============================================================================
  DESCRIPTION:
    Write data to the I/O Expander.

  INPUT:
    Byte to be written.

  NOTES:
    Be sure you use read to Mask any inputs.
    Need to add error handling.
-------------------------------------------------------------------------------
*/
int write_byte_ioexpander(uchar data)
{
    Wait_uSecs(3000);
    i2cWriteDirectByte(IOEXPANDER_ADDRESS,data|IOEXPANDER_INPUTS);
    return 0;
}

/*
===============================================================================
  DESCRIPTION:
    Read data from I/O expander.

  INPUT:
    Data to be written to I/O expander.

  NOTES:
    Need to add error handling.
-------------------------------------------------------------------------------
*/
int read_byte_ioexpander(void)
{
   return i2cReadDirectByte(IOEXPANDER_ADDRESS);
}
