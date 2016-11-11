/* protypes for other.c */

ulong PCI_Read_CFG_Reg(int BusDevFunc, int Reg, int Width);
int   PCI_Write_CFG_Reg(int BusDevFunc, int Reg, ulong Value, int Width);

#define PCI_ISA_CONFIG  0x80009000      /* ISA PCI Config Base       */
#define PCI_IDE_CONFIG  0x80009200      /* IDE PCI Config Base       */
#define PCI_PM_CONFIG   0x80009100      /* PM PCI Config Base        */


#define PCI_CONFIG_ADDR 0x00000CF8
#define PCI_CONFIG_DATA 0x00000CFC


#define Wait_uSecs udelay
