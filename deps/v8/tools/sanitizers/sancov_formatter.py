#!/usr/bin/env python3
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to transform and merge sancov files into human readable json-format.

The script supports three actions:
all: Writes a json file with all instrumented lines of all executables.
merge: Merges sancov files with coverage output into an existing json file.
split: Split json file into separate files per covered source file.

The json data is structured as follows:
{
  "version": 1,
  "tests": ["executable1", "executable2", ...],
  "files": {
    "file1": [[<instr line 1>, <bit_mask>], [<instr line 2>, <bit_mask>], ...],
    "file2": [...],
    ...
  }
}

The executables are sorted and determine the test bit mask. Their index+1 is
the bit, e.g. executable1 = 1, executable3 = 4, etc. Hence, a line covered by
executable1 and executable3 will have bit_mask == 5 == 0b101. The number of
tests is restricted to 52 in version 1, to allow javascript JSON parsing of
the bitsets encoded as numbers. JS max safe int is (1 << 53) - 1.

The line-number-bit_mask pairs are sorted by line number and don't contain
duplicates.

Split json data preserves the same format, but only contains one file per
json file.

The sancov tool is expected to be in the llvm compiler-rt third-party
directory. It's not checked out by default and must be added as a custom deps:
'v8/third_party/llvm/projects/compiler-rt':
    'https://chromium.googlesource.com/external/llvm.org/compiler-rt.git'
