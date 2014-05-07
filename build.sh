#!/bin/bash

export LOCALVERSION="-XpauM-0.1"
export CROSS_COMPILE=/home/xpaum/android/arm-cortex_a15-toolchain/bin/arm-cortex_a15-linux-gnueabihf-
export ARCH=arm
export KBUILD_BUILD_USER=xpaum
export KBUILD_BUILD_HOST="host"

echo
echo "Making f4ktion_defconfig"

DATE_START=$(date +"%s")

#make msm8930_cane_defconfig VARIANT_DEFCONFIG=msm8930_cane_wilcox_eur_lte_defconfig SELINUX_DEFCONFIG=selinux_defconfig
#make menuconfig

INIT_DIR=/home/xpaum/android/boottools/g3815
MODULES_DIR=../cwm/lib/modules
KERNEL_DIR=`pwd`
OUTPUT_DIR=output/
CWM_DIR=../cwm
CWM_ANY_DIR=../boottools

echo
echo "Removing old kernels files"
if [ -e $KERNEL_DIR/arch/arm/boot/zImage ]; then
rm $CWM_DIR/boot.img
rm $CWM_ANY_DIR/zImage
rm arch/arm/boot/zImage
else
echo "No old kernels found"
fi

echo
echo "Removing old modules"
rm -R `find $KERNEL_DIR -name '*.ko'`
rm `echo $MODULES_DIR"/*"`
rm `echo $CWM_DIR/system/lib/modules/"*.ko"`

echo
echo "LOCALVERSION="$LOCALVERSION
echo "CROSS_COMPILE="$CROSS_COMPILE
echo "ARCH="$ARCH
echo "INIT_DIR="$INIT_DIR
echo "MODULES_DIR="$MODULES_DIR
echo "KERNEL_DIR="$KERNEL_DIR
echo "OUTPUT_DIR="$OUTPUT_DIR
echo "CWM_DIR="$CWM_DIR
echo "CWN_ANY_DIR="$CWM_ANY_DIR

make -j4

echo
find $KERNEL_DIR -name '*.ko' -exec cp -v {} $MODULES_DIR \;
find $MODULES_DIR -name '*.ko' -exec cp -v {} $CWM_DIR"/lib/modules/" \;

echo
if [ -e $KERNEL_DIR/arch/arm/boot/zImage ]; then
cp arch/arm/boot/zImage $CWM_ANY_DIR
cd $CWM_ANY_DIR
./mkbootfs $INIT_DIR| gzip > $CWM_ANY_DIR/ramdisk.gz
./mkbootimg --cmdline 'console = null androidboot.hardware=qcom user_debug=31' --kernel zImage --ramdisk ramdisk.gz --base 0x80200000 --pagesize 2048 --ramdiskaddr 0x82200000 --output ../cwm/boot.img
echo
echo "Make zip package"
cd ../cwm
zip -r `echo kernel_update`.zip *
mv `echo kernel_update`.zip ../$OUTPUT_DIR
cd ../$OUTPUT_DIR
echo
FILE_NAME=kernel_update.zip
FILE_SIZE=$(stat -c%s "$FILE_NAME")
echo "kernel_update.zip size is $FILE_SIZE bytes."
else
echo "KERNEL DID NOT BUILD! no zImage exist"
fi;

cd $KERNEL_DIR

DATE_END=$(date +"%s")
echo
DIFF=$(($DATE_END - $DATE_START))
echo "Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."
