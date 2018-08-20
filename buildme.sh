#make ARCH=arm CROSS_COMPILE=arm-linux-gnu- mmp2_defconfig oldconfig
#time make ARCH=arm CROSS_COMPILE=arm-linux-gnu- -j16
#perl append.pl ./arch/arm/boot/zImage ./arch/arm/boot/dts/mmp2-brownstone.dtb >/run/media/lkundrak/19DE-9DE4/boot/zImage && umount /run/media/lkundrak/19DE-9DE4
#make ARCH=arm CROSS_COMPILE=arm-linux-gnu- mmp2_defconfig oldconfig
#make ARCH=arm CROSS_COMPILE=arm-linux-gnu- olpc_xo175_defconfig savedefconfig; cp defconfig ./arch/arm/configs/olpc_xo175_defconfig
stty -F /dev/xoec 406:0:18b2:8a30:3:1c:7f:15:4:2:64:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0
echo P0 >/dev/xoec 
time make ARCH=arm CROSS_COMPILE=arm-linux-gnu- savedefconfig
cp defconfig ./arch/arm/configs/olpc_xo175_defconfig
time make ARCH=arm CROSS_COMPILE=arm-linux-gnu- -j4 olpc-zImage &&
echo P1 >/dev/xoec 
