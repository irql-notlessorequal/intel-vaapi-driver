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
meson build
cd build
ninja

# prepare to build deb
cd ..
cp build/src/i965_drv_video.so distribution/debian/i965-va-driver/usr/lib/x86_64-linux-gnu/dri
# cp src/.libs/i965_drv_video.so distribution/debian/i965-va-driver/usr/lib/x86_64-linux-gnu/dri
# sudo chown -R root:root distribution/debian/i965-va-driver

# build deb
cd distribution/debian
dpkg-deb --build i965-va-driver

echo 'Ubuntu/debian package has been built at distribution/debian/i965-va-driver.deb'
echo 'Install with: sudo dpkg -i i965-va-driver.deb'
