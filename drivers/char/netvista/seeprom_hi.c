/*
===============================================================================
  NOTES:
    This module contains code for reading and writing "nvram".  This "nvram" may
    actually be NVRAM or it may be a SEEPROM, FLASH, or some other type of
    nonvolatile memory.  Or it may just simulate nonvolatile memory.

  CHANGES:
    11/02/99 JMS
    Initial version based on
        base/nvram.c, ncbootfw_base, hondofw2.0 1.14 8/12/99 15:51:22.

    Modified for use in driver "module" for Linux to access SEEPROM.

    04/27/00 JMS
    Fixed sumcheck calculation.  The sizes used in the sumcheck calculations
    was based on the sizeof() parts of a structure in seeprom.h.  The sizes
    that should be used are the values stored in the structure.
===============================================================================
*/

//#define NVRAM_DEBUG

#include <linux/types.h>
#include <linux/delay.h>

#include "seeprom.h"

#define FromBE_short(w) (((w & 0x00FF) <<  8) | ((w & 0xFF00) >>  8))

/*
  This is the number of bytes to add to the ->size for sumchecking.  This is
  the size of size, sumcheck, and the NVRAM version.
*/
#define sumcheckFudge 4

#define bnvram ((boot_nvramDef*)(&(nvramShadow.boot_nvram)))
#define fnvram ((fixed_nvramDef*)(&(nvramShadow.fixed_nvram)))
#define osnvram ((os_nvramDef*)(&(nvramShadow.os_nvram)))

#define sumcheck_osnvram \
  simpleSumcheck((unsigned char*)(((ulong)(&(osnvram->size))) - FromBE_short(osnvram->size)), \
                         sumcheckFudge + FromBE_short(osnvram->size))
#define sumcheck_bnvram \
  simpleSumcheck((unsigned char*)(&(nvramShadow.boot_nvram.version)), \
                       sumcheckFudge + FromBE_short(bnvram->size))


total_nvramDef nvram;           // All of "NVRAM".
total_nvramDef nvramShadow;     // Shadow of all of "NVRAM".

int smbus_init(void);
static u_char simpleSumcheck(u_char *start, int len);

/*
===============================================================================
  DESCRIPTION:
    This reads the nvram into memory and verifies the data.

  OUTPUT:
    Returns 0 if successful and data is good.

  NOTES:
    This must be run before accessing NVRAM data.

    There is no checking of the version or sumchecks.  This shouldn't be
    necessary unless version is important or the method of calculating the
    sumchecks changes.  Version will be important if some information required
    is not available in an earlier version and a value of 0 for the information
    will cause problems or if some information used by the OS has moved or the
    meaning of the values has changed between versions.

    There should be no sumcheck errors unless there is a hardware problem or if
    the bootcode or OS has a software bug in the "NVRAM" code or the definition
    of the sumchecks has changed between "NVRAM" versions.
-------------------------------------------------------------------------------
*/
int nvram_init(void)
{
    u_char *ucPtr0 = (u_char*)(&nvram);
    u_char *ucPtrShadow = (u_char*)(&nvramShadow);
    uint i;


#ifdef NVRAM_DEBUG
    printk(KERN_DEBUG "retVal = 0x%08X", retVal);
#endif /* NVRAM_DEBUG */
/* Copy NVRAM into shadow copy */
    for (i = 0; i < sizeof(total_nvramDef); i++)
    {
        ucPtr0[i] = read_byte_seeprom(i);
        ucPtrShadow[i] = ucPtr0[i];
    }
    return 0;
}

