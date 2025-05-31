#!/usr/bin/env bash
# Based on https://github.com/negativo17/intel-vaapi-driver/blob/master/intel-vaapi-driver.py

if [[ $# != 1 ]]; then
  echo "usage: $0 src/i965_pciids.h" >&2
  exit 1
fi

declare -A pids # to avoid duplicates
while IFS= read -r line; do
  [[ "$line" != CHIPSET* ]] && continue
  hex_pid="${line:10:4}"
  pids["$((16#$hex_pid))"]=1
done <"$1"

declare -r vid=$((0x8086))
for pid in "${!pids[@]}"; do
  printf 'pci:v%08Xd%08Xsv*sd*bc*sc*i*\n' "$vid" "$pid"
done
