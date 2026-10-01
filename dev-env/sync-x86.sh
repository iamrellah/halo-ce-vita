#!/bin/bash
# copy every file changed or added in port-arm (the Vita working tree) into port (the x86 test tree)
set -e
cd "$(dirname "$0")/port-arm"
{ git diff --name-only; git ls-files --others --exclude-standard; } | grep -v '^build/' | sort -u | while read -r f; do
  mkdir -p "../port/$(dirname "$f")"; cp -p "$f" "../port/$f"
done
