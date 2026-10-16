#!/usr/bin/env python3
"""
Build ceforth with several E4_NOS / E4_RTOP (and optional align-functions) settings
and time the 'yy' looper.  Linux/x86-64 host, no sanitizers (use a separate build for those).

  python3 bench_nos_rtop.py --src .                       # 4 NOS/RTOP combos, -O2
  python3 bench_nos_rtop.py --src . --opt -Os --runs=9
  python3 bench_nos_rtop.py --src . --align=4,16,32       # layout sensitivity
  python3 bench_nos_rtop.py --src . --configs 0,1 1,1 --perf
  python3 bench_nos_rtop.py --src . --configs 0,1 1,1 --sweep 4                    # aligned placements
  python3 bench_nos_rtop.py --src . --configs 0,1 1,1 --sweep 4 --sweep-start 2    # 2-mod-4 (misaligned)

The sweep (needs the static pmem_buf version) shifts pmem by 0..4095 bytes inside a page-aligned
buffer and reports which placements run slow.  Finding on x86-64: with pmem 4-byte aligned no
placement is slow; with pmem 2 mod 4 some placements are up to ~2x slower (token fetches that
straddle a cache line), and which placements those are differs between NOS settings, so an
unaligned pmem can make NOS=0 vs NOS=1 look 20-30% apart in either direction.

--src must contain ceforth.cpp, ceforth.h, config.h and the stub harness
(host_harness.cpp, or pass --harness).  Needs VM::base as IU (see ceforth.h edit).
"""
import argparse, os, platform, shutil, statistics, subprocess, sys, tempfile, time

FILES = ["ceforth.cpp", "ceforth.h", "config.h"]

def sh(cmd, **kw):
    return subprocess.run(cmd, text=True, capture_output=True, **kw)

def build(srcdir, harness, workdir, nos, rtop, align, a, sweep=False):
    d = os.path.join(workdir, f"n{nos}r{rtop}a{align}")
    os.makedirs(d, exist_ok=True)
    for f in FILES:
        shutil.copy(os.path.join(srcdir, f), d)
    shutil.copy(harness, os.path.join(d, "harness.cpp"))
    if align:                                    # config.h has: #pragma GCC optimize("align-functions=4")
        p = os.path.join(d, "config.h")
        s = open(p).read()
        assert "align-functions=4" in s, "align pragma not found in config.h"
        open(p, "w").write(s.replace("align-functions=4", f"align-functions={align}"))
    if sweep:                                    # shift pmem at run time: PMEM_SHIFT=<bytes>
        import re
        p = os.path.join(d, "ceforth.cpp")
        s = open(p).read()
        s, n1 = re.subn(r"(alignas\(\d+\)\s+)?static U8 _pmem_buf\[E4_PMEM_SZ\];",
                        "alignas(4096) static U8 _pmem_buf[E4_PMEM_SZ + 8192];", s)
        s, n2 = re.subn(r"pmem\s*=\s*_pmem_buf\s*;",
                        'pmem = _pmem_buf + (getenv("PMEM_SHIFT") ? atoi(getenv("PMEM_SHIFT")) : 0);', s)
        if not (n1 and n2):
            sys.exit("--sweep needs the static buffer version: 'static U8 _pmem_buf[E4_PMEM_SZ];' and 'pmem = _pmem_buf;'")
        open(p, "w").write(s)
    exe = os.path.join(d, "bench")
    cmd = [a.cxx, "-std=c++17", a.opt, "-no-pie", f"-DXT0_U32={a.xt0}",
           f"-DE4_NOS={nos}", f"-DE4_RTOP={rtop}",
           "-include", "cstdlib", "-include", "cerrno", "-include", "algorithm",
           os.path.join(d, "ceforth.cpp"), os.path.join(d, "harness.cpp"), "-o", exe] + a.extra
    if platform.machine() in ("x86_64", "AMD64"):
        cmd.insert(3, "-fcf-protection=none")    # drop endbr64 so listings look like other targets
    r = sh(cmd, cwd=d)
    if r.returncode:
        sys.exit(f"build failed for NOS={nos} RTOP={rtop} align={align}:\n{r.stderr[:2000]}")
    return exe

def script(path, n):
    open(path, "w").write(
        ": fib dup 2 < if drop 1 else dup 1- fib swap 2 - fib + then ;\n"
        f": z {n} for 30 fib drop next ; z\n")
#        ": xx 999 for 34 i + drop next ; : yy 999 for xx next ;\n"
#        f": z {n} for yy next ; z\n")

def timed(exe, fs):
    with open(fs) as f:
        t = time.perf_counter()
        r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        dt = time.perf_counter() - t
    if r.returncode:
        sys.exit(f"{exe} exited with {r.returncode}")
    return dt * 1000.0

