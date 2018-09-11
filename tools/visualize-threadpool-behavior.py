#!/usr/bin/env python
# Author: Jamie Davis <davisjam@vt.edu>
# Description: Visualize threadpool behavior.
#   Relies on the statistics dumped by NodeThreadpool::PrintStats.

import matplotlib.pyplot as plt
import json
from collections import defaultdict
import matplotlib.ticker
import argparse
import re
import os

import pandas as pd
import numpy as np

import StringIO

# Parse args
parser = argparse.ArgumentParser(description='Visualize Node.js threadpool behavior')
parser.add_argument('--log-file', '-f', help='Log file containing LOG statements emitted by src/node_threadpool.cc during a Node.js application run.', required = True)
parser.add_argument('--vis-dir', '-d', help='Where to save the visualizatons?', required = False)

args = parser.parse_args()

# Parsed args
logFile = args.log_file
if args.vis_dir:
  visDir = args.vis_dir
else:
  visDir = '.'

# Extract CSV lines from log file
queueLengths_header = ''
queueLengths_data = []
taskSummaries_header = ''
taskSummaries_data = []
# And map from TP ID to label
tpIDToLabel = {}
taskOriginIDToLabel = {}
taskTypeIDToLabel = {}
for line in open(logFile, 'r'):
  # Skip lines that cannot contain CSV data
  if ',' not in line:
    continue

  m = re.match('^TP key: (\d+),\s*(.+)\s*,(\d+)$', line)
  if m:
    tpID = int(m.group(1))
    label = m.group(2) + " (size {})".format(m.group(3))
    tpIDToLabel[tpID] = label
    print "{} -> {}".format(tpID, label)

  m = re.match('^QueueLengths data format: (.+)$', line)
  if m:
    queueLengths_header = m.group(1)

  m = re.match('^QueueLengths data: (.+)$', line)
  if m:
    queueLengths_data.append(m.group(1))

  m = re.match('^TaskSummary data format: (.+)$', line)
  if m:
    taskSummaries_header = m.group(1)

  m = re.match('^TaskSummary data: (.+)$', line)
  if m:
    taskSummaries_data.append(m.group(1))

# Write these to a StringIO so we can pandas.read_csv easily
queueLengths = [queueLengths_header]
queueLengths = queueLengths + queueLengths_data

taskSummaries = [taskSummaries_header]
taskSummaries = taskSummaries + taskSummaries_data

print "Got {} queueLengths".format(len(queueLengths))
print "Got {} taskSummaries".format(len(taskSummaries))
queueLengths_sio = StringIO.StringIO('\n'.join(queueLengths))
taskSummaries_sio = StringIO.StringIO('\n'.join(taskSummaries))

# Read as data frames
df_ql = pd.read_csv(queueLengths_sio)
df_ts = pd.read_csv(taskSummaries_sio)

print "First queue lengths data frame"
print df_ql.head(1)
print "First task summaries data frame"
print df_ts.head(1)

# Align the queue length samples.
# Normalize queue length sample times to zero -- the smallest value across all df_ql.time values.
print "Normalizing QL times"
minQLTime = df_ql['time'].min()
minTSTime = df_ts['time-at-completion'].min()
minObservedTime = min(minQLTime, minTSTime)
df_ql['time'] = df_ql['time'] - minObservedTime
df_ts['time-at-completion'] = df_ts['time-at-completion'] - minObservedTime

# Describe the distribution of task types in each pool
print "Distribution of task types by origin"
print df_ts.groupby(['task-origin', 'task-type']).size()

tpIDs = df_ql.TP.unique()
tpIDToColor = { 0: 'red', 1: 'blue', 2: 'green', 3: 'orange', 4: 'yellow' }

# Plot per-TP queue lengths over time.
if True:
  print "TPs: {}".format(tpIDs)
  for tp in tpIDs:
    print "\n-----------------------"
    print "TP {}: {}".format(tp, tpIDToLabel[tp])
    print "-----------------------\n"
    df_ql_tp = df_ql.loc[df_ql['TP'] == tp]
    print df_ql_tp.describe()

    try:
      # Queue length for this tp: line chart
      #ax = df_ql_tp.plot(ax=ax, x='time', y='queue-length', color=colors[tp], kind='line', label='TP {}'.format(tp))

      # Queue length for this tp: scatter
      fig, ax = plt.subplots()
      df_ql_tp.plot.scatter(ax=ax, x='time', y='queue-length', c='xkcd:{}'.format(tpIDToColor[tp]), s=1, label=tpIDToLabel[tp])
      # Queue length for this tp's CPU tasks: scatter
      df_ql_tp.plot.scatter(ax=ax, x='time', y='n_cpu', c='xkcd:light {}'.format(tpIDToColor[tp]), s=1, label=tpIDToLabel[tp] + " CPU tasks")

      ax.set_title('Queue lengths over time')
      ax.set_xlabel('Time (ns)')
      ax.set_ylabel('Queue length')
      fname = os.path.join(visDir, 'tp-{}-queueLengths.png'.format(tp))
      plt.savefig(fname)
      print "See {} for plot of TP {} queue lengths".format(fname, tp)
    except:
      pass

# Plot per-TP task running times as a histogram.
if True:
  tpIDs = df_ql.TP.unique()
  print "TPs: {}".format(tpIDs)
  for tp in tpIDs:
    df_ts_tp = df_ts.loc[df_ts['TP'] == tp]
    print "TP {}".format(tp)
    print df_ts_tp.describe()
		# TODO It would also be nice to emit the count of the different types of tasks.
    #df_ts_tp['run_time'].plot.hist(bins=10, title='Histogram of running times for tasks in TP {}'.format(tp))
    #plt.show()