/*
===============================================================================
  DESCRIPTION:
    Writes all bytes that have changed to the "nvram".

  OUTPUT:
    Returns 0 if successful and data is good.
-------------------------------------------------------------------------------
*/
int nvram_update(void)
{
  u_char *ucPtr0 = (u_char*)(&nvram);
  u_char *ucPtrShadow = (u_char*)(&nvramShadow);
  unsigned int i;
  u_char sum = 0;
  int change = 0;

/* Calculate boot portion sumcheck */
  nvramShadow.boot_nvram.sumcheck = 0;
  sum = sumcheck_bnvram;
  nvramShadow.boot_nvram.sumcheck = -sum;

  nvramShadow.os_nvram.sumcheck = 0;  // Clear sumcheck
  sum = sumcheck_osnvram;
  nvramShadow.os_nvram.sumcheck = -sum;

#ifdef NVRAM_DEBUG
  printk("\nNVRAM update\n");
  printk("\n-sum = 0x%02X,0x%02x\n",
              nvramShadow.boot_nvram.sumcheck,nvram.boot_nvram.sumcheck);
#endif /* NVRAM_DEBUG */
  for (i = 0; i < sizeof(total_nvramDef); i++)
  {
    if (ucPtr0[i] != ucPtrShadow[i])
    {
      change = 1;
      write_byte_seeprom((ushort)i, ucPtrShadow[i]);
      ucPtr0[i] = ucPtrShadow[i];
#ifdef NVRAM_DEBUG
printk("%02X* ", ucPtrShadow[i]);
#endif /* NVRAM_DEBUG */
    }
    else
    {
#ifdef NVRAM_DEBUG
printk("%02X  ", ucPtrShadow[i]);
#endif /* NVRAM_DEBUG */
    }
  }
  if (change != 0)
  {
#if 1 /* max udelay is 20000 for i386 */
    udelay(20000);
    udelay(20000);
    udelay(20000);
    udelay(20000);
    udelay(20000);
#else
    udelay(100000);
#endif
  }
/*!!!!! Need to add error handling. !!!!!*/
#ifdef NVRAM_DEBUG
printk("\n");
#endif /* NVRAM_DEBUG */
  return 0;
}

void nvram_restore(void)
{
  u_char *ucPtr0 = (u_char*)(&nvram);
  u_char *ucPtrShadow = (u_char*)(&nvramShadow);
  unsigned int i;

  for (i = 0; i < sizeof(total_nvramDef); i++)
  {
    ucPtrShadow[i] = ucPtr0[i];
  }
}

#if 0
void nvramShadow_dump(void)
{
  u_char *nvramptr = (u_char *)&nvramShadow;
  uint i;

  for (i = 0; i < sizeof(total_nvramDef); i++)
  {
    if (0 == (i%24))
    {
      printf("\n");
      printf("%04X  ", i);
    }

    printf("%02X ", (uint)*nvramptr++);
  }
}

/*
===============================================================================
  DESCRIPTION:
    Sumcheck of NVRAM.

  OUTPUT:
    Returns sumcheck.
-------------------------------------------------------------------------------
*/
static u_char NVRAMSUMCHECK(NVRAMDEF *NVR)
{
  int start;
  uint i;
  u_char sum = 0;
  u_char *ucptr = (u_char*)nvr;

  start = (ulong)(&(nvr->version)) - (ulong)nvr;
#ifdef NVRAM_DEBUG
printk("\nnvramSumCheck(0x%08X), start = 0x%04X", nvr, start);
printk("\n0x%08X", (ulong)(&(nvr->version)));
#endif /* NVRAM_DEBUG */
  for (i = start; i < sizeof(nvramDef); i++)
  {
    sum += ucptr[i];
  }
#ifdef NVRAM_DEBUG
printk(" sum = 0x%02X", sum);
#endif
  return sum;
}
#endif

/*
===============================================================================
  DESCRIPTION:
    Sumcheck of NVRAM.

  OUTPUT:
    Returns sumcheck.
-------------------------------------------------------------------------------
*/
static u_char simpleSumcheck(u_char *start, int len)
{
  int i;
  u_char sum = 0;

#ifdef NVRAM_DEBUG
printk("\nnvramSumCheck(0x%08X, 0x%04X)", start, len);
#endif /* NVRAM_DEBUG */
  for (i = 0; i < len; i++)
  {
    sum += *start++;
  }
#ifdef NVRAM_DEBUG
printk(" sum = 0x%02X", sum);
#endif
  return sum;
}
