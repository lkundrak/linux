#ifndef __I2C_H
#define __I2C_H

int i2cWriteBytes(uchar* buff, uchar devAddr, uchar startAddr, int nBytes);
int i2cReadBytes(uchar* buff, uchar devAddr, uchar startAddr, int nBytes);
int i2cReadByte(uchar devAddr, uchar startAddr);
int i2cWriteByte(uchar devAddr, uchar startAddr, uchar data);
int i2cReadCurrentBytes(uchar* buff, uchar devAddr, int nBytes);

int i2cReadDirectByte(uchar devAddr);
int i2cWriteDirectByte(uchar devAddr, uchar data);    


#endif /* __I2C_H */
