#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np

# Dataset parsing from benchmark logs
labels = [
    'O2\n(0,0)', 'O2\n(0,1)', 'O2\n(1,0)', 'O2\n(1,1)',
    'O3\n(0,0)', 'O3\n(0,1)', 'O3\n(1,0)', 'O3\n(1,1)',
    'Os\n(0,0)', 'Os\n(0,1)', 'Os\n(1,0)', 'Os\n(1,1)'
]

# Formatted configurations: (NOS, RTOP)
instructions = np.array([
    6512942452, 5946476737, 7361216995, 6794867314, # -O2
    6512592509, 5947235861, 7361460560, 6794933067, # -O3
    6513069682, 5946831775, 7361647789, 6795451111  # -Os
]) / 1e9  # Scale to Billions

cycles = np.array([
    3970326673, 3269937970, 4064184766, 3572502947, # -O2
    3835544117, 3635868287, 4237494764, 3595971275, # -O3
    4076466861, 3632655033, 4281021786, 3830045605  # -Os
]) / 1e9  # Scale to Billions

branch_misses = np.array([
    60888109, 61303415, 60212823, 60198005, # -O2
    60884885, 61419365, 60200345, 61259681, # -O3
    62481182, 61415917, 61264509, 61262536  # -Os
]) / 1e7  # Scale to Billions

run_time = np.array([       # ms/yy
    35.873, 29.468, 36.540, 32.138,
    34.573, 32.717, 38.087, 32.358,
    36.671, 32.689, 38.285, 34.423
])
perf = 1 / run_time
mn   = perf.min() 
perf = (perf / mn) - 1

x = np.arange(len(labels))

fig, ax1 = plt.subplots(figsize=(12, 6.5))

ax1.set_xlabel('Optimization Level & Caching Config (NOS, RTOP)', fontsize=11, fontweight='bold', labelpad=12)
ax1.set_ylabel('Instructions/Cycles/BranchMisses (Billions)', color='black', fontsize=11, fontweight='bold')
ax1.set_ylim(0, 8)
ax1.tick_params(axis='y', labelcolor='black')
ax1.set_xticks(x)
ax1.set_xticklabels(labels, fontsize=10)
ax1.grid(True, axis='y', linestyle='--', alpha=0.4)

# Plot primary bars (Instructions)
color_ins = '#2b5c8f'
bar1 = ax1.bar(x-0.2, instructions, width=0.4, color=color_ins, alpha=0.85, edgecolor='#1a3a5f', label='Instructions')
color_miss = '#d5d0f3'
bar2 = ax1.bar(x+0.2, branch_misses, width=0.4, color=color_miss, alpha=0.85, edgecolor='#1a3a5f', label='Branch Misses * 100')

# Plot secondary data line overlapping (Cycles)
color_cyc = '#75d0b3'
line1 = ax1.plot(x, cycles, color=color_cyc, linewidth=0, marker='s', markersize=20, label='Cycles')

color_perf = '#d95f02'
line2 = ax1.plot(x, (perf+1) * 4, color=color_perf, linewidth=0,  marker='o', markersize=28, label='Relative Perf')
# Instantiate secondary y-axis for scale-sensitive Branch Misses
ax2 = ax1.twinx()
ax2.set_ylabel('Relative Performance', color=color_perf, fontsize=11, fontweight='bold')
ax2.set_ylim(0.5, 1.5)
ax2.tick_params(axis='y', labelcolor=color_perf)

x_range = x.max() - x.min()
x_offset = x_range * 0.02

for xi, yi, txt in zip(x, perf, perf):
    ax1.text(xi + x_offset, (yi+1)*4, round(txt+1,2), ha='right', va='center', fontsize=11, fontweight='bold', color='black')

# Section divider background shading
ax1.axvspan(-0.5, 3.5, color='gray', alpha=0.1)
ax1.axvspan(3.5, 7.5, color='gray', alpha=0.2)
ax1.axvspan(7.5, 11.5, color='gray', alpha=0.1)

# Typography Labels
ax1.text(1.5, ax1.get_ylim()[1]*0.95, '-O2 Profiling', ha='center', fontsize=11, fontweight='bold', alpha=0.5)
ax1.text(5.5, ax1.get_ylim()[1]*0.95, '-O3 Profiling', ha='center', fontsize=11, fontweight='bold', alpha=0.5)
ax1.text(9.5, ax1.get_ylim()[1]*0.95, '-Os Profiling', ha='center', fontsize=11, fontweight='bold', alpha=0.5)

# Unify Legending
lines_labels = [bar1] + [bar2] + line1 + line2
labels_all = [l.get_label() for l in lines_labels]
ax1.legend(lines_labels, labels_all, loc='lower right', framealpha=0.95)

plt.title('eForth Hardware Metric Profiles: Register Caching Strategies vs. Optimization Flags', fontsize=13, fontweight='bold', pad=15)
plt.tight_layout()
plt.savefig('forth_perf_comparison.png', dpi=200)
print("Chart safely compiled and exported as 'forth_perf_comparison.png'.")
