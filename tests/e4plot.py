#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np

option = 0   # or 0=fib or 1=yy

# Dataset parsing from benchmark logs
labels = [
    'O2\n(0,0)', 'O2\n(0,1)', 'O2\n(1,0)', 'O2\n(1,1)',
    'O3\n(0,0)', 'O3\n(0,1)', 'O3\n(1,0)', 'O3\n(1,1)',
    'Os\n(0,0)', 'Os\n(0,1)', 'Os\n(1,0)', 'Os\n(1,1)'
]

stats_fib = [
{ 'nos':0, 'rtop':0, 'align':4, 'best':1071.5, 'median':1077.5, 'avg':35.717, 
  'perf':  {'instructions': 6512850387.0, 'cycles': 3969494929.0, 'branch-misses': 60886506.0, 'ipc': 1.64} },
{ 'nos':0, 'rtop':1, 'align':4, 'best':882.7, 'median':882.9, 'avg':29.424, 
  'perf':  {'instructions': 5946470538.0, 'cycles': 3269283258.0, 'branch-misses': 61308065.0, 'ipc': 1.82} },
{ 'nos':1, 'rtop':0, 'align':4, 'best':1095.9, 'median':1089.4, 'avg':36.532, 
  'perf':  {'instructions': 7361459728.0, 'cycles': 4064828905.0, 'branch-misses': 60214451.0, 'ipc': 1.81} },
{ 'nos':1, 'rtop':1, 'align':4, 'best':962.7, 'median':964.5, 'avg':32.088, 
  'perf':  {'instructions': 6794931990.0, 'cycles': 3572599860.0, 'branch-misses': 60197714.0, 'ipc': 1.9} },
{ 'nos':0, 'rtop':0, 'align':4, 'best':1034.3, 'median':1041.8, 'avg':34.478, 
  'perf':  {'instructions': 6512896571.0, 'cycles': 3834989195.0, 'branch-misses': 60885661.0, 'ipc': 1.7} },
{ 'nos':0, 'rtop':1, 'align':4, 'best':982.5, 'median':981.5, 'avg':32.750, 
  'perf':  {'instructions': 5946827513.0, 'cycles': 3634475960.0, 'branch-misses': 61416813.0, 'ipc': 1.64} },
{ 'nos':1, 'rtop':0, 'align':4, 'best':1142.3, 'median':1143.3, 'avg':38.076, 
  'perf':  {'instructions': 7361422341.0, 'cycles': 4236389763.0, 'branch-misses': 60200391.0, 'ipc': 1.74} },
{ 'nos':1, 'rtop':1, 'align':4, 'best':976.7, 'median':978.5, 'avg':32.555, 
  'perf':  {'instructions': 6794961508.0, 'cycles': 3595024436.0, 'branch-misses': 61260071.0, 'ipc': 1.89} },
{ 'nos':0, 'rtop':0, 'align':4, 'best':1099.8, 'median':1097.8, 'avg':36.660, 
  'perf':  {'instructions': 6513095251.0, 'cycles': 4075936136.0, 'branch-misses': 62479718.0, 'ipc': 1.6} },
{ 'nos':0, 'rtop':1, 'align':4, 'best':981.1, 'median':980.7, 'avg':32.702, 
  'perf':  {'instructions': 5946899994.0, 'cycles': 3632870052.0, 'branch-misses': 61416136.0, 'ipc': 1.64} },
{ 'nos':1, 'rtop':0, 'align':4, 'best':1154.1, 'median':1154.2, 'avg':38.471, 
  'perf':  {'instructions': 7361658425.0, 'cycles': 4279044220.0, 'branch-misses': 61263617.0, 'ipc': 1.72} },
{ 'nos':1, 'rtop':1, 'align':4, 'best':1038.0, 'median':1033.4, 'avg':34.599, 
  'perf':  {'instructions': 6795335803.0, 'cycles': 3829651882.0, 'branch-misses': 61262696.0, 'ipc': 1.77} },
]

