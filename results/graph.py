#! /usr/bin/env python3
from argparse import ArgumentParser
from IPython import embed
from pathlib import Path
from pprint import pprint

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import math

def rgb(h):
    assert isinstance(h, str) and len(h) == 7 and h[0] == '#'
    r = int(h[1:3], base=16)
    g = int(h[3:5], base=16)
    b = int(h[5:7], base=16)
    return (r / 255.0, g / 255.0, b / 255.0, 1.0)

def graph(mean_df, ci_df, output_file):
    # Preample

    plt.rcParams['hatch.linewidth'] = 0.5
    plt.rcParams['font.family']     = 'serif'
    plt.rcParams['font.size']       = 6
    plt.rcParams['axes.labelsize']  = 6

    # Graph
    
    ax = mean_df.plot.bar(yerr=ci_df, edgecolor='k', linewidth=0.5, 
        color=[rgb('#a2e8f1'), rgb('#3E78B2'), rgb('#772D8B')])

    plt.ylabel('Throughput (op/sec)')
    plt.xticks(rotation=0)
    plt.ylim(0, 8000)
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.05),
                      ncol=3, fancybox=True, shadow=True)

    # Output
    fig = plt.gcf()
    fig.set_size_inches(3.5, 1.75)
    fig.tight_layout(pad=0.0, h_pad=0.0)
    plt.savefig(output_file, dpi=300, bbox_inches='tight', pad_inches=0.02)

def prepare(df):
    # Remove servers we don't want
    smap = {'redis-server-dumb': r'Redis$_{H-intra}$', 
            'redis-server': 'Redis-pmem', 
            'redis-server-heuristic': r'Redis$_{H-full}$'}
    wmap = {'load': 'Load', 'workloada': 'A', 'workloadb': 'B', 
            'workloadc': 'C', 'workloadd': 'D', 'workloade': 'E', 'workloadf': 'F'}
    
    servers = df['Server'].isin(smap.keys())
    df = df[servers]


    # Main Workloads
    wdf = df[df['Command'] == 'run']
    wdf = wdf[['Server', 'Workload', 'Throughput']]

    # "Load" workload
    ldf = df[df['Command'] == 'load']
    ldf = ldf[['Server', 'Throughput']]
    ldf['Workload'] = ['load'] * len(ldf)
    
    cdf = pd.concat([ldf, wdf]).groupby(['Workload', 'Server'])

    mean = cdf.mean()
    std_err = cdf.std() / np.sqrt(cdf.count())
    ci = 1.96 * std_err

    # Index should be workload.
    mean = mean.unstack(level=1)
    mean.columns = [x[1] for x in mean.columns]
    mean = mean[smap.keys()] # sort

    ci = ci.unstack(level=1)
    ci.columns = [x[1] for x in ci.columns]

    # Clean up the field names
    mean = mean.rename(columns=smap, index=wmap)
    ci = ci.rename(columns=smap, index=wmap)

    return mean, ci

def main():
    parser = ArgumentParser()
    parser.add_argument('input_file', type=Path)
    parser.add_argument('output_file', type=Path)

    args = parser.parse_args()
    assert args.input_file.exists()

    df = pd.read_csv(args.input_file)
    mean_df, ci_df = prepare(df)
    graph(mean_df, ci_df, args.output_file)

    # Normalize and print
    norm = lambda s: s / mean_df['Redis-pmem']
    pprint(mean_df.apply(norm, axis=0))
    
if __name__ == '__main__':
    main()
