#!/usr/bin/env python

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

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import math
import re
import signal
import subprocess
import sys

PARSER = argparse.ArgumentParser(
    description="A script that averages numbers from another script's output",
    epilog="Example:\n\tavg.py 10 bash -c \"echo A: 100; echo B 120; sleep .1\""
)
PARSER.add_argument(
    'repetitions',
    type=int,
    help="number of times the command should be repeated")
PARSER.add_argument(
    'command',
    nargs=argparse.REMAINDER,
    help="command to run (no quotes needed)")
PARSER.add_argument(
    '--echo',
    '-e',
    action='store_true',
    default=False,
    help="set this flag to echo the command's output")

ARGS = vars(PARSER.parse_args())

if not ARGS['command']:
  print("No command provided.")
  exit(1)


class FieldWidth:

  def __init__(self, points=0, key=0, average=0, stddev=0, min_width=0, max_width=0):
    self.widths = dict(points=points, key=key, average=average, stddev=stddev,
                       min=min_width, max=max_width)

  def max_widths(self, other):
    self.widths = {k: max(v, other.widths[k]) for k, v in self.widths.items()}

  def __getattr__(self, key):
    return self.widths[key]


def fmtS(string, width=0):
  return "{0:<{1}}".format(string, width)


def fmtN(num, width=0):
  return "{0:>{1},.2f}".format(num, width)


def fmt(num):
  return "{0:>,.2f}".format(num)


def format_line(points, key, average, stddev, min_value, max_value,
                unit_string, widths):
  return "{:>{}};  {:<{}};  {:>{}};  {:>{}};  {:>{}};  {:>{}};  {}".format(
      points, widths.points,
      key, widths.key,
      average, widths.average,
      stddev, widths.stddev,
      min_value, widths.min,
      max_value, widths.max,
      unit_string)


def fmt_reps(msrmnt):
  rep_string = str(ARGS['repetitions'])
  return "[{0:>{1}}/{2}]".format(msrmnt.size(), len(rep_string), rep_string)


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

  def status(self, widths):
    return "{} {}: avg {} stddev {} ({} - {}) {}".format(
        fmt_reps(self),
        fmtS(self.key, widths.key), fmtN(self.average, widths.average),
        fmtN(self.stddev(), widths.stddev), fmtN(self.min, widths.min),
        fmtN(self.max, widths.max), fmtS(self.unit_string()))

  def result(self, widths):
    return format_line(self.size(), self.key, fmt(self.average),
                       fmt(self.stddev()), fmt(self.min),
                       fmt(self.max), self.unit_string(),
                       widths)

  def unit_string(self):
    if not self.unit:
      return ""
    return self.unit

  def variance(self):
    if self.count < 2:
      return float('NaN')
    return self.M2 / (self.count - 1)

  def stddev(self):
    return math.sqrt(self.variance())

  def size(self):
    return len(self.values)

  def widths(self):
    return FieldWidth(
        points=len("{}".format(self.size())) + 2,
        key=len(self.key),
        average=len(fmt(self.average)),
        stddev=len(fmt(self.stddev())),
        min_width=len(fmt(self.min)),
        max_width=len(fmt(self.max)))


def result_header(widths):
  return format_line("#/{}".format(ARGS['repetitions']),
                     "id", "avg", "stddev", "min", "max", "unit", widths)


class Measurements:

  def __init__(self):
    self.all = {}
    self.default_key = '[default]'
    self.max_widths = FieldWidth(
        points=len("{}".format(ARGS['repetitions'])) + 2,
        key=len("id"),
        average=len("avg"),
        stddev=len("stddev"),
        min_width=len("min"),
        max_width=len("max"))
    self.last_status_len = 0

  def record(self, key, value, unit):
    if not key:
      key = self.default_key
    if key not in self.all:
      self.all[key] = Measurement(key, unit)
    self.all[key].addValue(value)
    self.max_widths.max_widths(self.all[key].widths())

  def any(self):
    if self.all:
      return next(iter(self.all.values()))
    return None

  def print_results(self):
    print("{:<{}}".format("", self.last_status_len), end="\r")
    print(result_header(self.max_widths), sep=" ")
    for key in sorted(self.all):
      print(self.all[key].result(self.max_widths), sep=" ")

  def print_status(self):
    status = "No results found. Check format?"
    measurement = MEASUREMENTS.any()
    if measurement:
      status = measurement.status(MEASUREMENTS.max_widths)
    print("{:<{}}".format(status, self.last_status_len), end="\r")
    self.last_status_len = len(status)


MEASUREMENTS = Measurements()


def signal_handler(signum, frame):
  print("", end="\r")
  MEASUREMENTS.print_results()
  sys.exit(0)


signal.signal(signal.SIGINT, signal_handler)

SCORE_REGEX = (r'\A((console.timeEnd: )?'
               r'(?P<key>[^\s:,]+)[,:]?)?'
               r'(^\s*|\s+)'
               r'(?P<value>[0-9]+(.[0-9]+)?)'
               r'\ ?(?P<unit>[^\d\W]\w*)?[.\s]*\Z')

for x in range(0, ARGS['repetitions']):
  proc = subprocess.Popen(ARGS['command'], stdout=subprocess.PIPE)
  for line in proc.stdout:
    if ARGS['echo']:
      print(line.decode(), end="")
    for m in re.finditer(SCORE_REGEX, line.decode()):
      MEASUREMENTS.record(m.group('key'), m.group('value'), m.group('unit'))
  proc.wait()
  if proc.returncode != 0:
    print("Child exited with status %d" % proc.returncode)
    break

  MEASUREMENTS.print_status()

# Print final results
MEASUREMENTS.print_results()
