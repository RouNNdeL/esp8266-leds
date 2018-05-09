#!/usr/bin/env bash

cd /home/pi/Programs/WiFiController
git pull
platformio run
VERSION_CODE=$(cat src/config.h | sed -nr 's/#define VERSION_CODE ([0-9]+)/\1/p')
cd .pioenvs/
for d in */; do
mkdir -p $1/$d
cp $d/firmware.bin $1/$d/bin_$VERSION_CODE.bin
done