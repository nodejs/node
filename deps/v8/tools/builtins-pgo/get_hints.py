#!/usr/bin/env python3

# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
"""
This script generates the branch hints for profile-guided optimization of
the builtins in the following format:

block_hint,<builtin_name>,<basic_block_id_for_true_destination>,<basic_block_id_for_false_destination>,<hint>

where hint is an integer representation of the expected boolean result of the
branch condition. The expected boolean result is generated for a specific given
branch by comparing the counts of the two destination basic blocks. V8's
control flow graph is always in edge-split form, guaranteeing that each
destination block only has a single predecessor, and thus guaranteeing that the
execution counts of these basic blocks are equal to how many times the branch
condition is true or false.

Usage: get_hints.py [--min MIN] [--ratio RATIO] log_file output_file

where:
    1. log_file is the file produced after running v8 with the
       --turbo-profiling-output=log_file flag after building with
       v8_enable_builtins_profiling = true.
    2. output_file is the file which the hints and builtin hashes are written
       to.
    3. --min MIN provides the minimum count at which a basic block will be taken
       as a valid destination of a hinted branch decision.
    4. --ratio RATIO provides the ratio at which, when compared to the
       alternative destination's count, a branch destination's count is
       considered sufficient to require a branch hint to be produced.
"""

import argparse
import sys

PARSER = argparse.ArgumentParser(
    description="A script that generates the branch hints for profile-guided \
                optimization",
    epilog="Example:\n\tget_hints.py --min n1 --ratio n2 branches_file log_file output_file\""
)
PARSER.add_argument(
    '--min',
    type=int,
    default=1000,
    help="The minimum count at which a basic block will be taken as a valid \
          destination of a hinted branch decision")
PARSER.add_argument(
    '--ratio',
    type=int,
    default=40,
    help="The ratio at which, when compared to the alternative destination's \
          count,a branch destination's count is considered sufficient to \
          require a branch hint to be produced")
PARSER.add_argument(
    'log_file',
    help="The v8.log file produced after running v8 with the --turbo-profiling-output=log_file flag after building with v8_enable_builtins_profiling = true"
)
PARSER.add_argument(
    'output_file',
    help="The file which the hints and builtin hashes are written to")

ARGS = vars(PARSER.parse_args())

BLOCK_COUNT_MARKER = "block"
BRANCH_HINT_MARKER = "block_hint"
BUILTIN_HASH_MARKER = "builtin_hash"
NORMALIZED_BLOCK_COUNT_MARKER = "block_count"
NORMALIZED_BUILTIN_COUNT_MARKER = "builtin_count"

MAX_NORMALIZED_COUNT = 10000


def parse_log_file(log_file):
  block_counts = {}
  branches = []
  builtin_hashes = {}
  max_execution_count = 0
  try:
    with open(log_file, "r") as f:
      for line in f.readlines():
        fields = line.split('\t')
        if fields[0] == BLOCK_COUNT_MARKER:
          builtin_name = fields[1]
          block_id = int(fields[2])
          count = float(fields[3])
          if block_id == 0 and count > max_execution_count:
            max_execution_count = count
          if builtin_name not in block_counts:
            block_counts[builtin_name] = []
          while len(block_counts[builtin_name]) <= block_id:
            block_counts[builtin_name].append(0)
          block_counts[builtin_name][block_id] += count
        elif fields[0] == BUILTIN_HASH_MARKER:
          builtin_name = fields[1]
          builtin_hash = int(fields[2])
          if builtin_name in builtin_hashes:
            old_hash = builtin_hashes[builtin_name]
            assert old_hash == builtin_hash, (
                "Merged PGO file contains multiple incompatible builtin "
                "versions: {old_hash} != {builtin_hash}")
          else:
            builtin_hashes[builtin_name] = builtin_hash
        elif fields[0] == BRANCH_HINT_MARKER:
          builtin_name = fields[1]
          true_block_id = int(fields[2])
          false_block_id = int(fields[3])
          branches.append((builtin_name, true_block_id, false_block_id))
  except IOError as e:
    print(f"Cannot read from {log_file}. {e.strerror}.")
    sys.exit(1)
  return [block_counts, branches, builtin_hashes, max_execution_count]


def get_branch_hints(block_counts, branches, min_count, threshold_ratio):
  branch_hints = {}
  for (builtin_name, true_block_id, false_block_id) in branches:
    if builtin_name in block_counts:
      true_block_count = 0
      false_block_count = 0
      if true_block_id < len(block_counts[builtin_name]):
        true_block_count = block_counts[builtin_name][true_block_id]
      if false_block_id < len(block_counts[builtin_name]):
        false_block_count = block_counts[builtin_name][false_block_id]
      hint = -1
      if (true_block_count >= min_count) and (true_block_count / threshold_ratio
                                              >= false_block_count):
        hint = 1
      elif (false_block_count >= min_count) and (
          false_block_count / threshold_ratio >= true_block_count):
        hint = 0
      if hint >= 0:
        branch_hints[(builtin_name, true_block_id, false_block_id)] = hint
  return branch_hints


def normalize_count(block_counts, max_count):
  normalized_block_counts = {}

  for builtin in block_counts:
    for block, count in enumerate(block_counts[builtin]):
      block_count_normalized = int(count * MAX_NORMALIZED_COUNT / max_count)
      if builtin not in normalized_block_counts:
        normalized_block_counts[builtin] = {}
      normalized_block_counts[builtin][block] = block_count_normalized

  return normalized_block_counts


def write_hints_to_output(output_file, branch_hints, builtin_hashes,
                          block_counts):
  try:
    with open(output_file, "w") as f:
      for key in branch_hints:
        f.write("{},{},{},{},{}\n".format(BRANCH_HINT_MARKER, key[0], key[1],
                                          key[2], branch_hints[key]))
      # Put the NORMALIZED_BUILTIN_COUNT_MARKER before NORMALIZED_BLOCK_COUNT_MARKER,
      # because we will use them to calculate call probability.
      # The normalized count of a builtin/block means how much frequently the
      # builtin/block was executed.
      # We will take it as "density" of the builtin/block in post process, hence
      # the execution time could be "density * size".
      for builtin_name in block_counts.keys():
        f.write("{},{},{}\n".format(NORMALIZED_BUILTIN_COUNT_MARKER,
                                    builtin_name,
                                    block_counts[builtin_name][0]))

      for builtin_name in block_counts:
        for block_id in block_counts[builtin_name]:
          f.write("{},{},{},{}\n".format(NORMALIZED_BLOCK_COUNT_MARKER,
                                         builtin_name, block_id,
                                         block_counts[builtin_name][block_id]))

      for builtin_name in builtin_hashes:
        f.write("{},{},{}\n".format(BUILTIN_HASH_MARKER, builtin_name,
                                    builtin_hashes[builtin_name]))
  except IOError as e:
    print(f"Cannot read from {output_file}. {e.strerror}.")
    sys.exit(1)


[block_counts, branches, builtin_hashes,
 max_count] = parse_log_file(ARGS['log_file'])
branch_hints = get_branch_hints(block_counts, branches, ARGS['min'],
                                ARGS['ratio'])

block_counts = normalize_count(block_counts, max_count)

write_hints_to_output(ARGS['output_file'], branch_hints, builtin_hashes,
                      block_counts)
