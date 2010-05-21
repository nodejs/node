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


from __future__ import with_statement
import sys, types, re, subprocess

def flatten(l):
  flat = []
  for i in l: flat.extend(i)
  return flat

def split_nvp(s):
  t = {}
  for m in re.finditer(r"(\w+)=(-?\d+)", s):
    t[m.group(1)] = int(m.group(2))
  return t

def parse_gc_trace(input):
  trace = []
  with open(input) as f:
    for line in f:
      info = split_nvp(line)
      if info and 'pause' in info and info['pause'] > 0:
        info['i'] = len(trace)
        trace.append(info)
  return trace

def extract_field_names(script):
  fields = { 'data': true, 'in': true }

  for m in re.finditer(r"$(\w+)", script):
    field_name = m.group(1)
    if field_name not in fields:
      fields[field] = field_count
      field_count = field_count + 1

  return fields

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
    if type(field) is types.ListType:
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
  if t is types.StringType:
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
    print 'Plotting %s...' % outfilename
    gnuplot(script)

  return charts

def reclaimed_bytes(row):
  return row['total_size_before'] - row['total_size_after']

plots = [
  [
    Set('style fill solid 0.5 noborder'),
    Set('style histogram rowstacked'),
    Set('style data histograms'),
    Plot(Item('Marking', 'mark', lc = 'purple'),
         Item('Sweep', 'sweep', lc = 'blue'),
         Item('Compaction', 'compact', lc = 'red'),
         Item('Other',
              lambda r: r['pause'] - r['mark'] - r['sweep'] - r['compact'],
              lc = 'grey'))
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

def process_trace(filename):
  trace = parse_gc_trace(filename)
  total_gc = reduce(lambda t,r: t + r['pause'], trace, 0)
  max_gc = reduce(lambda t,r: max(t, r['pause']), trace, 0)
  avg_gc = total_gc / len(trace)

  charts = plot_all(plots, trace, filename)

  with open(filename + '.html', 'w') as out:
    out.write('<html><body>')
    out.write('Total in GC: <b>%d</b><br/>' % total_gc)
    out.write('Max in GC: <b>%d</b><br/>' % max_gc)
    out.write('Avg in GC: <b>%d</b><br/>' % avg_gc)
    for chart in charts:
      out.write('<img src="%s">' % chart)
      out.write('</body></html>')

  print "%s generated." % (filename + '.html')

if len(sys.argv) != 2:
  print "Usage: %s <GC-trace-filename>" % sys.argv[0]
  sys.exit(1)

process_trace(sys.argv[1])