stats_yy = [
{ 'nos':0, 'rtop':0, 'align':4, 'best':111.8, 'median':112.5, 'avg':3.728, 
  'perf':  {'instructions': 1206156180.0, 'cycles': 391123402.0, 'branch-misses': 52738.0, 'ipc': 3.08} },
{ 'nos':0, 'rtop':1, 'align':4, 'best':106.6, 'median':107.1, 'avg':3.554, 
  'perf':  {'instructions': 1055691829.0, 'cycles': 379534656.0, 'branch-misses': 51766.0, 'ipc': 2.78} },
{ 'nos':1, 'rtop':0, 'align':4, 'best':113.9, 'median':119.4, 'avg':3.797, 
  'perf':  {'instructions': 1386226410.0, 'cycles': 426862094.0, 'branch-misses': 52300.0, 'ipc': 3.25} },
{ 'nos':1, 'rtop':1, 'align':4, 'best':112.3, 'median':111.3, 'avg':3.744, 
  'perf':  {'instructions': 1235835872.0, 'cycles': 395849303.0, 'branch-misses': 52944.0, 'ipc': 3.12} },
{ 'nos':0, 'rtop':0, 'align':4, 'best':107.1, 'median':109.4, 'avg':3.569, 
  'perf':  {'instructions': 1206110418.0, 'cycles': 380686183.0, 'branch-misses': 52159.0, 'ipc': 3.17} },
{ 'nos':0, 'rtop':1, 'align':4, 'best':100.9, 'median':105.2, 'avg':3.365, 
  'perf':  {'instructions': 1055700963.0, 'cycles': 379105005.0, 'branch-misses': 52129.0, 'ipc': 2.78} },
{ 'nos':1, 'rtop':0, 'align':4, 'best':110.5, 'median':109.4, 'avg':3.683, 
  'perf':  {'instructions': 1386212450.0, 'cycles': 414361975.0, 'branch-misses': 52561.0, 'ipc': 3.35} },
{ 'nos':1, 'rtop':1, 'align':4, 'best':96.6, 'median':96.8, 'avg':3.221, 
  'perf':  {'instructions': 1235713760.0, 'cycles': 338930975.0, 'branch-misses': 51770.0, 'ipc': 3.65} },
{ 'nos':0, 'rtop':0, 'align':4, 'best':109.6, 'median':109.6, 'avg':3.654, 
  'perf':  {'instructions': 1236181068.0, 'cycles': 385100932.0, 'branch-misses': 111287.0, 'ipc': 3.21} },
{ 'nos':0, 'rtop':1, 'align':4, 'best':88.5, 'median':95.6, 'avg':2.949, 
  'perf':  {'instructions': 1085673077.0, 'cycles': 342194704.0, 'branch-misses': 110026.0, 'ipc': 3.17} },
{ 'nos':1, 'rtop':0, 'align':4, 'best':119.0, 'median':118.3, 'avg':3.966, 
  'perf':  {'instructions': 1416236298.0, 'cycles': 421794038.0, 'branch-misses': 109992.0, 'ipc': 3.36} },
{ 'nos':1, 'rtop':1, 'align':4, 'best':103.0, 'median':108.6, 'avg':3.434, 
  'perf':  {'instructions': 1265762211.0, 'cycles': 386687306.0, 'branch-misses': 110963.0, 'ipc': 3.27} },
]

out_fn   = [ 'e4plot_fib.png', 'e4plot_yy.png'  ][option]
stat_src = [ stats_fib       , stats_yy         ][option]
rate     = [[1e9,1e9,1e7,1e0], [2e8,2e8,2e4,1e0]][option]
title    = [ 'fib'           , 'looper'         ][option]
legend   = [ 'upper right'   , 'upper left'     ][option]

