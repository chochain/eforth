#!/usr/bin/env python3
"""
e4stats.py - machine-code statistics for the ceForth primitive words.

For every CODE("name", ...) / IMMD("name", ...) lambda in g_rom it reports:
  * instruction count             (padding nops excluded)
  * whether the word spills regs  (x86/ARM: push/pop, RISC-V/Xtensa: stores to sp)
  * whether the word ends in an indirect tail-jump (=> the tail-call chain works)

How it works: compile with -g1, run `objdump -d -S`, and use the interleaved
source line `CODE("+", ...` that objdump prints under each lambda symbol
(_ZN5g_romMUl...) to give the machine code a Forth name.

Examples (run from the directory holding ceforth.cpp / ceforth.h / config.h):

  # compare two builds side by side (each quoted string = extra compiler flags)
  ./e4stats.py "-DE4_NOS=1 -DE4_RTOP=1"  "-DE4_NOS=0 -DE4_RTOP=1"

  # try every ordering of tos/nos/rp in the argument list (needs NOS=1, RTOP=1)
  ./e4stats.py --perm

  # disassemble a single word in each build
  ./e4stats.py --show "+" "-DE4_NOS=1 -DE4_RTOP=1"

  # cross compile: pick the compiler and objdump, flags go in the build string
  ./e4stats.py --cxx arm-none-eabi-g++ --objdump arm-none-eabi-objdump \
                     "-Os -mcpu=cortex-m4 -mthumb"

  # analyse object files you built yourself (they must have been built with -g)
  ./e4stats.py --obj a.o --obj b.o
"""
import argparse, itertools, os, re, shutil, subprocess, sys, tempfile

HOT = "dup drop swap over + - * and or xor lshift rshift i _next _loop _; _: _lit".split()

# ---------------------------------------------------------------- per-arch regexes
ARCH = {
    # name:    (spill regex,                         indirect tail-jump regex)
    "x86":     (r"^push",                            r"^(notrack\s+)?jmp\s+\*"),
    "arm":     (r"^push",                            r"^bx\s+(r\d+|ip)\b"),
    "riscv":   (r"^s[wd]\s+\S+,\s*-?\d+\(sp\)",      r"^jr\s+(a|t|s)\d"),
    "xtensa":  (r"^s32i(\.n)?\s+a\d+,\s*a1,",        r"^jx\s+a\d+"),
}
PADDING = re.compile(r"^(nop|xchg\s+%ax,%ax|cs\s+nop|data16|\.byte|c\.nop)")


def detect_arch(objdump_text):
    m = re.search(r"file format (\S+)", objdump_text)
    f = m.group(1) if m else ""
    if "x86" in f or "i386" in f: return "x86"
    if "arm" in f:                return "arm"
    if "riscv" in f:              return "riscv"
    if "xtensa" in f:             return "xtensa"
    sys.exit(f"unsupported object format: {f!r}")


# ---------------------------------------------------------------- analysis
def analyse(obj, objdump, prefix="_ZN5g_rom"):
    out = subprocess.run([objdump, "-d", "-S", "--no-show-raw-insn", obj],
                         capture_output=True, text=True, check=True).stdout
    arch = detect_arch(out)
    spill_re, tail_re = (re.compile(r) for r in ARCH[arch])
    funcs, tag, cur = {}, {}, None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.*)>:", line)
        if m:
            if m.group(1).startswith((".L", "$", "L")) and cur is not None:
                continue                                    # local label inside a function: not a new function
            cur = m.group(1)
            funcs[cur] = []
            continue
        if cur is None:
            continue
        if re.match(r"^\s+[0-9a-f]+:\s", line):                    # an instruction
            ins = re.sub(r"\s#\s.*$", "", line.split(":", 1)[1]).strip()   # drop objdump "# addr <sym>" comments, keep ARM "#imm"
            if ins and not PADDING.match(ins):
                funcs[cur].append(ins)
        elif cur.startswith(prefix):                                # interleaved source
            m = re.search(r'(?:CODE|IMMD)\("((?:[^"\\]|\\.)*)"', line)
            if m:
                tag.setdefault(m.group(1), cur)
    if not tag:
        sys.exit(f"{obj}: no CODE(...) source lines found - was it compiled with -g?")
    words = {}
    for name, sym in tag.items():
        ins = funcs[sym]
        words[name] = dict(
            n=len(ins), ins=ins,
            spill=sum(1 for i in ins if spill_re.match(i)),
            tail=any(tail_re.match(i) for i in ins))
    return dict(arch=arch, words=words)


# ---------------------------------------------------------------- building
def default_flags(cxx):
    f = "-std=gnu++17 -O2 -g1 -w -include iostream -include cerrno -include algorithm -include cstdlib"
    if not re.search(r"(arm|riscv|xtensa|aarch64|avr)", os.path.basename(cxx)):
        f += " -fcf-protection=none -fno-pie -DXT0_U32=1"       # x86: fold XT0, drop endbr64
    return f


