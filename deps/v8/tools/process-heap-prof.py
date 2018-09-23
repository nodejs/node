#!/usr/bin/env python
#
# Copyright 2009 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This is an utility for converting V8 heap logs into .hp files that can
# be further processed using 'hp2ps' tool (bundled with GHC and Valgrind)
# to produce heap usage histograms.

# Sample usage:
# $ ./shell --log-gc script.js
# $ tools/process-heap-prof.py v8.log | hp2ps -c > script-heap-graph.ps
# ('-c' enables color, see hp2ps manual page for more options)
# or
# $ tools/process-heap-prof.py --js-cons-profile v8.log | hp2ps -c > script-heap-graph.ps
# to get JS constructor profile


import csv, sys, time, optparse

def ProcessLogFile(filename, options):
  if options.js_cons_profile:
    itemname = 'heap-js-cons-item'
  else:
    itemname = 'heap-sample-item'

  first_call_time = None
  sample_time = 0.0
  sampling = False
  try:
    logfile = open(filename, 'rb')
    try:
      logreader = csv.reader(logfile)

      print('JOB "v8"')
      print('DATE "%s"' % time.asctime(time.localtime()))
      print('SAMPLE_UNIT "seconds"')
      print('VALUE_UNIT "bytes"')

      for row in logreader:
        if row[0] == 'heap-sample-begin' and row[1] == 'Heap':
          sample_time = float(row[3])/1000.0
          if first_call_time == None:
            first_call_time = sample_time
          sample_time -= first_call_time
          print('BEGIN_SAMPLE %.2f' % sample_time)
          sampling = True
        elif row[0] == 'heap-sample-end' and row[1] == 'Heap':
          print('END_SAMPLE %.2f' % sample_time)
          sampling = False
        elif row[0] == itemname and sampling:
          print(row[1]),
          if options.count:
            print('%d' % (int(row[2]))),
          if options.size:
            print('%d' % (int(row[3]))),
          print
    finally:
      logfile.close()
  except:
    sys.exit('can\'t open %s' % filename)


def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("--js_cons_profile", help="Constructor profile",
      default=False, action="store_true")
  result.add_option("--size", help="Report object size",
      default=False, action="store_true")
  result.add_option("--count", help="Report object count",
      default=False, action="store_true")
  return result


def ProcessOptions(options):
  if not options.size and not options.count:
    options.size = True
  return True


def Main():
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options):
    parser.print_help()
    sys.exit();

  if not args:
    print "Missing logfile"
    sys.exit();

  ProcessLogFile(args[0], options)


if __name__ == '__main__':
  sys.exit(Main())
