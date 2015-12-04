Linux kernel 3.0.x for gamestick based on https://github.com/Stane1983/kernel-amlogic-mx
## Setup development machine install cross compiler
You will need Ubuntu 14.04 with development packages installed and third party gcc cross compiler 4_9-2015q3 from [launchpad](https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q3-update/+download/gcc-arm-none-eabi-4_9-2015q3-20150921-linux.tar.bz2) extracted to /opt

## Customise Initramfs
The initramfs image, i.e. initramfs.cpio.lzma, used in the kernel is based on ubuntu 14.04 armhf platform with added scripts/init-premount/ubi to mount 3 system ubi partitions. You can modify the initramfs image accordingly.

## Build
Run the following command to build the kernel and modules
```
export PATH=/opt/gcc-arm-none-eabi-4_9-2015q3/bin:$PATH
cp gamestick.config .config
make ARCH=arm CROSS_COMPILE=arm-none-eabi- uImage
make ARCH=arm CROSS_COMPILE=arm-none-eabi- INSTALL_MOD_PATH=modules modules
make ARCH=arm CROSS_COMPILE=arm-none-eabi- INSTALL_MOD_PATH=modules modules_install
```
If you would prefer to create your own kernel configuration from scratch then you can replace the cp command with
```
make ARCH=arm CROSS_COMPILE=arm-none-eabi- meson6_g02_dongle_defconfig
make ARCH=arm CROSS_COMPILE=arm-none-eabi- menuconfig
```
## Install
### Install from recovery
### Install within Linux
Copy the uImage from arch/arm/boot/uImage on the build machine to gamestick
then run the following command on the gamestick
```
flash_erase /dev/mtd1 0 0
nandwrite -p /dev/mtd1 uImage
```
## What's working
The following basic functions essential for hedless server are tested fullly working
* CPU with frequency scaling
* Memory
* NAND/MTD
* USB OTG
* SDIO WIFI
* USB Ethernet on GameStick Dock
* USB Hard Disks

## Unbrick gamestick with bad MTD partition table
The NAND/MTD partition table is embeded into the linux kernel as defined in customer/boards/board-m6g02-dongle.c. If the uboot got stuck at reading the NAND/MTD partition table you can unbrick it by short NAND pin7-8 while booting up for couple of seconds. Reference [here](http://www.slatedroid.com/topic/41478-how-to-unbrick-your-amlogic-aml8726-mx-series-tablet-v20/).
