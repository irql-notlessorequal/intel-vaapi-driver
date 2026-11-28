#!/usr/bin/env bash
set -Eeuo pipefail

declare -r repo='irql-notlessorequal/intel-vaapi-driver'

# https://docs.github.com/en/rest/reference/repos#get-the-latest-release
function latest_version() {
  curl -fsSL "https://api.github.com/repos/$repo/releases/latest" | jq -er '.tag_name'
}

if (( $# > 1 )); then
  echo 'Usage: build-rpm.sh [version]' >&2
  exit 1
fi

output=$PWD
remote=false
if [[ -n ${BASH_SOURCE[0]:-} ]]; then
  src=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
else
  remote=true
  src=$(mktemp -d -t intel-vaapi-driver.XXXXXXXXXX)
  trap 'rm -rf -- "$src"' EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  echo "Build working directory: $src"
fi

# see .spec's BuildRequires, also see
# dnf provides "pkgconfig(egl)" "pkgconfig(libdrm)" "pkgconfig(libva)" "pkgconfig(libva-drm)" "pkgconfig(libva-wayland)" "pkgconfig(libva-x11)" "pkgconfig(wayland-client)" "pkgconfig(wayland-scanner)"
dependencies=(gcc git meson rpm-build jq
  libappstream-glib libglvnd-devel libdrm-devel libva-devel wayland-devel)
if ! rpm -q --quiet "${dependencies[@]}"; then
  echo 'Installing build dependencies...'
  # DNF must read confirmation from the terminal, not the piped script.
  {
    sudo dnf install "${dependencies[@]}"
  } </dev/tty
fi

version=${1:-"$(latest_version)"}
if [[ ! $version =~ ^[0-9]+(\.[0-9]+)*$ ]]; then
  echo "Invalid release version: $version" >&2
  exit 1
fi
echo "Building rpm for version $version..."

if "$remote"; then
  packaging_ref="refs/tags/$version"
  # shortcut: master fixes old packaging; pin a compatible ref if master diverges.
  if { echo "$version"; echo '2.4.5'; } | sort --version-sort --check=quiet; then
    packaging_ref='refs/heads/master'
  fi
  echo "Packaging ref: $packaging_ref"
  for file in intel-vaapi-driver.spec intel-vaapi-driver.metainfo.xml intel-vaapi-driver.metainfo-chipsets.sh; do
    curl -fsSL -o "$src/$file" \
      "https://raw.githubusercontent.com/$repo/$packaging_ref/distribution/rpm/$file"
  done
  chmod +x "$src/intel-vaapi-driver.metainfo-chipsets.sh"
fi

cd -- "$src"

# see https://stackoverflow.com/a/46153901
rpmbuild \
  --undefine '_disable_source_fetch' --undefine 'source_date_epoch_from_changelog' \
  --define "_sourcedir $src" --define "_topdir $src" --define "_tmppath $src" \
  --define "version $version" \
  -bb --nodebuginfo \
  "$src/intel-vaapi-driver.spec"

mapfile -d '' -t packages < <(find "$src/RPMS" -type f -name "intel-vaapi-driver-$version-*.rpm" -print0)
if (( ${#packages[@]} == 0 )); then
  echo 'rpmbuild produced no matching RPM.' >&2
  exit 1
fi
for package in "${packages[@]}"; do
  if "$remote"; then
    destination="$output/${package##*/}"
    if [[ -e $destination || -L $destination ]]; then
      destination=$(mktemp -- "${destination%.rpm}-$(date +%s)-XXXXXX.rpm")
      cp -- "$package" "$destination"
    else
      cp --update=none-fail -- "$package" "$destination"
    fi
    package=$destination
  fi
  echo ''
  echo 'You can now install the built package:'
  echo "sudo dnf install ${package@Q}"
done

echo ''
echo 'To revert to the stock driver:'
echo 'sudo dnf swap intel-vaapi-driver libva-intel-driver'
