#!/usr/bin/env bash
set -euo pipefail
sdk_dir="${1:-.deps/pico-sdk}"
sdk_commit=079c6f39023649b154152db30f1d781e884879bc
if [[ ! -d "$sdk_dir/.git" ]]; then
  git clone --branch 2.3.1 --depth 1 https://github.com/raspberrypi/pico-sdk.git "$sdk_dir"
fi
if [[ "$(git -C "$sdk_dir" rev-parse HEAD)" != "$sdk_commit" ]]; then
  echo "SDK commit mismatch; use an empty destination or SDK 2.3.1." >&2
  exit 1
fi
git -C "$sdk_dir" submodule update --init --depth 1 lib/tinyusb
printf 'SDK ready: %s\n' "$sdk_dir"
