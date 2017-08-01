#! /usr/bin/python2
#
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

import argparse
import collections
import re
import subprocess
import sys


__DESCRIPTION = """
Processes a perf.data sample file and reports the hottest Ignition bytecodes,
or write an input file for flamegraph.pl.
"""


__HELP_EPILOGUE = """
examples:
  # Get a flamegraph for Ignition bytecode handlers on Octane benchmark,
  # without considering the time spent compiling JS code, entry trampoline
  # samples and other non-Ignition samples.
  #
  $ tools/run-perf.sh out/x64.release/d8 \\
        --ignition --noturbo --noopt run.js
  $ tools/ignition/linux_perf_report.py --flamegraph -o out.collapsed
  $ flamegraph.pl --colors js out.collapsed > out.svg

  # Same as above, but show all samples, including time spent compiling JS code,
  # entry trampoline samples and other samples.
  $ # ...
  $ tools/ignition/linux_perf_report.py \\
      --flamegraph --show-all -o out.collapsed
  $ # ...

  # Same as above, but show full function signatures in the flamegraph.
  $ # ...
  $ tools/ignition/linux_perf_report.py \\
      --flamegraph --show-full-signatures -o out.collapsed
  $ # ...

  # See the hottest bytecodes on Octane benchmark, by number of samples.
  #
  $ tools/run-perf.sh out/x64.release/d8 \\
        --ignition --noturbo --noopt octane/run.js
  $ tools/ignition/linux_perf_report.py
"""


COMPILER_SYMBOLS_RE = re.compile(
  r"v8::internal::(?:\(anonymous namespace\)::)?Compile|v8::internal::Parser")
JIT_CODE_SYMBOLS_RE = re.compile(
  r"(LazyCompile|Compile|Eval|Script):(\*|~)")
GC_SYMBOLS_RE = re.compile(
  r"v8::internal::Heap::CollectGarbage")


def strip_function_parameters(symbol):
  if symbol[-1] != ')': return symbol
  pos = 1
  parenthesis_count = 0
  for c in reversed(symbol):
    if c == ')':
      parenthesis_count += 1
    elif c == '(':
      parenthesis_count -= 1
    if parenthesis_count == 0:
      break
    else:
      pos += 1
  return symbol[:-pos]


def collapsed_callchains_generator(perf_stream, hide_other=False,
                                   hide_compiler=False, hide_jit=False,
                                   hide_gc=False, show_full_signatures=False):
  current_chain = []
  skip_until_end_of_chain = False
  compiler_symbol_in_chain = False

  for line in perf_stream:
    # Lines starting with a "#" are comments, skip them.
    if line[0] == "#":
      continue

    line = line.strip()

    # Empty line signals the end of the callchain.
    if not line:
      if (not skip_until_end_of_chain and current_chain
          and not hide_other):
        current_chain.append("[other]")
        yield current_chain
      # Reset parser status.
      current_chain = []
      skip_until_end_of_chain = False
      compiler_symbol_in_chain = False
      continue

    if skip_until_end_of_chain:
      continue

    # Trim the leading address and the trailing +offset, if present.
    symbol = line.split(" ", 1)[1].split("+", 1)[0]
    if not show_full_signatures:
      symbol = strip_function_parameters(symbol)

    # Avoid chains of [unknown]
    if (symbol == "[unknown]" and current_chain and
        current_chain[-1] == "[unknown]"):
      continue

    current_chain.append(symbol)

    if symbol.startswith("BytecodeHandler:"):
      current_chain.append("[interpreter]")
      yield current_chain
      skip_until_end_of_chain = True
    elif JIT_CODE_SYMBOLS_RE.match(symbol):
      if not hide_jit:
        current_chain.append("[jit]")
        yield current_chain
        skip_until_end_of_chain = True
    elif GC_SYMBOLS_RE.match(symbol):
      if not hide_gc:
        current_chain.append("[gc]")
        yield current_chain
        skip_until_end_of_chain = True
    elif symbol == "Stub:CEntryStub" and compiler_symbol_in_chain:
      if not hide_compiler:
        current_chain.append("[compiler]")
        yield current_chain
      skip_until_end_of_chain = True
    elif COMPILER_SYMBOLS_RE.match(symbol):
      compiler_symbol_in_chain = True
    elif symbol == "Builtin:InterpreterEntryTrampoline":
      if len(current_chain) == 1:
        yield ["[entry trampoline]"]
      else:
        # If we see an InterpreterEntryTrampoline which is not at the top of the
        # chain and doesn't have a BytecodeHandler above it, then we have
        # skipped the top BytecodeHandler due to the top-level stub not building
        # a frame. File the chain in the [misattributed] bucket.
        current_chain[-1] = "[misattributed]"
        yield current_chain
      skip_until_end_of_chain = True


