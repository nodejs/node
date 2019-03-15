#!/usr/bin/env python
#
# Copyright 2010 the V8 project authors. All rights reserved.
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
#

#
# This is an utility for plotting charts based on GC traces produced by V8 when
# run with flags --trace-gc --trace-gc-nvp. Relies on gnuplot for actual
# plotting.
#
# Usage: gc-nvp-trace-processor.py <GC-trace-filename>
#


# for py2/py3 compatibility
from __future__ import with_statement
from __future__ import print_function
from functools import reduce

import sys, types, subprocess, math
import gc_nvp_common


try:
  long        # Python 2
except NameError:
  long = int  # Python 3


def flatten(l):
  flat = []
  for i in l: flat.extend(i)
  return flat

def gnuplot(script):
  gnuplot = subprocess.Popen(["gnuplot"], stdin=subprocess.PIPE)
  gnuplot.stdin.write(script)
  gnuplot.stdin.close()
  gnuplot.wait()

x1y1 = 'x1y1'
x1y2 = 'x1y2'
x2y1 = 'x2y1'
x2y2 = 'x2y2'

class Item(object):
  def __init__(self, title, field, axis = x1y1, **keywords):
    self.title = title
    self.axis = axis
    self.props = keywords
    if type(field) is list:
      self.field = field
    else:
      self.field = [field]

  def fieldrefs(self):
    return self.field

  def to_gnuplot(self, context):
    args = ['"%s"' % context.datafile,
            'using %s' % context.format_fieldref(self.field),
            'title "%s"' % self.title,
            'axis %s' % self.axis]
    if 'style' in self.props:
      args.append('with %s' % self.props['style'])
    if 'lc' in self.props:
      args.append('lc rgb "%s"' % self.props['lc'])
    if 'fs' in self.props:
      args.append('fs %s' % self.props['fs'])
    return ' '.join(args)

class Plot(object):
  def __init__(self, *items):
    self.items = items

  def fieldrefs(self):
    return flatten([item.fieldrefs() for item in self.items])

  def to_gnuplot(self, ctx):
    return 'plot ' + ', '.join([item.to_gnuplot(ctx) for item in self.items])

class Set(object):
  def __init__(self, value):
    self.value = value

  def to_gnuplot(self, ctx):
    return 'set ' + self.value

  def fieldrefs(self):
    return []

class Context(object):
  def __init__(self, datafile, field_to_index):
    self.datafile = datafile
    self.field_to_index = field_to_index

  def format_fieldref(self, fieldref):
    return ':'.join([str(self.field_to_index[field]) for field in fieldref])

def collect_fields(plot):
  field_to_index = {}
  fields = []

  def add_field(field):
    if field not in field_to_index:
      fields.append(field)
      field_to_index[field] = len(fields)

  for field in flatten([item.fieldrefs() for item in plot]):
    add_field(field)

  return (fields, field_to_index)

def is_y2_used(plot):
  for subplot in plot:
    if isinstance(subplot, Plot):
      for item in subplot.items:
        if item.axis == x1y2 or item.axis == x2y2:
          return True
  return False

def get_field(trace_line, field):
  t = type(field)
  if t is bytes:
    return trace_line[field]
  elif t is types.FunctionType:
    return field(trace_line)

def generate_datafile(datafile_name, trace, fields):
  with open(datafile_name, 'w') as datafile:
    for line in trace:
      data_line = [str(get_field(line, field)) for field in fields]
      datafile.write('\t'.join(data_line))
      datafile.write('\n')

def generate_script_and_datafile(plot, trace, datafile, output):
  (fields, field_to_index) = collect_fields(plot)
  generate_datafile(datafile, trace, fields)
  script = [
      'set terminal png',
      'set output "%s"' % output,
      'set autoscale',
      'set ytics nomirror',
      'set xtics nomirror',
      'set key below'
  ]

  if is_y2_used(plot):
    script.append('set autoscale y2')
    script.append('set y2tics')

  context = Context(datafile, field_to_index)

  for item in plot:
    script.append(item.to_gnuplot(context))

  return '\n'.join(script)

def plot_all(plots, trace, prefix):
  charts = []

  for plot in plots:
    outfilename = "%s_%d.png" % (prefix, len(charts))
    charts.append(outfilename)
    script = generate_script_and_datafile(plot, trace, '~datafile', outfilename)
    print('Plotting %s...' % outfilename)
    gnuplot(script)

  return charts

def reclaimed_bytes(row):
  return row['total_size_before'] - row['total_size_after']

def other_scope(r):
  if r['gc'] == 's':
    # there is no 'other' scope for scavenging collections.
    return 0
  return r['pause'] - r['mark'] - r['sweep'] - r['external']

def scavenge_scope(r):
  if r['gc'] == 's':
    return r['pause'] - r['external']
  return 0


def real_mutator(r):
  return r['mutator'] - r['steps_took']

