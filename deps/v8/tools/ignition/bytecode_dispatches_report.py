#! /usr/bin/python
#
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import heapq
import json
from matplotlib import colors
from matplotlib import pyplot
import numpy
import struct
import sys


__DESCRIPTION = """
Process v8.ignition_dispatches_counters.json and list top counters,
or plot a dispatch heatmap.

Please note that those handlers that may not or will never dispatch
(e.g. Return or Throw) do not show up in the results.
"""


__HELP_EPILOGUE = """
examples:
  # Print the hottest bytecodes in descending order, reading from
  # default filename v8.ignition_dispatches_counters.json (default mode)
  $ tools/ignition/bytecode_dispatches_report.py

  # Print the hottest 15 bytecode dispatch pairs reading from data.json
  $ tools/ignition/bytecode_dispatches_report.py -t -n 15 data.json

  # Save heatmap to default filename v8.ignition_dispatches_counters.svg
  $ tools/ignition/bytecode_dispatches_report.py -p

  # Save heatmap to filename data.svg
  $ tools/ignition/bytecode_dispatches_report.py -p -o data.svg

  # Open the heatmap in an interactive viewer
  $ tools/ignition/bytecode_dispatches_report.py -p -i

  # Display the top 5 sources and destinations of dispatches to/from LdaZero
  $ tools/ignition/bytecode_dispatches_report.py -f LdaZero -n 5
"""

__COUNTER_BITS = struct.calcsize("P") * 8  # Size in bits of a pointer
__COUNTER_MAX = 2**__COUNTER_BITS - 1


def warn_if_counter_may_have_saturated(dispatches_table):
  for source, counters_from_source in iteritems(dispatches_table):
    for destination, counter in iteritems(counters_from_source):
      if counter == __COUNTER_MAX:
        print("WARNING: {} -> {} may have saturated.".format(source,
                                                             destination))


def find_top_bytecode_dispatch_pairs(dispatches_table, top_count):
  def flattened_counters_generator():
    for source, counters_from_source in iteritems(dispatches_table):
      for destination, counter in iteritems(counters_from_source):
        yield source, destination, counter

  return heapq.nlargest(top_count, flattened_counters_generator(),
                        key=lambda x: x[2])


def print_top_bytecode_dispatch_pairs(dispatches_table, top_count):
  top_bytecode_dispatch_pairs = (
    find_top_bytecode_dispatch_pairs(dispatches_table, top_count))
  print("Top {} bytecode dispatch pairs:".format(top_count))
  for source, destination, counter in top_bytecode_dispatch_pairs:
    print("{:>12d}\t{} -> {}".format(counter, source, destination))


def find_top_bytecodes(dispatches_table):
  top_bytecodes = []
  for bytecode, counters_from_bytecode in iteritems(dispatches_table):
    top_bytecodes.append((bytecode, sum(itervalues(counters_from_bytecode))))

  top_bytecodes.sort(key=lambda x: x[1], reverse=True)
  return top_bytecodes


def print_top_bytecodes(dispatches_table):
  top_bytecodes = find_top_bytecodes(dispatches_table)
  print("Top bytecodes:")
  for bytecode, counter in top_bytecodes:
    print("{:>12d}\t{}".format(counter, bytecode))


def find_top_dispatch_sources_and_destinations(
    dispatches_table, bytecode, top_count, sort_source_relative):
  sources = []
  for source, destinations in iteritems(dispatches_table):
    total = float(sum(itervalues(destinations)))
    if bytecode in destinations:
      count = destinations[bytecode]
      sources.append((source, count, count / total))

  destinations = []
  bytecode_destinations = dispatches_table[bytecode]
  bytecode_total = float(sum(itervalues(bytecode_destinations)))
  for destination, count in iteritems(bytecode_destinations):
    destinations.append((destination, count, count / bytecode_total))

  return (heapq.nlargest(top_count, sources,
                         key=lambda x: x[2 if sort_source_relative else 1]),
          heapq.nlargest(top_count, destinations, key=lambda x: x[1]))


def print_top_dispatch_sources_and_destinations(dispatches_table, bytecode,
                                                top_count, sort_relative):
  top_sources, top_destinations = find_top_dispatch_sources_and_destinations(
      dispatches_table, bytecode, top_count, sort_relative)
  print("Top sources of dispatches to {}:".format(bytecode))
  for source_name, counter, ratio in top_sources:
    print("{:>12d}\t{:>5.1f}%\t{}".format(counter, ratio * 100, source_name))

  print("\nTop destinations of dispatches from {}:".format(bytecode))
  for destination_name, counter, ratio in top_destinations:
    print("{:>12d}\t{:>5.1f}%\t{}".format(counter, ratio * 100, destination_name))


