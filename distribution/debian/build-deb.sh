#!/bin/bash
set -eou pipefail

REQUIRED_PACKAGES=(libva-dev libdrm-dev libx11-dev libwayland-dev meson)
MISSING=0

# check for missing dependencies
function package_exists() {
  dpkg -l "$1" &> /dev/null
  return $?
}

for pkg in "${REQUIRED_PACKAGES[@]}"; do
  if ! package_exists "$pkg"; then
      echo "ERROR: $pkg is not installed on your machine."
      MISSING=1
  fi
done

if [ $MISSING -ne 0 ]; then
  exit 1
fi

# enter the build directory
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