def calculate_samples_count_per_callchain(callchains):
  chain_counters = collections.defaultdict(int)
  for callchain in callchains:
    key = ";".join(reversed(callchain))
    chain_counters[key] += 1
  return chain_counters.items()


def calculate_samples_count_per_handler(callchains):
  def strip_handler_prefix_if_any(handler):
    return handler if handler[0] == "[" else handler.split(":", 1)[1]

  handler_counters = collections.defaultdict(int)
  for callchain in callchains:
    handler = strip_handler_prefix_if_any(callchain[-1])
    handler_counters[handler] += 1
  return handler_counters.items()


def write_flamegraph_input_file(output_stream, callchains):
  for callchain, count in calculate_samples_count_per_callchain(callchains):
    output_stream.write("{}; {}\n".format(callchain, count))


def write_handlers_report(output_stream, callchains):
  handler_counters = calculate_samples_count_per_handler(callchains)
  samples_num = sum(counter for _, counter in handler_counters)
  # Sort by decreasing number of samples
  handler_counters.sort(key=lambda entry: entry[1], reverse=True)
  for bytecode_name, count in handler_counters:
    output_stream.write(
      "{}\t{}\t{:.3f}%\n".format(bytecode_name, count,
                                 100. * count / samples_num))


def parse_command_line():
  command_line_parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description=__DESCRIPTION,
    epilog=__HELP_EPILOGUE)

  command_line_parser.add_argument(
    "perf_filename",
    help="perf sample file to process (default: perf.data)",
    nargs="?",
    default="perf.data",
    metavar="<perf filename>"
  )
  command_line_parser.add_argument(
    "--flamegraph", "-f",
    help="output an input file for flamegraph.pl, not a report",
    action="store_true",
    dest="output_flamegraph"
  )
  command_line_parser.add_argument(
    "--hide-other",
    help="Hide other samples",
    action="store_true"
  )
  command_line_parser.add_argument(
    "--hide-compiler",
    help="Hide samples during compilation",
    action="store_true"
  )
  command_line_parser.add_argument(
    "--hide-jit",
    help="Hide samples from JIT code execution",
    action="store_true"
  )
  command_line_parser.add_argument(
    "--hide-gc",
    help="Hide samples from garbage collection",
    action="store_true"
  )
  command_line_parser.add_argument(
    "--show-full-signatures", "-s",
    help="show full signatures instead of function names",
    action="store_true"
  )
  command_line_parser.add_argument(
    "--output", "-o",
    help="output file name (stdout if omitted)",
    type=argparse.FileType('wt'),
    default=sys.stdout,
    metavar="<output filename>",
    dest="output_stream"
  )

  return command_line_parser.parse_args()


def main():
  program_options = parse_command_line()

  perf = subprocess.Popen(["perf", "script", "--fields", "ip,sym",
                           "-i", program_options.perf_filename],
                          stdout=subprocess.PIPE)

  callchains = collapsed_callchains_generator(
    perf.stdout, program_options.hide_other, program_options.hide_compiler,
    program_options.hide_jit, program_options.hide_gc,
    program_options.show_full_signatures)

  if program_options.output_flamegraph:
    write_flamegraph_input_file(program_options.output_stream, callchains)
  else:
    write_handlers_report(program_options.output_stream, callchains)


if __name__ == "__main__":
  main()
