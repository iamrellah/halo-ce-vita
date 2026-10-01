#!/bin/bash
# deploy.sh: push the current Vita build (and any shaders Vita3K compiled that the Vita lacks), launch it, and
# confirm the new process started (the log begins again)
set -e
SP=/home/birchwoodgod/github/halo-ce-vita-spike
F=ftp://192.168.0.205:1337
COMPANION="python3 /home/birchwoodgod/xita-backups/2026-09-18-unified-games/source/tools/vita_companion.py"
V3K=/home/birchwoodgod/.local/share/Vita3K/Vita3K/ux0/data/haloce-vita/shaders
for attempt in 1 2 3; do
	$COMPANION quit all >/dev/null
	$COMPANION wait 3s >/dev/null
	old=$(timeout 15 curl -s "$F/ux0:/data/haloce-vita/halo.log" | grep -o 'realtime [0-9]*' | head -1)
	# (the Vita3K shader sync is opt-in: every FTP operation costs the Vita's FTP daemon memory it never frees)
	n=0
	if [ -n "$SYNC_SHADERS" ]; then
		have=$(timeout 20 curl -s --list-only "$F/ux0:/data/haloce-vita/shaders/" || true)
		for f in $(ls $V3K 2>/dev/null); do
			case " $(echo $have) " in *" $f "*) ;; *) curl -s --ftp-create-dirs -T "$V3K/$f" "$F/ux0:/data/haloce-vita/shaders/$f"; n=$((n+1));; esac
		done
	fi
	if ! curl -s --max-time 300 -T $SP/port-arm/build/vita/eboot.bin "$F/ux0:/app/HCEV00001/eboot.bin"; then
		echo "EBOOT UPLOAD FAILED (FTP STOR): the Vita's FTP daemon is likely out of memory - power-cycle the Vita"; exit 2
	fi
	echo "shaders uploaded: $n; eboot uploaded: $(stat -c %s $SP/port-arm/build/vita/eboot.bin) bytes"
	$COMPANION launch HCEV00001 >/dev/null
	sleep 6
	new=$(timeout 15 curl -s "$F/ux0:/data/haloce-vita/halo.log" | grep -o 'realtime [0-9]*' | head -1)
	if [ "$new" != "$old" ] && [ -n "$new" ]; then
		# the elf of this launch, for symbolizing its crash dumps (psp2core-<realtime>...)
		mkdir -p $SP/dumps/elf; cp $SP/port-arm/build/vita/halo.elf "$SP/dumps/elf/$(echo $new | tr -d ' ' ).elf" 2>/dev/null
		echo "launched (fresh log: $new)"; exit 0
	fi
	echo "the previous process kept running ($old); retrying"
done
echo "could not restart the game"; exit 1
