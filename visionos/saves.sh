#!/bin/bash
# Copy a guest's save data between the Mac and the device's app container.
#
#   KLEPTON_TARGET=<t> ./saves.sh pull <dir> <device>   # device -> <dir>
#   KLEPTON_TARGET=<t> ./saves.sh push <dir> <device>   # <dir> -> device
#
# "Save data" is the guest's files/ and shared_prefs/ under android-files/,
# minus files/il2cpp (the runtime unpacks that itself). An install rotates the
# data container, so pull before a reinstall and push after it. A Quest's
# /sdcard/Android/data/<package>/files pulled with adb has the same layout and
# pushes the same way.
set -euo pipefail
cd "$(dirname "$0")"
eval "$(python3 targets.py "${KLEPTON_TARGET:-}")"
BUNDLE_ID="${KLEPTON_BUNDLE_ID:-$KLT_BUNDLE}"
MODE="${1:-}"; DIR="${2:-}"; DEV="${3:-${KLEPTON_DEVICE:-}}"
[ -n "$MODE" ] && [ -n "$DIR" ] && [ -n "$DEV" ] || {
  echo "usage: KLEPTON_TARGET=<t> $0 pull|push <dir> <device>"; exit 2; }
ROOT=Documents/android-files

dc() {
  xcrun devicectl device copy "$1" --device "$DEV" --domain-type appDataContainer \
    --domain-identifier "$BUNDLE_ID" --source "$2" --destination "$3" >/dev/null
}

case "$MODE" in
pull)
  xcrun devicectl device info files --device "$DEV" --domain-type appDataContainer \
      --domain-identifier "$BUNDLE_ID" 2>/dev/null \
    | awk -v r="$ROOT/" '$1 ~ "^"r"(files|shared_prefs)/" && $0 !~ /Directory/ {print $1}' \
    | grep -v "^$ROOT/files/il2cpp/" \
    | while read -r f; do
        rel="${f#$ROOT/}"
        mkdir -p "$DIR/$(dirname "$rel")"
        dc from "$f" "$DIR/$rel" && echo "pulled $rel"
      done ;;
push)
  (cd "$DIR" && find files shared_prefs -type f 2>/dev/null | grep -v '^files/il2cpp/') \
    | while read -r rel; do
        dc to "$DIR/$rel" "$ROOT/$rel" && echo "pushed $rel"
      done ;;
*) echo "usage: KLEPTON_TARGET=<t> $0 pull|push <dir> <device>"; exit 2 ;;
esac
