#!/usr/bin/env bash

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$parent_path"

VERSION_CODE=$(cat ../src/config.h | sed -nr 's/#define VERSION_CODE ([0-9]+)/\1/p')
cd ./../.pioenvs/
for d in */; do
ssh -i ../deploy_key -o "UserKnownHostsFile ../known_hosts" pi@zdul.xyz "mkdir -p /var/www/html/smart/iot_binaries/$d"
scp -i ../deploy_key -o "UserKnownHostsFile ../known_hosts" $d\firmware.bin pi@zdul.xyz:/var/www/html/smart/iot_binaries/$d/bin_$VERSION_CODE.bin
done