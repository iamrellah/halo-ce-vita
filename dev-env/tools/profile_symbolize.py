#!/usr/bin/env python3
"""Symbolizes a HALO_PROFILE sample file (lines of "pc lr" in hex, from
port/linux/src/posix_profile.c) against the binary that produced it, and
prints the functions by share of samples, then the files.

usage: profile_symbolize.py BINARY SAMPLES [--top N] [--filter SUBSTRING]
       [--callers FUNCTION] [--threads] [--thread TID]

--threads lists the samples per thread (the third column, when the sampler
recorded it); --thread keeps one thread's samples.

--filter keeps only samples whose function name contains the substring (the
share is then of the filtered samples, with the total shown).
--callers lists, for the samples inside FUNCTION, the functions the link
register points into (ARM only: the sampler records lr; a leaf's caller).
"""
import argparse
import collections
import subprocess
import sys


ADDR2LINE = "/usr/bin/llvm-addr2line"


def symbolize(binary, addresses):
    """address -> (function, file:line)"""
    if not addresses:
        return {}
    output = subprocess.run(
        [ADDR2LINE, "-f", "-C", "-e", binary] + [f"0x{a:x}" for a in addresses],
        capture_output=True, text=True, check=True).stdout.splitlines()
    result = {}
    for index, address in enumerate(addresses):
        function = output[index * 2] if index * 2 < len(output) else "??"
        location = output[index * 2 + 1] if index * 2 + 1 < len(output) else "??"
        result[address] = (function, location.split(" ")[0])
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("samples")
    parser.add_argument("--top", type=int, default=40)
    parser.add_argument("--filter", default=None)
    parser.add_argument("--callers", default=None)
    parser.add_argument("--lines", default=None, help="the sample counts by source line inside this function")
    parser.add_argument("--addr2line", default=None, help="the addr2line to use (a cross toolchain's for an ARM binary)")
    parser.add_argument("--threads", action="store_true", help="the samples per thread")
    parser.add_argument("--thread", type=int, default=None, help="keep only this thread's samples")
    args = parser.parse_args()
    global ADDR2LINE
    if args.addr2line:
        ADDR2LINE = args.addr2line

    pcs = []
    lrs = []
    threads = collections.Counter()
    with open(args.samples) as file:
        for line in file:
            parts = line.split()
            if len(parts) >= 1:
                thread = int(parts[2]) if len(parts) > 2 else 0
                threads[thread] += 1
                if args.thread is not None and thread != args.thread:
                    continue
                pcs.append(int(parts[0], 16))
                lrs.append(int(parts[1], 16) if len(parts) > 1 else 0)
    if args.threads:
        for thread, count in threads.most_common():
            print(f"thread {thread}: {count} samples")
    total = len(pcs)
    unique = sorted(set(pcs) | set(lrs))
    # addr2line in chunks (a command line has limits)
    names = {}
    for start in range(0, len(unique), 4000):
        names.update(symbolize(args.binary, unique[start:start + 4000]))

    by_function = collections.Counter()
    by_file = collections.Counter()
    kept = 0
    callers = collections.Counter()
    lines = collections.Counter()
    for pc, lr in zip(pcs, lrs):
        function, location = names.get(pc, ("??", "??"))
        if args.filter and args.filter not in function:
            continue
        kept += 1
        by_function[function] += 1
        by_file[location.rsplit(":", 1)[0]] += 1
        if args.callers and function == args.callers:
            callers[names.get(lr, ("??", "??"))[0]] += 1
        if args.lines and function == args.lines:
            lines[location] += 1

    base = kept if args.filter else total
    print(f"{total} samples" + (f", {kept} in functions matching '{args.filter}'" if args.filter else ""))
    print("\nfunctions:")
    for function, count in by_function.most_common(args.top):
        print(f"{100.0 * count / base:6.2f}%  {count:7d}  {function}")
    print("\nfiles:")
    for location, count in by_file.most_common(args.top // 2):
        print(f"{100.0 * count / base:6.2f}%  {count:7d}  {location}")
    if args.callers:
        print(f"\ncallers of {args.callers} (by link register):")
        for function, count in callers.most_common(20):
            print(f"{count:7d}  {function}")
    if args.lines:
        print(f"\nlines of {args.lines}:")
        for location, count in lines.most_common(25):
            print(f"{count:7d}  {location}")


if __name__ == "__main__":
    main()
