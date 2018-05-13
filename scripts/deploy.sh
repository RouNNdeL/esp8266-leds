#!/usr/bin/env bash


VERSION_CODE=$(cat src/config.h | sed -nr 's/#define VERSION_CODE ([0-9]+)/\1/p')
cd .pioenvs/
for d in */; do
mkdir -p $1/$d
scp -i deploy_key $d\firmware.bin pi@zdul.xyz:/var/www/html/smart/iot_binaries/$d/bin_$VERSION_CODE.bin
done