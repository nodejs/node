#!/usr/bin/env python3
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
"""
This script averages numbers output from another script. It is useful
to average over a benchmark that outputs one or more results of the form
  <key> <number> <unit>
key and unit are optional, but only one number per line is processed.

For example, if
  $ bch --allow-natives-syntax toNumber.js
outputs
  Number('undefined'):  155763
  (+'undefined'):  193050 Kps
  23736 Kps
then
  $ avg.py 10 bch --allow-natives-syntax toNumber.js
will output
  [10/10] (+'undefined')         : avg 192,240.40 stddev   6,486.24 (185,529.00 - 206,186.00)
  [10/10] Number('undefined')    : avg 156,990.10 stddev  16,327.56 (144,718.00 - 202,840.00) Kps
  [10/10] [default]              : avg  22,885.80 stddev   1,941.80 ( 17,584.00 -  24,266.00) Kps
"""

import argparse
import subprocess
import re
import numpy
import time
import sys
import signal

parser = argparse.ArgumentParser(
    description="A script that averages numbers from another script's output",
    epilog="Example:\n\tavg.py 10 bash -c \"echo A: 100; echo B 120; sleep .1\""
)
parser.add_argument(
    'repetitions',
    type=int,
    help="number of times the command should be repeated")
parser.add_argument(
    'command',
    nargs=argparse.REMAINDER,
    help="command to run (no quotes needed)")
parser.add_argument(
    '--echo',
    '-e',
    action='store_true',
    default=False,
    help="set this flag to echo the command's output")

args = vars(parser.parse_args())

if (len(args['command']) == 0):
  print("No command provided.")
  exit(1)


class FieldWidth:

  def __init__(self, key=0, average=0, stddev=0, min=0, max=0):
    self.w = dict(key=key, average=average, stddev=stddev, min=min, max=max)

  def max_with(self, w2):
    self.w = {k: max(v, w2.w[k]) for k, v in self.w.items()}

  def __getattr__(self, key):
    return self.w[key]


def fmtS(string, width=0):
  return "{0:<{1}}".format(string, width)


def fmtN(num, width=0):
  return "{0:>{1},.2f}".format(num, width)


class Measurement:

  def __init__(self, key, unit):
    self.key = key
    self.unit = unit
    self.values = []
    self.average = 0
    self.count = 0
    self.M2 = 0
    self.min = float("inf")
    self.max = -float("inf")

  def addValue(self, value):
    try:
      num_value = float(value)
      self.values.append(num_value)
      self.min = min(self.min, num_value)
      self.max = max(self.max, num_value)
      self.count = self.count + 1
      delta = num_value - self.average
      self.average = self.average + delta / self.count
      delta2 = num_value - self.average
      self.M2 = self.M2 + delta * delta2
    except ValueError:
      print("Ignoring non-numeric value", value)

  def status(self, w):
    return "{}: avg {} stddev {} ({} - {}) {}".format(
        fmtS(self.key, w.key), fmtN(self.average, w.average),
        fmtN(self.stddev(), w.stddev), fmtN(self.min, w.min),
        fmtN(self.max, w.max), fmtS(self.unit_string()))

  def unit_string(self):
    if self.unit == None:
      return ""
    return self.unit

  def variance(self):
    if self.count < 2:
      return float('NaN')
    return self.M2 / (self.count - 1)

  def stddev(self):
    return numpy.sqrt(self.variance())

  def size(self):
    return len(self.values)

  def widths(self):
    return FieldWidth(
        key=len(fmtS(self.key)),
        average=len(fmtN(self.average)),
        stddev=len(fmtN(self.stddev())),
        min=len(fmtN(self.min)),
        max=len(fmtN(self.max)))


rep_string = str(args['repetitions'])


class Measurements:

  def __init__(self):
    self.all = {}
    self.default_key = '[default]'
    self.max_widths = FieldWidth()

  def record(self, key, value, unit):
    if (key == None):
      key = self.default_key
    if key not in self.all:
      self.all[key] = Measurement(key, unit)
    self.all[key].addValue(value)
    self.max_widths.max_with(self.all[key].widths())

  def any(self):
    if len(self.all) >= 1:
      return next(iter(self.all.values()))
    else:
      return None

  def format_status(self):
    m = self.any()
    if m == None:
      return ""
    return m.status(self.max_widths)

  def format_num(self, m):
    return "[{0:>{1}}/{2}]".format(m.size(), len(rep_string), rep_string)

  def print_status(self):
    if len(self.all) == 0:
      print("No results found. Check format?")
      return
    print(self.format_num(self.any()), self.format_status(), sep=" ", end="")

  def print_results(self):
    for key in self.all:
      m = self.all[key]
      print(self.format_num(m), m.status(self.max_widths), sep=" ")


measurements = Measurements()


def signal_handler(signal, frame):
  print("", end="\r")
  measurements.print_status()
  print()
  measurements.print_results()
  sys.exit(0)


signal.signal(signal.SIGINT, signal_handler)

for x in range(0, args['repetitions']):
  proc = subprocess.Popen(args['command'], stdout=subprocess.PIPE)
  for line in proc.stdout:
    if args['echo']:
      print(line.decode(), end="")
    for m in re.finditer(
        r'\A((?P<key>.*[^\s\d:]+)[:]?)?\s*(?P<value>[0-9]+(.[0-9]+)?)\ ?(?P<unit>[^\d\W]\w*)?\s*\Z',
        line.decode()):
      measurements.record(m.group('key'), m.group('value'), m.group('unit'))
  proc.wait()
  if proc.returncode != 0:
    print("Child exited with status %d" % proc.returncode)
    break
  measurements.print_status()
  print("", end="\r")

measurements.print_results()