def perf_stat(exe, fs):
    if not shutil.which("perf"):
        return None
    # split_loads = cache-line-crossing loads (Intel); fall back if the event does not exist
    for ev in ("instructions,cycles,branch-misses,mem_inst_retired.split_loads",
               "instructions,cycles,branch-misses"):
        with open(fs) as f:
            r = subprocess.run(["perf", "stat", "-x,", "-e", ev, exe],
                               stdin=f, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        out = {}
        for line in r.stderr.splitlines():
            p = line.split(",")
            if len(p) > 2 and p[0].replace(".", "").isdigit():
                out[p[2].split(":")[0]] = float(p[0])
        if out:
            out["ipc"] = round(out["instructions"] / out["cycles"], 2)
            return out
    return None

def sweep(exes, a, work):
    fs = os.path.join(work, "sweep.fs"); script(fs, 3)
    shifts = list(range(a.sweep_start, 4096, a.sweep))
    for (nos, rtop, al), exe in exes.items():
        res = []
        for sh_ in shifts:
            env = dict(os.environ, PMEM_SHIFT=str(sh_))
            best = 1e9
            for _ in range(2):
                with open(fs) as f:
                    t = time.perf_counter()
                    subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL, env=env)
                    best = min(best, time.perf_counter() - t)
            res.append(best * 1000)
        med = statistics.median(res)
        slow = [s_ for s_, x in zip(shifts, res) if x > 1.25 * med]
        runs, cur = [], None                      # merge consecutive slow shifts into windows
        for s_ in slow:
            if cur and s_ - cur[1] <= a.sweep: cur[1] = s_
            else: cur = [s_, s_]; runs.append(cur)
        worst = max(res) / med
        print(f"NOS={nos} RTOP={rtop}: median {med:.1f} ms, worst {worst:.2f}x, "
              f"slow at {len(slow)}/{len(shifts)} shifts")
        if runs:
            print("   slow windows (byte offsets):", ", ".join(f"{x}-{y}" for x, y in runs[:12]),
                  "..." if len(runs) > 12 else "")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", default=".")
    ap.add_argument("--harness", default=None)
    ap.add_argument("--configs", nargs="*", default=["0,0", "0,1", "1,0", "1,1"], help="NOS,RTOP pairs")
    ap.add_argument("--align", default="", help="comma list, e.g. 4,16,32 (default: leave config.h as is)")
    ap.add_argument("--repeat", type=int, default=29, help="yy repetitions per run (z count)")
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--opt", default="-O2")
    ap.add_argument("--cxx", default="g++")
    ap.add_argument("--xt0", type=int, default=1)
    ap.add_argument("--perf", action="store_true")
    ap.add_argument("--sweep-start", type=int, default=0, help="first pmem shift in the sweep (use 2 for misaligned)")
    ap.add_argument("--sweep", type=int, default=0, metavar="STEP", help="pmem placement sweep, step in bytes (e.g. 4)")
    ap.add_argument("--extra", nargs="*", default=[])
    a = ap.parse_args()

    harness = a.harness or next((os.path.join(a.src, h) for h in ("host_harness.cpp", "harness.cpp")
                                 if os.path.exists(os.path.join(a.src, h))), None)
    if not harness:
        sys.exit("no harness found; pass --harness")
    aligns = [int(x) for x in a.align.split(",")] if a.align else [0]
    cfgs = [tuple(map(int, c.split(","))) for c in a.configs]

    work = tempfile.mkdtemp(prefix="e4bench_")
    fs_run, fs_zero = os.path.join(work, "run.fs"), os.path.join(work, "zero.fs")
    script(fs_run, a.repeat); script(fs_zero, 0)

    exes = {}
    for al in aligns:
        for nos, rtop in cfgs:
            exes[(nos, rtop, al)] = build(a.src, harness, work, nos, rtop, al, a, sweep=bool(a.sweep))
    if a.sweep:
        sweep(exes, a, work)
        return

    t_run = {k: [] for k in exes}
    t_zero = {k: [] for k in exes}
    for _ in range(a.runs):                      # round-robin so drift hits every config equally
        for k, exe in exes.items():
            t_zero[k].append(timed(exe, fs_zero))
            t_run[k].append(timed(exe, fs_run))

    reps = a.repeat + 1                          # 'n for ... next' runs n+1 times
    base = None
    print(f"{a.cxx} {a.opt} XT0_U32={a.xt0}, {a.runs} runs, z={a.repeat} (-> {reps} yy)")
    print(f"{'NOS':>3} {'RTOP':>4} {'align':>5} {'best ms':>8} {'median':>8} {'ms/yy':>7} {'vs first':>8}")
    for (nos, rtop, al), exe in exes.items():
        best = min(t_run[(nos, rtop, al)]) - min(t_zero[(nos, rtop, al)])   # subtract startup
        med = statistics.median(t_run[(nos, rtop, al)]) - statistics.median(t_zero[(nos, rtop, al)])
        base = base or best
        print(f"{nos:>3} {rtop:>4} {al or '0':>5} {best:8.1f} {med:8.1f} {best/reps:7.3f} {best/base:8.2f}x")
        print(f"{{ 'nos':{nos}, 'rtop':{rtop}, 'align':{al or '0'}, 'best':{best:0.1f}, 'median':{med:0.1f}, 'avg':{best/reps:0.3f}, ")
        if a.perf:
            p = perf_stat(exe, fs_run)
            print("  'perf': ", p if p else "unavailable", "},")
    print(f"binaries kept in {work}")

if __name__ == "__main__":
    main()
