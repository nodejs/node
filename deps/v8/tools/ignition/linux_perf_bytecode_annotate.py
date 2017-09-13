#! /usr/bin/python2
#
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

import argparse
import collections
import os
import subprocess
import sys


__DESCRIPTION = """
Processes a perf.data sample file and annotates the hottest instructions in a
given bytecode handler.
"""


__HELP_EPILOGUE = """
Note:
  This tool uses the disassembly of interpreter's bytecode handler codegen
  from out/<arch>.debug/d8. you should ensure that this binary is in-sync with
  the version used to generate the perf profile.

  Also, the tool depends on the symbol offsets from perf samples being accurate.
  As such, you should use the ":pp" suffix for events.

Examples:
  EVENT_TYPE=cycles:pp tools/run-perf.sh out/x64.release/d8
  tools/ignition/linux_perf_bytecode_annotate.py Add
"""


def bytecode_offset_generator(perf_stream, bytecode_name):
  skip_until_end_of_chain = False
  bytecode_symbol = "BytecodeHandler:" + bytecode_name;

  for line in perf_stream:
    # Lines starting with a "#" are comments, skip them.
    if line[0] == "#":
      continue
    line = line.strip()

    # Empty line signals the end of the callchain.
    if not line:
      skip_until_end_of_chain = False
      continue

    if skip_until_end_of_chain:
      continue

    symbol_and_offset = line.split(" ", 1)[1]

    if symbol_and_offset.startswith("BytecodeHandler:"):
      skip_until_end_of_chain = True

      if symbol_and_offset.startswith(bytecode_symbol):
        yield int(symbol_and_offset.split("+", 1)[1], 16)


def bytecode_offset_counts(bytecode_offsets):
  offset_counts = collections.defaultdict(int)
  for offset in bytecode_offsets:
    offset_counts[offset] += 1
  return offset_counts


def bytecode_disassembly_generator(ignition_codegen, bytecode_name):
  name_string = "name = " + bytecode_name
  for line in ignition_codegen:
    if line.startswith(name_string):
      break

  # Found the bytecode disassembly.
  for line in ignition_codegen:
    line = line.strip()
    # Blank line marks the end of the bytecode's disassembly.
    if not line:
      return

    # Only yield disassembly output.
    if not line.startswith("0x"):
      continue

    yield line


def print_disassembly_annotation(offset_counts, bytecode_disassembly):
  total = sum(offset_counts.values())
  offsets = sorted(offset_counts, reverse=True)
  def next_offset():
    return offsets.pop() if offsets else -1

  current_offset = next_offset()
  print current_offset;

  for line in bytecode_disassembly:
    disassembly_offset = int(line.split()[1])
    if disassembly_offset == current_offset:
      count = offset_counts[current_offset]
      percentage = 100.0 * count / total
      print "{:>8d} ({:>5.1f}%) ".format(count, percentage),
      current_offset = next_offset()
    else:
      print "                ",
    print line

  if offsets:
    print ("WARNING: Offsets not empty. Output is most likely invalid due to "
           "a mismatch between perf output and debug d8 binary.")


def parse_command_line():
  command_line_parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description=__DESCRIPTION,
      epilog=__HELP_EPILOGUE)

  command_line_parser.add_argument(
      "--arch", "-a",
      help="The architecture (default: x64)",
      default="x64",
  )
  command_line_parser.add_argument(
      "--input", "-i",
      help="perf sample file to process (default: perf.data)",
      default="perf.data",
      metavar="<perf filename>",
      dest="perf_filename"
  )
  command_line_parser.add_argument(
      "--output", "-o",
      help="output file name (stdout if omitted)",
      type=argparse.FileType("wt"),
      default=sys.stdout,
      metavar="<output filename>",
      dest="output_stream"
  )
  command_line_parser.add_argument(
      "bytecode_name",
      metavar="<bytecode name>",
      nargs="?",
      help="The bytecode handler to annotate"
  )

  return command_line_parser.parse_args()


def main():
  program_options = parse_command_line()
  perf = subprocess.Popen(["perf", "script", "-f", "ip,sym,symoff",
                           "-i", program_options.perf_filename],
                          stdout=subprocess.PIPE)

  v8_root_path = os.path.dirname(__file__) + "/../../"
  d8_path = "{}/out/{}.debug/d8".format(v8_root_path, program_options.arch)
  d8_codegen = subprocess.Popen([d8_path, "--trace-ignition-codegen",
                                 "-e", "1"],
                                stdout=subprocess.PIPE)

  bytecode_offsets = bytecode_offset_generator(
      perf.stdout, program_options.bytecode_name)
  offset_counts = bytecode_offset_counts(bytecode_offsets)

  bytecode_disassembly = bytecode_disassembly_generator(
      d8_codegen.stdout, program_options.bytecode_name)

  print_disassembly_annotation(offset_counts, bytecode_disassembly)


if __name__ == "__main__":
  main()