"""

# for py2/py3 compatibility
from __future__ import print_function
from functools import reduce

import argparse
import json
import logging
import os
import re
import subprocess
import sys

from multiprocessing import Pool, cpu_count


logging.basicConfig(level=logging.INFO)

# Files to exclude from coverage. Dropping their data early adds more speed.
# The contained cc files are already excluded from instrumentation, but inlined
# data is referenced through v8's object files.
EXCLUSIONS = [
  'buildtools',
  'src/third_party',
  'third_party',
  'test',
  'testing',
]

# Executables found in the build output for which no coverage is generated.
# Exclude them from the coverage data file.
EXE_EXCLUSIONS = [
  'generate-bytecode-expectations',
  'hello-world',
  'mksnapshot',
  'parser-shell',
  'process',
  'shell',
]

# V8 checkout directory.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))

# The sancov tool location.
SANCOV_TOOL = os.path.join(
    BASE_DIR, 'third_party', 'llvm', 'projects', 'compiler-rt',
    'lib', 'sanitizer_common', 'scripts', 'sancov.py')

# Simple script to sanitize the PCs from objdump.
SANITIZE_PCS = os.path.join(BASE_DIR, 'tools', 'sanitizers', 'sanitize_pcs.py')

# The llvm symbolizer location.
SYMBOLIZER = os.path.join(
    BASE_DIR, 'third_party', 'llvm-build', 'Release+Asserts', 'bin',
    'llvm-symbolizer')

# Number of cpus.
CPUS = cpu_count()

# Regexp to find sancov files as output by sancov_merger.py. Also grabs the
# executable name in group 1.
SANCOV_FILE_RE = re.compile(r'^(.*)\.result.sancov$')


def executables(build_dir):
  """Iterates over executable files in the build directory."""
  for f in os.listdir(build_dir):
    file_path = os.path.join(build_dir, f)
    if (os.path.isfile(file_path) and
        os.access(file_path, os.X_OK) and
        f not in EXE_EXCLUSIONS):
      yield file_path


def process_symbolizer_output(output, build_dir):
  """Post-process llvm symbolizer output.

  Excludes files outside the v8 checkout or given in exclusion list above
  from further processing. Drops the character index in each line.

  Returns: A mapping of file names to lists of line numbers. The file names
           have relative paths to the v8 base directory. The lists of line
           numbers don't contain duplicate lines and are sorted.
  """
  # Path prefix added by the llvm symbolizer including trailing slash.
  output_path_prefix = os.path.join(build_dir, '..', '..', '')

  # Drop path prefix when iterating lines. The path is redundant and takes
  # too much space. Drop files outside that path, e.g. generated files in
  # the build dir and absolute paths to c++ library headers.
  def iter_lines():
    for line in output.strip().splitlines():
      if line.startswith(output_path_prefix):
        yield line[len(output_path_prefix):]

  # Map file names to sets of instrumented line numbers.
  file_map = {}
  for line in iter_lines():
    # Drop character number, we only care for line numbers. Each line has the
    # form: <file name>:<line number>:<character number>.
    file_name, number, _ = line.split(':')
    file_map.setdefault(file_name, set([])).add(int(number))

  # Remove exclusion patterns from file map. It's cheaper to do it after the
  # mapping, as there are few excluded files and we don't want to do this
  # check for numerous lines in ordinary files.
  def keep(file_name):
    for e in EXCLUSIONS:
      if file_name.startswith(e):
        return False
    return True

  # Return in serializable form and filter.
  return {k: sorted(file_map[k]) for k in file_map if keep(k)}


def get_instrumented_lines(executable):
  """Return the instrumented lines of an executable.

  Called trough multiprocessing pool.

  Returns: Post-processed llvm output as returned by process_symbolizer_output.
  """
  # The first two pipes are from llvm's tool sancov.py with 0x added to the hex
  # numbers. The results are piped into the llvm symbolizer, which outputs for
  # each PC: <file name with abs path>:<line number>:<character number>.
  # We don't call the sancov tool to get more speed.
  process = subprocess.Popen(
      'objdump -d %s | '
      'grep \'^\s\+[0-9a-f]\+:.*\scall\(q\|\)\s\+[0-9a-f]\+ '
      '<__sanitizer_cov\(_with_check\|\|_trace_pc_guard\)\(@plt\|\)>\' | '
      'grep \'^\s\+[0-9a-f]\+\' -o | '
      '%s | '
      '%s --obj %s -functions=none' %
          (executable, SANITIZE_PCS, SYMBOLIZER, executable),
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.PIPE,
      cwd=BASE_DIR,
      shell=True,
  )
  output, _ = process.communicate()
  assert process.returncode == 0
  return process_symbolizer_output(output, os.path.dirname(executable))


def merge_instrumented_line_results(exe_list, results):
  """Merge multiprocessing results for all instrumented lines.

  Args:
    exe_list: List of all executable names with absolute paths.
    results: List of results as returned by get_instrumented_lines.

  Returns: Dict to be used as json data as specified on the top of this page.
           The dictionary contains all instrumented lines of all files
           referenced by all executables.
  """
  def merge_files(x, y):
    for file_name, lines in y.iteritems():
      x.setdefault(file_name, set([])).update(lines)
    return x
  result = reduce(merge_files, results, {})

  # Return data as file->lines mapping. The lines are saved as lists
  # with (line number, test bits (as int)). The test bits are initialized with
  # 0, meaning instrumented, but no coverage.
  # The order of the test bits is given with key 'tests'. For now, these are
  # the executable names. We use a _list_ with two items instead of a tuple to
  # ease merging by allowing mutation of the second item.
  return {
    'version': 1,
    'tests': sorted(map(os.path.basename, exe_list)),
    'files': {f: map(lambda l: [l, 0], sorted(result[f])) for f in result},
  }


def write_instrumented(options):
  """Implements the 'all' action of this tool."""
  exe_list = list(executables(options.build_dir))
  logging.info('Reading instrumented lines from %d executables.',
               len(exe_list))
  pool = Pool(CPUS)
  try:
    results = pool.imap_unordered(get_instrumented_lines, exe_list)
  finally:
    pool.close()

  # Merge multiprocessing results and prepare output data.
  data = merge_instrumented_line_results(exe_list, results)

  logging.info('Read data from %d executables, which covers %d files.',
               len(data['tests']), len(data['files']))
  logging.info('Writing results to %s', options.json_output)

  # Write json output.
  with open(options.json_output, 'w') as f:
    json.dump(data, f, sort_keys=True)


def get_covered_lines(args):
  """Return the covered lines of an executable.

  Called trough multiprocessing pool. The args are expected to unpack to:
    cov_dir: Folder with sancov files merged by sancov_merger.py.
    executable: Absolute path to the executable that was called to produce the
                given coverage data.
    sancov_file: The merged sancov file with coverage data.

  Returns: A tuple of post-processed llvm output as returned by
           process_symbolizer_output and the executable name.
  """
  cov_dir, executable, sancov_file = args

  # Let the sancov tool print the covered PCs and pipe them through the llvm
  # symbolizer.
  process = subprocess.Popen(
      '%s print %s 2> /dev/null | '
      '%s --obj %s -functions=none' %
          (SANCOV_TOOL,
           os.path.join(cov_dir, sancov_file),
           SYMBOLIZER,
           executable),
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.PIPE,
      cwd=BASE_DIR,
      shell=True,
  )
  output, _ = process.communicate()
  assert process.returncode == 0
  return (
      process_symbolizer_output(output, os.path.dirname(executable)),
      os.path.basename(executable),
  )


def merge_covered_line_results(data, results):
  """Merge multiprocessing results for covered lines.

  The data is mutated, the results are merged into it in place.

  Args:
    data: Existing coverage data from json file containing all instrumented
          lines.
    results: List of results as returned by get_covered_lines.
  """

  # List of executables and mapping to the test bit mask. The number of
  # tests is restricted to 52, to allow javascript JSON parsing of
  # the bitsets encoded as numbers. JS max safe int is (1 << 53) - 1.
  exe_list = data['tests']
  assert len(exe_list) <= 52, 'Max 52 different tests are supported.'
  test_bit_masks = {exe:1<<i for i, exe in enumerate(exe_list)}

  def merge_lines(old_lines, new_lines, mask):
    """Merge the coverage data of a list of lines.

    Args:
      old_lines: Lines as list of pairs with line number and test bit mask.
                 The new lines will be merged into the list in place.
      new_lines: List of new (covered) lines (sorted).
      mask: The bit to be set for covered lines. The bit index is the test
            index of the executable that covered the line.
    """
    i = 0
    # Iterate over old and new lines, both are sorted.
    for l in new_lines:
      while old_lines[i][0] < l:
        # Forward instrumented lines not present in this coverage data.
        i += 1
        # TODO: Add more context to the assert message.
        assert i < len(old_lines), 'Covered line %d not in input file.' % l
      assert old_lines[i][0] == l, 'Covered line %d not in input file.' % l

      # Add coverage information to the line.
      old_lines[i][1] |= mask

  def merge_files(data, result):
    """Merge result into data.

    The data is mutated in place.

    Args:
      data: Merged coverage data from the previous reduce step.
      result: New result to be merged in. The type is as returned by
              get_covered_lines.
    """
    file_map, executable = result
    files = data['files']
    for file_name, lines in file_map.iteritems():
      merge_lines(files[file_name], lines, test_bit_masks[executable])
    return data

  reduce(merge_files, results, data)


def merge(options):
  """Implements the 'merge' action of this tool."""

  # Check if folder with coverage output exists.
  assert (os.path.exists(options.coverage_dir) and
          os.path.isdir(options.coverage_dir))

  # Inputs for multiprocessing. List of tuples of:
  # Coverage dir, absoluate path to executable, sancov file name.
  inputs = []
  for sancov_file in os.listdir(options.coverage_dir):
    match = SANCOV_FILE_RE.match(sancov_file)
    if match:
      inputs.append((
          options.coverage_dir,
          os.path.join(options.build_dir, match.group(1)),
          sancov_file,
      ))

  logging.info('Merging %d sancov files into %s',
               len(inputs), options.json_input)

  # Post-process covered lines in parallel.
  pool = Pool(CPUS)
  try:
    results = pool.imap_unordered(get_covered_lines, inputs)
  finally:
    pool.close()

  # Load existing json data file for merging the results.
  with open(options.json_input, 'r') as f:
    data = json.load(f)

  # Merge muliprocessing results. Mutates data.
  merge_covered_line_results(data, results)

  logging.info('Merged data from %d executables, which covers %d files.',
               len(data['tests']), len(data['files']))
  logging.info('Writing results to %s', options.json_output)

  # Write merged results to file.
  with open(options.json_output, 'w') as f:
    json.dump(data, f, sort_keys=True)


def split(options):
  """Implements the 'split' action of this tool."""
  # Load existing json data file for splitting.
  with open(options.json_input, 'r') as f:
    data = json.load(f)

  logging.info('Splitting off %d coverage files from %s',
               len(data['files']), options.json_input)

  for file_name, coverage in data['files'].iteritems():
    # Preserve relative directories that are part of the file name.
    file_path = os.path.join(options.output_dir, file_name + '.json')
    try:
      os.makedirs(os.path.dirname(file_path))
    except OSError:
      # Ignore existing directories.
      pass

    with open(file_path, 'w') as f:
      # Flat-copy the old dict.
      new_data = dict(data)

      # Update current file.
      new_data['files'] = {file_name: coverage}

      # Write json data.
      json.dump(new_data, f, sort_keys=True)


def main(args=None):
  parser = argparse.ArgumentParser()
  # TODO(machenbach): Make this required and deprecate the default.
  parser.add_argument('--build-dir',
                      default=os.path.join(BASE_DIR, 'out', 'Release'),
                      help='Path to the build output directory.')
  parser.add_argument('--coverage-dir',
                      help='Path to the sancov output files.')
  parser.add_argument('--json-input',
                      help='Path to an existing json file with coverage data.')
  parser.add_argument('--json-output',
                      help='Path to a file to write json output to.')
  parser.add_argument('--output-dir',
                      help='Directory where to put split output files to.')
  parser.add_argument('action', choices=['all', 'merge', 'split'],
                      help='Action to perform.')

  options = parser.parse_args(args)
  options.build_dir = os.path.abspath(options.build_dir)
  if options.action.lower() == 'all':
    if not options.json_output:
      print('--json-output is required')
      return 1
    write_instrumented(options)
  elif options.action.lower() == 'merge':
    if not options.coverage_dir:
      print('--coverage-dir is required')
      return 1
    if not options.json_input:
      print('--json-input is required')
      return 1
    if not options.json_output:
      print('--json-output is required')
      return 1
    merge(options)
  elif options.action.lower() == 'split':
    if not options.json_input:
      print('--json-input is required')
      return 1
    if not options.output_dir:
      print('--output-dir is required')
      return 1
    split(options)
  return 0


if __name__ == '__main__':
  sys.exit(main())