def build(cxx, src, incdir, flags, outdir, tag):
    obj = os.path.join(outdir, tag + ".o")
    cmd = [cxx] + flags.split() + [f"-I{incdir}", "-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit("compile failed:\n  " + " ".join(cmd) + "\n" + r.stderr[-1500:])
    return obj


def permuted_sources(src, workdir, order):
    """copy ceforth.{cpp,h}, config.h into workdir with the rp/tos/nos argument order changed"""
    d = os.path.dirname(os.path.abspath(src))
    for f in ("ceforth.cpp", "ceforth.h", "config.h"):
        shutil.copy(os.path.join(d, f), workdir)
    hp = os.path.join(workdir, "ceforth.h")
    s = open(hp).read()
    decl = {"tos": "DU tos", "nos": "DU nos", "rp": "DU *rp"}
    s, a = re.subn(r"^#define\s+ARGS\s+[^\n]*\brp\b[^\n]*$",
                   "#define ARGS      " + ", ".join(decl[p] for p in order), s, flags=re.M)
    s, b = re.subn(r"^#define\s+PASS\s+[^\n]*\brp\b[^\n]*$",
                   "#define PASS      " + ", ".join(order), s, flags=re.M)
    print("#define ARGS      " + ", ".join(decl[p] for p in order))
    print("#define PASS      " + ", ".join(order))
    if a != 1 or b != 1:
        sys.exit("--perm: could not find the E4_RTOP 'ARGS'/'PASS' definitions in ceforth.h")
    open(hp, "w").write(s)


# ---------------------------------------------------------------- reporting
def table(cols, hot, show_names):
    names = [c[0] for c in cols]
    w = max(12, max(len(n) for n in names) + 1)
    def row(label, vals):
        print(f"{label:<28}" + "".join(f"{v:>{w}}" for v in vals))
    print()
    row("", names)
    row("-" * 27, ["-" * (w - 2)] * len(cols))
    row("words with spill (push/pop)", [sum(1 for x in a["words"].values() if x["spill"]) for _, a in cols])
    row("total instructions",          [sum(x["n"] for x in a["words"].values())          for _, a in cols])
    row("words w/o indirect tail-jump", [sum(1 for x in a["words"].values() if not x["tail"]) for _, a in cols])
    print()
    for h in hot:
        if not any(h in a["words"] for _, a in cols):
            continue
        vals = []
        for _, a in cols:
            x = a["words"].get(h)
            vals.append("-" if x is None else f"{x['n']}{'*' if x['spill'] else ''}")
        row(f'"{h}"', vals)
    print("\n  * = the word spills registers (push/pop or stores to the stack)")
    for n, a in cols:
        bad = sorted(k for k, x in a["words"].items() if not x["tail"])
        if bad:
            print(f"  [{n}] no indirect tail-jump in: {' '.join(bad[:12])}{' ...' if len(bad) > 12 else ''}"
                  "   (compile-time/IO words are fine; hot words here = broken tail call)")
    pushers = {n: sorted(k for k, x in a["words"].items() if x["spill"]) for n, a in cols}
    if len(cols) == 2:
        a, b = (set(pushers[n]) for n in names)
        only0, only1 = sorted(a - b), sorted(b - a)
        if only0: print(f"  spill only in [{names[0]}]: {' '.join(only0)}")
        if only1: print(f"  spill only in [{names[1]}]: {' '.join(only1)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("builds", nargs="*", help='extra compiler flags per build, e.g. "-DE4_NOS=0 -DE4_RTOP=1"')
    ap.add_argument("-s", "--src", default="ceforth.cpp")
    ap.add_argument("--cxx", default="g++")
    ap.add_argument("--objdump", default="objdump")
    ap.add_argument("--flags", help="replace the default base compiler flags")
    ap.add_argument("--obj", action="append", default=[], help="analyse an existing object file (built with -g)")
    ap.add_argument("--perm", action="store_true", help="sweep the 6 orderings of tos/nos/rp (NOS=1,RTOP=1)")
    ap.add_argument("--words", default=" ".join(HOT), help="words to list (space separated)")
    ap.add_argument("--show", metavar="WORD", help="print the disassembly of WORD for every build")
    a = ap.parse_args()

    base = a.flags if a.flags else default_flags(a.cxx)
    tmp = tempfile.mkdtemp(prefix="e4stats_")
    cols = []
    try:
        if a.perm:
            for order in itertools.permutations(["tos", "nos", "rp"]):
                wd = os.path.join(tmp, "_".join(order)); os.makedirs(wd)
                permuted_sources(a.src, wd, order)
                obj = build(a.cxx, os.path.join(wd, "ceforth.cpp"), wd,
                            base + " -DE4_NOS=1 -DE4_RTOP=1 " + " ".join(a.builds), wd, "perm")
                cols.append((",".join(order), analyse(obj, a.objdump)))
        else:
            inc = os.path.dirname(os.path.abspath(a.src))
            for i, extra in enumerate(a.builds or [""]):
                if not a.obj or a.builds:
                    obj = build(a.cxx, a.src, inc, base + " " + extra, tmp, f"b{i}")
                    cols.append((extra.strip() or "defaults", analyse(obj, a.objdump)))
            for o in a.obj:
                cols.append((os.path.basename(o), analyse(o, a.objdump)))
        table(cols, a.words.split(), True)
        if a.show:
            for n, x in cols:
                w = x["words"].get(a.show)
                print(f'\n=== "{a.show}"  [{n}]  ({x["arch"]})' + ("" if w else "  - not found"))
                if w:
                    print("\n".join("    " + i for i in w["ins"]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