instr = np.array([])
cycle = np.array([])
miss  = np.array([])
bench = np.array([])
for e in stat_src:
    instr = np.append(instr, e['perf']['instructions'] / rate[0])    
    cycle = np.append(cycle, e['perf']['cycles']       / rate[1])
    miss  = np.append(miss,  e['perf']['branch-misses']/ rate[2])
    bench = np.append(bench, e['avg']                  / rate[3])

for e in [instr, cycle, miss, bench]:
    print(e)
    
perf = 1 / bench
base = perf[0]
perf = (perf / base) - 1

x = np.arange(len(labels))

fig, ax1 = plt.subplots(figsize=(6, 6))

ax1.set_xlabel('Optimization Level & Caching Config (NOS, RTOP)', fontsize=10, fontweight='bold', labelpad=12)
ax1.set_ylabel('Instructions/Cycles/BR Misses (Billions)', color='black', fontsize=10, fontweight='bold')
ax1.set_ylim(0, 8)
ax1.tick_params(axis='y', labelcolor='black')
ax1.set_xticks(x)
ax1.set_xticklabels(labels, fontsize=8)
ax1.grid(True, axis='y', linestyle='-', alpha=0.4)

# Plot primary bars (Inst)
color_instr = '#2b5c8f'
bar1 = ax1.bar(x-0.2, instr, width=0.4, color=color_instr, alpha=0.85, edgecolor='#1a3a5f', label='Instruction Count')
color_miss = '#c5d0f3'
bar2 = ax1.bar(x+0.2, miss, width=0.4, color=color_miss, alpha=0.85, edgecolor='#1a3a5f', label=f"Branch Misses*{rate[0]/rate[2]}")

# Plot secondary data line overlapping (Cycles)
color_cycle = '#75d0b3'
line1 = ax1.plot(x, cycle, color=color_cycle, linewidth=0, marker='s', markersize=6, label='Cycle Executed')

color_perf = '#f9afa2'
line2 = ax1.plot(x, (perf*20)+2, color=color_perf, linewidth=0,  marker='o', markersize=20, label='Relative Performance')
# Instantiate secondary y-axis for scale-sensitive Branch Misses
ax2 = ax1.twinx()
ax2.set_ylabel('Relative Performance', color='black', fontsize=10, fontweight='bold')
ax2.set_ylim(0.90, 1.30)
ax2.tick_params(axis='y', labelcolor=color_perf)

x_range  = x.max() - x.min()
x_offset = x_range * 0.0            # center
for xi, yi, txt in zip(x, perf, perf):
    ax1.text(xi, (yi*20)+2-0.03, round(txt+1,2), ha='center', va='center', fontsize=8, color='black')

# Section divider background shading
ax1.axvspan(-0.5, 3.5, color='gray', alpha=0.1)
ax1.axvspan(3.5, 7.5,  color='gray', alpha=0.2)
ax1.axvspan(7.5, 11.5, color='gray', alpha=0.1)

# Typography Labels
ax1.text(2.5,  ax1.get_ylim()[1]*0.95, '-O2 Profile', ha='center', fontsize=9, fontweight='bold', alpha=0.5)
ax1.text(6.5,  ax1.get_ylim()[1]*0.95, '-O3 Profile', ha='center', fontsize=9, fontweight='bold', alpha=0.5)
ax1.text(10.5, ax1.get_ylim()[1]*0.95, '-Os Profile', ha='center', fontsize=9, fontweight='bold', alpha=0.5)

# Unify Legending
lines_labels = [bar1] + [bar2] + line1 + line2
labels_all = [l.get_label() for l in lines_labels]
ax1.legend(lines_labels, labels_all, loc=legend, framealpha=0.60)

plt.title(f'eForth {title}: Register Caching vs. Optimization', fontsize=11, fontweight='bold', pad=15)
plt.tight_layout()
plt.savefig(out_fn, dpi=200)
print(f"Chart safely compiled and exported as {out_fn}.")
