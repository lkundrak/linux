/* @(#)base/seeprom.h, ncbootfw_base, hondofw2.0 1.4 5/17/99 12:08:14 */
/*
===============================================================================
  NOTES:
    The driver only needs the sizes and sumcheck locations from this structure.

    Any OS application accessing NVRAM will may need an up to date structure.

  CHANGES:
    04/27/00 JMS
    Updated to match latest structure used in boot code.

===============================================================================
*/
#ifndef __SEEPROM_H
#define __SEEPROM_H

#include <linux/types.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/ioctl.h>


#define uchar u_char
#define SEEPROM_UPDATE  _IO('E', 0x40)  /* Write to SEEPROM (with sumchecks) */

#pragma pack(1) /* These structures must be packed. */
typedef struct
{
  uchar unitSN[16];     /* Unit Serial Number                          0x000 */
  uchar unitMN[8];      /* Unit Model Number                           0x010 */
  uchar permMacAddr[6]; /* MAC address                                 0x018 */
} fixed_nvramDef;

typedef struct
{
  uchar version;        /* Version                                     0x01E */
  uchar sumcheck;       /* Sumcheck for bootcode portion of NVRAM      0x01F */
  ushort size;          /* Size of bootcode portion of NVRAM           0x020 */
  uchar macAddress[6];  /* MAC address                                 0x022 */
  ulong ipAddress;      /* IP address of system                        0x028 */
  ulong ipGateway;      /* IP address of gateway                       0x02C */
  ulong ipSubnetMask;   /* IP address of subnet mask                   0x030 */
  ulong ipBootServer1;  /* IP address of bootserver 1                  0x034 */
  ulong ipBootServer2;  /* IP address of bootserver 2                  0x038 */
  ulong ipBootServer3;  /* IP address of bootserver 3                  0x03C */
  ulong ipNameServer;   /* DNS name server                             0x040 */

/* Ethernet */
  uchar ethMode:4,
          /* Bit 0  b'0' = full duplex, b'1' = half duplex             0x044 */
          /* Bit 2,1  b'00' = 10Mb; b'01' = 100Mb; b'10' = 1Gb         0x044 */
          /* Bit 3  b'0' = Autonegotiate; b'1' = Manually set          0x044 */
        ethFrameType:1, /* Bit 4  b'0' = standard V2; b'1' = IEEE 802.30x044 */
        ethReserved:3;

/* Token Ring */
  uchar trMode:4,
          /* Bit 0  b'0' = full duplex; b'1' = half duplex             0x045 */
          /* Bit 2,1 b'00' = 4Mb; b'01' = 16Mb; b'10' = 100Mb          0x045 */
          /* Bit 3  b'0' = Auto Sense;  b'1' = Manually set            0x045 */
    trReserved:4;

  ushort mtu_size;      /* MTU Size for Token Ring                     0x046 */

  uchar ipUseAddrDiscovery:1,   /* ip Use Address Discovery          0 0x048 */
    tcpipBroadcastBootReq:1,    /* TCPIP Broadcast Boot Request      1 0x048 */
    enableBroadcastBoot:1,      /* Enable Broadcast Boot             2 0x048 */
    dupMode:1,                  /* Enable Duplication of DLL packets 3 0x048 */
    tftpsbDebugMode:1,          /* Send TFTPSB messages to 3rd party 4 0x048 */
    macAddrSel:1,               /* MAC address selection (Local/Perm)5 0x048 */
    remoteMessEnable:1,         /* debug_printf messages to network  6 0x048 */
    gotoMenuInterrupt:1;        /* go to menus not OS interrupt      7 0x048 */

/* Boot flags */
  uchar verboseMode:1,  /* If non-zero debug messags enabled         0 0x049 */
    bootPersLoading:1,  /* Boot persistent loading                   1 0x049 */
    fastBoot:1,         /* Boot persistent loading                   2 0x049 */
    bootTestRam:1,      /* Boot test RAM                             3 0x049 */
    bootAutomatic:1,    /* Boot automatically                        4 0x049 */
    numLock:1,          /* numLock on at boot                        5 0x049 */
    BadgerPowerState:2; /* 00 Full on, 01 soft off (Don't move!!) 6-7 0x049 */

  uchar nBootRetries;   /* Number of boot retries                      0x04A */

  /* Boot order */
  uchar bootpOrder:4,   /* Priority of BOOTP (0 - disabled)            0x04B */
    dhcpOrder:4;        /* Priority of DHCP (0 - disabled)             0x04B */
  uchar nvramOrder:4,   /* Priority of NVRAM (0 - disabled)            0x04C */
    localOrder:4;       /* Priority of Local (0 - disabled)            0x04C */

  ushort blockSize;     /* Block Size up to 65535(FFFF)                0x04D */

  uchar nfsOrder:4,     /* Priority of NFS (0 - disabled)              0x04F */
    tftpOrder:4;        /* Priority of TFTP (0 - disabled)             0x04F */

  uchar localFlashOrder:4, /* Priority of Local flash (0 - disabled)   0x050 */
    lfoReserved:4;

  uchar keyboardType;   /* Keyboard Language user selection            0x051 */
  uchar bootLanguage;   /* Language of boot code                       0x052 */
  uchar monitorNumber;  /* Monitor number                              0x053 */
  uchar colorDepth;     /* Color depth                                 0x054 */
  uchar tagStrings[255];    /* Tagged strings                          0x055 */
  uchar password[48];       /* Password                                0x154 */
  ulong dupAddr;            /* IP address for DLL duplication          0x184 */
  ushort dupPort;           /* Port number for DLL duplication         0x188 */

  /* Retry Counts */
  uchar tftpDelay:4,    /* TFTP wait delay in seconds for retry        0x18A */
    tftpRetry:4;        /* TFTP retry count for read packets           0x18A */
  uchar nfsDelay:4,     /* NFS wait delay in seconds for retry         0x18B */
    nfsRetry:4;         /* NFS retry count for read packets            0x18B */

  uchar colorTheme:4,           /* Index to color themes               0x18C */
                                /* 0 - white on black                        */
                                /* 1 - black on white                        */
                                /* 2 - black on blue                         */
                                /* 3 - white on blue                         */
                                /* 4-15 undefined                            */
#ifdef FWREL3
    reserved_18C:2,             /* Identification of OS                0x18C */
    menuType:1,                 /* Type of menus                       0x18C */
    BadgerWOL:1;                /* Badger WOL (Do not move this bit!!) 0x18C */
#else
    reserved_18C:3,             /* Unused bits                         0x18C */
    BadgerWOL:1;                /* Badger WOL (Do not move this bit!!) 0x18C */
#endif /* FWREL3 */
  ulong altGateway;         /* Alternate gateway                       0x18D */
  ulong broadcastIPAddr;    /* Broadcast IP Address                    0x191 */
//  Norb's version ulong broadcastIP;    /* Broadcast IP address       0x191 */
  uchar bootDirName_2:4,    /* If 0 user defined else predefined index 0x195 */
    bootDirName_1:4;
  uchar wsConfigDirNamePredefined_1:4,      /*                         0x196 */
    bootDirName_3:4;
  uchar bootProtRetries; /* Boot Protocol retries.  0 = infinity       0x197 */
  uchar nvramVersionExtension:4,        /*                             0x198 */
    wsConfigDirNamePredefined_2:4;      /*                             0x198 */
  uchar nsbootDebugMode:1,              /*                             0x199 */
        monitorPM:1,                    /*                             0x199 */
        langHasBeenSelected:1,          /* Language selected           0x199 */
#ifdef FWREL3
        DHCP_override:1,                /* Override some DHCP parms    0x199 */
        CF_update:1,                    /* OS Compact flash update     0x199 */
        firstBoot:1,                    /* If 1, first time boot.      0x18C */
        resetNvram:1,                   /* Reset NVRAM                 0x199 */
        moreBootFlagsResv:1;            /*                             0x199 */
#else
        moreBootFlagsResv:5;            /*                             0x199 */
#endif /* FWREL3 */

  uchar retryAcc:4,     /* Boot Retry acceleration factor              0x19A */
    reserved_19A:4;     /* Extra space                                 0x19A */
} boot_nvramDef;                        /*                             0x19B */

