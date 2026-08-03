#!/usr/bin/env python3
#
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to analyze GCTracer's NVP output."""


# for py2/py3 compatibility
from __future__ import print_function


from argparse import ArgumentParser
from copy import deepcopy
from gc_nvp_common import split_nvp
from math import ceil, log
from sys import stdin


class LinearBucket:
  def __init__(self, granularity):
    self.granularity = granularity

  def value_to_bucket(self, value):
    return int(value / self.granularity)

  def bucket_to_range(self, bucket):
    return (bucket * self.granularity, (bucket + 1) * self.granularity)


class Log2Bucket:
  def __init__(self, start):
    self.start = int(log(start, 2)) - 1

  def value_to_bucket(self, value):
    index = int(log(value, 2))
    index -= self.start
    if index < 0:
      index = 0
    return index

  def bucket_to_range(self, bucket):
    if bucket == 0:
      return (0, 2 ** (self.start + 1))
    bucket += self.start
    return (2 ** bucket, 2 ** (bucket + 1))


class Histogram:
  def __init__(self, bucket_trait, fill_empty):
    self.histogram = {}
    self.fill_empty = fill_empty
    self.bucket_trait = bucket_trait

  def add(self, key):
    index = self.bucket_trait.value_to_bucket(key)
    if index not in self.histogram:
      self.histogram[index] = 0
    self.histogram[index] += 1

  def __str__(self):
    ret = []
    keys = self.histogram.keys()
    keys.sort()
    last = keys[len(keys) - 1]
    for i in range(0, last + 1):
      (min_value, max_value) = self.bucket_trait.bucket_to_range(i)
      if i == keys[0]:
        keys.pop(0)
        ret.append("  [{0},{1}[: {2}".format(
          str(min_value), str(max_value), self.histogram[i]))
      else:
        if self.fill_empty:
          ret.append("  [{0},{1}[: {2}".format(
            str(min_value), str(max_value), 0))
    return "\n".join(ret)


class Category:
  def __init__(self, key, histogram, csv, percentiles):
    self.key = key
    self.values = []
    self.histogram = histogram
    self.csv = csv
    self.percentiles = percentiles

  def process_entry(self, entry):
    if self.key in entry:
      self.values.append(float(entry[self.key]))
      if self.histogram:
        self.histogram.add(float(entry[self.key]))

  def min(self):
    return min(self.values)

  def max(self):
    return max(self.values)

  def avg(self):
    if len(self.values) == 0:
      return 0.0
    return sum(self.values) / len(self.values)

  def empty(self):
    return len(self.values) == 0

  def _compute_percentiles(self):
    ret = []
    if len(self.values) == 0:
      return ret
    sorted_values = sorted(self.values)
    for percentile in self.percentiles:
      index = int(ceil((len(self.values) - 1) * percentile / 100))
      ret.append("  {0}%: {1}".format(percentile, sorted_values[index]))
    return ret

  def __str__(self):
    if self.csv:
      ret = [self.key]
      ret.append(len(self.values))
      ret.append(self.min())
      ret.append(self.max())
      ret.append(self.avg())
      ret = [str(x) for x in ret]
      return ",".join(ret)
    else:
      ret = [self.key]
      ret.append("  len: {0}".format(len(self.values)))
      if len(self.values) > 0:
        ret.append("  min: {0}".format(self.min()))
        ret.append("  max: {0}".format(self.max()))
        ret.append("  avg: {0}".format(self.avg()))
        if self.histogram:
          ret.append(str(self.histogram))
        if self.percentiles:
          ret.append("\n".join(self._compute_percentiles()))
      return "\n".join(ret)

  def __repr__(self):
    return "<Category: {0}>".format(self.key)


def make_key_func(cmp_metric):
  def key_func(a):
    return getattr(a, cmp_metric)()
  return key_func


def main():
  parser = ArgumentParser(description="Process GCTracer's NVP output")
  parser.add_argument('keys', metavar='KEY', type=str, nargs='+',
                      help='the keys of NVPs to process')
  parser.add_argument('--histogram-type', metavar='<linear|log2>',
                      type=str, nargs='?', default="linear",
                      help='histogram type to use (default: linear)')
  linear_group = parser.add_argument_group('linear histogram specific')
  linear_group.add_argument('--linear-histogram-granularity',
                            metavar='GRANULARITY', type=int, nargs='?',
                            default=5,
                            help='histogram granularity (default: 5)')
  log2_group = parser.add_argument_group('log2 histogram specific')
  log2_group.add_argument('--log2-histogram-init-bucket', metavar='START',
                          type=int, nargs='?', default=64,
                          help='initial buck size (default: 64)')
  parser.add_argument('--histogram-omit-empty-buckets',
                      dest='histogram_omit_empty',
                      action='store_true',
                      help='omit empty histogram buckets')
  parser.add_argument('--no-histogram', dest='histogram',
                      action='store_false', help='do not print histogram')
  parser.set_defaults(histogram=True)
  parser.set_defaults(histogram_omit_empty=False)
  parser.add_argument('--rank', metavar='<no|min|max|avg>',
                      type=str, nargs='?',
                      default="no",
                      help="rank keys by metric (default: no)")
  parser.add_argument('--csv', dest='csv',
                      action='store_true', help='provide output as csv')
  parser.add_argument('--percentiles', dest='percentiles',
                      type=str, default="",
                      help='comma separated list of percentiles')
  args = parser.parse_args()

  histogram = None
  if args.histogram:
    bucket_trait = None
    if args.histogram_type == "log2":
      bucket_trait = Log2Bucket(args.log2_histogram_init_bucket)
    else:
      bucket_trait = LinearBucket(args.linear_histogram_granularity)
    histogram = Histogram(bucket_trait, not args.histogram_omit_empty)

  percentiles = []
  for percentile in args.percentiles.split(','):
    try:
      percentiles.append(float(percentile))
    except ValueError:
      pass

  categories = [ Category(key, deepcopy(histogram), args.csv, percentiles)
                 for key in args.keys ]

  while True:
    line = stdin.readline()
    if not line:
      break
    obj = split_nvp(line)
    for category in categories:
      category.process_entry(obj)

  # Filter out empty categories.
  categories = [x for x in categories if not x.empty()]

  if args.rank != "no":
    categories = sorted(categories, key=make_key_func(args.rank), reverse=True)

  for category in categories:
    print(category)


if __name__ == '__main__':
  main()