def build_counters_matrix(dispatches_table):
  labels = sorted(dispatches_table.keys())

  counters_matrix = numpy.empty([len(labels), len(labels)], dtype=int)
  for from_index, from_name in enumerate(labels):
    current_row = dispatches_table[from_name];
    for to_index, to_name in enumerate(labels):
      counters_matrix[from_index, to_index] = current_row.get(to_name, 0)

  # Reverse y axis for a nicer appearance
  xlabels = labels
  ylabels = list(reversed(xlabels))
  counters_matrix = numpy.flipud(counters_matrix)

  return counters_matrix, xlabels, ylabels


def plot_dispatches_table(dispatches_table, figure, axis):
  counters_matrix, xlabels, ylabels = build_counters_matrix(dispatches_table)

  image = axis.pcolor(
    counters_matrix,
    cmap="jet",
    norm=colors.LogNorm(),
    edgecolor="grey",
    linestyle="dotted",
    linewidth=0.5
  )

  axis.xaxis.set(
    ticks=numpy.arange(0.5, len(xlabels)),
    label="From bytecode handler"
  )
  axis.xaxis.tick_top()
  axis.set_xlim(0, len(xlabels))
  axis.set_xticklabels(xlabels, rotation="vertical")

  axis.yaxis.set(
    ticks=numpy.arange(0.5, len(ylabels)),
    label="To bytecode handler",
    ticklabels=ylabels
  )
  axis.set_ylim(0, len(ylabels))

  figure.colorbar(
    image,
    ax=axis,
    fraction=0.01,
    pad=0.01
  )


def parse_command_line():
  command_line_parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=__DESCRIPTION,
    epilog=__HELP_EPILOGUE
  )
  command_line_parser.add_argument(
    "--plot-size", "-s",
    metavar="N",
    default=30,
    help="shorter side in inches of the output plot (default 30)"
  )
  command_line_parser.add_argument(
    "--plot", "-p",
    action="store_true",
    help="plot dispatch pairs heatmap"
  )
  command_line_parser.add_argument(
    "--interactive", "-i",
    action="store_true",
    help="open the heatmap in an interactive viewer, instead of writing to file"
  )
  command_line_parser.add_argument(
    "--top-bytecode-dispatch-pairs", "-t",
    action="store_true",
    help="print the top bytecode dispatch pairs"
  )
  command_line_parser.add_argument(
    "--top-entries-count", "-n",
    metavar="N",
    type=int,
    default=10,
    help="print N top entries when running with -t or -f (default 10)"
  )
  command_line_parser.add_argument(
    "--top-dispatches-for-bytecode", "-f",
    metavar="<bytecode name>",
    help="print top dispatch sources and destinations to the specified bytecode"
  )
  command_line_parser.add_argument(
    "--output-filename", "-o",
    metavar="<output filename>",
    default="v8.ignition_dispatches_table.svg",
    help=("file to save the plot file to. File type is deduced from the "
          "extension. PDF, SVG, PNG supported")
  )
  command_line_parser.add_argument(
    "--sort-sources-relative", "-r",
    action="store_true",
    help=("print top sources in order to how often they dispatch to the "
          "specified bytecode, only applied when using -f")
  )
  command_line_parser.add_argument(
    "input_filename",
    metavar="<input filename>",
    default="v8.ignition_dispatches_table.json",
    nargs='?',
    help="Ignition counters JSON file"
  )

  return command_line_parser.parse_args()


def itervalues(d):
  return d.values() if sys.version_info[0] > 2 else d.itervalues()


def iteritems(d):
  return d.items() if sys.version_info[0] > 2 else d.iteritems()


def main():
  program_options = parse_command_line()

  with open(program_options.input_filename) as stream:
    dispatches_table = json.load(stream)

  warn_if_counter_may_have_saturated(dispatches_table)

  if program_options.plot:
    figure, axis = pyplot.subplots()
    plot_dispatches_table(dispatches_table, figure, axis)

    if program_options.interactive:
      pyplot.show()
    else:
      figure.set_size_inches(program_options.plot_size,
                             program_options.plot_size)
      pyplot.savefig(program_options.output_filename)
  elif program_options.top_bytecode_dispatch_pairs:
    print_top_bytecode_dispatch_pairs(
      dispatches_table, program_options.top_entries_count)
  elif program_options.top_dispatches_for_bytecode:
    print_top_dispatch_sources_and_destinations(
      dispatches_table, program_options.top_dispatches_for_bytecode,
      program_options.top_entries_count, program_options.sort_sources_relative)
  else:
    print_top_bytecodes(dispatches_table)


if __name__ == "__main__":
  main()