/*
===============================================================================
  Definition of NVRAM structure for NCi OS data.
===============================================================================
*/
typedef struct
{
#ifdef FWREL3
  uchar camEnable:1,            /*                                     0x1C6 */
    camGenesis:1,               /*                                     0x1C6 */
    camExtReadMode:1,           /*                                     0x1C6 */
    camIntReadMode:1,           /*                                     0x1C6 */
    enableSMNPUpdate:1,         /*                                     0x1C6 */
    reserved_1C6:3;             /*                                     0x1C6 */
#endif /* FWREL3 */
  ulong authenIPAddr_1;         /*                                     0x1C7 */
  ulong authenIPAddr_2;         /*                                     0x1CB */
  uchar authenProtocol_1:4,     /*                                     0x1CF */
    authenProtocol_2:4;         /*                                     0x1CF */
  uchar flags;                  /*                                     0x1D0 */
  uchar ConfigurationFlags;     /*                                     0x1D1 */
  ulong ipNameServer1;          /*                                     0x1D2 */
  ulong ipNameServer2;          /*                                     0x1D6 */
  ulong ipTerminalServer1;      /*                                     0x1DA */
  ulong ipTerminalServer2;      /*                                     0x1DE */

  uchar protocolTermServer1:4;  /*                                     0x1E2 */
    uchar protocolTermServer2:4;/*                                     0x1E2 */

  uchar lightPenInfo[12];       /*                                     0x1E3 */
  uchar touchScreenInfo[12];    /*                                     0x1EF */

  ushort size;                  /*                                     0x1FB */
  uchar sumcheck;               /*                                     0x1FD */
  uchar version;                /*                                     0x1FE */
  uchar reserved;               /*                                     0x1FF */
} os_nvramDef;

