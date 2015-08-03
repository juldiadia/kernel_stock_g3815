export PATH=$PATH:/home/xpaum/android/arm-toolchain/bin

make ARCH=arm CROSS_COMPILE=arm-eabi- $1
