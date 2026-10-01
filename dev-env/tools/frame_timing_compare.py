#!/usr/bin/env python3
"""Compares the frame-timing lines of two or more runs on the game-tick axis
(the same game time, whatever each run's frame rate), in bins of ticks.

usage: frame_timing_compare.py LOG [LOG ...] [--bin TICKS] [--from TICK] [--to TICK]

Each frame-timing line (port/linux/src/frame_timing.c) names the game tick
it was written at; a run's windows are averaged per bin, weighted by their
frame counts, and the columns are frame, render, tick (ms/tick), other
(the join and the rest) in milliseconds. A run that ends earlier leaves its
later bins blank.
"""
import argparse
import re

LINE = re.compile(r"frame-timing: game tick (\d+) frames (\d+) ticks (\d+) \| frame ([\d.]+) ms \(max ([\d.]+)\) \| "
                  r"ticks ([\d.]+) ms/frame ([\d.]+) ms/tick \| render ([\d.]+) \| throttle\+present ([\d.]+) \| "
                  r"other ([\d.]+)")


def parse(path):
    windows = []
    with open(path, errors="replace") as file:
        for line in file:
            match = LINE.search(line)
            if match:
                tick, frames, ticks, frame, _maximum, _tpf, per_tick, render, present, other = match.groups()
                windows.append((int(tick), int(frames), float(frame), float(per_tick), float(render), float(other),
                                float(present)))
    return windows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("logs", nargs="+")
    parser.add_argument("--bin", type=int, default=600)
    parser.add_argument("--from", dest="start", type=int, default=0)
    parser.add_argument("--to", dest="end", type=int, default=1 << 30)
    args = parser.parse_args()

    runs = [(path, parse(path)) for path in args.logs]
    last = max((w[0] for _, windows in runs for w in windows), default=0)
    print("columns per run: frame / render / tick / other (ms)")
    header = f"{'ticks':>13}"
    for path, _ in runs:
        name = path.rsplit("/", 1)[-1][-28:]
        header += f" | {name:>28}"
    print(header)
    totals = [[0.0, 0.0, 0.0, 0.0, 0] for _ in runs]
    start = max(args.start, 0)
    while start <= min(last, args.end):
        line = f"{start:6d}-{start + args.bin - 1:6d}"
        for index, (_, windows) in enumerate(runs):
            inside = [w for w in windows if start <= w[0] < start + args.bin]
            if not inside:
                line += f" | {'':>28}"
                continue
            frames = sum(w[1] for w in inside)
            frame = sum(w[2] * w[1] for w in inside) / frames
            per_tick = sum(w[3] * w[1] for w in inside) / frames
            render = sum(w[4] * w[1] for w in inside) / frames
            other = sum(w[5] * w[1] for w in inside) / frames
            line += f" | {frame:6.2f} {render:6.2f} {per_tick:6.2f} {other:6.2f}"
            totals[index][0] += frame * frames
            totals[index][1] += render * frames
            totals[index][2] += per_tick * frames
            totals[index][3] += other * frames
            totals[index][4] += frames
        print(line)
        start += args.bin
    line = f"{'mean':>13}"
    for total in totals:
        if total[4]:
            line += f" | {total[0] / total[4]:6.2f} {total[1] / total[4]:6.2f} {total[2] / total[4]:6.2f} {total[3] / total[4]:6.2f}"
        else:
            line += f" | {'':>28}"
    print(line)


if __name__ == "__main__":
    main()
