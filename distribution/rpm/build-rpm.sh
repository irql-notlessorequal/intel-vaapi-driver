#!/usr/bin/env bash
set -Eeuo pipefail

declare -r repo='irql-notlessorequal/intel-vaapi-driver'

# https://docs.github.com/en/rest/reference/repos#get-the-latest-release
function latest_version() {
  curl -sSL "https://api.github.com/repos/$repo/releases/latest" | jq -er '.tag_name'
}

declare -r src="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
cd "$src"

# see .spec's BuildRequires, also see
# dnf provides "pkgconfig(egl)" "pkgconfig(libdrm)" "pkgconfig(libva)" "pkgconfig(libva-drm)" "pkgconfig(libva-wayland)" "pkgconfig(libva-x11)" "pkgconfig(wayland-client)" "pkgconfig(wayland-scanner)"
echo 'Installing build dependencies...'
sudo dnf install gcc meson rpm-build jq \
  libglvnd-devel libdrm-devel libva-devel wayland-devel

declare -r version="${1:-"$(latest_version)"}"
echo "Building rpm for version $version..."

# see https://stackoverflow.com/a/46153901
rpmbuild \
  --undefine '_disable_source_fetch' --undefine 'source_date_epoch_from_changelog' \
  --define "_sourcedir $src" --define "_topdir $src" --define "version $version" \
  -bb --nodebuginfo \
  intel-vaapi-driver.spec

echo ''
echo 'You can now install the built package:'
echo "sudo dnf install $(find "$src/RPMS/x86_64" -name "intel-vaapi-driver-$version"\* -print -quit)"

echo ''
echo 'To revert to the stock driver:'
echo 'sudo dnf swap intel-vaapi-driver libva-intel-driver'
