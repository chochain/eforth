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
    fib = f"""
: fib dup 2 < if drop 1 else dup 1- fib swap 2 - fib + then ;
: z {n} for 30 fib drop next ; z
"""
    looper = f"""
: xx 9999 for 34 i + drop next ; : yy 999 for xx next ;
: z {n} for yy next ; z
"""
    chacha20 = f"""
\ : th ( arr n -- &arr[n] ) cells swap + ;
: a@ ( n arr -- v ) swap th @ ;
: a! ( v n arr -- ) swap th ! ;
: a+! ( v n arr -- ) swap th +! ;
: 4@ ( a b c d arr -- va vb vc vd )
  >r ( keep arr )
  2swap i a@ swap i  a@ swap
  2swap i a@ swap r> a@ swap ;
: 4! ( a b c d va vb vc vd arr -- )
  >r ( keep arr )
  4 pick i a! 4 pick i  a!
  4 pick i a! 4 pick r> a!
  2drop 2drop ;
: rol ( v n -- v' )
  >r dup r@ lshift swap
  $20 r> - rshift or ;
: hx ( c d a b n -- a' b c d' )
  >r ( keep n  ) swap over + dup
  >r ( keep a' ) swap
  2swap  ( a' b c d )
  r> xor ( a' b c d^a' ) r> rol ;
: qround ( a b c d -- a b c d )
  2swap  ( c d a b )
  $10 hx ( a b c d ) $C hx ( c d a b )
  $8  hx ( a b c d ) $7 hx ( c d a b )
  2swap ;
create t0 $11111111 , $01020304 , $9b8d6f43 , $01234567 ,
8 constant NT
create hidx $e943d872 , $cb61fa50 , $fb73ea62 , $d951c840 ,
create st
  $61707865 , $3320646e , $79622d32 , $6b206574 ,
  $03020100 , $07060504 , $0b0a0908 , $0f0e0d0c ,
  $13121110 , $17161514 , $1b1a1918 , $1f1e1d1c ,
  $00000001 , $09000000 , $4a000000 , $00000000 ,
create vbase NT $40 * allot
: vt ( -- ) vbase rank $40 * + ;
: st2vt ( -- )  $f for i st a@ i vt a! next ;
: vt+=st ( -- ) $f for i st a@ i vt a+! next ;
: 4x8 ( dcba -- a b c d a b c d )
  3 for
    dup $f and swap 4 rshift
  next drop 2over 2over ;
: quarter ( bcda -- ) 4x8 vt 4@ qround vt 4! ;
: odd_even ( -- )
  3 for
    i hidx a@
    dup        quarter
    $10 rshift quarter
  next ;
: one_block ( -- ) st2vt $9 for odd_even next vt+=st ;
: bench ( -- ) 999 for one_block next ;
: z {n} for bench next ; z
"""
    fft = f"""
: a@ ( n arr -- v ) swap th @ ;        \ fetch a[n]
: a! ( v n arr -- ) swap th ! ;        \ a[n] = v
$100 constant N                        \ points (256)
8    constant LOGN                     \ log2(N)
$80  constant TOL                      \ max |error| allowed vs exact DFT (LSB)
create re N cells allot                \ real part
create im N cells allot                \ imaginary part
\ twiddle w[t] = exp(-j*2*pi*t/N) * 2^15, t = 0..N/2-1
\ tables hold value + bias (all positive hex, same on every VM); fix removes it
create wr
  $10000 , $fff6 , $ffd9 , $ffa7 , $ff62 , $ff0a , $fe9d , $fe1e ,
  $fd8a , $fce4 , $fc2a , $fb5d , $fa7d , $f98a , $f885 , $f76c ,
  $f642 , $f505 , $f3b6 , $f255 , $f0e3 , $ef5f , $edca , $ec24 ,
  $ea6e , $e8a7 , $e6d0 , $e4e9 , $e2f2 , $e0ec , $ded7 , $dcb4 ,
  $da82 , $d843 , $d5f6 , $d39b , $d134 , $cec0 , $cc40 , $c9b4 ,
  $c71d , $c47b , $c1ce , $bf17 , $bc57 , $b98d , $b6ba , $b3df ,
  $b0fc , $ae11 , $ab1f , $a827 , $a528 , $a224 , $9f1a , $9c0c ,
  $98f9 , $95e2 , $92c8 , $8fab , $8c8c , $896b , $8648 , $8324 ,
  $8000 , $7cdc , $79b8 , $7695 , $7374 , $7055 , $6d38 , $6a1e ,
  $6707 , $63f4 , $60e6 , $5ddc , $5ad8 , $57d9 , $54e1 , $51ef ,
  $4f04 , $4c21 , $4946 , $4673 , $43a9 , $40e9 , $3e32 , $3b85 ,
  $38e3 , $364c , $33c0 , $3140 , $2ecc , $2c65 , $2a0a , $27bd ,
  $257e , $234c , $2129 , $1f14 , $1d0e , $1b17 , $1930 , $1759 ,
  $1592 , $13dc , $1236 , $10a1 , $0f1d , $0dab , $0c4a , $0afb ,
  $09be , $0894 , $077b , $0676 , $0583 , $04a3 , $03d6 , $031c ,
  $0276 , $01e2 , $0163 , $00f6 , $009e , $0059 , $0027 , $000a ,
create wi
  $8000 , $7cdc , $79b8 , $7695 , $7374 , $7055 , $6d38 , $6a1e ,
  $6707 , $63f4 , $60e6 , $5ddc , $5ad8 , $57d9 , $54e1 , $51ef ,
  $4f04 , $4c21 , $4946 , $4673 , $43a9 , $40e9 , $3e32 , $3b85 ,
  $38e3 , $364c , $33c0 , $3140 , $2ecc , $2c65 , $2a0a , $27bd ,
  $257e , $234c , $2129 , $1f14 , $1d0e , $1b17 , $1930 , $1759 ,
  $1592 , $13dc , $1236 , $10a1 , $0f1d , $0dab , $0c4a , $0afb ,
  $09be , $0894 , $077b , $0676 , $0583 , $04a3 , $03d6 , $031c ,
  $0276 , $01e2 , $0163 , $00f6 , $009e , $0059 , $0027 , $000a ,
  $0000 , $000a , $0027 , $0059 , $009e , $00f6 , $0163 , $01e2 ,
  $0276 , $031c , $03d6 , $04a3 , $0583 , $0676 , $077b , $0894 ,
  $09be , $0afb , $0c4a , $0dab , $0f1d , $10a1 , $1236 , $13dc ,
  $1592 , $1759 , $1930 , $1b17 , $1d0e , $1f14 , $2129 , $234c ,
  $257e , $27bd , $2a0a , $2c65 , $2ecc , $3140 , $33c0 , $364c ,
  $38e3 , $3b85 , $3e32 , $40e9 , $43a9 , $4673 , $4946 , $4c21 ,
  $4f04 , $51ef , $54e1 , $57d9 , $5ad8 , $5ddc , $60e6 , $63f4 ,
  $6707 , $6a1e , $6d38 , $7055 , $7374 , $7695 , $79b8 , $7cdc ,
\ gold: exact DFT of the test signal, bins 0..15
create gre
  $101f5b , $105a5e ,  $f9e07 , $e986b , $103178 ,  $e17cc , $11a3db ,  $fed36 ,
  $116f85 , $104a21 , $112190 , $f0921 ,  $faaf8 , $1047c9 , $10f7f7 , $106c82 ,
create gim
  $100000 , $1061c8 , $f6a5e ,  $e79c3 , $113d3a , $10c239 , $10d250 , $1046e4 ,
   $e6c0d ,  $fa455 , $f3722 , $10fbe4 , $10a879 ,  $e3f1d ,  $f853d ,  $eeec7 ,

\ complex multiply, Q15
: q15* ( a b -- a*b>>15 ) $8000 */ ;
: cmul ( xr xi wr wi -- tr ti )        \ (xr+j*xi)*(wr+j*wi)
  3 pick 2 pick q15*                   \ xr*wr
  3 pick 2 pick q15* -                 \ - xi*wi        = tr
  4 pick 2 pick q15*                   \ xr*wi
  4 pick 4 pick q15* +                 \ + xi*wr        = ti
  >r >r 2drop 2drop r> r> ;

\ butterfly: x[p] = x[p] + t, x[q] = x[p] - t   (one of re / im)
: put2 ( x t p q arr -- )
  >r
  3 pick 3 pick + 2 pick r@ a!         \ arr[p] = x + t
  3 pick 3 pick - over r> a!           \ arr[q] = x - t
  2drop 2drop ;
: twid ( t -- wr wi ) dup wr a@ swap wi a@ ;
: x2 ( q -- xr xi ) dup re a@ swap im a@ ;
: bre ( p q tr ti -- p q tr ti )       \ real half of the butterfly
  3 pick re a@ 2 pick 5 pick 5 pick re put2 ;
: bim ( p q tr ti -- )                 \ imaginary half
  3 pick im a@ over 5 pick 5 pick im put2 2drop 2drop ;
: bfly ( p q t -- )
  twid 2 pick x2 2swap cmul            \ p q tr ti = x[q] * w[t]
  bre bim ;

\ one group of h butterflies starting at g, twiddle stride s
: grp ( h g s -- h g s )
  2 pick 1- for
    over i + dup 4 pick + 2 pick i * bfly
  next ;
: stage ( h -- )                       \ all groups of half-size h
  dup 2* N swap /  dup 1- for          \ s = N/(2h) groups
    over 2* i * swap grp swap drop
  next 2drop ;

\ bit reversal permutation
: rev ( i -- r )
  0 swap LOGN 1- for
    dup 1 and rot 2* or swap 2/
  next drop ;
: xch ( i j arr -- )                   \ swap arr[i], arr[j]
  >r over r@ a@ over r@ a@ swap rot r@ a! swap r> a! ;
: brev1 ( i -- )
  dup rev 2dup < if 2dup re xch 2dup im xch then 2drop ;
: bitrev ( -- ) N 1- for i brev1 next ;

: fft ( -- )
  bitrev 1 LOGN 1- for dup stage 2* next drop ;

\ remove table bias once at load
: unb ( arr n bias -- )
  swap 1- for
    i 2 pick a@ over - i 3 pick a!
  next 2drop ;
: fix ( -- )
  wr $80 $8000 unb  wi $80 $8000 unb
  gre $10 $100000 unb  gim $10 $100000 unb ;
fix

\ test signal: 14-bit pseudo random, same on every VM (no division)
: lcg ( x -- x' ) 1+ $4b * 1- $10001 mod ;
: sig ( -- )
  1 N 1- for
    lcg dup $3fff and $2000 - i re a! 0 i im a!
  next drop ;

\ test: compare bins 0..15 with gold
: dre ( k -- d ) dup re a@ swap gre a@ - abs ;
: dim ( k -- d ) dup im a@ swap gim a@ - abs ;
: worst ( -- n ) 0 $f for i dre max i dim max next ;
: check ( -- ) sig fft worst . worst TOL <= . ;   \ prints max error, then -1 if ok
: bench ( -- ) $ff for sig fft next ;             \ 256 transforms
: z {n} for bench next ; z
"""
    open(path, "w").write(fft)

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
    print("fs=", fs)
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
    ap.add_argument("--repeat", type=int, default=9, help="repetitions per run (z count)")
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
    print(f"{a.cxx} {a.opt} XT0_U32={a.xt0}, {a.runs} epochs, each with z={a.repeat} (-> repeat {reps} loops)")
    print(f"{'NOS':>3} {'RTOP':>4} {'align':>5} {'best ms':>8} {'median':>8} {'ms/run':>7} {'vs first':>8}")
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
