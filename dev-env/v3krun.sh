#!/bin/bash
# v3krun.sh NAME SHOTS [ENV_LINE...]: fresh Vita3K run of port-arm's eboot with extra env lines;
# waits for SHOTS screenshots and saves the newest as $OUT/NAME.png (OUT defaults to the scratchpad)
OUT=${OUT:-/tmp/claude-1000/-home-birchwoodgod-github-xboxvita/c3d84c3b-7dc8-40eb-b890-e282ea2f49bb/scratchpad}
D=~/.local/share/Vita3K/Vita3K/ux0/data/haloce-vita
name=$1; want=$2; shift 2
mkdir -p $OUT
for p in $(pgrep -x Vita3K); do kill -9 $p; done
sleep 1
cp $D/env.base $D/env.txt
for line in "$@"; do echo "$line" >> $D/env.txt; done
find $D/shots -name '*.bmp' -delete
cp ~/github/halo-ce-vita-spike/port-arm/build/vita/eboot.bin ~/.local/share/Vita3K/Vita3K/ux0/app/HCEV00001/eboot.bin
(cd ~/vita3k/ubuntu && DISPLAY=:1 timeout 300 ./Vita3K -f -w -l 1 -r HCEV00001 2>&1 | grep -v "export_sceIoOpen\|stat_file\|io_error_impl" | head -c 20000000 > $OUT/v3k-$name.log &)
for i in $(seq 1 120); do n=$(ls $D/shots 2>/dev/null | grep -c frame); [ "$n" -ge "$want" ] && break; sleep 2; done
f=$(ls -t $D/shots/frame*.bmp 2>/dev/null | head -1)
[ -n "$f" ] && python3 -c "from PIL import Image; Image.open('$f').save('$OUT/$name.png')"; echo "$name: $f"