#define UNUSED_NVRAM (0x200 - (sizeof(os_nvramDef) + sizeof(boot_nvramDef) + sizeof(fixed_nvramDef)))

typedef struct
{
  fixed_nvramDef fixed_nvram;
  boot_nvramDef boot_nvram;
  uchar unused[UNUSED_NVRAM];
  os_nvramDef os_nvram;
} total_nvramDef;
#pragma pack()      /* end of forcing packed structures */


#ifdef __KERNEL__

#ifndef SEEPROM_MAJOR
#define SEEPROM_MAJOR 0     /* dynamic major by default */
#endif

/* PCI Configuration Register to set SMBUS base address */
#define SMBUS_BASE_ADDRESS_REG  0x90

/* PCI Configuration Register to set Power Management Index base address */
#define PMI_BASE_ADDRESS_REG    0x40

extern int smbus_base_io;
extern int pmi_base_io;

#define outpb(a, b) outb(b, a)
#define inpb(a) inb(a)

#define outpl(a, b) outl(b, a)
#define inpl(a) inl(a)

#define outps(a, b) outw(b, a)
#define inps(a) inw(a)

#define SEEPROM_ADDRESS 0xA4

#define GPOREG_OFFSET   0x34
#define GPIREG_OFFSET   0x30

int write_byte_seeprom(ushort addr, u_char data);
int read_byte_seeprom(ushort addr);

#define IOEXPANDER_ADDRESS 0x04E
#define IOEXPANDER_INPUTS 0x01F
int write_byte_ioexpander(u_char data);
int read_byte_ioexpander(void);

extern ulong errFlags;
extern total_nvramDef nvramShadow;     // Shadow of all of "NVRAM".

int nvram_update(void);
int nvram_init(void);
void nvram_restore(void);
void nvramDefault(void);


#endif /* __KERNEL__ */

#endif  /* __SEEPROM_H */