plots = [
  [
    Set('style fill solid 0.5 noborder'),
    Set('style histogram rowstacked'),
    Set('style data histograms'),
    Plot(Item('Scavenge', scavenge_scope, lc = 'green'),
         Item('Marking', 'mark', lc = 'purple'),
         Item('Sweep', 'sweep', lc = 'blue'),
         Item('External', 'external', lc = '#489D43'),
         Item('Other', other_scope, lc = 'grey'),
         Item('IGC Steps', 'steps_took', lc = '#FF6347'))
  ],
  [
    Set('style fill solid 0.5 noborder'),
    Set('style histogram rowstacked'),
    Set('style data histograms'),
    Plot(Item('Scavenge', scavenge_scope, lc = 'green'),
         Item('Marking', 'mark', lc = 'purple'),
         Item('Sweep', 'sweep', lc = 'blue'),
         Item('External', 'external', lc = '#489D43'),
         Item('Other', other_scope, lc = '#ADD8E6'),
         Item('External', 'external', lc = '#D3D3D3'))
  ],

  [
    Plot(Item('Mutator', real_mutator, lc = 'black', style = 'lines'))
  ],
  [
    Set('style histogram rowstacked'),
    Set('style data histograms'),
    Plot(Item('Heap Size (before GC)', 'total_size_before', x1y2,
              fs = 'solid 0.4 noborder',
              lc = 'green'),
         Item('Total holes (after GC)', 'holes_size_before', x1y2,
              fs = 'solid 0.4 noborder',
              lc = 'red'),
         Item('GC Time', ['i', 'pause'], style = 'lines', lc = 'red'))
  ],
  [
    Set('style histogram rowstacked'),
    Set('style data histograms'),
    Plot(Item('Heap Size (after GC)', 'total_size_after', x1y2,
              fs = 'solid 0.4 noborder',
              lc = 'green'),
         Item('Total holes (after GC)', 'holes_size_after', x1y2,
              fs = 'solid 0.4 noborder',
              lc = 'red'),
         Item('GC Time', ['i', 'pause'],
              style = 'lines',
              lc = 'red'))
  ],
  [
    Set('style fill solid 0.5 noborder'),
    Set('style data histograms'),
    Plot(Item('Allocated', 'allocated'),
         Item('Reclaimed', reclaimed_bytes),
         Item('Promoted', 'promoted', style = 'lines', lc = 'black'))
  ],
]

def freduce(f, field, trace, init):
  return reduce(lambda t,r: f(t, r[field]), trace, init)

def calc_total(trace, field):
  return freduce(lambda t,v: t + long(v), field, trace, long(0))

def calc_max(trace, field):
  return freduce(lambda t,r: max(t, r), field, trace, 0)

def count_nonzero(trace, field):
  return freduce(lambda t,r: t if r == 0 else t + 1, field, trace, 0)


def process_trace(filename):
  trace = gc_nvp_common.parse_gc_trace(filename)

  marksweeps = filter(lambda r: r['gc'] == 'ms', trace)
  scavenges = filter(lambda r: r['gc'] == 's', trace)
  globalgcs = filter(lambda r: r['gc'] != 's', trace)


  charts = plot_all(plots, trace, filename)

  def stats(out, prefix, trace, field):
    n = len(trace)
    total = calc_total(trace, field)
    max = calc_max(trace, field)
    if n > 0:
      avg = total / n
    else:
      avg = 0
    if n > 1:
      dev = math.sqrt(freduce(lambda t,r: t + (r - avg) ** 2, field, trace, 0) /
                      (n - 1))
    else:
      dev = 0

    out.write('<tr><td>%s</td><td>%d</td><td>%d</td>'
              '<td>%d</td><td>%d [dev %f]</td></tr>' %
              (prefix, n, total, max, avg, dev))

  def HumanReadable(size):
    suffixes = ['bytes', 'kB', 'MB', 'GB']
    power = 1
    for i in range(len(suffixes)):
      if size < power*1024:
        return "%.1f" % (float(size) / power) + " " + suffixes[i]
      power *= 1024

  def throughput(name, trace):
    total_live_after = calc_total(trace, 'total_size_after')
    total_live_before = calc_total(trace, 'total_size_before')
    total_gc = calc_total(trace, 'pause')
    if total_gc == 0:
      return
    out.write('GC %s Throughput (after): %s / %s ms = %s/ms<br/>' %
              (name,
               HumanReadable(total_live_after),
               total_gc,
               HumanReadable(total_live_after / total_gc)))
    out.write('GC %s Throughput (before): %s / %s ms = %s/ms<br/>' %
              (name,
               HumanReadable(total_live_before),
               total_gc,
               HumanReadable(total_live_before / total_gc)))


  with open(filename + '.html', 'w') as out:
    out.write('<html><body>')
    out.write('<table>')
    out.write('<tr><td>Phase</td><td>Count</td><td>Time (ms)</td>')
    out.write('<td>Max</td><td>Avg</td></tr>')
    stats(out, 'Total in GC', trace, 'pause')
    stats(out, 'Scavenge', scavenges, 'pause')
    stats(out, 'MarkSweep', marksweeps, 'pause')
    stats(out, 'Mark', filter(lambda r: r['mark'] != 0, trace), 'mark')
    stats(out, 'Sweep', filter(lambda r: r['sweep'] != 0, trace), 'sweep')
    stats(out,
          'External',
          filter(lambda r: r['external'] != 0, trace),
          'external')
    out.write('</table>')
    throughput('TOTAL', trace)
    throughput('MS', marksweeps)
    throughput('OLDSPACE', globalgcs)
    out.write('<br/>')
    for chart in charts:
      out.write('<img src="%s">' % chart)
      out.write('</body></html>')

  print("%s generated." % (filename + '.html'))

if len(sys.argv) != 2:
  print("Usage: %s <GC-trace-filename>" % sys.argv[0])
  sys.exit(1)

process_trace(sys.argv[1])
