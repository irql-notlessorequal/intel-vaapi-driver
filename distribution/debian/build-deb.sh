#!/bin/bash
set -eou pipefail

cd $(dirname "$0")
cd ../../

# verify we are in intel-vaapi-driver
currentdir=$(basename $(pwd))
if [ ! $currentdir == 'intel-vaapi-driver' ]; then
  echo $currentdir
  echo 'Wrong dir';
  exit;
fi

# build
sudo apt install libva-dev libx11-dev libdrm-dev meson
meson build
cd build
ninja

# prepare to build deb
cd ..
mkdir -p distribution/debian/i965-va-driver/usr/lib/x86_64-linux-gnu/dri
cp build/src/i965_drv_video.so distribution/debian/i965-va-driver/usr/lib/x86_64-linux-gnu/dri

# build deb
cd distribution/debian
dpkg-deb --build i965-va-driver

echo ''
echo 'Ubuntu/debian package has been built at distribution/debian/i965-va-driver.deb'
echo 'Install with: sudo dpkg -i i965-va-driver.deb'
echo ''
echo 'To revert back to the regular driver, you can install i965-va-driver-shaders'
echo 'with: sudo apt install i965-va-driver-shaders - which removes the existing'
echo 'i965-va-driver. Then run: sudo apt install i965-va-driver - to install the'
echo 'regular driver from the official repo.'
