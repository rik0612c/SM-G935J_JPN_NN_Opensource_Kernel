#!/bin/bash

export ARCH=arm64
export PATH=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PATH

mkdir output

make -C $(pwd) O=output CROSS_COMPILE=aarch64-linux-android- KCFLAGS=-mno-android hero2qlte_jpn_kdi_defconfig 
make -j64 -C $(pwd) O=output CROSS_COMPILE=aarch64-linux-android- KCFLAGS=-mno-android

cp output/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image
