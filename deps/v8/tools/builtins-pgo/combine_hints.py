#!/usr/bin/env python3

# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
"""
This script combines the branch hints for profile-guided optimization
produced by get_hints.py. The hints can simply be concatenated in priority order
instead of using this script if the earliest seen hint is to be used.

Usage: combine_hints.py combine-option N output_file hints_file_1 weight_1 hints_file_2 weight_2 ...

where weights_n is the integer weight applied to the hints in hints_file_n
and combine-option N is one of the below:
    diff N:   Only use the hint when the weighted sum of the hints in one
              direction is equal to or greater than the weighted sum of hints
              in the opposite direction by at least N.
    agreed N: Only use the hint if every file containing this branch agrees
              and the weighted sum of these hints is at least N.

Using diff num_input_files and using a weight of 1 for every hints_file will
give the strict intersection of all files.
"""

import argparse
import sys

PARSER = argparse.ArgumentParser(
    description="A script that combines the hints produced by get_hints.py",
    epilog="Example:\n\tcombine_hints.py combine-option N output_file hints_file_1 2 hints_file_2 1 ...\""
)
PARSER.add_argument(
    'combine_option',
    choices=['diff', 'agreed'],
    help="The combine option dictates how the hints will be combined, diff \
          only uses the hint if the positive/negative hints outweigh the \
          negative/positive hints by N, while agreed only uses the hint if \
          the weighted sum of hints in one direction matches or exceeds N and \
          no conflicting hints are found.")
PARSER.add_argument(
    'weight_threshold',
    type=int,
    help="The threshold value which the hint's weight must match or exceed \
          to be used.")
PARSER.add_argument(
    'output_file',
    help="The file which the hints and builtin hashes are written to")

PARSER.add_argument(
    'hint_files_and_weights',
    nargs=argparse.REMAINDER,
    help="The hint files produced by get_hints.py along with their weights")

ARGS = vars(PARSER.parse_args())

BRANCH_HINT_MARKER = "block_hint"
BUILTIN_HASH_MARKER = "builtin_hash"

must_agree = ARGS['combine_option'] == "agreed"
weight_threshold = max(1, ARGS['weight_threshold'])

hint_args = ARGS['hint_files_and_weights']
hint_files_and_weights = zip(hint_args[0::2], hint_args[1::2])


def add_branch_hints(hint_file, weight, branch_hints, builtin_hashes):
  try:
    with open(hint_file, "r") as f:
      for line in f.readlines():
        fields = line.split(',')
        if fields[0] == BRANCH_HINT_MARKER:
          builtin_name = fields[1]
          true_block_id = int(fields[2])
          false_block_id = int(fields[3])
          key = (builtin_name, true_block_id, false_block_id)
          delta = weight if (int(fields[4]) > 0) else -weight
          if key not in branch_hints:
            if must_agree:
              # The boolean value records whether or not any conflicts have been
              # found for this branch.
              initial_hint = (False, 0)
            else:
              initial_hint = 0
            branch_hints[key] = initial_hint
          if must_agree:
            (has_conflicts, count) = branch_hints[key]
            if not has_conflicts:
              if abs(delta) + abs(count) == abs(delta + count):
                branch_hints[key] = (False, count + delta)
              else:
                branch_hints[key] = (True, 0)
          else:
            branch_hints[key] += delta
        elif fields[0] == BUILTIN_HASH_MARKER:
          builtin_name = fields[1]
          builtin_hash = int(fields[2])
          if builtin_name in builtin_hashes:
            if builtin_hashes[builtin_name] != builtin_hash:
              print("Builtin hashes {} and {} for {} do not match.".format(
                  builtin_hashes[builtin_name], builtin_hash, builtin_name))
              sys.exit(1)
          else:
            builtin_hashes[builtin_name] = builtin_hash
  except IOError as e:
    print("Cannot read from {}. {}.".format(hint_file, e.strerror))
    sys.exit(1)


def write_hints_to_output(output_file, branch_hints, builtin_hashes):
  try:
    with open(output_file, "w") as f:
      for key in branch_hints:
        if must_agree:
          (has_conflicts, count) = branch_hints[key]
          if has_conflicts:
            count = 0
        else:
          count = branch_hints[key]
        if abs(count) >= abs(weight_threshold):
          hint = 1 if count > 0 else 0
          f.write("{},{},{},{},{}\n".format(BRANCH_HINT_MARKER, key[0], key[1],
                                            key[2], hint))
      for builtin_name in builtin_hashes:
        f.write("{},{},{}\n".format(BUILTIN_HASH_MARKER, builtin_name,
                                    builtin_hashes[builtin_name]))
  except IOError as e:
    print("Cannot write to {}. {}.".format(output_file, e.strerror))
    sys.exit(1)


branch_hints = {}
builtin_hashes = {}
for (hint_file, weight) in hint_files_and_weights:
  add_branch_hints(hint_file, int(weight), branch_hints, builtin_hashes)

write_hints_to_output(ARGS['output_file'], branch_hints, builtin_hashes)
